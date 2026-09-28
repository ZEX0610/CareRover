import test from 'node:test';
import assert from 'node:assert/strict';
import * as p from '../js/protocol.js';
import * as s from '../js/state.js';
import { computeContainFit, mapImageRectToCanvas, smoothDisplayRect } from '../js/video-overlay.js';
import { joystickVector } from '../js/joystick.js';
import { RobotSim } from '../js/sim.js';
import { CONFIG } from '../js/config.js';
import { MockTransport } from '../js/mock-transport.js';

const telemetry = (robot = {}) => ({ type: 'telemetry', ts: Date.now(), connection: { camera: true, main_mcu: true }, robot: { mode:'MANUAL', state:'READY', estop:false, ...robot } });
test('contain mapping centers a 4:3 image in widescreen and portrait canvases', () => {
  assert.deepEqual(computeContainFit(320,240,1280,720), { ox:160, oy:0, dw:960, dh:720, scale:3 });
  assert.deepEqual(mapImageRectToCanvas(100,40,80,160,320,240,1280,720), { x:460,y:120,w:240,h:480 });
  assert.deepEqual(computeContainFit(320,240,360,600), { ox:0,oy:165,dw:360,dh:270,scale:1.125 });
  assert.equal(computeContainFit(0,0,0,0).scale,1);
});
test('display-only box smoothing fills telemetry gaps without a visible jump', () => {
  const start = { x: 0, y: 20, w: 80, h: 100 };
  const target = { x: 100, y: 30, w: 90, h: 110 };
  assert.deepEqual(smoothDisplayRect(null, target, 16), target);
  const first = smoothDisplayRect(start, target, 16);
  assert.ok(first.x > start.x && first.x < target.x);
  assert.ok(first.y > start.y && first.y < target.y);
  let box = first;
  for (let i = 0; i < 10; i++) box = smoothDisplayRect(box, target, 16);
  assert.ok(box.x > 85 && box.x <= target.x, 'large correction reaches most of the new detection within 176 ms');
  assert.deepEqual(smoothDisplayRect(box, target, 1000), target, 'background tab resumes at current position');
});
test('joystick eight directions, radius limiting, and deadzone use robot axes', () => {
  for (const [dx,dy,sx,sy] of [[0,-100,1,0],[100,-100,1,1],[100,0,0,1],[100,100,-1,1],[0,100,-1,0],[-100,100,-1,-1],[-100,0,0,-1],[-100,-100,1,-1]]) {
    const v=joystickVector(dx,dy,100); assert.equal(Math.sign(v.vx)||0,sx); assert.equal(Math.sign(v.vy)||0,sy);
    assert.ok(Math.hypot(v.vx,v.vy)<=1.000001);
  }
  assert.equal(joystickVector(2,3,100).vx,0);
  assert.equal(joystickVector(0,-1000,100).vx,1);
});
test('protocol rejects malformed frames, unsafe commands, bad samples and ACKs', () => {
  for (const raw of ['{','null','[]','{"type":"bogus"}', ' '.repeat(65537)]) assert.equal(p.decode(raw).ok,false);
  assert.equal(p.decode('{"type":"ack"}').msg.ok,false);
  assert.equal(p.decode('{"type":"ppg","value":"bad"}').ok,false);
  assert.ok(p.validateOutgoing({type:'cmd_vel',ts:1,vx:NaN,vy:0,wz:0}));
  assert.ok(p.validateOutgoing({type:'set_mode',ts:1,mode:'ESTOP'}));
  const msg=p.decode(JSON.stringify({type:'ppg_batch',samples:[1,null,'x',2]})).msg;
  assert.deepEqual(msg.samples,[1,2]);
  assert.equal(p.decode(JSON.stringify({type:'ppg_batch',samples:Array(2000).fill(3)})).msg.samples.length,512);
  assert.equal(p.decode('{"type":"telemetry","vision":{"image_width":0}}').msg.vision.image_width,undefined);
});
test('watch mode is displayed from telemetry but cannot be requested by the web client', () => {
  const decoded=p.decode(JSON.stringify(telemetry({mode:'WATCH_CONTROL',state:'READY'})));
  assert.equal(decoded.ok,true);
  assert.equal(decoded.msg.robot.mode,'WATCH_CONTROL');
  assert.ok(p.validateOutgoing({type:'set_mode',ts:1,mode:'WATCH_CONTROL'}));
});
test('manual control requires fresh robot confirmation, healthy links, and no safety latch', () => {
  const state=s.getState(); s.setConnectionState('connected'); s.applyTelemetry(telemetry());
  state.ui.replaying=false; s.markEstopLocal(false); s.setRequestedMode(null);
  assert.equal(s.isManualEnabled(),true);
  s.markEstopLocal(true); s.applyTelemetry(telemetry()); assert.equal(s.isManualEnabled(),false,'late telemetry cannot clear local ESTOP');
  s.markEstopLocal(false); s.setRequestedMode('PERSON_FOLLOW'); assert.equal(s.isManualEnabled(),false);
  s.setRequestedMode(null); s.applyTelemetry(telemetry({state:'FAULT'})); assert.equal(s.isManualEnabled(),false);
  s.applyTelemetry(telemetry()); state.connection.lastRobotTs=Date.now()-2000;
  s.applyTelemetry({type:'telemetry',health:{sqi:.9}}); assert.equal(s.isManualEnabled(),false,'health-only telemetry cannot renew manual lease');
  s.applyTelemetry(telemetry()); s.applyTelemetry({type:'telemetry',connection:{main_mcu:false}}); assert.equal(s.isManualEnabled(),false);
  s.resetForDisconnect(); assert.deepEqual(state.ui.joystick,{vx:0,vy:0}); assert.equal(state.connection.lastRobotTs,0);
});
test('PPG storage stays bounded through 5 minutes at 50 Hz', () => {
  const state=s.getState(); state.ppg.ring.clear();
  const began=Date.now();
  for(let i=0;i<3000;i++) s.appendPpgSamples([1,2,3,4,5],50,began+i*100);
  assert.equal(state.ppg.ring.size,CONFIG.PPG_RING_CAPACITY);
  assert.equal(state.ppg.ring.values.byteLength,CONFIG.PPG_RING_CAPACITY*4);
  let count=0;state.ppg.ring.each(state.ppg.lastTs-8000,()=>count++);assert.ok(count<=401);
});
test('simulator enforces mode arbitration, hard ESTOP, explicit recovery and 250 ms watchdog', () => {
  const sim=new RobotSim(); assert.equal(sim.handleCommand(p.cmdVel(1,0,0))[0].code,'NOT_IN_MANUAL');
  sim.handleCommand(p.setMode('MANUAL')); sim.handleCommand(p.cmdVel(.6,.4,0)); sim.step(.1);
  assert.ok(sim.vel.vx>0); sim.step(.151); assert.deepEqual(sim.vel,{vx:0,vy:0,wz:0});
  sim.handleCommand(p.cmdVel(1,0,0));sim.step(.02);sim.handleCommand(p.estop());assert.deepEqual(sim.vel,{vx:0,vy:0,wz:0});
  assert.equal(sim.handleCommand(p.cmdVel(1,0,0))[0].code,'ESTOP_ACTIVE');
  sim.handleCommand(p.clearEstop());assert.equal(sim.mode,'IDLE');assert.equal(sim.estop,false);
  assert.equal(sim.handleCommand({...p.cmdVel(0,0,0),vx:Infinity})[0].code,'INVALID_COMMAND');
});
test('MockTransport removes completed timeout handles and reconnects at zero', async () => {
  const mock=new MockTransport({connectDelayMs:1,fakeLatencyMs:1});
  const wait=ms=>new Promise(r=>setTimeout(r,ms));
  mock.connect();await wait(10);
  for(let i=0;i<200;i++)mock.send(p.ping(i));
  await wait(25);assert.equal(mock.pending.size,0);
  mock.send(p.setMode('MANUAL'));await wait(10);mock.send(p.cmdVel(1,0,0));await wait(25);
  mock.simulateDrop(20);assert.deepEqual(mock.sim.vel,{vx:0,vy:0,wz:0});await wait(30);
  assert.equal(mock.isOpen,true);assert.equal(mock.sim.mode,'IDLE');mock.disconnect();assert.equal(mock.intervals.length,0);
});

test('pointer capture loss, touch cancellation, blur and hidden all release targets', async () => {
  const { MotionInput } = await import('../js/joystick.js');
  class Element extends EventTarget {
    constructor() { super();this.dataset={};this.style={};this.captures=new Set(); }
    getBoundingClientRect(){return {left:0,top:0,width:200,height:200};}
    setPointerCapture(id){this.captures.add(id);}
    hasPointerCapture(id){return this.captures.has(id);}
    releasePointerCapture(id){this.captures.delete(id);}
    closest(){return null;}
  }
  const event=(target,type,props={})=>{ const e=new Event(type,{cancelable:true});Object.assign(e,props);target.dispatchEvent(e); };
  globalThis.window=new Element();globalThis.document=new Element();document.hidden=false;
  let target, immediate, enabled=true, estops=0;
  const pad=new Element(),knob=new Element(),left=new Element(),right=new Element(),stop=new Element();
  const input=new MotionInput({pad,knob,left,right,stop,enabled:()=>enabled,change:(v,i)=>{target=v;immediate=i;},emergency:()=>estops++});
  const down=()=>event(pad,'pointerdown',{pointerId:1,button:0,clientX:155,clientY:45});
  const stopped=()=>{assert.deepEqual(target,{vx:0,vy:0,wz:0});assert.equal(immediate,true);};
  try {
    for(const reason of ['pointerup','pointercancel','lostpointercapture']){
      down();assert.ok(target.vx>0&&target.vy>0);event(pad,reason,{pointerId:1});stopped();
    }
    down();event(window,'blur');stopped();
    down();document.hidden=true;event(document,'visibilitychange');stopped();document.hidden=false;
    down();event(pad,'pointerdown',{pointerId:2,button:0,clientX:0,clientY:0});assert.equal(input.pointer,1);input.reset(true);
    event(right,'pointerdown',{pointerId:3,button:0});assert.equal(target.wz,1);event(right,'pointercancel',{pointerId:3});stopped();
    event(window,'keydown',{key:'w',code:'KeyW'});assert.equal(target.vx,1);
    event(window,'keyup',{key:'w',code:'KeyW'});stopped();
    enabled=false;down();stopped();event(window,'keydown',{key:'Escape',code:'Escape'});assert.equal(estops,1);
  } finally {input.destroy();delete globalThis.window;delete globalThis.document;}
});
