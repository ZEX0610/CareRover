/**
 * 全局 UI 状态。
 *
 * 约定：
 *  - 任何模块都不得直接读写 WebSocket 对象；数据只能经由这里的接口进出。
 *  - 遥测里"字段缺失"= 保留旧值，"字段为 0"= 真实的 0。两者严格区分。
 *  - PPG 使用固定容量环形缓冲，绝不随时间无限增长。
 */
import { CONFIG, LINK } from './config.js';

/* ── PPG 环形缓冲 ─────────────────────────────────────────────────────── */
class PpgRing {
  constructor(capacity) {
    this.cap = capacity;
    this.values = new Float32Array(capacity);
    this.times = new Float64Array(capacity);
    this.count = 0;   // 已写入总数（可超过 cap）
  }
  push(value, timeMs) {
    const i = this.count % this.cap;
    this.values[i] = value;
    this.times[i] = timeMs;
    this.count++;
  }
  get size() { return Math.min(this.count, this.cap); }
  /** 按时间顺序遍历 timeMs >= tMin 的样本。热路径，避免分配临时数组。 */
  each(tMin, fn) {
    const size = this.size;
    const start = this.count - size;             // 逻辑起点
    for (let k = 0; k < size; k++) {
      const i = (start + k) % this.cap;
      if (this.times[i] >= tMin) fn(this.values[i], this.times[i]);
    }
  }
  clear() { this.count = 0; }
}

/* ── 状态树 ───────────────────────────────────────────────────────────── */
const state = {
  connection: {
    link: LINK.DISCONNECTED,   // Transport 连接状态机
    camera: undefined,         // CAM ↔ 浏览器（由机器人上报）
    main_mcu: undefined,       // CAM ↔ 主控（由机器人上报）
    latencyMs: undefined,      // ping/pong 往返
    lastTelemetryTs: 0,
    lastRobotTs: 0,
    lastHealthTs: 0,
    lastPongTs: 0
  },
  front: null,
  robot: {
    mode: undefined, state: undefined, estop: false,
    battery_pct: undefined, vx: 0, vy: 0, wz: 0
  },
  device: {},
  video: {},
  lastImuTs: 0,
  imu: { yaw_deg: undefined, pitch_deg: undefined, roll_deg: undefined },
  vision: {
    image_width: CONFIG.DEFAULT_IMAGE_WIDTH,
    image_height: CONFIG.DEFAULT_IMAGE_HEIGHT,
    ai_fps: undefined,
    person: { found: false },
    gesture: { label: 'NONE', confidence: 0, stable: false },
    lastPersonTs: 0,
    lastGestureTs: 0
  },
  health: {
    hr_bpm: undefined, spo2_pct: undefined, sqi: undefined,
    finger_detected: false, state: 'NO_FINGER'
  },
  ppg: {
    ring: new PpgRing(CONFIG.PPG_RING_CAPACITY),
    sampleRateHz: CONFIG.PPG_EXPECTED_RATE_HZ,
    lastTs: 0
  },
  ui: {
    transportName: 'mock',
    requestedMode: null,       // 已发出 set_mode 但尚未确认的目标模式
    speedScale: CONFIG.DEFAULT_SPEED_SCALE,
    joystick: { vx: 0, vy: 0 },
    rotate: 0,                 // 旋转按钮/键盘产生的 wz（未乘速度上限）
    txCount: 0, rxCount: 0,
    replaying: false,
    estopLatch: false
  }
};

export function getState() { return state; }

/* ── 订阅 ─────────────────────────────────────────────────────────────── */
const subs = new Map(); // topic -> Set<fn>

/** 订阅某个主题；返回取消订阅函数。主题：telemetry / connection / ui / ppg / message */
export function subscribe(topic, fn) {
  if (!subs.has(topic)) subs.set(topic, new Set());
  subs.get(topic).add(fn);
  return () => subs.get(topic)?.delete(fn);
}

export function emit(topic, payload) {
  const set = subs.get(topic);
  if (!set) return;
  for (const fn of set) {
    // 单个订阅者抛错不能拖垮其它订阅者，也不能中断数据流
    try { fn(payload, state); } catch (err) { console.error(`[state] subscriber failed on "${topic}"`, err); }
  }
}

/* ── 更新接口 ─────────────────────────────────────────────────────────── */

/** 只覆盖 src 中"有定义"的字段，undefined 表示本包未上报，保留旧值。 */
function mergeDefined(target, src) {
  if (!src) return false;
  let changed = false;
  for (const k of Object.keys(src)) {
    if (src[k] !== undefined && target[k] !== src[k]) { target[k] = src[k]; changed = true; }
    else if (src[k] !== undefined) target[k] = src[k];
  }
  return changed;
}

/** 应用一条已净化的 telemetry。 */
export function applyTelemetry(msg) {
  const t = Date.now();
  state.connection.lastTelemetryTs = t;
  if(msg.tuning_profile)state.device.tuning_profile=msg.tuning_profile;

  if (msg.connection) mergeDefined(state.connection, msg.connection);
  state.front=msg.front ? { ...msg.front, receivedAt:Date.now() } : null;
  if (msg.robot) {
    mergeDefined(state.robot, msg.robot);
    if (msg.robot.mode !== undefined && msg.robot.estop !== undefined) state.connection.lastRobotTs = t;
  }
  if (msg.device) mergeDefined(state.device, msg.device);
  if (msg.imu) { mergeDefined(state.imu, msg.imu); state.lastImuTs = t - Math.max(0, msg.imu.age_ms || 0); }
  if (msg.video) mergeDefined(state.video, msg.video);

  if (msg.vision) {
    const v = msg.vision;
    if (v.image_width !== undefined) state.vision.image_width = v.image_width;
    if (v.image_height !== undefined) state.vision.image_height = v.image_height;
    if (v.ai_fps !== undefined) state.vision.ai_fps = v.ai_fps;
    // person / gesture 各自独立打时间戳：它们的更新频率不同，过期判定也必须独立
    if (v.person) {
      const same = v.person.seq !== undefined && v.person.seq === state.vision.person?.seq;
      const sourceTs = t - Math.max(0, v.person.age_ms || 0);
      state.vision.person = v.person;
      state.vision.lastPersonTs = same ? Math.min(state.vision.lastPersonTs, sourceTs) : sourceTs;
    }
    if (v.gesture) { state.vision.gesture = v.gesture; state.vision.lastGestureTs = t; }
  }

  if (msg.health) {
    if(msg.health.hr_valid===undefined)state.health.hr_valid=undefined;
    if(msg.health.spo2_valid===undefined)state.health.spo2_valid=undefined;
    mergeDefined(state.health, msg.health); state.connection.lastHealthTs = t; }

  // 机器人确认了模式 → 清除 pending
  if (state.ui.requestedMode && state.robot.mode === state.ui.requestedMode) {
    state.ui.requestedMode = null;
  }

  emit('telemetry', msg);
}

/** 追加 PPG 样本。ts 是本批"最后一个样本"的时间戳（见 docs/protocol.md）。 */
export function appendPpgSamples(samples, sampleRateHz, ts) {
  if (!samples || samples.length === 0) return;
  const rate = sampleRateHz > 0 ? sampleRateHz : state.ppg.sampleRateHz;
  state.ppg.sampleRateHz = rate;
  const step = 1000 / rate;
  const last = ts ?? Date.now();
  const n = samples.length;
  for (let k = 0; k < n; k++) {
    state.ppg.ring.push(samples[k], last - (n - 1 - k) * step);
  }
  state.ppg.lastTs = last;
  // 刻意不 emit：PPG 由 requestAnimationFrame 主动拉取，
  // 让 50 Hz 的数据到达频率与 60 Hz 的屏幕刷新彻底解耦。
}

export function setConnectionState(link) {
  if (state.connection.link === link) return;
  state.connection.link = link;
  if (link !== LINK.CONNECTED) {
    state.connection.latencyMs = undefined;
    // 链路断开时，机器人侧链路状态变为"未知"而不是"正常"
    state.connection.camera = undefined;
    state.connection.main_mcu = undefined;
    state.connection.simulated = undefined;
  }
  emit('connection', link);
}

export function setLatency(ms) {
  state.connection.latencyMs = ms;
  state.connection.lastPongTs = Date.now();
}

export function setRequestedMode(mode) { state.ui.requestedMode = mode; emit('ui', 'requestedMode'); }
export function setSpeedScale(v)       { state.ui.speedScale = v;      emit('ui', 'speedScale'); }
export function setJoystick(vx, vy)    { state.ui.joystick.vx = vx; state.ui.joystick.vy = vy; emit('ui', 'joystick'); }
export function setRotate(wz)          { state.ui.rotate = wz;         emit('ui', 'rotate'); }
export function countTx()              { state.ui.txCount++; }
export function countRx()              { state.ui.rxCount++; }

/** 本地强制标记急停（用于收到 error: ESTOP_ACTIVE 时立刻锁 UI，不等下一包遥测）。 */
export function markEstopLocal(active) {
  state.ui.estopLatch = active;
  emit('telemetry', { type: 'local_estop' });
}

/* ── 派生查询 ─────────────────────────────────────────────────────────── */

export function isTelemetryStale(nowMs = Date.now()) {
  return state.connection.link !== LINK.CONNECTED ||
         (nowMs - state.connection.lastTelemetryTs) > CONFIG.TELEMETRY_STALE_MS;
}

export function isPersonStale(nowMs = Date.now()) {
  return (nowMs - state.vision.lastPersonTs) > CONFIG.VISION_STALE_MS;
}

export function isGestureStale(nowMs = Date.now()) {
  // 手势更新频率低（2–4 Hz），过期阈值放宽到 bbox 的 4 倍
  return (nowMs - state.vision.lastGestureTs) > 2200;
}

/**
 * 手动控制是否可用。
 * 三个条件缺一不可：链路正常、未急停、机器人已确认处于 MANUAL。
 * 注意判断的是 robot.mode（机器人回报的真实模式），不是 UI 请求的模式。
 */
export function isManualEnabled() {
  return state.connection.link === LINK.CONNECTED &&
         !state.robot.estop && !state.ui.estopLatch && !state.ui.replaying &&
         state.robot.control_allowed !== false &&
         !state.ui.requestedMode && state.robot.state !== 'FAULT' &&
         state.connection.camera === true && state.connection.main_mcu === true &&
         Date.now() - state.connection.lastRobotTs <= CONFIG.TELEMETRY_STALE_MS &&
         state.robot.mode === 'MANUAL' &&
         !isTelemetryStale();
}

export function resetForDisconnect() {
  state.device = {};
  state.video = {}; state.lastImuTs = 0;
  state.imu = { valid: false };
  state.front=null;
  state.robot.control_allowed = undefined;
  state.robot.motion_output_installed = undefined;
  state.connection.lastHealthTs = 0;
  state.health.hr_valid=undefined;state.health.spo2_valid=undefined;state.health.hr_held=false;state.health.spo2_held=false;
  state.health.hr_bpm = undefined; state.health.spo2_pct = undefined;
  state.connection.lastRobotTs = 0;
  state.connection.lastTelemetryTs = 0;
  state.vision.lastPersonTs = 0;
  state.vision.lastGestureTs = 0;
  state.ppg.ring.clear();
  state.robot.vx = 0; state.robot.vy = 0; state.robot.wz = 0;
  state.ui.joystick.vx = 0; state.ui.joystick.vy = 0;
  state.ui.rotate = 0;
  state.ui.requestedMode = null;
}
