import test from 'node:test';
import assert from 'node:assert/strict';
import {normalizeTelemetry} from '../js/protocol.js';
import * as store from '../js/state.js';

test('held metrics remain independent, old telemetry replaces validity, disconnect clears state',()=>{
  store.resetForDisconnect();
  const message=normalizeTelemetry({health:{hr_bpm:72,hr_valid:true,hr_held:true,hr_age_ms:8200,spo2_pct:null,spo2_valid:false,finger_detected:true,state:'VALID'}});
  store.applyTelemetry(message);
  const state=store.getState();assert.equal(state.health.hr_bpm,72);assert.equal(state.health.hr_held,true);assert.equal(state.health.spo2_valid,false);
  store.applyTelemetry(normalizeTelemetry({health:{hr_bpm:73,spo2_pct:97,finger_detected:true,state:'VALID'}}));
  assert.equal(state.health.hr_valid,undefined);assert.equal(state.health.hr_held,false);assert.equal(state.health.spo2_pct,97);
  store.resetForDisconnect();assert.equal(state.health.hr_bpm,undefined);assert.equal(state.health.hr_held,false);
});
test('prediction age comes from last measured seq and expires even with repeated telemetry',()=>{
  store.resetForDisconnect();const t=Date.now();
  const message=normalizeTelemetry({vision:{person:{found:true,predicted:true,seq:8,age_ms:600,x:100,y:60,w:80,h:80,confidence:.5}}});
  store.applyTelemetry(message);assert.equal(store.getState().vision.person.predicted,true);
  assert.equal(store.isPersonStale(t),false);assert.equal(store.isPersonStale(t+700),false);assert.equal(store.isPersonStale(t+900),true);
  store.applyTelemetry(normalizeTelemetry({vision:{person:{found:true,predicted:true,seq:8,age_ms:0}}}));
  assert.equal(store.isPersonStale(t+900),true);
});
test('gesture holding is distinct from confidence and IMU rejected count survives decoding',()=>{
  const message=normalizeTelemetry({vision:{gesture:{label:'LIKE',confidence:.18,stable:true,held:true,age_ms:420}},imu:{valid:true,held:true,rejected_frames:3,age_ms:40}});
  assert.equal(message.vision.gesture.stable,true);assert.equal(message.vision.gesture.held,true);
  assert.equal(message.imu.held,true);assert.equal(message.imu.rejected_frames,3);
});
