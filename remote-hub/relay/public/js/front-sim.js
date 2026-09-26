// DEMONSTRATION FIXTURE ONLY. These numbers are never uploaded as physical calibration.
export class FrontSim {
  constructor() { this.distance=120;this.phase='NONE';this.held=false;this.blocked=false;this.reason='boot';this.at=0;this.rightAt=0;this.near=0;this.clear=0;this.samples=[];this.sampleAt=-1;this.valid=false;this.raw=120;this.filtered=120; }
  cancel(reason) { this.phase='NONE';this.reason=reason; }
  release() { this.held=false; }
  enter(phase,t) { this.phase=phase;this.at=t;if(phase==='RIGHT')this.rightAt=t; }
  step(t,target,following) {
    const zero={vx:0,vy:0,wz:0};let abort=false;
    let distance=this.distance;
    if(t-this.sampleAt>=.07) {
      this.sampleAt=t;this.valid=Number.isFinite(distance)&&distance>=2&&distance<=400;this.raw=distance;
      if(this.valid) { this.samples.push(distance);if(this.samples.length>3)this.samples.shift();this.filtered=[...this.samples].sort((a,b)=>a-b)[Math.floor(this.samples.length/2)];this.near=distance<=20?this.near+1:0;this.clear=distance>=70?this.clear+1:0;if(distance<=20)this.blocked=true;else if(this.clear>=3)this.blocked=false; }
      else {this.samples=[];this.near=this.clear=0;}
    }
    if(!this.valid) { this.cancel('front_unknown');return {target:zero,abort:true}; }
    // Manual-only automatic bypass (no demo toggle): Halt -> Right -> Margin -> Pass -> done.
    if(!following&&this.phase==='NONE'&&target.vx>0&&this.blocked) this.enter('HALT',t);
    if(this.phase==='HALT'&&this.near>=3&&t-this.at>=.21)this.enter('RIGHT',t);
    else if(this.phase==='RIGHT'&&this.clear>=3)this.enter('MARGIN',t);
    else if(this.phase==='MARGIN') {
      if(this.raw<70) {this.phase='RIGHT';this.clear=0;}
      else if(t-this.at>=.4)this.enter('PASS',t);
    } else if(this.phase==='PASS') {
      if(this.raw<50) {this.cancel('bypass_blocked');return {target:zero,abort:true};}
      if(t-this.at>=.8)this.cancel('bypass_complete');
    }
    if(['RIGHT','MARGIN'].includes(this.phase)&&t-this.rightAt>=4) {this.cancel('bypass_timeout');abort=true;}
    if(this.phase==='HALT'&&t-this.at>=3) {this.cancel('front_unstable');abort=true;}
    if(abort)return {target:zero,abort};
    if(this.phase==='HALT')return {target:zero};
    if(['RIGHT','MARGIN'].includes(this.phase))return {target:{vx:0,vy:.15,wz:0}};
    if(this.phase==='PASS')return {target:{vx:.15,vy:0,wz:0}};
    if(this.held)return {target:zero};
    if(target.vx>0&&this.blocked) {this.held=true;this.reason='front_obstacle';return {target:zero};}
    return {target:{...target,vx:target.vx>0?target.vx*Math.max(0,Math.min(1,(Math.min(this.raw,this.filtered)-20)/30)):target.vx}};
  }
  telemetry(t) {
    return { enabled:true,ready:true,valid:this.valid,distance_cm:this.valid?this.filtered:null,age_ms:Math.max(0,(t-this.sampleAt)*1000),
      seated:false,release_required:this.held,phase:this.phase,stop_reason:this.reason,
      status:!this.valid?'UNKNOWN':this.held?'STOPPED':this.phase!=='NONE'?'BYPASS':this.blocked?'BLOCKED':this.filtered<50?'SLOW':this.filtered<60?'WARN':'CLEAR' };
  }
}
