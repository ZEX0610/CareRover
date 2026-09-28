import test from 'node:test';
import assert from 'node:assert/strict';
import { CareEventTracker } from '../js/care-events.js';
import { decode } from '../js/protocol.js';

function frame(found, { camera = true, seated = true, call = { requested: false, sequence: 0 } } = {}) {
  const parsed = decode(JSON.stringify({ type: 'telemetry', connection: { camera },
    robot: { mode: 'IDLE' }, vision: { person: { found, predicted: false, age_ms: 0 } },
    front: { valid: true, distance_cm: 50, age_ms: 0, seated }, call }));
  assert.equal(parsed.ok, true);
  return parsed.msg;
}

test('local R5 page raises one 30-second no-face banner and removes it on a fresh face', () => {
  const tracker = new CareEventTracker();
  assert.deepEqual(tracker.update(frame(false), 0), []);
  assert.deepEqual(tracker.update(frame(false), 29_999), []);
  assert.deepEqual(tracker.update(frame(false), 30_000), ['face_absent']);
  assert.deepEqual(tracker.update(frame(false), 31_000), []);
  assert.deepEqual(tracker.update(frame(true), 31_100), ['face_restored']);
});

test('local seating and CALL provide distinct events; initial state does not claim arrival', () => {
  const tracker = new CareEventTracker();
  assert.deepEqual(tracker.update(frame(true), 0), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false }), 100), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false }), 900), ['seat_left']);
  assert.deepEqual(tracker.update(frame(true, { seated: false,
    call: { requested: true, sequence: 1 } }), 1000), ['call_invite']);
  assert.deepEqual(tracker.update(frame(true, { seated: false,
    call: { requested: true, sequence: 1 } }), 1100), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false,
    call: { requested: false, sequence: 2 } }), 1200), ['call_cancelled']);
});
