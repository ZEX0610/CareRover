const $ = (id) => document.getElementById(id);
let socket, stream, context, source, worklet, mutedSink;
let speaking = false, sequence = 0, nextPlay = 0, rx = 0, tx = 0, late = 0;
const state = (message) => { $('state').textContent = message; };

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
  const buffer = context.createBuffer(1, 320, 16000), pcm = buffer.getChannelData(0);
  for (let i = 0; i < 320; ++i) pcm[i] = v.getInt16(8 + i * 2, true) / 32768;
  const node = context.createBufferSource(); node.buffer = buffer; node.connect(context.destination);
  const minTime = context.currentTime + 0.09;
  if (nextPlay < minTime || nextPlay > minTime + 0.35) { nextPlay = minTime; late++; }
  node.start(nextPlay); nextPlay += .02; rx++;
  if (rx % 50 === 0) state(`接收 ${rx} 帧，发送 ${tx} 帧，播放重同步 ${late} 次。`);
}

async function disconnect() {
  speaking = false; socket?.close(); socket = null;
  stream?.getTracks().forEach((track) => track.stop()); stream = null;
  await context?.close(); context = null;
  $('connect').disabled = false; $('disconnect').disabled = true; $('talk').disabled = true;
  state('已结束通话，麦克风已关闭。');
}

$('connect').addEventListener('click', async () => {
  const token = $('token').value.trim();
  if (!token) { state('请先输入家长端口令。'); return; }
  if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) {
    state('浏览器麦克风需要 HTTPS 或 localhost；192.168.4.1 的 HTTP 页面无法直接采集。'); return;
  }
  $('connect').disabled = true;
  try {
    stream = await navigator.mediaDevices.getUserMedia({ audio: {
      channelCount: 1, echoCancellation: true, noiseSuppression: true, autoGainControl: true
    }, video: false });
    context = new AudioContext(); await context.resume();
    await context.audioWorklet.addModule('/capture-worklet.js');
    worklet = new AudioWorkletNode(context, 'capture-pcm16');
    source = context.createMediaStreamSource(stream);
    mutedSink = context.createGain(); mutedSink.gain.value = 0;
    source.connect(worklet).connect(mutedSink).connect(context.destination);
    worklet.port.onmessage = ({ data }) => {
      if (!speaking || socket?.readyState !== WebSocket.OPEN || socket.bufferedAmount > 65536) return;
      socket.send(packet(new Int16Array(data))); tx++;
    };
    const scheme = location.protocol === 'https:' ? 'wss:' : 'ws:';
    socket = new WebSocket(`${scheme}//${location.host}/audio`, ['audio-v1', `parent.${token}`]);
    socket.binaryType = 'arraybuffer';
    socket.onopen = () => { $('disconnect').disabled = false; $('talk').disabled = false; state('通话连接成功，按住说话。'); };
    socket.onmessage = ({ data }) => play(data);
    socket.onerror = () => state('连接出错，请核对口令、证书与中继状态。');
    socket.onclose = () => { if (context) disconnect(); };
  } catch (error) { await disconnect(); state(`无法连接或打开麦克风：${error.message}`); }
});
$('disconnect').addEventListener('click', disconnect);
const talk = $('talk');
talk.addEventListener('pointerdown', (event) => { event.preventDefault(); talk.setPointerCapture(event.pointerId); speaking = true; });
for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) {
  talk.addEventListener(event, () => { speaking = false; });
}
window.addEventListener('blur', () => { speaking = false; });
