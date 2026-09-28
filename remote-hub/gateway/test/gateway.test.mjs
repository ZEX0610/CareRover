import test from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { WebSocket, WebSocketServer } from 'ws';
import { createGateway, MjpegParser } from '../gateway.mjs';
import { makeServer } from '../../relay/server.mjs';

const token = 'local-device-token-for-gateway-test';
const parentToken = 'local-parent-token-for-gateway-test';
const jpeg = Buffer.from([0xff, 0xd8, 0xff, 0xd9]);
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function until(check, ms = 5000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { if (await check()) return; await delay(50); }
  assert.fail('Timed out waiting for gateway state');
}
const listen = (server) => new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const close = (server) => new Promise((resolve) => server.close(resolve));

test('MJPEG parser handles split headers and frames and rejects oversized input', () => {
  const found = [];
  const parser = new MjpegParser((frame) => found.push(Buffer.from(frame)));
  const part = Buffer.concat([Buffer.from('--carerover\r\nContent-Type: image/jpeg\r\nContent-Length: 4\r\n\r\n'), jpeg, Buffer.from('\r\n')]);
  parser.push(part.subarray(0, 16));
  parser.push(Buffer.concat([part.subarray(16), part]));
  assert.deepEqual(found, [jpeg, jpeg]);
  assert.throws(() => parser.push(Buffer.alloc(1025, 65)), /header too large/);
});

test('gateway couples board telemetry and remote control, uploads live CAM frames', async () => {
  const boardHttp = createServer();
  const boardWs = new WebSocketServer({ server: boardHttp, path: '/ws' });
  const audioHttp = createServer();
  const audioWs = new WebSocketServer({ server: audioHttp, path: '/audio' });
  const callStates = [];
  audioWs.on('connection', (ws) => ws.on('message', (data, binary) => {
    if (binary) ws.send(data);
    else callStates.push(JSON.parse(data.toString()));
  }));
  const camHttp = createServer((req, res) => {
    assert.equal(req.url, '/stream');
    res.writeHead(200, { 'Content-Type': 'multipart/x-mixed-replace;boundary=carerover' });
    const frame = Buffer.concat([Buffer.from('--carerover\r\nContent-Type: image/jpeg\r\nContent-Length: 4\r\n\r\n'), jpeg, Buffer.from('\r\n')]);
    const timer = setInterval(() => res.write(frame), 100);
    req.on('close', () => clearInterval(timer));
  });
  await Promise.all([listen(boardHttp), listen(camHttp), listen(audioHttp)]);
  let boardPing = false, boardReceived = false;
  boardWs.on('connection', (ws) => {
    ws.on('message', (data) => {
      const msg = JSON.parse(data.toString());
      if (msg.type === 'ping') {
        boardPing = true;
        ws.send(JSON.stringify({ type: 'pong', ts: Date.now(), id: msg.id }));
      }
      if (msg.type === 'set_mode') boardReceived = true;
    });
  });
  const app = await makeServer({ deviceToken: token, parentToken, localOnly: true });
  const root = `http://127.0.0.1:${app.address.port}`;
  const gateway = createGateway({ boardUrl: `ws://127.0.0.1:${boardHttp.address().port}/ws`,
    cameraUrl: `http://127.0.0.1:${camHttp.address().port}/stream`, relayUrl: root,
    audioUrl: `ws://127.0.0.1:${audioHttp.address().port}/audio`,
    audioEnabled: true, deviceToken: token });
  let parent, parentAudio;
  try {
    gateway.start();
    await until(() => gateway.stats.boardUp && gateway.stats.relayUp &&
      gateway.stats.audioBoardUp && gateway.stats.audioRelayUp && boardPing && gateway.stats.framesUploaded >= 2);
    await until(() => callStates.some((s) => s.type === 'call_state' && s.active === false));
    assert.equal((await (await fetch(`${root}/health`)).json()).videoFresh, true);
    const login = await fetch(`${root}/login`, { method: 'POST', redirect: 'manual',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: `token=${parentToken}` });
    const cookie = login.headers.get('set-cookie').split(';')[0];
    parent = new WebSocket(`ws://127.0.0.1:${app.address.port}/ws`, { headers: { Cookie: cookie, Origin: root } });
    await new Promise((resolve, reject) => { parent.once('open', resolve); parent.once('error', reject); });
    parent.send(JSON.stringify({ type: 'set_mode', mode: 'IDLE', ts: Date.now() }));
    await until(() => boardReceived);
    assert.equal((await (await fetch(`${root}/health`)).json()).controlDevice, true);
    let callSignal;
    parent.on('message', (data) => {
      const message = JSON.parse(data.toString());
      if (message.type === 'telemetry') callSignal = message.call;
    });
    for (const client of boardWs.clients)
      client.send(JSON.stringify({ type: 'telemetry', call: { requested: true, sequence: 1 } }));
    await until(() => callSignal?.sequence === 1);
    assert.deepEqual(callSignal, { requested: true, sequence: 1 });
    parentAudio = new WebSocket(`ws://127.0.0.1:${app.address.port}/audio`,
      ['audio-v1', `parent.${parentToken}`], { headers: { Origin: root } });
    await new Promise((resolve, reject) => { parentAudio.once('open', resolve); parentAudio.once('error', reject); });
    await until(() => callStates.some((s) => s.type === 'call_state' && s.active === true));
    const packet = Buffer.alloc(648); packet.set([67, 82, 1, 1]);
    parentAudio.send(packet);
    await until(() => gateway.stats.audioDownFrames > 0);
    assert.equal(gateway.stats.audioUpFrames, 0, 'speaker echo must not return while parent speaks');
    await delay(750);
    for (const client of audioWs.clients) client.send(packet);
    await until(() => gateway.stats.audioUpFrames > 0);
    assert.equal((await (await fetch(`${root}/health`)).json()).audioPaired, true);
  } finally {
    parent?.terminate(); parentAudio?.terminate(); gateway.stop();
    await app.close(); boardWs.close(); audioWs.close();
    await Promise.all([close(boardHttp), close(camHttp), close(audioHttp)]);
  }
});
