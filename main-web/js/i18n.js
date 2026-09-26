/**
 * 极简 i18n：中文为默认（答辩语言），英文备用。
 * 用法：
 *   HTML 里写 data-i18n="key" 替换 textContent，
 *   data-i18n-attr="aria-label:key" 替换属性。
 * 动态文本调用 t('key') 或 t('key', {n: 1})。
 */

const DICT = {
  zh: {
    'cam.file.open': '打开文件',
    'cam.gesture': '手势识别', 'cam.unavailable': '正在等待视频信号', 'dbg.drop': '模拟断线', 'dbg.resume': '返回实时', 'ctl.pad': '全向平移摇杆',
    'app.subtitle': '多模态移动健康机器人 · 控制台',

    'link.disconnected': '未连接',
    'link.connecting':   '连接中',
    'link.connected':    '已连接',
    'link.reconnecting': '重连中',
    'link.error':        '连接错误',
    'top.latency': '延迟',
    'top.link':    '链路',

    'cam.title':      '实时视野',
    'cam.source':     '画面源',
    'cam.src.canvas': '合成画面',
    'cam.src.mjpeg':  '摄像头视频',
    'cam.src.file':   '本地视频…',
    'cam.stale':      '视觉数据过期',
    'cam.streamfail': '/stream 无法加载，已回退到合成画面',

    'robot.title':    '机器人状态',
    'robot.mode':     '模式',
    'robot.state':    '状态',
    'robot.yaw':      '相对航向 Yaw',
    'robot.pitchroll':'俯仰 / 横滚',
    'robot.aifps':    '视觉帧率',
    'robot.vel':      '速度指令',
    'robot.battery':  '电池',
    'robot.linkcam':  '摄像头链路',
    'robot.linkmcu':  '主控链路',
    'robot.stale':    '遥测中断',

    'health.title':      '生理监测',
    'health.hr':         '心率',
    'health.spo2':       '血氧',
    'health.sqi':        '信号质量',
    'health.nofinger':   '未检测到手指',
    'health.finger':     '已检测到手指',
    'health.disclaimer': '原型健康监测功能 — 不可用于医疗诊断',

    'ctl.title':  '运动控制',
    'ctl.locked': '需切换至手动模式',
    'ctl.ready':  '手动控制已启用',
    'ctl.fwd':    '前', 'ctl.back': '后', 'ctl.left': '左', 'ctl.right': '右',
    'ctl.rotl':   '左转', 'ctl.rotr': '右转', 'ctl.stop': '停止',
    'ctl.speed':  '速度上限',
    'ctl.keys':   '键盘：W A S D 平移 · Q E 旋转 · 空格停止 · Esc 急停',

    'mode.IDLE':            '待机',
    'mode.MANUAL':          '手动',
    'mode.PERSON_FOLLOW':   '跟随',
    'mode.GESTURE_CONTROL': '手势',
    'mode.HEALTH_CHECK':    '健康',
    'mode.ESTOP':           '急停',
    'mode.FAULT':           '故障',

    'state.IDLE': '待机', 'state.READY': '就绪', 'state.DRIVING': '运动中',
    'state.TRACKING': '跟随中', 'state.SEARCHING': '搜索目标', 'state.MEASURING': '测量中',
    'state.ESTOP': '急停锁定', 'state.FAULT': '故障',

    'gesture.none': '无手势',
    'g.ONE': '数字一', 'g.TWO': '数字二', 'g.THREE': '数字三', 'g.FOUR': '数字四', 'g.FIVE': '张开五指',
    'g.OK': 'OK', 'g.CALL': '打电话', 'g.LIKE': '点赞', 'g.DISLIKE': '点踩',
    'g.NONE': '无', 'g.PALM': '手掌', 'g.FIST': '握拳', 'g.THUMB_UP': '点赞',
    'g.VICTORY': '胜利手势', 'g.POINT_LEFT': '指向左', 'g.POINT_RIGHT': '指向右', 'g.UNKNOWN': '未识别',

    'hs.NO_FINGER':   '未放置手指',
    'hs.ACQUIRING':   '信号采集中',
    'hs.MEASURING':   '测量中',
    'hs.VALID':       '读数有效',
    'hs.LOW_QUALITY': '信号质量低',
    'hs.ERROR':       '传感器异常',

    'safety.estop':  '紧急停止',
    'safety.clear':  '解除急停',
    'safety.clear.title': '解除紧急停止？',
    'safety.clear.msg':   '解除后机器人将重新接受运动指令。请先确认机器人周围没有人员和障碍物。',

    'dlg.ok': '确认', 'dlg.cancel': '取消',

    'dbg.title':  '调试与联调',
    'dbg.rec':    '开始录制', 'dbg.recstop': '停止录制',
    'dbg.save':   '导出 JSON', 'dbg.replay': '回放文件…', 'dbg.clear': '清空日志',
    'dbg.tx': '发送包数', 'dbg.rx': '接收包数', 'dbg.age': '遥测时延',
    'dbg.fps': '浏览器帧率', 'dbg.vec': '当前摇杆向量', 'dbg.ppg': 'PPG 缓冲',
    'dbg.transport': '传输实现', 'dbg.rec.n': '已录制',

    'foot.note':  'Mock 模式：无需任何硬件。切换真实硬件仅需替换 /stream 与 /ws 后端。',
    'foot.proto': '通信协议',

    't.estop':         '紧急停止已触发',
    't.estop.clear':   '急停已解除',
    't.estop.reject':  '急停生效中，运动指令被拒绝',
    't.mode.fail':     '模式切换未收到确认',
    't.mode.ok':       '已切换至 {mode}',
    't.link.lost':     '连接断开，已本地归零速度',
    't.link.back':     '连接已恢复',
    't.safety.zero':   '{reason}，速度已归零',
    't.reason.hidden': '页面切到后台',
    't.reason.blur':   '窗口失去焦点',
    't.reason.cancel': '指针被系统中断',
    't.reason.close':  '连接断开',
    't.rec.start':     '开始录制遥测',
    't.rec.stop':      '录制结束，共 {n} 条',
    't.replay.start':  '回放开始：{n} 条',
    't.replay.end':    '回放结束',
    't.replay.bad':    '回放文件格式不正确'
  },

  en: {
    'cam.file.open': 'Open file',
    'cam.gesture': 'GESTURE', 'cam.unavailable': 'Waiting for video signal', 'dbg.drop': 'Disconnect once', 'dbg.resume': 'Return to live', 'ctl.pad': 'Omnidirectional joystick',
    'app.subtitle': 'Multi-modal Mobile Health Robot · Console',

    'link.disconnected': 'DISCONNECTED',
    'link.connecting':   'CONNECTING',
    'link.connected':    'CONNECTED',
    'link.reconnecting': 'RECONNECTING',
    'link.error':        'ERROR',
    'top.latency': 'Latency',
    'top.link':    'Link',

    'cam.title':      'Live view',
    'cam.source':     'Source',
    'cam.src.canvas': 'Synthetic',
    'cam.src.mjpeg':  'Camera stream',
    'cam.src.file':   'Local video…',
    'cam.stale':      'VISION STALE',
    'cam.streamfail': '/stream unavailable — fell back to synthetic scene',

    'robot.title':    'Robot Status',
    'robot.mode':     'Mode',
    'robot.state':    'State',
    'robot.yaw':      'Relative yaw',
    'robot.pitchroll':'Pitch / Roll',
    'robot.aifps':    'AI FPS',
    'robot.vel':      'Velocity cmd',
    'robot.battery':  'Battery',
    'robot.linkcam':  'Camera link',
    'robot.linkmcu':  'Main MCU link',
    'robot.stale':    'TELEMETRY STALE',

    'health.title':      'Vitals',
    'health.hr':         'Heart rate',
    'health.spo2':       'SpO₂',
    'health.sqi':        'Signal',
    'health.nofinger':   'No finger detected',
    'health.finger':     'Finger detected',
    'health.disclaimer': 'Prototype health monitoring — not for medical diagnosis',

    'ctl.title':  'Motion Control',
    'ctl.locked': 'Switch to MANUAL to enable',
    'ctl.ready':  'Manual control enabled',
    'ctl.fwd':    'FWD', 'ctl.back': 'BACK', 'ctl.left': 'LEFT', 'ctl.right': 'RIGHT',
    'ctl.rotl':   'Rotate L', 'ctl.rotr': 'Rotate R', 'ctl.stop': 'Stop',
    'ctl.speed':  'Speed limit',
    'ctl.keys':   'Keys: W A S D translate · Q E rotate · Space stop · Esc E-STOP',

    'mode.IDLE':            'IDLE',
    'mode.MANUAL':          'MANUAL',
    'mode.PERSON_FOLLOW':   'FOLLOW',
    'mode.GESTURE_CONTROL': 'GESTURE',
    'mode.HEALTH_CHECK':    'HEALTH',
    'mode.ESTOP':           'ESTOP',
    'mode.FAULT':           'FAULT',

    'state.IDLE': 'IDLE', 'state.READY': 'READY', 'state.DRIVING': 'DRIVING',
    'state.TRACKING': 'TRACKING', 'state.SEARCHING': 'SEARCHING', 'state.MEASURING': 'MEASURING',
    'state.ESTOP': 'ESTOP', 'state.FAULT': 'FAULT',

    'gesture.none': 'No gesture',
    'g.ONE': 'One', 'g.TWO': 'Two', 'g.THREE': 'Three', 'g.FOUR': 'Four', 'g.FIVE': 'Five',
    'g.OK': 'OK', 'g.CALL': 'Call', 'g.LIKE': 'Like', 'g.DISLIKE': 'Dislike',
    'g.NONE': 'None', 'g.PALM': 'Palm', 'g.FIST': 'Fist', 'g.THUMB_UP': 'Thumb up',
    'g.VICTORY': 'Victory', 'g.POINT_LEFT': 'Point left', 'g.POINT_RIGHT': 'Point right', 'g.UNKNOWN': 'Unknown',

    'hs.NO_FINGER':   'No finger',
    'hs.ACQUIRING':   'Acquiring',
    'hs.MEASURING':   'Measuring',
    'hs.VALID':       'Valid',
    'hs.LOW_QUALITY': 'Low quality',
    'hs.ERROR':       'Sensor error',

    'safety.estop':  'EMERGENCY STOP',
    'safety.clear':  'Clear E-STOP',
    'safety.clear.title': 'Clear emergency stop?',
    'safety.clear.msg':   'The robot will start accepting motion commands again. Make sure the area around it is clear.',

    'dlg.ok': 'Confirm', 'dlg.cancel': 'Cancel',

    'dbg.title':  'Debug',
    'dbg.rec':    'Record', 'dbg.recstop': 'Stop recording',
    'dbg.save':   'Export JSON', 'dbg.replay': 'Replay file…', 'dbg.clear': 'Clear log',
    'dbg.tx': 'TX packets', 'dbg.rx': 'RX packets', 'dbg.age': 'Telemetry age',
    'dbg.fps': 'Browser FPS', 'dbg.vec': 'Joystick vector', 'dbg.ppg': 'PPG buffer',
    'dbg.transport': 'Transport', 'dbg.rec.n': 'Recorded',

    'foot.note':  'Mock mode: no hardware required. Going live only swaps the /stream and /ws backends.',
    'foot.proto': 'Protocol',

    't.estop':         'Emergency stop engaged',
    't.estop.clear':   'Emergency stop cleared',
    't.estop.reject':  'Motion rejected — E-STOP active',
    't.mode.fail':     'Mode change was not acknowledged',
    't.mode.ok':       'Mode set to {mode}',
    't.link.lost':     'Link lost — velocity zeroed locally',
    't.link.back':     'Link restored',
    't.safety.zero':   '{reason} — velocity zeroed',
    't.reason.hidden': 'Page hidden',
    't.reason.blur':   'Window lost focus',
    't.reason.cancel': 'Pointer cancelled',
    't.reason.close':  'Link closed',
    't.rec.start':     'Recording telemetry',
    't.rec.stop':      'Recording stopped — {n} messages',
    't.replay.start':  'Replay started — {n} messages',
    't.replay.end':    'Replay finished',
    't.replay.bad':    'Invalid replay file'
  }
};

const STORAGE_KEY = 'carerover.lang';
let lang = 'zh';
const listeners = new Set();

/** 取翻译；缺 key 时回落到中文，再回落到 key 本身，绝不抛异常。 */
export function t(key, vars) {
  let s = DICT[lang]?.[key] ?? DICT.zh[key] ?? key;
  if (vars) for (const k of Object.keys(vars)) s = s.replaceAll(`{${k}}`, vars[k]);
  return s;
}

export function getLang() { return lang; }

export function setLang(next) {
  if (next !== 'zh' && next !== 'en') return;
  lang = next;
  try { localStorage.setItem(STORAGE_KEY, next); } catch { /* 隐私模式下忽略 */ }
  document.documentElement.lang = next === 'zh' ? 'zh-CN' : 'en';
  applyDom(document);
  listeners.forEach(fn => fn(lang));
}

export function onLangChange(fn) { listeners.add(fn); return () => listeners.delete(fn); }

export function initLang() {
  let saved = null;
  try { saved = localStorage.getItem(STORAGE_KEY); } catch { /* 忽略 */ }
  lang = saved === 'en' ? 'en' : 'zh';
  document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en';
  applyDom(document);
}

/** 把 data-i18n / data-i18n-attr 标记的节点刷成当前语言。 */
export function applyDom(root) {
  root.querySelectorAll('[data-i18n]').forEach(el => {
    el.textContent = t(el.dataset.i18n);
  });
  root.querySelectorAll('[data-i18n-attr]').forEach(el => {
    // 形如 "aria-label:ctl.rotl" 或 "title:a;aria-label:b"
    for (const pair of el.dataset.i18nAttr.split(';')) {
      const idx = pair.indexOf(':');
      if (idx < 0) continue;
      el.setAttribute(pair.slice(0, idx).trim(), t(pair.slice(idx + 1).trim()));
    }
  });
}
