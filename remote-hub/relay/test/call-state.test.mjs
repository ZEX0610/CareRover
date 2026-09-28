import test from 'node:test';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import { WebSocket } from 'ws';
import { makeServer } from '../server.mjs';

const deviceToken = 'call-state-device-token-2026';
const parentToken = 'call-state-parent-token-2026';

async function connect(url, options) {
  const ws = new WebSocket(url, options.protocols, { headers: options.headers });
  await once(ws, 'open');
  return ws;
}
const message = (ws) => Promise.race([
  once(ws, 'message'),
  new Promise((_, reject) => setTimeout(() => reject(new Error('call state message timed out')), 500)),
]);

test('parent audio connection announces active and inactive call to device audio socket', async () => {
  const app = await makeServer({ localOnly: true, deviceToken, parentToken });
  const root = `http://127.0.0.1:${app.address.port}`;
  const url = `${root.replace('http:', 'ws:')}/audio`;
  let device, parent;
  try {
    device = new WebSocket(url, { headers: { Authorization: `Bearer ${deviceToken}` } });
    const initial = message(device);
    await once(device, 'open');
    assert.deepEqual(JSON.parse((await initial)[0].toString()), { type: 'call_state', active: false });
    const connected = message(device);
    parent = await connect(url, { protocols: ['audio-v1', `parent.${parentToken}`],
      headers: { Origin: root } });
    assert.deepEqual(JSON.parse((await connected)[0].toString()), { type: 'call_state', active: true });
    const disconnected = message(device);
    parent.close();
    await once(parent, 'close');
    assert.deepEqual(JSON.parse((await disconnected)[0].toString()), { type: 'call_state', active: false });
  } finally {
    device?.terminate(); parent?.terminate(); await app.close();
  }
});
