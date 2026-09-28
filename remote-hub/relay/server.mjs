import { createServer as createHttpServer } from 'node:http';
import { createServer as createHttpsServer } from 'node:https';
import { readFile, readdir } from 'node:fs/promises';
import { timingSafeEqual, randomBytes } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { WebSocket, WebSocketServer } from 'ws';
import { CareEventTracker, CARE_EVENT_TEXT } from './public/js/care-events.js';

const here = dirname(fileURLToPath(import.meta.url));
const MAX_BUFFERED = 96 * 1024;
const PACKET_BYTES = 648;
const MAX_JPEG = 128 * 1024;
const LOGIN_HTML = Buffer.from(`<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CareRover 家长登录</title><form method="post" action="/login"><h1>CareRover 远程控制台</h1><p>请输入单独生成的家长令牌。不要使用 Wi-Fi 密码。</p><input name="token" type="password" required minlength="24" autocomplete="off"><button>登录</button></form></html>`);

function equalToken(actual, expected) {
  if (!actual || !expected || actual.length > 256 || expected.length > 256) return false;
  const a = Buffer.from(actual);
  const b = Buffer.from(expected);
  return a.length === b.length && timingSafeEqual(a, b);
}

function audioFrame(data) {
  return data.length === PACKET_BYTES && data[0] === 67 && data[1] === 82 &&
    data[2] === 1 && data[3] === 1;
}

export async function makeServer({ deviceToken, parentToken, tlsKey, tlsCert,
  localOnly = false, proxyOrigin, port = 0, listenHost } = {}) {
  if (!deviceToken || !parentToken || deviceToken === parentToken ||
      deviceToken.length < 24 || parentToken.length < 24) {
    throw new Error('Distinct random AUDIO_DEVICE_TOKEN and AUDIO_PARENT_TOKEN (24+ chars) are required');
  }
  if (localOnly && proxyOrigin) throw new Error('Local test mode and trusted proxy mode are exclusive');
  if (proxyOrigin) {
    const parsed = new URL(proxyOrigin);
    if (parsed.protocol !== 'https:' || parsed.origin !== proxyOrigin || parsed.pathname !== '/' ||
        parsed.search || parsed.hash || parsed.username || parsed.password) {
      throw new Error('AUDIO_PROXY_PUBLIC_ORIGIN must be an HTTPS origin');
    }
  }
  if (!localOnly && !proxyOrigin && (!tlsKey || !tlsCert)) throw new Error('TLS key/certificate required');
  const page = await readFile(join(here, 'public', 'index.html'));
  const consolePage = await readFile(join(here, 'public', 'console.html'));
  const clientJs = await readFile(join(here, 'public', 'client.js'));
  const workletJs = await readFile(join(here, 'public', 'capture-worklet.js'));
  const css = await readFile(join(here, 'public', 'style.css'));
  const consoleCss = await readFile(join(here, 'public', 'css', 'app.css'));
  const workspaceCss = await readFile(join(here, 'public', 'css', 'workspace.css'));
  const extraCss = await readFile(join(here, 'public', 'css', 'remote.css'));
  const jsFiles = new Map(await Promise.all((await readdir(join(here, 'public', 'js')))
    .filter((name) => /^[a-z0-9-]+\.js$/.test(name))
    .map(async (name) => [name, await readFile(join(here, 'public', 'js', name))])));
  const counters = { deviceFrames: 0, parentFrames: 0, dropped: 0 };
  const peers = { device: null, parent: null, controlDevice: null, controlParent: null };
  const eventClients = new Set();
  const careEvents = new CareEventTracker();
  let eventSequence = 0, audioPaired = false;
  const publishEvent = (event) => {
    const label = CARE_EVENT_TEXT[event.kind];
    if (!label) return;
    const payload = JSON.stringify({ type: 'care_event', event_id: `${Date.now()}-${++eventSequence}`,
      occurred_at: new Date().toISOString(), ...event, ...label,
      requires_confirmation: event.kind === 'call_invite', source: 'carerover-relay' });
    for (const client of eventClients) {
      if (client.readyState === WebSocket.OPEN && client.bufferedAmount < MAX_BUFFERED) client.send(payload);
    }
    if (peers.controlParent?.readyState === WebSocket.OPEN && peers.controlParent.bufferedAmount < MAX_BUFFERED)
      peers.controlParent.send(payload);
  };
  const syncAudioPair = () => {
    const paired = peers.device?.readyState === WebSocket.OPEN && peers.parent?.readyState === WebSocket.OPEN;
    if (paired !== audioPaired) {
      audioPaired = paired;
      publishEvent({ kind: paired ? 'call_connected' : 'call_ended', active: paired });
    }
  };
  const sendCallState = () => {
    if (peers.device?.readyState === WebSocket.OPEN)
      peers.device.send(JSON.stringify({ type: 'call_state', active: peers.parent?.readyState === WebSocket.OPEN }));
    syncAudioPair();
  };
  // A lease on the board expires if the relay, gateway or Wi-Fi disappears.
  const callHeartbeat = setInterval(() => { if (peers.parent) sendCallState(); }, 1000);
  callHeartbeat.unref();
  const sessions = new Map();
  const streamClients = new Set();
  let lastJpeg = null, lastJpegAt = 0;
  const sessionFor = (req) => {
    const sid = /(?:^|;\s*)cr_session=([a-f0-9]{64})(?:;|$)/.exec(req.headers.cookie ?? '')?.[1];
    const expires = sid && sessions.get(sid);
    if (!expires || expires < Date.now()) { if (sid) sessions.delete(sid); return false; }
    return true;
  };
  const deviceFor = (req) => {
    const bearer = /^Bearer ([A-Za-z0-9._~-]{24,256})$/.exec(req.headers.authorization ?? '');
    return !!(bearer && equalToken(bearer[1], deviceToken) && !req.headers.origin);
  };
  const pushJpeg = (jpeg) => {
    lastJpeg = jpeg; lastJpegAt = Date.now();
    const header = Buffer.from(`--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ${jpeg.length}\r\n\r\n`);
    for (const res of streamClients) {
      if (res.destroyed || res.writableLength > MAX_BUFFERED) { streamClients.delete(res); res.destroy(); continue; }
      res.write(header); res.write(jpeg); res.write('\r\n');
    }
  };
  const request = (req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'SAMEORIGIN');
    const mediaSocket = proxyOrigin ? proxyOrigin.replace(/^https:/, 'wss:') :
      `${localOnly ? 'ws' : 'wss'}://${req.headers.host}`;
    res.setHeader('Content-Security-Policy', `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; media-src 'self' blob:; connect-src 'self' ${mediaSocket}; worker-src 'self'; frame-src 'self'; form-action 'self'; base-uri 'none'`);
    const path = new URL(req.url, 'https://local.invalid').pathname;
    if (path === '/login' && req.method === 'POST') {
      const parts = []; let size = 0;
      req.on('data', (part) => { size += part.length; if (size > 512) req.destroy(); else parts.push(part); });
      req.on('end', () => {
        const token = new URLSearchParams(Buffer.concat(parts).toString()).get('token') ?? '';
        if (!equalToken(token, parentToken)) { res.writeHead(403).end('Invalid token'); return; }
        const sid = randomBytes(32).toString('hex'); sessions.set(sid, Date.now() + 8 * 3600_000);
        res.setHeader('Set-Cookie', `cr_session=${sid}; HttpOnly; SameSite=Strict; Path=/; Max-Age=28800${localOnly ? '' : '; Secure'}`);
        res.writeHead(303, { Location: '/?transport=ws&video=mjpeg' }).end();
      }); return;
    }
    if (path === '/login' && req.method === 'GET') { res.setHeader('Content-Type','text/html; charset=utf-8'); res.end(LOGIN_HTML); return; }
    if (path === '/device/frame' && req.method === 'POST') {
      if (!deviceFor(req) || req.headers['content-type'] !== 'image/jpeg') { res.writeHead(403).end(); return; }
      const length = Number(req.headers['content-length']);
      if (!Number.isInteger(length) || length < 4 || length > MAX_JPEG) { res.writeHead(413).end(); return; }
      const parts = []; let size = 0;
      req.on('data', (part) => { size += part.length; if (size > MAX_JPEG) req.destroy(); else parts.push(part); });
      req.on('end', () => {
        const jpeg = Buffer.concat(parts);
        if (jpeg.length !== length || jpeg[0] !== 0xff || jpeg[1] !== 0xd8 || jpeg.at(-2) !== 0xff || jpeg.at(-1) !== 0xd9) { res.writeHead(400).end(); return; }
        pushJpeg(jpeg); res.writeHead(204).end();
      }); return;
    }
    if (path === '/health') {
      res.setHeader('Content-Type', 'application/json');
      res.end(JSON.stringify({ audioPaired: !!(peers.device && peers.parent), controlDevice: !!peers.controlDevice, videoFresh: Date.now()-lastJpegAt<2000, ...counters }));
      return;
    }
    if (!sessionFor(req)) { res.writeHead(303, { Location: '/login' }).end(); return; }
    if (path === '/stream') {
      res.setHeader('Content-Type', 'multipart/x-mixed-replace; boundary=frame');
      res.writeHead(200); streamClients.add(res);
      req.on('close', () => streamClients.delete(res));
      if (lastJpeg && Date.now()-lastJpegAt<2000) {
        res.write(`--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ${lastJpeg.length}\r\n\r\n`); res.write(lastJpeg); res.write('\r\n');
      }
      return;
    }
    const file = path === '/' ? consolePage : path === '/call' ? page : path === '/client.js' ? clientJs :
      path === '/capture-worklet.js' ? workletJs : path === '/style.css' ? css : null;
    const asset = path === '/css/app.css' ? consoleCss :
      path === '/css/workspace.css' ? workspaceCss : path === '/css/remote.css' ? extraCss :
      path.startsWith('/js/') ? jsFiles.get(path.slice(4)) : null;
    if (!file && !asset) { res.writeHead(404).end(); return; }
    res.setHeader('Content-Type', path === '/' || path === '/call' ? 'text/html; charset=utf-8' :
      path.endsWith('.css') ? 'text/css; charset=utf-8' : 'text/javascript; charset=utf-8');
    res.end(file ?? asset);
  };
  const server = (localOnly || proxyOrigin) ? createHttpServer(request) :
    createHttpsServer({ key: tlsKey, cert: tlsCert }, request);
  const wss = new WebSocketServer({ noServer: true, maxPayload: PACKET_BYTES,
    maxFragments: 8, maxBufferedChunks: 16, perMessageDeflate: false,
    handleProtocols: (protocols) =>
      protocols.has('audio-v1') ? 'audio-v1' : false });

  server.on('upgrade', (req, socket, head) => {
    const path = new URL(req.url, 'https://local.invalid').pathname;
    if (path === '/events') {
      const allowedOrigin = proxyOrigin ?? `${localOnly ? 'http' : 'https'}://${req.headers.host}`;
      const browser = sessionFor(req) && req.headers.origin === allowedOrigin;
      const app = deviceFor(req) === false && equalToken(
        /^Bearer ([A-Za-z0-9._~-]{24,256})$/.exec(req.headers.authorization ?? '')?.[1], parentToken) && !req.headers.origin;
      if ((!browser && !app) || eventClients.size >= 8) {
        socket.write('HTTP/1.1 403 Forbidden\r\n\r\n'); socket.destroy(); return;
      }
      eventWss.handleUpgrade(req, socket, head, (ws) => {
        eventClients.add(ws);
        ws.send(JSON.stringify({ ...careEvents.snapshot(), audio_paired: audioPaired,
          updated_at: new Date().toISOString() }));
        ws.on('message', () => ws.close(1003, 'read-only channel'));
        ws.on('close', () => eventClients.delete(ws));
        ws.on('error', () => {});
      }); return;
    }
    if (path === '/ws' || path === '/device/ws') {
      const role = path === '/ws' ? 'controlParent' : 'controlDevice';
      const allowedOrigin = proxyOrigin ?? `${localOnly ? 'http' : 'https'}://${req.headers.host}`;
      const okay = role === 'controlParent' ? sessionFor(req) && req.headers.origin === allowedOrigin : deviceFor(req);
      if (!okay || peers[role] || (role === 'controlParent' && !peers.controlDevice)) {
        socket.write('HTTP/1.1 403 Forbidden\r\n\r\n'); socket.destroy(); return;
      }
      controlWss.handleUpgrade(req, socket, head, (ws) => {
        peers[role] = ws;
        if (role === 'controlParent')
          ws.send(JSON.stringify({ ...careEvents.snapshot(), audio_paired: audioPaired,
            updated_at: new Date().toISOString() }));
        ws.on('message', (data, binary) => {
          if (binary || data.length > 8192) { ws.close(1003); return; }
          if (role === 'controlParent') {
            const now = Date.now();
            const rate = ws._rate ?? { start: now, count: 0 };
            if (now-rate.start >= 1000) { rate.start = now; rate.count = 0; }
            ws._rate = rate;
            if (++rate.count > 40) return;
            let message;
            try { message = JSON.parse(data.toString()); } catch { ws.close(1003); return; }
            if (!['cmd_vel','set_mode','estop','clear_estop','ping'].includes(message?.type)) { ws.close(1003); return; }
          } else {
            let telemetry;
            try { telemetry = JSON.parse(data.toString()); } catch { telemetry = null; }
            if (telemetry?.type === 'telemetry')
              for (const event of careEvents.update(telemetry)) publishEvent(event);
          }
          const other = peers[role === 'controlParent' ? 'controlDevice' : 'controlParent'];
          if (other?.readyState === WebSocket.OPEN && other.bufferedAmount < MAX_BUFFERED) other.send(data.toString());
        });
        ws.on('close', () => {
          if (peers[role] === ws) peers[role] = null;
          if (role === 'controlParent' && peers.controlDevice?.readyState === WebSocket.OPEN)
            peers.controlDevice.send(JSON.stringify({ type: 'cmd_vel', ts: Date.now(), vx: 0, vy: 0, wz: 0 }));
          if (role === 'controlDevice') peers.controlParent?.close(1011, 'device disconnected');
        });
        ws.on('error', () => {});
      }); return;
    }
    if (path !== '/audio') {
      socket.write('HTTP/1.1 404 Not Found\r\n\r\n'); socket.destroy(); return;
    }
    const bearer = /^Bearer ([A-Za-z0-9._~-]{24,256})$/.exec(req.headers.authorization ?? '');
    const protocols = (req.headers['sec-websocket-protocol'] ?? '').split(',').map((s) => s.trim());
    const parentCredential = protocols.find((p) => p.startsWith('parent.'))?.slice(7);
    const role = bearer && equalToken(bearer[1], deviceToken) ? 'device' :
      (equalToken(parentCredential, parentToken) || sessionFor(req)) ? 'parent' : null;
    const origin = req.headers.origin;
    const allowedOrigin = proxyOrigin ?? `${localOnly ? 'http' : 'https'}://${req.headers.host}`;
    // Browser connection must be same-origin; the device has no Origin header.
    if (!role || (role === 'parent' && origin !== allowedOrigin) ||
        (role === 'device' && origin) || peers[role]) {
      socket.write('HTTP/1.1 403 Forbidden\r\n\r\n'); socket.destroy(); return;
    }
    wss.handleUpgrade(req, socket, head, (ws) => {
      peers[role] = ws;
      sendCallState();
      ws.on('message', (data, isBinary) => {
        if (!isBinary || !audioFrame(data)) { ws.close(1003, 'audio frame required'); return; }
        const now = Date.now();
        const state = ws._audioRate ?? { start: now, count: 0 };
        if (now - state.start >= 1000) { state.start = now; state.count = 0; }
        ws._audioRate = state;
        if (++state.count > 60) { counters.dropped++; return; }
        counters[`${role}Frames`]++;
        const other = peers[role === 'device' ? 'parent' : 'device'];
        if (other?.readyState === WebSocket.OPEN && other.bufferedAmount < MAX_BUFFERED) {
          other.send(data, { binary: true, compress: false });
        } else counters.dropped++;
      });
      ws.on('close', () => { if (peers[role] === ws) { peers[role] = null; sendCallState(); } });
      ws.on('error', () => { /* close event owns cleanup; do not log credentials */ });
    });
  });
  const controlWss = new WebSocketServer({ noServer: true, maxPayload: 8192, perMessageDeflate: false });
  const eventWss = new WebSocketServer({ noServer: true, maxPayload: 1024, perMessageDeflate: false });
  if ((proxyOrigin || localOnly) && listenHost && listenHost !== '127.0.0.1') {
    throw new Error('Plain HTTP backend must listen on 127.0.0.1');
  }
  await new Promise((resolve) => server.listen(port, listenHost ?? (localOnly || proxyOrigin ? '127.0.0.1' : '0.0.0.0'), resolve));
  return { server, wss, counters, address: server.address(), async close() {
    clearInterval(callHeartbeat);
    for (const peer of Object.values(peers)) peer?.terminate();
    for (const client of eventClients) client.terminate();
    for (const res of streamClients) res.destroy();
    await new Promise((resolve) => eventWss.close(resolve));
    await new Promise((resolve) => controlWss.close(resolve));
    await new Promise((resolve) => wss.close(resolve));
    await new Promise((resolve) => server.close(resolve));
  } };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const localOnly = process.env.AUDIO_INSECURE_LOCALHOST === '1';
  const proxyOrigin = process.env.AUDIO_PROXY_PUBLIC_ORIGIN;
  const key = (localOnly || proxyOrigin) ? undefined : await readFile(process.env.AUDIO_TLS_KEY);
  const cert = (localOnly || proxyOrigin) ? undefined : await readFile(process.env.AUDIO_TLS_CERT);
  const app = await makeServer({ deviceToken: process.env.AUDIO_DEVICE_TOKEN,
    parentToken: process.env.AUDIO_PARENT_TOKEN, tlsKey: key, tlsCert: cert,
    localOnly, proxyOrigin, port: Number(process.env.PORT || (localOnly || proxyOrigin ? 8088 : 443)) });
  console.log(`Audio lab relay listening on ${localOnly || proxyOrigin ? 'http://127.0.0.1' : 'https://0.0.0.0'}:${app.address.port}`);
}
