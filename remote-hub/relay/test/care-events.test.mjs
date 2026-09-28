import test from 'node:test';
import assert from 'node:assert/strict';
import { CareEventTracker } from '../public/js/care-events.js';

function frame({ camera = true, found = false, predicted = false, seat = false,
  frontValid = true, mode = 'IDLE', call = { requested: false, sequence: 0 } } = {}) {
  return { type: 'telemetry', connection: { camera }, robot: { mode },
    vision: { person: { found, predicted, age_ms: 0 } },
    front: { valid: frontValid, seated: seat }, call };
}

test('30 seconds of fresh camera no-face raises once; real face clears; camera outage cannot raise', () => {
  const tracker = new CareEventTracker();
  assert.deepEqual(tracker.update(frame(), 0), []);
  assert.deepEqual(tracker.update(frame({ camera: false }), 29_000), []);
  assert.deepEqual(tracker.update(frame(), 30_000), []);
  assert.deepEqual(tracker.update(frame(), 59_999), []);
  assert.equal(tracker.update(frame(), 60_000)[0].kind, 'face_absent');
  assert.deepEqual(tracker.update(frame(), 70_000), []);
  assert.deepEqual(tracker.update(frame({ found: true, predicted: true }), 70_100), []);
  assert.equal(tracker.update(frame({ found: true }), 70_200)[0].kind, 'face_restored');
  assert.equal(tracker.snapshot().face_alert_active, false);
});

test('seat transitions are distinct from face and require stable valid IDLE observations', () => {
  const tracker = new CareEventTracker();
  tracker.update(frame({ found: true, seat: true }), 0);
  assert.deepEqual(tracker.update(frame({ found: true, seat: false }), 10), []);
  assert.deepEqual(tracker.update(frame({ found: true, seat: false, frontValid: false }), 900), []);
  tracker.update(frame({ found: true, seat: false }), 1000);
  assert.equal(tracker.update(frame({ found: true, seat: false }), 1800)[0].kind, 'seat_left');
  tracker.update(frame({ found: true, seat: true }), 2000);
  assert.equal(tracker.update(frame({ found: true, seat: true }), 2800)[0].kind, 'seat_returned');
  assert.equal(tracker.snapshot().seat, 'seated');
});

test('CALL sequence invites once and a second gesture cancels; initial requested state is replayed', () => {
  const tracker = new CareEventTracker();
  assert.equal(tracker.update(frame({ found: true, call: { requested: true, sequence: 4 } }), 0)[0].kind, 'call_invite');
  assert.deepEqual(tracker.update(frame({ found: true, call: { requested: true, sequence: 4 } }), 50), []);
  assert.equal(tracker.update(frame({ found: true, call: { requested: false, sequence: 5 } }), 100)[0].kind, 'call_cancelled');
});
