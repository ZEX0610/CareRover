import test from 'node:test';
import assert from 'node:assert/strict';
import { safeStreamUrl, streamUrl, decode } from '../js/protocol.js';
import * as store from '../js/state.js';
import {CONFIG} from '../js/config.js';
const telemetry = value => decode(JSON.stringify({type:'telemetry',...value})).msg;
test('stream query and telemetry share URL validation and deterministic precedence',()=>{
  for(const value of ['javascript:alert(1)','data:image/png,xx','file:///etc/passwd','http://u:p@host/stream','x'.repeat(513)])assert.equal(safeStreamUrl(value),undefined);
  assert.equal(streamUrl('javascript:bad','http://192.168.4.2/stream','/stream'),'http://192.168.4.2/stream');
  assert.equal(streamUrl('/custom','http://192.168.4.2/stream','/stream'),'/custom');
  assert.equal(streamUrl(null,null,'/stream'),'/stream');
});
test('duplicate person sequence cannot refresh its deadline; fresh sequence can',()=>{
  const realNow=Date.now;let clock=1000;Date.now=()=>clock;
  try {
    const update=(seq,age=0,found=true)=>store.applyTelemetry(telemetry({vision:{person:{seq,age_ms:age,found,x:20,y:20,w:40,h:40,confidence:.9}}}));
    store.resetForDisconnect();update(1);assert.equal(store.isPersonStale(),false);
    clock=1400;update(1);assert.equal(store.getState().vision.lastPersonTs,1000);
    clock=1000+CONFIG.VISION_STALE_MS+1;update(1);assert.equal(store.isPersonStale(),true);
    update(2,200);assert.equal(store.isPersonStale(),false);
    update(3,CONFIG.VISION_STALE_MS+1);assert.equal(store.isPersonStale(),true);
    update(4,0,false);assert.equal(store.getState().vision.person.found,false);
  } finally {Date.now=realNow;}
});
test('tracking capabilities, IMU validity and explicit null survive decoding',()=>{
  const msg=telemetry({device:{supported_modes:['IDLE','PERSON_FOLLOW','INVALID']},imu:{valid:false,calibrated:true,tilt_fault:true,yaw_deg:null},video:{stream_url:'http://192.168.4.2/stream'}});
  assert.deepEqual(msg.device.supported_modes,['IDLE','PERSON_FOLLOW']);assert.equal(msg.imu.valid,false);assert.equal(msg.imu.yaw_deg,null);
  store.applyTelemetry(msg);assert.equal(store.getState().video.stream_url,'http://192.168.4.2/stream');
  store.resetForDisconnect();assert.deepEqual(store.getState().video,{});assert.equal(store.getState().imu.valid,false);
});
