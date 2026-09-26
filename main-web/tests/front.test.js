import test from 'node:test';
import assert from 'node:assert/strict';
import { normalizeTelemetry } from '../js/protocol.js';
import { frontView } from '../js/front-panel.js';
import { RobotSim } from '../js/sim.js';
import { FrontSim } from '../js/front-sim.js';
const cmd=(s,type,extra={})=>s.handleCommand({type,ts:Date.now(),...extra});
test('front protocol does not retain malformed/no-echo/stale ranges and decodes seating',()=>{
  const base={enabled:true,ready:true,valid:true,distance_cm:25,age_ms:10,status:'SLOW'};
  for(const patch of [{valid:false},{distance_cm:null},{distance_cm:-1},{distance_cm:401},{age_ms:220},{age_ms:-1}]) {
    const f=normalizeTelemetry({front:{...base,...patch}}).front;assert.equal(f.valid,false);assert.equal(f.distance_cm,null);
  }
  assert.equal(normalizeTelemetry({front:{...base,seated:true}}).front.seated,true);
  const f={...normalizeTelemetry({front:base}).front,receivedAt:1000};
  assert.equal(frontView(f,1100).distance,'25 cm');assert.equal(frontView(f,1220).distance,'—');assert.equal(frontView(f,1000,true).status,'UNKNOWN');
});
test('manual obstacle enters auto-bypass Halt and stays manual',()=>{
  const s=new RobotSim();cmd(s,'set_mode',{mode:'MANUAL'});
  s.front.distance=18;cmd(s,'cmd_vel',{vx:1,vy:0,wz:0});
  s.step(.08);assert.equal(s.front.phase,'HALT');assert.equal(s.mode,'MANUAL');
});
test('manual auto-bypass clears and returns to none',()=>{
  const s=new RobotSim();cmd(s,'set_mode',{mode:'MANUAL'});s.front.distance=18;cmd(s,'cmd_vel',{vx:1,vy:0,wz:0});
  const phases=new Set();
  for(let i=0;i<120;i++){cmd(s,'ping',{id:i});s.step(.05);phases.add(s.front.phase);if(s.front.phase==='RIGHT')s.front.distance=120;}
  assert.ok(phases.has('HALT')&&phases.has('RIGHT'));assert.equal(s.front.phase,'NONE');assert.equal(s.mode,'MANUAL');
});
test('missing echo stops manual',()=>{
  const s=new RobotSim();cmd(s,'set_mode',{mode:'MANUAL'});s.front.distance=120;cmd(s,'cmd_vel',{vx:1,vy:0,wz:0});
  s.step(.08);assert.ok(s.vel.vx>0);
  s.front.distance=null;s.step(.08);assert.equal(s.mode,'IDLE');assert.equal(s.vel.vx,0);
});
test('bypass timeout aborts the lateral move',()=>{
  const f=new FrontSim();f.distance=18;let result;
  for(let i=1;i<400;i++){result=f.step(i*.02,{vx:.1,vy:0,wz:0},false);if(result.abort)break;}
  assert.equal(result.abort,true);assert.equal(f.reason,'bypass_timeout');
});
test('ESTOP cancels the bypass',()=>{
  const s=new RobotSim();cmd(s,'set_mode',{mode:'MANUAL'});s.front.distance=18;cmd(s,'cmd_vel',{vx:1,vy:0,wz:0});
  for(let i=0;i<25;i++){cmd(s,'ping',{id:i});s.step(.02);}
  assert.equal(s.front.phase,'RIGHT');
  cmd(s,'estop');s.step(.02);assert.equal(s.front.phase,'NONE');assert.equal(s.vel.vy,0);
});
