const $ = (id) => document.getElementById(id);
let socket, stream, context, source, worklet, mutedSink, receiveGain, limiter, listenGate;
let speaking = false, sequence = 0, nextPlay = 0, rx = 0, tx = 0, late = 0, captured = 0;
let mutedUntil = 0;
let unmuteTimer;
const state = (message) => { $('state').textContent = message; };
const updateAudioStats = () => {
  $('audioStats').textContent = `${speaking ? '按住中' : '已松开'} · 麦克风采集 ${captured} 帧 · 已发送 ${tx} 帧`;
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
  if (rx % 50 === 0) state(`接收 ${rx} 帧，发送 ${tx} 帧，播放重同步 ${late} 次。`);
}

function setSpeaking(next) {
  speaking = next;
  if (next) void context?.resume();
  updateAudioStats();
  mutedUntil = next ? Number.POSITIVE_INFINITY : Date.now() + 700;
  nextPlay = 0;
  clearTimeout(unmuteTimer);
  if (listenGate && context) listenGate.gain.setTargetAtTime(0, context.currentTime, 0.005);
  if (!next) unmuteTimer = setTimeout(() => {
    if (listenGate && context && !speaking)
      listenGate.gain.setTargetAtTime(1, context.currentTime, 0.04);
  }, 700);
}

async function disconnect() {
  setSpeaking(false); socket?.close(); socket = null;
  stream?.getTracks().forEach((track) => track.stop()); stream = null;
  clearTimeout(unmuteTimer);
  await context?.close(); context = null; receiveGain = limiter = listenGate = null;
  $('connect').disabled = false; $('disconnect').disabled = true; $('talk').disabled = true;
  state('已结束通话，麦克风已关闭。');
}

$('connect').addEventListener('click', async () => {
  const token = $('token').value.trim();
  if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) {
    state('浏览器麦克风需要 HTTPS 或 localhost；192.168.4.1 的 HTTP 页面无法直接采集。'); return;
  }
  $('connect').disabled = true;
  try {
    captured = tx = rx = late = 0;
    updateAudioStats();
    stream = await navigator.mediaDevices.getUserMedia({ audio: {
      channelCount: 1, echoCancellation: true, noiseSuppression: true, autoGainControl: true
    }, video: false });
    context = new AudioContext(); await context.resume();
    receiveGain = context.createGain(); receiveGain.gain.value = 10;
    limiter = context.createDynamicsCompressor(); limiter.threshold.value = -24;
    limiter.knee.value = 3; limiter.ratio.value = 12;
    listenGate = context.createGain();
    receiveGain.connect(limiter).connect(listenGate).connect(context.destination);
    await context.audioWorklet.addModule('/capture-worklet.js');
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
      if (tx % 50 === 0) state(`接收 ${rx} 帧，发送 ${tx} 帧。`);
    };
    const scheme = location.protocol === 'https:' ? 'wss:' : 'ws:';
    socket = new WebSocket(`${scheme}//${location.host}/audio`,
      token ? ['audio-v1', `parent.${token}`] : ['audio-v1']);
    socket.binaryType = 'arraybuffer';
    socket.onopen = () => { $('disconnect').disabled = false; $('talk').disabled = false; state('通话连接成功，按住说话、松开收听。'); };
    socket.onmessage = ({ data }) => play(data);
    socket.onerror = () => state('连接出错，请核对口令、证书与中继状态。');
    socket.onclose = () => { if (context) disconnect(); };
  } catch (error) { await disconnect(); state(`无法连接或打开麦克风：${error.message}`); }
});
$('disconnect').addEventListener('click', disconnect);
window.addEventListener('carerover-call-close', () => { if (socket || context) disconnect(); });
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
