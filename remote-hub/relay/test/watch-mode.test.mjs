import test from 'node:test';
import assert from 'node:assert/strict';
import { decode, validateOutgoing } from '../public/js/protocol.js';

test('remote parent page displays watch mode but cannot request it', () => {
  const frame = decode(JSON.stringify({
    type: 'telemetry',
    robot: { mode: 'WATCH_CONTROL', state: 'READY', estop: false },
  }));
  assert.equal(frame.ok, true);
  assert.equal(frame.msg.robot.mode, 'WATCH_CONTROL');
  assert.ok(validateOutgoing({ type: 'set_mode', ts: 1, mode: 'WATCH_CONTROL' }));
});
