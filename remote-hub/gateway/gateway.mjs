import http from 'node:http';
import { WebSocket } from 'ws';

const MAX_JPEG = 128 * 1024;
const MAX_HEADER = 1024;
const CONTROL_LIMIT = 8192;
const RETRY_MS = 1500;

export class MjpegParser {
  constructor(onFrame) { this.onFrame = onFrame; this.buffer = Buffer.alloc(0); this.length = -1; }
  push(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    for (;;) {
      if (this.length < 0) {
        const end = this.buffer.indexOf('\r\n\r\n');
        if (end < 0) {
          if (this.buffer.length > MAX_HEADER) throw new Error('MJPEG header too large');
          return;
        }
        if (end > MAX_HEADER) throw new Error('MJPEG header too large');
        const header = this.buffer.subarray(0, end).toString('latin1');
        const match = /(?:^|\r\n)Content-Length:\s*(\d+)(?:\r\n|$)/i.exec(header);
        const size = Number(match?.[1]);
        if (!Number.isInteger(size) || size < 4 || size > MAX_JPEG) throw new Error('Invalid JPEG size');
        this.length = size;
        this.buffer = this.buffer.subarray(end + 4);
      }
      if (this.buffer.length < this.length) return;
      const frame = this.buffer.subarray(0, this.length);
      if (frame[0] !== 0xff || frame[1] !== 0xd8 || frame.at(-2) !== 0xff || frame.at(-1) !== 0xd9) {
        throw new Error('Invalid JPEG markers');
      }
      this.buffer = this.buffer.subarray(this.length);
      this.length = -1;
      this.onFrame(frame);
      if (this.buffer.length > 2 * MAX_JPEG) throw new Error('MJPEG buffer too large');
    }
  }
}

function websocket(url, options) {
  return new WebSocket(url, { ...options, handshakeTimeout: 4000, perMessageDeflate: false });
}

export function createGateway({ boardUrl = 'ws://192.168.4.1/ws',
  cameraUrl = 'http://192.168.4.2/stream', audioUrl = 'ws://192.168.4.1/audio',
  relayUrl = 'http://127.0.0.1:18088', deviceToken, audioEnabled = false,
  maxVideoFps = 5, log = () => {} } = {}) {
  const relay = new URL(relayUrl);
  if (relay.protocol !== 'http:' || !['127.0.0.1', 'localhost'].includes(relay.hostname) ||
      relay.pathname !== '/' || relay.search || relay.hash) throw new Error('Relay must be a local SSH forward');
  if (!/^[A-Za-z0-9._~-]{24,256}$/.test(deviceToken ?? '')) throw new Error('Valid device token required');
  if (!(maxVideoFps >= 1 && maxVideoFps <= 10)) throw new Error('Video FPS must be 1..10');
  const stats = { boardUp: false, relayUp: false, cameraUp: false,
    audioBoardUp: false, audioRelayUp: false, audioUpFrames: 0, audioDownFrames: 0,
    boardMessages: 0, remoteMessages: 0, framesUploaded: 0, framesDropped: 0 };
  let running = false, board, cloud, camera, boardRetry, cameraRetry, lastFrameAt = 0, posting = false;
  let audioBoard, audioCloud, audioRetry;
  let lastParentAudioAt = 0;
  let pendingFrame = null;
  const scheduleBoard = () => { if (running && !boardRetry) boardRetry = setTimeout(() => { boardRetry = null; connectBoard(); }, RETRY_MS); };
  const scheduleCamera = () => { if (running && !cameraRetry) cameraRetry = setTimeout(() => { cameraRetry = null; connectCamera(); }, RETRY_MS); };
  const scheduleAudio = () => { if (running && audioEnabled && !audioRetry)
    audioRetry = setTimeout(() => { audioRetry = null; connectAudio(); }, RETRY_MS); };
  const closeControl = () => {
    stats.boardUp = stats.relayUp = false;
    if (board) { const s = board; board = null; s.terminate(); }
    if (cloud) { const s = cloud; cloud = null; s.terminate(); }
    scheduleBoard();
  };
  const connectBoard = () => {
    if (!running || board) return;
    const local = websocket(boardUrl);
    board = local;
    local.on('open', () => {
      stats.boardUp = true;
      // Board telemetry is gated on a clock-ready session. This ping never moves the car.
      local.send(JSON.stringify({ type: 'ping', ts: Date.now(), id: Date.now() }));
      const wsUrl = new URL('/device/ws', relay);
      wsUrl.protocol = 'ws:';
      const remote = websocket(wsUrl, { headers: { Authorization: `Bearer ${deviceToken}` } });
      cloud = remote;
      remote.on('open', () => { stats.relayUp = true; log('control relay connected'); });
      remote.on('message', (data, binary) => {
        if (binary || data.length > CONTROL_LIMIT || local.readyState !== WebSocket.OPEN) { closeControl(); return; }
        stats.remoteMessages++;
        local.send(data.toString());
      });
      remote.on('error', () => closeControl());
      remote.on('close', () => closeControl());
    });
    local.on('message', (data, binary) => {
      if (binary || data.length > CONTROL_LIMIT) { closeControl(); return; }
      stats.boardMessages++;
      if (cloud?.readyState === WebSocket.OPEN && cloud.bufferedAmount < 65536) cloud.send(data.toString());
    });
    local.on('error', () => closeControl());
    local.on('close', () => closeControl());
  };
  const audioPacket = (data) => data.length === 648 && data[0] === 67 && data[1] === 82 &&
    data[2] === 1 && data[3] === 1;
  const closeAudio = () => {
    stats.audioBoardUp = stats.audioRelayUp = false;
    if (audioBoard) { const s = audioBoard; audioBoard = null; s.terminate(); }
    if (audioCloud) { const s = audioCloud; audioCloud = null; s.terminate(); }
    scheduleAudio();
  };
  const connectAudio = () => {
    if (!running || !audioEnabled || audioBoard) return;
    const local = websocket(audioUrl);
    audioBoard = local;
    local.on('open', () => {
      stats.audioBoardUp = true;
      const wsUrl = new URL('/audio', relay); wsUrl.protocol = 'ws:';
      const remote = websocket(wsUrl, { headers: { Authorization: `Bearer ${deviceToken}` } });
      audioCloud = remote;
      remote.on('open', () => { stats.audioRelayUp = true; log('audio relay connected'); });
      remote.on('message', (data, binary) => {
        if (!binary) {
          let signal;
          try { signal = JSON.parse(data.toString()); } catch { closeAudio(); return; }
          if (data.length > 64 || signal?.type !== 'call_state' || typeof signal.active !== 'boolean' ||
              Object.keys(signal).length !== 2) { closeAudio(); return; }
          if (local.readyState === WebSocket.OPEN)
            local.send(JSON.stringify({ type: 'call_state', active: signal.active }));
          return;
        }
        if (!audioPacket(data)) { closeAudio(); return; }
        lastParentAudioAt = Date.now();
        if (local.readyState === WebSocket.OPEN && local.bufferedAmount < 65536) {
          local.send(data, { binary: true }); stats.audioDownFrames++;
        }
      });
      remote.on('error', () => closeAudio());
      remote.on('close', () => closeAudio());
    });
    local.on('message', (data, binary) => {
      if (!binary || !audioPacket(data)) { closeAudio(); return; }
      // Strict half duplex: do not send the speaker's echo back to the parent.
      if (Date.now() - lastParentAudioAt < 700) return;
      if (audioCloud?.readyState === WebSocket.OPEN && audioCloud.bufferedAmount < 65536) {
        audioCloud.send(data, { binary: true }); stats.audioUpFrames++;
      }
    });
    local.on('error', () => closeAudio());
    local.on('close', () => closeAudio());
  };
  const upload = async (frame) => {
    posting = true;
    try {
      const response = await fetch(new URL('/device/frame', relay), { method: 'POST',
        headers: { Authorization: `Bearer ${deviceToken}`, 'Content-Type': 'image/jpeg' },
        body: frame, signal: AbortSignal.timeout(4000) });
      if (response.status === 204) stats.framesUploaded++; else stats.framesDropped++;
    } catch { stats.framesDropped++; }
    posting = false;
    if (pendingFrame && running) {
      const next = pendingFrame; pendingFrame = null;
      void upload(next);
    }
  };
  const connectCamera = () => {
    if (!running || camera) return;
    const parser = new MjpegParser((frame) => {
      const now = Date.now();
      if (now - lastFrameAt < 1000 / maxVideoFps) return;
      lastFrameAt = now;
      if (posting) { pendingFrame = Buffer.from(frame); stats.framesDropped++; }
      else void upload(Buffer.from(frame));
    });
    const request = http.get(cameraUrl, { timeout: 5000 }, (response) => {
      if (response.statusCode !== 200 || !String(response.headers['content-type']).startsWith('multipart/x-mixed-replace')) {
        response.destroy(new Error('CAM stream unavailable')); return;
      }
      stats.cameraUp = true;
      response.on('data', (part) => { try { parser.push(part); } catch (error) { response.destroy(error); } });
      response.on('end', () => { stats.cameraUp = false; camera = null; scheduleCamera(); });
      response.on('error', () => { stats.cameraUp = false; camera = null; scheduleCamera(); });
    });
    camera = request;
    request.on('timeout', () => request.destroy(new Error('CAM timeout')));
    request.on('error', () => { stats.cameraUp = false; camera = null; scheduleCamera(); });
  };
  return { stats, start() { if (running) return; running = true; connectBoard(); connectCamera(); connectAudio(); },
    stop() { running = false; clearTimeout(boardRetry); clearTimeout(cameraRetry);
      clearTimeout(audioRetry); boardRetry = cameraRetry = audioRetry = null;
      closeControl(); closeAudio(); camera?.destroy(); camera = null; stats.cameraUp = false; } };
}

if (process.argv[1] && new URL(`file:///${process.argv[1].replaceAll('\\', '/')}`).pathname === new URL(import.meta.url).pathname) {
  const gateway = createGateway({ deviceToken: process.env.CAREROVER_DEVICE_TOKEN,
    relayUrl: process.env.CAREROVER_RELAY_URL ?? 'http://127.0.0.1:18088',
    audioEnabled: process.env.CAREROVER_AUDIO_GATEWAY === '1', log: console.log });
  gateway.start();
  const timer = setInterval(() => console.log(JSON.stringify(gateway.stats)), 10000);
  process.on('SIGINT', () => { clearInterval(timer); gateway.stop(); process.exit(0); });
}
