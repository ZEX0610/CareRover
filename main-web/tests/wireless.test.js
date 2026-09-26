import test from 'node:test';
import assert from 'node:assert/strict';
import * as p from '../js/protocol.js';
import * as s from '../js/state.js';
import { CONFIG } from '../js/config.js';

test('independently invalidated HR/SpO2 explicitly clear old values', () => {
  const update=o=>s.applyTelemetry(p.decode(JSON.stringify({type:'telemetry',...o})).msg);
  update({health:{state:'VALID',finger_detected:true,hr_bpm:73,spo2_pct:98}});
  update({health:{state:'VALID',hr_bpm:74,spo2_pct:null}});
  assert.equal(s.getState().health.hr_bpm,74);
  assert.equal(s.getState().health.spo2_pct,null);
  update({health:{sqi:.9}}); assert.equal(s.getState().health.spo2_pct,null);
  update({health:{state:'NO_FINGER',finger_detected:false,hr_bpm:null,spo2_pct:null}});
  assert.equal(s.getState().health.hr_bpm,null);
});
test('wireless capabilities block non-owner input and reset on disconnect', () => {
  s.setConnectionState('connected'); s.markEstopLocal(false); s.setRequestedMode(null);
  s.applyTelemetry(p.decode(JSON.stringify({type:'telemetry',connection:{camera:true,main_mcu:true},robot:{mode:'MANUAL',state:'READY',estop:false,control_allowed:false,motion_output_installed:false},device:{firmware:'abc123',backend:'test_targets',stage:5}})).msg);
  assert.equal(s.isManualEnabled(),false);
  assert.equal(s.getState().device.firmware,'abc123');
  assert.equal(s.getState().robot.motion_output_installed,false);
  s.applyTelemetry({robot:{control_allowed:true}}); assert.equal(s.isManualEnabled(),true);
  s.resetForDisconnect(); assert.equal(s.getState().robot.control_allowed,undefined);
  assert.deepEqual(s.getState().device,{});
});
test('errors retain request correlation and sensor block freshness is independent', () => {
  const m=p.decode('{"type":"error","code":"UNSUPPORTED_MODE","request_type":"set_mode","request_id":42}').msg;
  assert.equal(m.request_type,'set_mode'); assert.equal(m.request_id,42);
  const state=s.getState(); state.connection.lastHealthTs=123; state.vision.lastGestureTs=456;
  s.applyTelemetry({robot:{mode:'IDLE',estop:false}});
  assert.equal(state.connection.lastHealthTs,123); assert.equal(state.vision.lastGestureTs,456);
  assert.equal(CONFIG.PPG_EXPECTED_RATE_HZ,25);
});
