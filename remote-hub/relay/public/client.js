const $ = (id) => document.getElementById(id);
let socket, stream, context, source, worklet, mutedSink, receiveGain, limiter, listenGate;
let speaking = false, sequence = 0, nextPlay = 0, rx = 0, tx = 0, late = 0, captured = 0;
let mutedUntil = 0;
let unmuteTimer;
let callGeneration = 0;
const state = (message) => { $('state').textContent = message; };
const phase = (value) => { const view = $('remoteCallDialog') || document.body; if (view?.dataset) view.dataset.callPhase = value; };
const updateAudioStats = () => {
  $('audioStats').textContent = `${speaking ? '按住中' : '已松开'} · 麦克风采集 ${captured} 帧 · 已发送 ${tx} 帧 · 已接收 ${rx} 帧 · 播放重同步 ${late} 次`;
};

function packet(samples) {
  const out = new ArrayBuffer(648), view = new DataView(out);
  view.setUint8(0, 67); view.setUint8(1, 82); view.setUint8(2, 1); view.setUint8(3, 1);
  view.setUint32(4, sequence++, true);
  for (let i = 0; i < 320; ++i) view.setInt16(8 + i * 2, samples[i], true);
  return out;
}

function play(data) {
  if (!(data instanceof ArrayBuffer) || data.byteLength !== 648) return;
  const v = new DataView(data);
  if (v.getUint8(0) !== 67 || v.getUint8(1) !== 82 || v.getUint8(2) !== 1 || v.getUint8(3) !== 1) return;
  if (speaking || Date.now() < mutedUntil) return;
  const buffer = context.createBuffer(1, 320, 16000), pcm = buffer.getChannelData(0);
  for (let i = 0; i < 320; ++i) pcm[i] = v.getInt16(8 + i * 2, true) / 32768;
  const node = context.createBufferSource(); node.buffer = buffer; node.connect(receiveGain);
  const minTime = context.currentTime + 0.09;
  if (nextPlay < minTime || nextPlay > minTime + 0.35) { nextPlay = minTime; late++; }
  node.start(nextPlay); nextPlay += .02; rx++;
  if (rx % 50 === 0) updateAudioStats();
}

function setSpeaking(next) {
  speaking = next;
  const dialog = $('remoteCallDialog') || document.body; if (dialog?.dataset) dialog.dataset.speaking = String(next);
  const talk = $('talk'); if (talk?.setAttribute) talk.setAttribute('aria-pressed', String(next));
  if (dialog?.dataset?.callPhase === 'connected') state(next ? '正在说话' : '切换到收听…');
  if (next) void context?.resume();
  updateAudioStats();
  mutedUntil = next ? Number.POSITIVE_INFINITY : Date.now() + 700;
  nextPlay = 0;
  clearTimeout(unmuteTimer);
  if (listenGate && context) listenGate.gain.setTargetAtTime(0, context.currentTime, 0.005);
  if (!next) unmuteTimer = setTimeout(() => {
    if (listenGate && context && !speaking) {
      listenGate.gain.setTargetAtTime(1, context.currentTime, 0.04);
      if (dialog?.dataset?.callPhase === 'connected') state('正在收听');
    }
  }, 700);
}

async function disconnect(message = '已结束通话，麦克风已关闭。', nextPhase = 'idle') {
  const generation = ++callGeneration;
  setSpeaking(false); socket?.close(); socket = null;
  stream?.getTracks().forEach((track) => track.stop()); stream = null;
  clearTimeout(unmuteTimer);
  const closingContext = context; context = null;
  await closingContext?.close();
  if (generation !== callGeneration) return;
  receiveGain = limiter = listenGate = null;
  $('connect').disabled = false; $('disconnect').disabled = true; $('talk').disabled = true;
  phase(nextPhase); state(message);
}

$('connect').addEventListener('click', async () => {
  const token = $('token').value.trim();
  if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) {
    phase('error'); state('浏览器麦克风需要 HTTPS 或 localhost；192.168.4.1 的 HTTP 页面无法直接采集。'); return;
  }
  $('connect').disabled = true;
  const generation = ++callGeneration;
  phase('connecting'); state('正在连接家里的声音…');
  try {
    captured = tx = rx = late = 0;
    updateAudioStats();
    const acquiredStream = await navigator.mediaDevices.getUserMedia({ audio: {
      channelCount: 1, echoCancellation: true, noiseSuppression: true, autoGainControl: true
    }, video: false });
    if (generation !== callGeneration) { acquiredStream.getTracks().forEach((track) => track.stop()); return; }
    stream = acquiredStream;
    context = new AudioContext(); await context.resume();
    if (generation !== callGeneration) return;
    receiveGain = context.createGain(); receiveGain.gain.value = 10;
    limiter = context.createDynamicsCompressor(); limiter.threshold.value = -24;
    limiter.knee.value = 3; limiter.ratio.value = 12;
    listenGate = context.createGain();
    receiveGain.connect(limiter).connect(listenGate).connect(context.destination);
    await context.audioWorklet.addModule('/capture-worklet.js');
    if (generation !== callGeneration) return;
    worklet = new AudioWorkletNode(context, 'capture-pcm16');
    source = context.createMediaStreamSource(stream);
    mutedSink = context.createGain(); mutedSink.gain.value = 0;
    source.connect(worklet).connect(mutedSink).connect(context.destination);
    worklet.port.onmessage = ({ data }) => {
      captured++;
      if (speaking || captured % 50 === 0) updateAudioStats();
      if (!speaking || socket?.readyState !== WebSocket.OPEN || socket.bufferedAmount > 65536) return;
      socket.send(packet(new Int16Array(data))); tx++;
      updateAudioStats();
    };
    const scheme = location.protocol === 'https:' ? 'wss:' : 'ws:';
    socket = new WebSocket(`${scheme}//${location.host}/audio`,
      token ? ['audio-v1', `parent.${token}`] : ['audio-v1']);
    const activeSocket = socket;
    socket.binaryType = 'arraybuffer';
    socket.onopen = () => { if (socket !== activeSocket) return; $('disconnect').disabled = false; $('talk').disabled = false; phase('connected'); state('正在收听'); };
    socket.onmessage = ({ data }) => play(data);
    socket.onerror = () => { if (socket === activeSocket) { phase('error'); state('连接出错，请核对口令、证书与中继状态。'); } };
    socket.onclose = () => { if (socket === activeSocket && context) disconnect('通话已断开，请检查连接后重试。', 'error'); };
  } catch (error) { if (generation === callGeneration) await disconnect(`无法连接或打开麦克风：${error.message}`, 'error'); }
});
$('disconnect').addEventListener('click', () => { void disconnect(); });
window.addEventListener('carerover-call-close', () => { void disconnect(); });
window.addEventListener('pagehide', () => { if (socket || stream || context) void disconnect(); });
const talk = $('talk');
talk.addEventListener('touchstart', (event) => { event.preventDefault(); setSpeaking(true); }, { passive: false });
for (const event of ['touchend', 'touchcancel']) {
  document.addEventListener(event, () => { if (speaking) setSpeaking(false); });
}
talk.addEventListener('pointerdown', (event) => {
  if (event.pointerType === 'touch') return;
  event.preventDefault();
  setSpeaking(true);
  try { talk.setPointerCapture?.(event.pointerId); } catch { /* release on window blur */ }
});
for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) {
  talk.addEventListener(event, (pointer) => { if (pointer.pointerType !== 'touch' && speaking) setSpeaking(false); });
}
window.addEventListener('blur', () => { setSpeaking(false); });
