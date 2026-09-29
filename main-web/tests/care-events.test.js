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

test('local R5 page emits face-based departure once and arrival on a fresh face', () => {
  const tracker = new CareEventTracker();
  for (let at = 0; at < 30_000; at += 1000)
    assert.deepEqual(tracker.update(frame(false, { seated: at % 2000 === 0 }), at), []);
  assert.deepEqual(tracker.update(frame(false), 30_000), ['seat_left']);
  assert.deepEqual(tracker.update(frame(false), 31_000), []);
  assert.deepEqual(tracker.update(frame(true), 31_100), ['seat_returned']);
});

test('ultrasonic seating changes never produce departure or arrival; CALL still works', () => {
  const tracker = new CareEventTracker();
  assert.deepEqual(tracker.update(frame(true), 0), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false }), 100), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false }), 900), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false,
    call: { requested: true, sequence: 1 } }), 1000), ['call_invite']);
  assert.deepEqual(tracker.update(frame(true, { seated: false,
    call: { requested: true, sequence: 1 } }), 1100), []);
  assert.deepEqual(tracker.update(frame(true, { seated: false,
    call: { requested: false, sequence: 2 } }), 1200), ['call_cancelled']);
});

test('camera outage and stale face observations never count toward the 30 seconds', () => {
  const tracker = new CareEventTracker();
  for (let at = 0; at < 20_000; at += 1000) tracker.update(frame(false), at);
  tracker.update(frame(false, { camera: false }), 20_000);
  for (let at = 21_000; at < 51_000; at += 1000) tracker.update(frame(false), at);
  assert.deepEqual(tracker.update(frame(false), 51_000), ['seat_left']);
});
