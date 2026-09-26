// Browser-only transport fixture. Never used by firmware or production UI.
(() => {
  const fixture = window.trackingFixture = { messages: [], mode: 'IDLE', estop: false, owner: true, seq: 0, freeze: false, imu: true, stream: '/stream', now: 0 };
  class Socket {
    static CONNECTING = 0; static OPEN = 1; static CLOSED = 3;
    readyState = 0; bufferedAmount = 0;
    constructor() {
      setTimeout(() => { this.readyState=1; this.onopen?.(); this.timer=setInterval(()=>this.telemetry(),100); }, 10);
    }
    emit(value) { this.onmessage?.({data: JSON.stringify({ts:Date.now(),...value})}); }
    send(raw) {
      const msg=JSON.parse(raw);fixture.messages.push({at:performance.now(),...msg});
      if(msg.type==='ping') { this.emit({type:'pong',id:msg.id});return; }
      if(msg.type==='set_mode') { fixture.mode=msg.mode;this.emit({type:'ack',ok:true,request_type:msg.type,request_id:msg.request_id}); }
      if(msg.type==='estop') {fixture.estop=true;fixture.mode='ESTOP';}
      if(msg.type==='clear_estop') {fixture.estop=false;fixture.mode='IDLE';this.emit({type:'ack',ok:true,request_type:msg.type,request_id:msg.request_id});}
      if(msg.type==='cmd_vel'&&!msg.vx&&!msg.vy&&!msg.wz&&fixture.mode==='PERSON_FOLLOW')fixture.mode='IDLE';
    }
    telemetry() {
      if(this.readyState!==1||fixture.silent)return;
      if(!fixture.freeze) { fixture.seq++;fixture.now=performance.now(); }
      this.emit({type:'telemetry',connection:{camera:true,main_mcu:true},
        robot:{mode:fixture.mode,state:fixture.mode==='PERSON_FOLLOW'?'TRACKING':'READY',estop:fixture.estop,control_allowed:fixture.owner,motion_output_installed:true,calibration_ready:true,vx:0,vy:0,wz:0},
        device:{firmware:'UI-TEST-ONLY',backend:'tracking',integration:'follow',supported_modes:['IDLE','MANUAL','PERSON_FOLLOW','HEALTH_CHECK']},
        video:{stream_url:fixture.stream,source_width:320,source_height:240,protocol:'mjpeg'},
        imu:{valid:fixture.imu,calibrated:true,tilt_fault:!fixture.imu,yaw_deg:fixture.imu?10:null,pitch_deg:0,roll_deg:0},
        vision:{image_width:320,image_height:240,person:{seq:fixture.seq,age_ms:performance.now()-fixture.now,found:true,x:124,y:32,w:76,h:176,confidence:.94}},...fixture.signalPayload});
    }
    close() {this.readyState=3;clearInterval(this.timer);this.onclose?.();}
  }
  window.WebSocket=Socket;
})();
