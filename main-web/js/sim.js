/**
 * 机器人行为模拟器（浏览器端）。
 *
 * 与 mock/server.py 共用消息契约和安全状态语义；模拟轨迹与数值不要求一致。
 * 浏览器版本让页面无需模拟后端即可演示，Python 版本验证实际 WebSocket 收发。
 *
 * 本文件不含 DOM 或网络代码，数值仿真与安全行为由 tests/core.test.js 检查。
 */
import { FrontSim } from './front-sim.js';
import { CONFIG } from './config.js';
import { validateOutgoing } from './protocol.js';

const GESTURE_CYCLE = ['NONE', 'PALM', 'FIST', 'THUMB_UP', 'VICTORY', 'POINT_LEFT', 'POINT_RIGHT', 'NONE'];

const rand = (a, b) => a + Math.random() * (b - a);
const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);

export class RobotSim {
  constructor(opts = {}) {
    this.imageWidth = opts.imageWidth ?? CONFIG.DEFAULT_IMAGE_WIDTH;
    this.imageHeight = opts.imageHeight ?? CONFIG.DEFAULT_IMAGE_HEIGHT;

    this.front = new FrontSim();
    this.t = 0;                 // 仿真时间(s)
    this.mode = 'IDLE';
    this.estop = false;

    // 网页下发的目标速度 + 最近一次到达时刻（用于 dead-man 超时）
    this.cmd = { vx: 0, vy: 0, wz: 0 };
    this.lastCmdAt = -1e9;

    // 机器人"实际"速度（一阶跟随目标，看起来更像真机）
    this.vel = { vx: 0, vy: 0, wz: 0 };

    this.yaw = 0; this.pitch = 0; this.roll = 0;
    this.battery = 87;

    this.personPhase = rand(0, Math.PI * 2);
    this.personFound = true;
    this.gestureIdx = 0;
    this.gestureAt = 0;
    this.gestureConf = 0.9;
    this.aiFps = 5.8;

    this.hr = 74; this.spo2 = 98; this.sqi = 0.9;
    this.fingerAt = 0; this.finger = true;
    this.ppgPhase = 0;
    this.healthState = 'VALID';
  }

  /* ── 命令入口 ───────────────────────────────────────────────────────── */

  /**
   * 处理一条来自网页的命令。
   * @returns {object[]} 需要立即回给网页的消息（ack / error）
   */
  handleCommand(msg) {
    const ts = Date.now();
    const invalid = validateOutgoing(msg);
    if (invalid) return [{ type: 'error', ts, code: 'INVALID_COMMAND', message: invalid }];
    const ack = ok => ({ type: 'ack', ts, request_type: msg.type, request_id: msg.request_id, ok });
    switch (msg.type) {
      case 'cmd_vel': {
        const nonZero = Math.abs(msg.vx) > 1e-6 || Math.abs(msg.vy) > 1e-6 || Math.abs(msg.wz) > 1e-6;
        // 安全裁决在机器人侧：急停期间一律拒绝非零速度
        if (this.estop && nonZero) {
          this.cmd = { vx: 0, vy: 0, wz: 0 };
          return [{ type: 'error', ts, code: 'ESTOP_ACTIVE',
                    message: 'Motion command rejected while ESTOP is active' }];
        }
        // 只有 MANUAL 模式接受网页的速度指令，其它模式由机器人自己决策
        if (this.mode !== 'MANUAL' && nonZero) {
          return [{ type: 'error', ts, code: 'NOT_IN_MANUAL',
                    message: `Motion command ignored in ${this.mode}` }];
        }
        if(!nonZero)this.front.release();
        if(nonZero&&this.front.held)return [{type:'error',ts,code:'FRONT_RELEASE_REQUIRED',message:'Release joystick first'}];
        this.cmd = { vx: msg.vx, vy: msg.vy, wz: msg.wz };
        if (!nonZero) { this.vel = { vx: 0, vy: 0, wz: 0 }; if(this.mode === 'PERSON_FOLLOW') this.mode='IDLE'; }
        this.lastCmdAt = this.t;
        return []; // cmd_vel 走高频通道，不逐包 ACK，靠 telemetry 回显确认
      }

      case 'set_mode': {
        if (this.estop) {
          return [ack(false),
                  { type: 'error', ts, code: 'ESTOP_ACTIVE',
                    message: 'Clear ESTOP before changing mode' }];
        }
        if(['MANUAL','PERSON_FOLLOW'].includes(msg.mode)&&this.front.held)return [{type:'error',ts,code:'FRONT_RELEASE_REQUIRED',message:'Release joystick first'}];
        this.front.cancel('mode_changed');
        this.mode = msg.mode;
        if(msg.mode === 'PERSON_FOLLOW') { const r=this._personRect(); this.referenceArea=r.w*r.h;this.lastFollowPing=this.t;this.turning=false; }
        this.cmd = { vx: 0, vy: 0, wz: 0 };   // 换模式一律先停
        this.vel = { vx: 0, vy: 0, wz: 0 };
        return [ack(true)];
      }

      case 'estop':
        this.front.cancel('estop');
        this.estop = true;
        this.cmd = { vx: 0, vy: 0, wz: 0 };
        this.vel = { vx: 0, vy: 0, wz: 0 };   // 急停是硬停，不做减速过渡
        return [ack(true)];

      case 'clear_estop':
        this.estop = false;
        this.mode = 'IDLE';                   // 解除后回到待机，不自动恢复运动
        return [ack(true)];

      case 'ping':
        this.lastFollowPing=this.t;
        return [{ type: 'pong', ts, id: msg.id }];

      default:
        return [{ type: 'error', ts, code: 'UNKNOWN_TYPE', message: `Unsupported type "${msg.type}"` }];
    }
  }

  /* ── 仿真步进 ───────────────────────────────────────────────────────── */

  step(dt) {
    this.t += dt;

    /* 目标速度：MANUAL 用网页指令并执行 dead-man 超时，自主模式自己编 */
    let target = { vx: 0, vy: 0, wz: 0 };
    if (!this.estop) {
      if (this.mode === 'MANUAL') {
        // 与协议约定一致：超过 DEADMAN_TIMEOUT_MS 没收到有效指令就自动停车。
        // 这条兜底逻辑在真机上必须由主 ESP32-S3 实现，这里如实模拟。
        const ageMs = (this.t - this.lastCmdAt) * 1000;
        if (ageMs > CONFIG.DEADMAN_TIMEOUT_MS) this.vel = { vx: 0, vy: 0, wz: 0 };
        target = ageMs > CONFIG.DEADMAN_TIMEOUT_MS ? { vx: 0, vy: 0, wz: 0 } : { ...this.cmd };
      } else if (this.mode === 'PERSON_FOLLOW') {
        if(!this.personFound || this.trackingScenario==='low-confidence' || this.trackingScenario==='imu-fault' || (this.t-this.lastFollowPing)*1000>=240) {
          this.mode='IDLE';this.vel={vx:0,vy:0,wz:0};
        } else {
          const r=this._personRect(), ex=(r.x+r.w/2-this.imageWidth/2)/(this.imageWidth/2);
          const dead=x=>Math.abs(x)<=.1?0:x-Math.sign(x)*.1;
          if(Math.abs(ex)>=.35)this.turning=true;else if(Math.abs(ex)<=.20)this.turning=false;
          if(this.turning)target.wz=clamp(.8*ex,-.3,.3);
          else {target.vy=clamp(.6*dead(ex),-.2,.2);target.vx=clamp(.5*dead(1-Math.sqrt(r.w*r.h/this.referenceArea)),-.25,.25);}
        }
      } else if (this.mode === 'GESTURE_CONTROL') {
        const g = GESTURE_CYCLE[this.gestureIdx];
        if (g === 'PALM') target = { vx: 0, vy: 0, wz: 0 };
        else if (g === 'THUMB_UP') target.vx = 0.4;
        else if (g === 'FIST') target.vx = -0.3;
        else if (g === 'POINT_LEFT') target.vy = -0.4;
        else if (g === 'POINT_RIGHT') target.vy = 0.4;
      }
    }

    if(this.mode!=='MANUAL'&&this.front.phase!=='NONE')this.front.cancel('mode_exit');
    const frontResult=this.front.step(this.t,target,this.mode==='PERSON_FOLLOW');
    if(['MANUAL','PERSON_FOLLOW','GESTURE_CONTROL'].includes(this.mode)) {
      target=frontResult.target;
      if(frontResult.abort){this.mode='IDLE';this.cmd={vx:0,vy:0,wz:0};}
      if(!target.vx&&!target.vy&&!target.wz)this.vel={vx:0,vy:0,wz:0};
    }
    // 一阶低通，模拟机械惯性
    const k = Math.min(1, dt * 6);
    this.vel.vx += (target.vx - this.vel.vx) * k;
    this.vel.vy += (target.vy - this.vel.vy) * k;
    this.vel.wz += (target.wz - this.vel.wz) * k;

    /* IMU */
    this.yaw += this.vel.wz * 90 * dt;                       // wz=1 → 90°/s
    this.yaw = ((this.yaw + 180) % 360 + 360) % 360 - 180;   // 归一到 (-180,180]
    this.pitch = Math.sin(this.t * 0.7) * 0.9 + this.vel.vx * 1.5;
    this.roll = Math.cos(this.t * 0.5) * 0.7 - this.vel.vy * 1.2;

    /* 电池：缓慢下降 + 负载相关 */
    const load = Math.abs(this.vel.vx) + Math.abs(this.vel.vy) + Math.abs(this.vel.wz);
    this.battery = clamp(this.battery - dt * (0.004 + load * 0.012), 5, 100);

    /* 视觉 */
    this.personPhase += dt * 0.42;
    // 偶尔丢目标几秒，用来验证前端的 stale / 隐藏框逻辑
    const lost = Math.sin(this.personPhase * 0.23) < -0.93;
    this.personFound = !lost && this.trackingScenario !== 'person-lost';
    this.aiFps = clamp(5.8 + Math.sin(this.t * 0.9) * 0.7 + rand(-0.25, 0.25), 3.5, 8);

    if (this.t - this.gestureAt > rand(2.6, 4.2)) {
      this.gestureAt = this.t;
      this.gestureIdx = (this.gestureIdx + 1) % GESTURE_CYCLE.length;
      this.gestureConf = rand(0.62, 0.97);
    }

    /* 健康：随机游走，保持在生理合理区间 */
    this.hr = clamp(this.hr + rand(-0.35, 0.35), 62, 96);
    this.spo2 = clamp(this.spo2 + rand(-0.12, 0.12), 94, 100);
    this.sqi = clamp(this.sqi + rand(-0.02, 0.02), 0.35, 0.99);

    // 每隔一段时间模拟"手指离开"，走完整的健康状态机
    if (this.t - this.fingerAt > (this.finger ? 24 : 4)) { this.fingerAt = this.t; this.finger = !this.finger; }
    if (!this.finger) this.healthState = 'NO_FINGER';
    else if (this.t - this.fingerAt < 1.5) this.healthState = 'ACQUIRING';
    else if (this.sqi < 0.5) this.healthState = 'LOW_QUALITY';
    else this.healthState = this.mode === 'HEALTH_CHECK' ? 'MEASURING' : 'VALID';
  }

  /* ── PPG 生成 ───────────────────────────────────────────────────────── */

  /**
   * 生成 n 个 PPG 样本。
   * 不用纯正弦：真实 PPG 有收缩期主峰、重搏切迹和重搏波，
   * 只有把这三段做出来，前端的自动缩放和绘制才算被真正验证过。
   */
  ppgSamples(n, rateHz) {
    const out = new Array(n);
    const beatHz = this.hr / 60;
    for (let k = 0; k < n; k++) {
      this.ppgPhase = (this.ppgPhase + beatHz / rateHz) % 1;
      const p = this.ppgPhase;
      const g = (c, w) => Math.exp(-((p - c) * (p - c)) / (2 * w * w));

      const systolic = 1.00 * g(0.17, 0.055);   // 收缩期主峰
      const notch    = -0.14 * g(0.33, 0.028);  // 重搏切迹
      const dicrotic = 0.34 * g(0.44, 0.075);   // 重搏波（第二个峰）
      const tail     = 0.10 * g(0.70, 0.18);

      const respiration = 0.09 * Math.sin(this.t * 2 * Math.PI * 0.24); // 呼吸基线漂移
      const drift = 0.03 * Math.sin(this.t * 0.11);
      const noise = (Math.random() - 0.5) * (this.finger ? 0.022 : 0.5);

      const shaped = this.finger ? (systolic + notch + dicrotic + tail) : 0;
      // 映射到类似 MAX30102 的 18 位 ADC 量级
      out[k] = 18400 + (shaped + respiration + drift + noise) * 2600;
    }
    return out;
  }

  /* ── 输出 ───────────────────────────────────────────────────────────── */

  _personRect() {
    // 在画面里走 8 字形，尺寸随"远近"变化
    const w = Math.round(this.imageWidth * (0.24 + 0.05 * Math.sin(this.personPhase * 0.7)));
    const h = Math.round(w * 2.2);
    const cx = this.imageWidth * (0.5 + 0.33 * Math.sin(this.personPhase));
    const cy = this.imageHeight * (0.54 + 0.07 * Math.sin(this.personPhase * 2));
    return {
      x: Math.round(clamp(cx - w / 2, 0, this.imageWidth - w)),
      y: Math.round(clamp(cy - h / 2, 0, Math.max(0, this.imageHeight - h))),
      w, h
    };
  }

  /** 当前场景的几何描述，供 mock 视频渲染器画出与 bbox 对齐的人形。 */
  scene() {
    const r = this._personRect();
    return { person: r, found: this.personFound, gesture: GESTURE_CYCLE[this.gestureIdx], t: this.t };
  }

  telemetry() {
    const r = this._personRect();
    const label = GESTURE_CYCLE[this.gestureIdx];
    return {
      type: 'telemetry',
      ts: Date.now(),
      front:this.front.telemetry(this.t),
      connection: { camera: true, main_mcu: true, simulated: true },
      robot: {
        mode: this.estop ? 'ESTOP' : this.mode,
        state: this._stateName(),
        estop: this.estop,
        control_allowed: true,
        battery_pct: Math.round(this.battery),
        vx: round3(this.vel.vx), vy: round3(this.vel.vy), wz: round3(this.vel.wz)
      },
      video: {stream_url:'/stream',source_width:320,source_height:240,protocol:'mjpeg'},
      imu: { valid: this.trackingScenario!=='imu-fault', calibrated:true, tilt_fault:this.trackingScenario==='imu-fault', yaw_deg: round2(this.yaw), pitch_deg: round2(this.pitch), roll_deg: round2(this.roll) },
      vision: {
        image_width: this.imageWidth,
        image_height: this.imageHeight,
        ai_fps: round2(this.aiFps),
        person: this.personFound
          ? { found: true, x: r.x, y: r.y, w: r.w, h: r.h,
              confidence: this.trackingScenario==='low-confidence' ? .4 : round2(clamp(0.87 + Math.sin(this.t * 1.3) * 0.09, 0.5, 0.99)) }
          : { found: false },
        gesture: { label, confidence: round2(this.gestureConf), stable: this.gestureConf >= CONFIG.GESTURE_STABLE_CONFIDENCE }
      },
      health: {
        hr_bpm: Math.round(this.hr),
        spo2_pct: Math.round(this.spo2),
        sqi: round2(this.sqi),
        finger_detected: this.finger,
        state: this.healthState
      }
    };
  }

  _stateName() {
    if (this.estop) return 'ESTOP';
    const moving = Math.abs(this.vel.vx) + Math.abs(this.vel.vy) + Math.abs(this.vel.wz) > 0.02;
    switch (this.mode) {
      case 'MANUAL':          return moving ? 'DRIVING' : 'READY';
      case 'PERSON_FOLLOW':   return this.personFound ? 'TRACKING' : 'IDLE';
      case 'GESTURE_CONTROL': return moving ? 'DRIVING' : 'READY';
      case 'HEALTH_CHECK':    return 'MEASURING';
      default:                return 'IDLE';
    }
  }
}

const round2 = v => Math.round(v * 100) / 100;
const round3 = v => Math.round(v * 1000) / 1000;
