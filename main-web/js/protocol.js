/**
 * CareRover WebSocket JSON 协议 —— 编解码与校验。
 * 规范文档：docs/protocol.md（本文件是它的唯一可执行实现，两者必须同步修改）
 *
 * 设计要点：
 *  - 所有入站数据都当作不可信输入：字段缺失、类型错误、超范围都不得让页面崩溃。
 *  - 校验结果是"净化后的对象"，UI 只消费净化结果，绝不直接碰原始 JSON。
 *  - 出站消息一律由本文件的构造函数生成，避免各处手拼字符串。
 */
import { MODES, SYSTEM_MODES, GESTURES, HEALTH_STATES } from './config.js';

export const ALL_MODES = [...MODES, ...SYSTEM_MODES];

/** 入站消息类型白名单。 */
export const IN_TYPES = ['telemetry', 'ppg', 'ppg_batch', 'ack', 'error', 'pong'];
/** 出站消息类型白名单。 */
export const OUT_TYPES = ['cmd_vel', 'set_mode', 'estop', 'clear_estop', 'ping'];

export const now = () => Date.now();

/* ── 基础净化工具 ─────────────────────────────────────────────────────── */

/** 有限数字才通过，否则返回 fallback。NaN / Infinity / 字符串一律拒绝。 */
export function num(v, fallback = undefined) {
  return (typeof v === 'number' && Number.isFinite(v)) ? v : fallback;
}

export function clamp(v, lo, hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/** 速度分量统一约束到 [-1, 1]。 */
export function clampVel(v) {
  const n = num(v, 0);
  return clamp(n, -1, 1);
}

export function bool(v, fallback = undefined) {
  return typeof v === 'boolean' ? v : fallback;
}

export function enumOf(v, list, fallback = undefined) {
  return (typeof v === 'string' && list.includes(v)) ? v : fallback;
}

/** 只接受对象（不含 null / 数组）。 */
function obj(v) {
  return (v && typeof v === 'object' && !Array.isArray(v)) ? v : null;
}

/* ── 出站消息构造 ─────────────────────────────────────────────────────── */

export function cmdVel(vx, vy, wz) {
  return { type: 'cmd_vel', ts: now(), vx: r3(clampVel(vx)), vy: r3(clampVel(vy)), wz: r3(clampVel(wz)) };
}
let requestSequence = 0;
export function setMode(mode) {
  return { type: 'set_mode', ts: now(), mode, request_id: ++requestSequence };
}
export function estop()      { return { type: 'estop',       ts: now() }; }
export function clearEstop() { return { type: 'clear_estop', ts: now(), request_id: ++requestSequence }; }
export function ping(id)     { return { type: 'ping',        ts: now(), id }; }

/** 保留三位小数：网络包更小，且避免 0.30000000000000004 这类噪声。 */
const r3 = v => Math.round(v * 1000) / 1000;

/** One URL policy for query parameters and telemetry. */
export function safeStreamUrl(value) {
  if (typeof value !== 'string' || !value || value.length > 512) return undefined;
  try { const u = new URL(value, typeof location !== 'undefined' ? location.href : 'http://localhost/');
    return ['http:', 'https:'].includes(u.protocol) && !u.username && !u.password ? value : undefined;
  } catch { return undefined; }
}
export function streamUrl(query, telemetry, fallback) {
  return safeStreamUrl(query) || safeStreamUrl(telemetry) || safeStreamUrl(fallback);
}

/* ── 入站解析 ─────────────────────────────────────────────────────────── */

/**
 * 解析一条原始入站消息。
 * @returns {{ok: true, msg: object} | {ok: false, error: string, raw?: string}}
 */
export function decode(raw) {
  if (typeof raw !== 'string') return { ok: false, error: 'non-text frame' };
  if (raw.length > 65536) return { ok: false, error: 'frame too large' };

  let data;
  try {
    data = JSON.parse(raw);
  } catch {
    return { ok: false, error: 'invalid JSON', raw: raw.slice(0, 120) };
  }

  const o = obj(data);
  if (!o) return { ok: false, error: 'payload is not an object' };
  if (typeof o.type !== 'string') return { ok: false, error: 'missing "type"' };
  if (!IN_TYPES.includes(o.type)) return { ok: false, error: `unknown type "${o.type}"` };

  const ts = num(o.ts, now());

  switch (o.type) {
    case 'telemetry': return { ok: true, msg: { ...normalizeTelemetry(o), ts } };
    case 'ppg':       return num(o.value) === undefined ? { ok: false, error: 'invalid PPG sample' } : { ok: true, msg: { type: 'ppg', ts, value: o.value } };
    case 'ppg_batch': return { ok: true, msg: normalizePpgBatch(o, ts) };
    case 'ack':       return { ok: true, msg: {
                          type: 'ack', ts,
                          request_type: enumOf(o.request_type, OUT_TYPES, 'unknown'),
                          ok: bool(o.ok, false), request_id: num(o.request_id)
                        } };
    case 'error':     return { ok: true, msg: {
                          type: 'error', ts,
                          code: typeof o.code === 'string' ? o.code.slice(0, 64) : 'UNKNOWN',
                          message: typeof o.message === 'string' ? o.message.slice(0, 240) : '',
                          request_type: enumOf(o.request_type, OUT_TYPES), request_id: num(o.request_id)
                        } };
    case 'pong':      return { ok: true, msg: { type: 'pong', ts, id: num(o.id, undefined) } };
    default:          return { ok: false, error: 'unreachable' };
  }
}

/**
 * telemetry 净化。任何子块缺失都返回 undefined，由 state 层决定"保留旧值"还是"标记未知"。
 * 注意：这里刻意不填默认值，因为"传感器没上报"和"传感器上报了 0"是两回事。
 */
export function normalizeTelemetry(o) {
  const out = { type: 'telemetry', tuning_profile: enumOf(o.tuning_profile,['SAFE_BASELINE','DEMO_BALANCED','DIAGNOSTIC_RAW']) };

  const c = obj(o.connection);
  if (c) out.connection = { camera: bool(c.camera), main_mcu: bool(c.main_mcu), simulated: bool(c.simulated) };

  const call = obj(o.call);
  if (call && typeof call.requested === 'boolean' &&
      Number.isSafeInteger(call.sequence) && call.sequence >= 0)
    out.call = { requested: call.requested, sequence: call.sequence };

  const f = obj(o.front);
  if(f) {
    const cm=num(f.distance_cm), age=num(f.age_ms);
    const valid=f.valid===true && cm!==undefined && cm>=2 && cm<=400 && age!==undefined && age>=0 && age<220;
    out.front={ enabled:f.enabled===true, ready:f.ready===true, valid, call_paused:f.call_paused===true,
      distance_cm:valid?cm:null, age_ms:age!==undefined&&age>=0?age:220,
      seated:f.seated===true, release_required:f.release_required===true,
      status:enumOf(f.status,['DISABLED','UNCONFIGURED','UNKNOWN','CLEAR','WARN','SLOW','BLOCKED','STOPPED','BYPASS'])??'UNKNOWN',
      phase:enumOf(f.phase,['NONE','HALT','RIGHT','MARGIN','PASS','REACQUIRE'])??'NONE',
      stop_reason:typeof f.stop_reason==='string'?f.stop_reason.slice(0,48):'' };
  }
  const r = obj(o.robot);
  if (r) out.robot = {
    mode:        enumOf(r.mode, ALL_MODES),
    state:       typeof r.state === 'string' ? r.state.slice(0, 32) : undefined,
    estop:       bool(r.estop),
    control_allowed: bool(r.control_allowed),
    motion_output_installed: bool(r.motion_output_installed),
    calibration_ready: bool(r.calibration_ready),
    battery_pct: num(r.battery_pct) !== undefined ? clamp(num(r.battery_pct), 0, 100) : undefined,
    vx: num(r.vx) !== undefined ? clampVel(r.vx) : undefined,
    vy: num(r.vy) !== undefined ? clampVel(r.vy) : undefined,
    wz: num(r.wz) !== undefined ? clampVel(r.wz) : undefined
  };

  const d = obj(o.device);
  if (d) out.device = {
    firmware: typeof d.firmware === 'string' ? d.firmware.slice(0, 96) : undefined,
    backend: typeof d.backend === 'string' ? d.backend.slice(0, 32) : undefined,
    supported_modes: Array.isArray(d.supported_modes) ? d.supported_modes.filter(m => MODES.includes(m)) : undefined,
    integration: typeof d.integration === 'string' ? d.integration.slice(0, 16) : undefined,
    stage: num(d.stage)
  };

  const i = obj(o.imu);
  if (i) out.imu = {
    yaw_deg: i.yaw_deg === null ? null : num(i.yaw_deg), pitch_deg: i.pitch_deg === null ? null : num(i.pitch_deg), roll_deg: i.roll_deg === null ? null : num(i.roll_deg),
    held: bool(i.held,false), warning_tilt: bool(i.warning_tilt,false), rejected_frames: num(i.rejected_frames),
    valid: bool(i.valid), calibrated: bool(i.calibrated), tilt_fault: bool(i.tilt_fault), age_ms: num(i.age_ms)
  };
  const video = obj(o.video);
  if (video) out.video = { stream_url: safeStreamUrl(video.stream_url),
    source_width: num(video.source_width) > 0 ? Math.min(video.source_width, 8192) : undefined,
    source_height: num(video.source_height) > 0 ? Math.min(video.source_height, 8192) : undefined,
    protocol: video.protocol === 'mjpeg' ? 'mjpeg' : undefined };


  const v = obj(o.vision);
  if (v) {
    out.vision = {
      image_width:  num(v.image_width) > 0 ? Math.min(v.image_width, 8192) : undefined,
      image_height: num(v.image_height) > 0 ? Math.min(v.image_height, 8192) : undefined,
      ai_fps:       v.ai_fps === null ? null : num(v.ai_fps)
    };
    const p = obj(v.person);
    if (p) {
      const found = bool(p.found, false);
      out.vision.person = found
        ? {
            found: true, predicted: bool(p.predicted,false),
            x: num(p.x, 0), y: num(p.y, 0),
            w: Math.max(0, num(p.w, 0)), h: Math.max(0, num(p.h, 0)),
            confidence: clamp(num(p.confidence, 0), 0, 1),
            seq: Number.isInteger(p.seq) && p.seq >= 0 ? p.seq : undefined,
            age_ms: num(p.age_ms) >= 0 ? p.age_ms : undefined
          }
        : { found: false, seq: Number.isInteger(p.seq) && p.seq >= 0 ? p.seq : undefined, age_ms: num(p.age_ms) >= 0 ? p.age_ms : undefined };
    }
    const g = obj(v.gesture);
    if (g) out.vision.gesture = {
      label: enumOf(g.label, GESTURES, 'UNKNOWN'),
      confidence: clamp(num(g.confidence, 0), 0, 1),
      stable: bool(g.stable, false), held: bool(g.held,false), age_ms: Math.max(0,num(g.age_ms,0))
    };
  }

  const h = obj(o.health);
  if (h) out.health = {
    hr_valid: bool(h.hr_valid), spo2_valid: bool(h.spo2_valid),
    hr_held: bool(h.hr_held,false), spo2_held: bool(h.spo2_held,false),
    hr_age_ms: Math.max(0,num(h.hr_age_ms,0)), spo2_age_ms: Math.max(0,num(h.spo2_age_ms,0)),
    quality: num(h.quality),
    hr_bpm:   h.hr_bpm === null ? null : num(h.hr_bpm),
    spo2_pct: h.spo2_pct === null ? null : num(h.spo2_pct),
    sqi:      num(h.sqi) !== undefined ? clamp(num(h.sqi), 0, 1) : undefined,
    finger_detected: bool(h.finger_detected),
    state:    enumOf(h.state, HEALTH_STATES)
  };

  return out;
}

/** ppg_batch 净化：非法样本直接丢弃，不让 NaN 进入绘图缓冲。 */
function normalizePpgBatch(o, ts) {
  const rate = num(o.sample_rate_hz);
  const src = Array.isArray(o.samples) ? o.samples : [];
  const samples = [];
  // 单包上限 512，防止恶意/异常大包一次性撑爆缓冲
  for (let k = 0; k < Math.min(src.length, 512); k++) {
    const s = num(src[k]);
    if (s !== undefined) samples.push(s);
  }
  return {
    type: 'ppg_batch',
    ts,
    sample_rate_hz: (rate !== undefined && rate > 0 && rate <= 2000) ? rate : undefined,
    samples
  };
}

/** 出站消息在真正 send 之前的最后一道自检，防止内部 bug 把非法包发到机器人。 */
export function validateOutgoing(msg) {
  const o = obj(msg);
  if (!o || !OUT_TYPES.includes(o.type)) return 'unknown outgoing type';
  if (num(o.ts) === undefined) return 'outgoing ts must be finite';
  if (o.type === 'cmd_vel') {
    for (const k of ['vx', 'vy', 'wz']) {
      const n = num(o[k]);
      if (n === undefined) return `cmd_vel.${k} must be a finite number`;
      if (n < -1 || n > 1) return `cmd_vel.${k} out of range [-1,1]`;
    }
  }
  if (o.type === 'set_mode' && !MODES.includes(o.mode)) return 'set_mode.mode not requestable';
  return null; // 合法
}
