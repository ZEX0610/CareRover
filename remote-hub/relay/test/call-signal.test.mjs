import test from 'node:test';
import assert from 'node:assert/strict';
import { CallSignalTracker } from '../public/js/call-signal.js';
import { decode } from '../public/js/protocol.js';

test('new CALL state invites once and the next gesture hangs up once', () => {
  const tracker = new CallSignalTracker();
  assert.equal(tracker.update({ requested: false, sequence: 0 }), null);
  assert.equal(tracker.update({ requested: true, sequence: 1 }), 'invite');
  assert.equal(tracker.update({ requested: true, sequence: 1 }), null);
  assert.equal(tracker.update({ requested: false, sequence: 2 }), 'hangup');
  assert.equal(tracker.update({ requested: false, sequence: 2 }), null);
});

test('joining while child is calling shows an invitation; invalid frames do not act', () => {
  const tracker = new CallSignalTracker();
  assert.equal(tracker.update({ requested: true, sequence: 7 }), 'invite');
  assert.equal(tracker.update({ requested: true, sequence: 7.5 }), null);
  assert.equal(tracker.update({ requested: 'true', sequence: 8 }), null);
  const decoded = decode(JSON.stringify({ type: 'telemetry', call: { requested: false, sequence: 8 } }));
  assert.equal(decoded.ok, true);
  assert.deepEqual(decoded.msg.call, { requested: false, sequence: 8 });
  assert.equal(tracker.update(decoded.msg.call), 'hangup');
});
