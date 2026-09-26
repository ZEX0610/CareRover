/**
 * 全局可调参数。
 * 规则：任何出现在业务逻辑里的数字，如果有物理含义或需要联调时调整，都放到这里。
 */
export const CONFIG = {
  /* ── 时序 / 新鲜度 ─────────────────────────────────────────────────── */
  TELEMETRY_STALE_MS: 1000,   // 超过此时间没收到 telemetry → 判定遥测中断
  HEALTH_STALE_MS: 2500,     // 健康结果约 1 Hz，单独维护新鲜度
  VISION_STALE_MS: 1400,      // 覆盖 2G:1P 调度间隔，避免单帧漏检导致画框闪烁
  PING_INTERVAL_MS: 2000,     // ping 周期（用于测 RTT）
  PING_TIMEOUT_MS: 4000,      // 超过此时间没有 pong → 延迟显示为 --

  /* ── 运动控制 ──────────────────────────────────────────────────────── */
  CMD_VEL_HZ: 20,             // cmd_vel 最大发送频率（20 Hz = 每 50 ms 一包）
  CMD_VEL_ZERO_REPEAT: 3,     // 归零后额外重复发送次数，防丢包
  JOYSTICK_DEADZONE: 0.08,    // 归一化死区半径
  DEFAULT_SPEED_SCALE: 0.6,   // 默认速度上限 60%
  ROTATE_MAGNITUDE: 1.0,      // 旋转按钮输出的 |wz| 基准（再乘速度上限）

  /**
   * 与机器人端的安全契约：主控若超过该时间未收到有效手动 cmd_vel，必须自动停车。
   * 网页侧只是复述这个约定并在 UI 中显示，真正的兜底必须在主 ESP32-S3 上实现。
   */
  DEADMAN_TIMEOUT_MS: 250,

  /* ── PPG ───────────────────────────────────────────────────────────── */
  PPG_WINDOW_SECONDS: 8,      // 波形窗口长度
  PPG_RING_CAPACITY: 4096,    // 环形缓冲容量（固定分配，杜绝无限增长）
  PPG_EXPECTED_RATE_HZ: 25,   // 期望采样率，仅用于 UI 显示与缺省推算

  /* ── 连接 ──────────────────────────────────────────────────────────── */
  WS_RECONNECT_BASE_MS: 500,  // 指数退避起点：0.5s → 1s → 2s → 4s → 封顶
  WS_RECONNECT_MAX_MS: 5000,
  WS_PATH: '/ws',
  STREAM_PATH: '/stream',

  /* ── 模式切换 ──────────────────────────────────────────────────────── */
  MODE_ACK_TIMEOUT_MS: 1500,  // 超时仍未收到 ACK / 一致 telemetry → 提示失败并回滚 UI

  /* ── 调试 ──────────────────────────────────────────────────────────── */
  DEBUG_LOG_LINES: 120,       // 日志环形容量
  DEBUG_RECORD_MAX: 20000,    // 录制条数上限，防止长时间录制吃内存

  /* ── 视觉默认值（真实硬件会在 telemetry 中下发覆盖） ──────────────── */
  DEFAULT_IMAGE_WIDTH: 320,
  DEFAULT_IMAGE_HEIGHT: 240,
  GESTURE_STABLE_CONFIDENCE: 0.45
};

/** 控制模式枚举。顺序即 UI 中的排列顺序。 */
export const MODES = ['IDLE', 'MANUAL', 'PERSON_FOLLOW', 'GESTURE_CONTROL', 'HEALTH_CHECK'];

/** 机器人可能上报、但网页不能主动请求的模式。 */
export const SYSTEM_MODES = ['ESTOP', 'FAULT'];

/** 手势枚举，必须与 CAM 端 ESP-DL 输出保持一致。 */
export const GESTURES = [
  'NONE', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'OK', 'CALL', 'LIKE', 'DISLIKE',
  'PALM', 'FIST', 'THUMB_UP', 'VICTORY', 'POINT_LEFT', 'POINT_RIGHT', 'UNKNOWN'
];

/** 健康测量状态机。 */
export const HEALTH_STATES = ['NO_FINGER', 'ACQUIRING', 'MEASURING', 'VALID', 'LOW_QUALITY', 'ERROR'];

/** Transport 连接状态机。 */
export const LINK = {
  DISCONNECTED: 'disconnected',
  CONNECTING: 'connecting',
  CONNECTED: 'connected',
  RECONNECTING: 'reconnecting',
  ERROR: 'error'
};
