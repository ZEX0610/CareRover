import test from 'node:test';
import assert from 'node:assert/strict';
import { CareBanners } from '../js/care-banners.js';

function fakeElement() {
  return { children: [], className: '', setAttribute() {},
    append(...items) { this.children.push(...items); },
    prepend(item) { this.children.unshift(item); },
    remove() { this.removed = true; } };
}

test('local departure and arrival banners clear themselves and do not remain sticky', () => {
  const originalDocument = globalThis.document;
  const originalSetTimeout = globalThis.setTimeout;
  const originalClearTimeout = globalThis.clearTimeout;
  const timers = new Map(); let nextTimer = 1;
  globalThis.document = { createElement: fakeElement, body: fakeElement() };
  globalThis.setTimeout = (callback, delay) => {
    assert.equal(delay, 6500); const id = nextTimer++; timers.set(id, callback); return id;
  };
  globalThis.clearTimeout = id => timers.delete(id);
  try {
    const banners = new CareBanners();
    banners.message('seat_left');
    assert.equal(banners.active.has('seat_left'), true);
    banners.message('seat_returned');
    assert.equal(banners.active.has('seat_left'), false);
    assert.equal(banners.active.has('seat_returned'), true);
    const [id, callback] = [...timers][0];
    timers.delete(id); callback();
    assert.equal(banners.active.has('seat_returned'), false);
    banners.destroy();
  } finally {
    globalThis.document = originalDocument;
    globalThis.setTimeout = originalSetTimeout;
    globalThis.clearTimeout = originalClearTimeout;
  }
});
