import test from 'node:test';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import { WebSocket } from 'ws';
import { makeServer } from '../server.mjs';

const frame = (sequence) => {
  const data = Buffer.alloc(648);
  data.write('CR', 0); data[2] = 1; data[3] = 1;
  data.writeUInt32LE(sequence, 4);
  return data;
};
const deviceToken = 'device-secret-for-audio-lab-2026';
const parentToken = 'parent-secret-for-audio-lab-2026';

async function connect(url, options) {
  const ws = new WebSocket(url, options.protocols, { headers: options.headers });
  await once(ws, 'open');
  return ws;
}

test('authenticated bidirectional PCM relay; invalid packets are closed', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const url = `ws://127.0.0.1:${app.address.port}/audio`;
  let device, parent;
  try {
    device = await connect(url, { headers: { Authorization: `Bearer ${deviceToken}` } });
    parent = await connect(url, { protocols: ['audio-v1', `parent.${parentToken}`],
      headers: { Origin: `http://127.0.0.1:${app.address.port}` } });
    const towardParent = once(parent, 'message');
    device.send(frame(23));
    assert.deepEqual((await towardParent)[0], frame(23));
    const towardDevice = once(device, 'message');
    parent.send(frame(24));
    assert.deepEqual((await towardDevice)[0], frame(24));
    assert.equal(app.counters.deviceFrames, 1);
    assert.equal(app.counters.parentFrames, 1);
    const closed = once(parent, 'close');
    parent.send(Buffer.from('bad'));
    assert.equal((await closed)[0], 1003);
  } finally {
    device?.terminate(); parent?.terminate(); await app.close();
  }
});

test('wrong credential, foreign Origin and duplicate device are refused', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const url = `ws://127.0.0.1:${app.address.port}/audio`;
  let device;
  try {
    const bad = new WebSocket(url, { headers: { Authorization: 'Bearer wrong-secret-for-audio-lab-2026' } });
    await assert.rejects(once(bad, 'open'));
    device = await connect(url, { headers: { Authorization: `Bearer ${deviceToken}` } });
    const duplicate = new WebSocket(url, { headers: { Authorization: `Bearer ${deviceToken}` } });
    await assert.rejects(once(duplicate, 'open'));
    const foreign = new WebSocket(url, ['audio-v1', `parent.${parentToken}`],
      { headers: { Origin: 'http://evil.invalid' } });
    await assert.rejects(once(foreign, 'open'));
  } finally {
    device?.terminate(); await app.close();
  }
});

test('logged-in remote parent can receive car audio without entering the token twice', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  const url = `${root.replace('http:', 'ws:')}/audio`;
  let device, parent;
  try {
    const login = await fetch(`${root}/login`, { method: 'POST', redirect: 'manual',
      body: new URLSearchParams({ token: parentToken }) });
    assert.equal(login.status, 303);
    const cookie = login.headers.get('set-cookie').split(';')[0];
    const anonymous = new WebSocket(url, ['audio-v1'], { headers: { Origin: root } });
    await assert.rejects(once(anonymous, 'open'));
    const foreign = new WebSocket(url, ['audio-v1'],
      { headers: { Origin: 'http://other.invalid', Cookie: cookie } });
    await assert.rejects(once(foreign, 'open'));
    device = await connect(url, { headers: { Authorization: `Bearer ${deviceToken}` } });
    parent = await connect(url, { protocols: ['audio-v1'], headers: { Origin: root, Cookie: cookie } });
    const received = once(parent, 'message');
    device.send(frame(17));
    assert.deepEqual((await received)[0], frame(17));
  } finally {
    device?.terminate(); parent?.terminate(); await app.close();
  }
});

test('R5 call controls run in the top-level video page, not a nested iframe', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  try {
    const login = await fetch(`${root}/login`, { method: 'POST', redirect: 'manual',
      body: new URLSearchParams({ token: parentToken }) });
    const cookie = login.headers.get('set-cookie').split(';')[0];
    const html = await (await fetch(root, { headers: { Cookie: cookie } })).text();
    assert.match(html, /id="remoteCallDialog"/);
    assert.match(html, /id="connect"/);
    assert.match(html, /id="talk"/);
    assert.match(html, /src="\/client\.js"/);
    assert.doesNotMatch(html, /<iframe/);
  } finally { await app.close(); }
});

test('serves the separate parent page and forwards paced PCM frames locally', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  let device, parent;
  try {
    const page = await fetch(root);
    assert.equal(page.status, 200);
    assert.match(await page.text(), /家长登录/);
    const login = await fetch(`${root}/login`, { method: 'POST', redirect: 'manual',
      body: new URLSearchParams({ token: parentToken }) });
    assert.equal(login.status, 303);
    const cookie = login.headers.get('set-cookie').split(';')[0];
    const consoleHtml = await (await fetch(root, { headers: { Cookie: cookie } })).text();
    assert.match(consoleHtml, /ui-20260926-r5/);
    assert.match(consoleHtml, /id="remoteCallDialog"/);
    assert.match(consoleHtml, /css\/workspace\.css/);
    assert.equal((await fetch(`${root}/css/workspace.css`, { headers: { Cookie: cookie } })).status, 200);
    assert.equal((await fetch(`${root}/js/workspace.js`, { headers: { Cookie: cookie } })).status, 200);
    const remoteApp = await (await fetch(`${root}/js/app.js`, { headers: { Cookie: cookie } })).text();
    assert.match(remoteApp, /new URL\('\/stream', location\.href\)/);
    assert.match(await (await fetch(`${root}/call`, { headers: { Cookie: cookie } })).text(), /按住说话/);
    assert.equal((await fetch(`${root}/style.css`, { headers: { Cookie: cookie } })).status, 200);
    device = await connect(`${root.replace('http:', 'ws:')}/audio`,
      { headers: { Authorization: `Bearer ${deviceToken}` } });
    parent = await connect(`${root.replace('http:', 'ws:')}/audio`,
      { protocols: ['audio-v1', `parent.${parentToken}`], headers: { Origin: root } });
    let received = 0;
    parent.on('message', () => received++);
    for (let i = 0; i < 100; ++i) {
      device.send(frame(i));
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
    assert.equal(app.counters.deviceFrames, 100);
    assert.equal(received, 100);
    assert.equal(app.counters.dropped, 0);
  } finally {
    device?.terminate(); parent?.terminate(); await app.close();
  }
});

test('authenticated control bridge and bounded JPEG endpoint', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  let device, parent;
  try {
    const login = await fetch(`${root}/login`, { method: 'POST', redirect: 'manual',
      body: new URLSearchParams({ token: parentToken }) });
    const cookie = login.headers.get('set-cookie').split(';')[0];
    const control = root.replace('http:', 'ws:');
    device = await connect(`${control}/device/ws`, { headers: { Authorization: `Bearer ${deviceToken}` } });
    parent = await connect(`${control}/ws`, { headers: { Cookie: cookie, Origin: root } });
    const toDevice = once(device, 'message');
    parent.send(JSON.stringify({ type: 'ping', id: 1, ts: 123 }));
    assert.deepEqual(JSON.parse((await toDevice)[0].toString()), { type: 'ping', id: 1, ts: 123 });
    const toParent = once(parent, 'message');
    device.send(JSON.stringify({ type: 'pong', id: 1, ts: 123 }));
    assert.equal(JSON.parse((await toParent)[0].toString()).type, 'pong');
    const jpeg = Buffer.from([0xff, 0xd8, 1, 2, 0xff, 0xd9]);
    assert.equal((await fetch(`${root}/device/frame`, { method: 'POST',
      headers: { Authorization: `Bearer ${deviceToken}`, 'Content-Type': 'image/jpeg' }, body: jpeg })).status, 204);
    assert.equal((await (await fetch(`${root}/health`)).json()).videoFresh, true);
    assert.equal((await fetch(`${root}/device/frame`, { method: 'POST',
      headers: { 'Content-Type': 'image/jpeg' }, body: jpeg })).status, 403);
  } finally { device?.terminate(); parent?.terminate(); await app.close(); }
});

test('tailnet HTTPS proxy keeps the backend loopback-only and accepts exact browser origin', async () => {
  const publicOrigin = 'https://care-relay.example.ts.net';
  const app = await makeServer({ proxyOrigin: publicOrigin, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  let device, parent;
  try {
    assert.equal(app.address.address, '127.0.0.1');
    const login = await fetch(`${root}/login`, { method: 'POST', redirect: 'manual',
      body: new URLSearchParams({ token: parentToken }) });
    assert.equal(login.status, 303);
    assert.match(login.headers.get('set-cookie'), /; Secure(?:;|$)/);
    const cookie = login.headers.get('set-cookie').split(';')[0];
    const page = await fetch(root, { headers: { Cookie: cookie } });
    assert.equal(page.status, 200);
    assert.match(page.headers.get('content-security-policy'), /wss:\/\/care-relay\.example\.ts\.net/);
    const control = root.replace('http:', 'ws:');
    device = await connect(`${control}/device/ws`, { headers: { Authorization: `Bearer ${deviceToken}` } });
    const wrongOrigin = new WebSocket(`${control}/ws`,
      { headers: { Cookie: cookie, Origin: root } });
    await assert.rejects(once(wrongOrigin, 'open'));
    parent = await connect(`${control}/ws`, { headers: { Cookie: cookie, Origin: publicOrigin } });
    const received = once(device, 'message');
    parent.send(JSON.stringify({ type: 'ping', id: 7 }));
    assert.equal(JSON.parse((await received)[0].toString()).id, 7);
  } finally { device?.terminate(); parent?.terminate(); await app.close(); }
  await assert.rejects(makeServer({ proxyOrigin: 'http://care-relay.example.ts.net', deviceToken, parentToken }),
    /HTTPS origin/);
  await assert.rejects(makeServer({ proxyOrigin: publicOrigin, listenHost: '0.0.0.0', deviceToken, parentToken }),
    /127\.0\.0\.1/);
});
