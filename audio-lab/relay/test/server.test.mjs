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

test('serves the separate parent page and forwards paced PCM frames locally', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  let device, parent;
  try {
    const page = await fetch(root);
    assert.equal(page.status, 200);
    assert.match(await page.text(), /按住说话/);
    assert.equal((await fetch(`${root}/style.css`)).status, 200);
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
