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
});
