import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';
import vm from 'node:vm';

test('phone touch hold stays active until touchend and exposes capture/send counters', async () => {
  const elements = new Map();
  for (const id of ['state', 'audioStats', 'token', 'connect', 'disconnect', 'talk']) {
    const target = new EventTarget();
    target.textContent = '';
    target.value = '';
    elements.set(id, target);
  }
  const document = new EventTarget();
  document.getElementById = (id) => elements.get(id);
  const window = new EventTarget();
  const script = await readFile(new URL('../public/client.js', import.meta.url), 'utf8');
  vm.runInNewContext(script, { document, window, setTimeout, clearTimeout });
  const touchStart = new Event('touchstart', { cancelable: true });
  elements.get('talk').dispatchEvent(touchStart);
  assert.equal(touchStart.defaultPrevented, true);
  assert.match(elements.get('audioStats').textContent, /按住中/);
  document.dispatchEvent(new Event('touchend'));
  assert.match(elements.get('audioStats').textContent, /已松开/);
  elements.get('disconnect').dispatchEvent(new Event('click'));
  await Promise.resolve();
  assert.equal(elements.get('state').textContent, '已结束通话，麦克风已关闭。');
});

test('closing the call while microphone permission is pending stops the late stream', async () => {
  const elements = new Map();
  for (const id of ['state', 'audioStats', 'token', 'connect', 'disconnect', 'talk']) {
    const target = new EventTarget();
    target.textContent = '';
    target.value = '';
    elements.set(id, target);
  }
  const document = new EventTarget();
  document.getElementById = (id) => elements.get(id);
  const window = new EventTarget();
  window.isSecureContext = true;
  let allowMicrophone;
  const navigator = { mediaDevices: { getUserMedia: () => new Promise((resolve) => { allowMicrophone = resolve; }) } };
  const script = await readFile(new URL('../public/client.js', import.meta.url), 'utf8');
  vm.runInNewContext(script, { document, window, navigator, setTimeout, clearTimeout, WebSocket: class { constructor() { throw new Error('call reopened after close'); } } });
  elements.get('connect').dispatchEvent(new Event('click'));
  window.dispatchEvent(new Event('carerover-call-close'));
  let stopped = false;
  allowMicrophone({ getTracks: () => [{ stop: () => { stopped = true; } }] });
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(stopped, true);
  assert.equal(elements.get('connect').disabled, false);
});
