import test from 'node:test';
import assert from 'node:assert/strict';
import { CareEventTracker } from '../public/js/care-events.js';

function frame({ camera = true, found = false, predicted = false, seat = false,
  frontValid = true, mode = 'IDLE', call = { requested: false, sequence: 0 } } = {}) {
  return { type: 'telemetry', connection: { camera }, robot: { mode },
    vision: { person: { found, predicted, age_ms: 0 } },
    front: { valid: frontValid, seated: seat }, call };
}

test('30 seconds of fresh camera no-face raises face-based departure once; real face clears', () => {
  const tracker = new CareEventTracker();
  for (let at = 0; at < 30_000; at += 1000)
    assert.deepEqual(tracker.update(frame({ seat: at % 2000 === 0 }), at), []);
  assert.equal(tracker.update(frame(), 30_000)[0].kind, 'seat_left');
  assert.deepEqual(tracker.update(frame(), 31_000), []);
  assert.deepEqual(tracker.update(frame({ found: true, predicted: true }), 31_100), []);
  assert.equal(tracker.update(frame({ found: true }), 31_200)[0].kind, 'seat_returned');
  assert.equal(tracker.snapshot().face_alert_active, false);
});

test('ultrasonic seating state and robot mode do not cause seat events', () => {
  const tracker = new CareEventTracker();
  tracker.update(frame({ found: true, seat: true }), 0);
  assert.deepEqual(tracker.update(frame({ found: true, seat: false }), 10), []);
  assert.deepEqual(tracker.update(frame({ found: true, seat: false, frontValid: false }), 900), []);
  for (let at = 1000; at <= 60_000; at += 1000)
    assert.deepEqual(tracker.update(frame({ found: true, seat: false, mode: 'MANUAL' }), at), []);
  assert.equal(tracker.snapshot().seat, 'seated');
});

test('camera outage resets negative timer and cannot cause a departure', () => {
  const tracker = new CareEventTracker();
  for (let at = 0; at < 20_000; at += 1000) tracker.update(frame(), at);
  assert.deepEqual(tracker.update(frame({ camera: false }), 20_000), []);
  for (let at = 21_000; at < 51_000; at += 1000) tracker.update(frame(), at);
  assert.equal(tracker.update(frame(), 51_000)[0].kind, 'seat_left');
});

test('CALL sequence invites once and a second gesture cancels; initial requested state is replayed', () => {
  const tracker = new CareEventTracker();
  assert.equal(tracker.update(frame({ found: true, call: { requested: true, sequence: 4 } }), 0)[0].kind, 'call_invite');
  assert.deepEqual(tracker.update(frame({ found: true, call: { requested: true, sequence: 4 } }), 50), []);
  assert.equal(tracker.update(frame({ found: true, call: { requested: false, sequence: 5 } }), 100)[0].kind, 'call_cancelled');
});
