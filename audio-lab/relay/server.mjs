import { createServer as createHttpServer } from 'node:http';
import { createServer as createHttpsServer } from 'node:https';
import { readFile } from 'node:fs/promises';
import { timingSafeEqual } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { WebSocket, WebSocketServer } from 'ws';

const here = dirname(fileURLToPath(import.meta.url));
const MAX_BUFFERED = 96 * 1024;
const PACKET_BYTES = 648;

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
  localOnly = false, port = 0, listenHost } = {}) {
  if (!deviceToken || !parentToken || deviceToken === parentToken ||
      deviceToken.length < 24 || parentToken.length < 24) {
    throw new Error('Distinct random AUDIO_DEVICE_TOKEN and AUDIO_PARENT_TOKEN (24+ chars) are required');
  }
  if (!localOnly && (!tlsKey || !tlsCert)) throw new Error('TLS key/certificate required');
  const page = await readFile(join(here, 'public', 'index.html'));
  const clientJs = await readFile(join(here, 'public', 'client.js'));
  const workletJs = await readFile(join(here, 'public', 'capture-worklet.js'));
  const css = await readFile(join(here, 'public', 'style.css'));
  const counters = { deviceFrames: 0, parentFrames: 0, dropped: 0 };
  const peers = { device: null, parent: null };
  const request = (req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('X-Content-Type-Options', 'nosniff');
    const mediaSocket = `${localOnly ? 'ws' : 'wss'}://${req.headers.host}`;
    res.setHeader('Content-Security-Policy', `default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self' ${mediaSocket}; worker-src 'self'`);
    const path = new URL(req.url, 'https://local.invalid').pathname;
    if (path === '/health') {
      res.setHeader('Content-Type', 'application/json');
      res.end(JSON.stringify({ paired: !!(peers.device && peers.parent), ...counters }));
      return;
    }
    const file = path === '/' ? page : path === '/client.js' ? clientJs :
      path === '/capture-worklet.js' ? workletJs : path === '/style.css' ? css : null;
    if (!file) { res.writeHead(404).end(); return; }
    res.setHeader('Content-Type', path === '/' ? 'text/html; charset=utf-8' :
      path === '/style.css' ? 'text/css; charset=utf-8' : 'text/javascript; charset=utf-8');
    res.end(file);
  };
  const server = localOnly ? createHttpServer(request) :
    createHttpsServer({ key: tlsKey, cert: tlsCert }, request);
  const wss = new WebSocketServer({ noServer: true, maxPayload: PACKET_BYTES,
    maxFragments: 8, maxBufferedChunks: 16, perMessageDeflate: false,
    handleProtocols: (protocols) =>
      protocols.has('audio-v1') ? 'audio-v1' : false });

  server.on('upgrade', (req, socket, head) => {
    if (new URL(req.url, 'https://local.invalid').pathname !== '/audio') {
      socket.write('HTTP/1.1 404 Not Found\r\n\r\n'); socket.destroy(); return;
    }
    const bearer = /^Bearer ([A-Za-z0-9._~-]{24,256})$/.exec(req.headers.authorization ?? '');
    const protocols = (req.headers['sec-websocket-protocol'] ?? '').split(',').map((s) => s.trim());
    const parentCredential = protocols.find((p) => p.startsWith('parent.'))?.slice(7);
    const role = bearer && equalToken(bearer[1], deviceToken) ? 'device' :
      equalToken(parentCredential, parentToken) ? 'parent' : null;
    const origin = req.headers.origin;
    const allowedOrigin = `${localOnly ? 'http' : 'https'}://${req.headers.host}`;
    // Browser connection must be same-origin; the device has no Origin header.
    if (!role || (role === 'parent' && origin !== allowedOrigin) ||
        (role === 'device' && origin) || peers[role]) {
      socket.write('HTTP/1.1 403 Forbidden\r\n\r\n'); socket.destroy(); return;
    }
    wss.handleUpgrade(req, socket, head, (ws) => {
      peers[role] = ws;
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
      ws.on('close', () => { if (peers[role] === ws) peers[role] = null; });
      ws.on('error', () => { /* close event owns cleanup; do not log credentials */ });
    });
  });
  await new Promise((resolve) => server.listen(port, listenHost ?? (localOnly ? '127.0.0.1' : '0.0.0.0'), resolve));
  return { server, wss, counters, address: server.address(), async close() {
    for (const peer of Object.values(peers)) peer?.terminate();
    await new Promise((resolve) => wss.close(resolve));
    await new Promise((resolve) => server.close(resolve));
  } };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const localOnly = process.env.AUDIO_INSECURE_LOCALHOST === '1';
  const key = localOnly ? undefined : await readFile(process.env.AUDIO_TLS_KEY);
  const cert = localOnly ? undefined : await readFile(process.env.AUDIO_TLS_CERT);
  const app = await makeServer({ deviceToken: process.env.AUDIO_DEVICE_TOKEN,
    parentToken: process.env.AUDIO_PARENT_TOKEN, tlsKey: key, tlsCert: cert,
    localOnly, port: Number(process.env.PORT || (localOnly ? 8088 : 443)) });
  console.log(`Audio lab relay listening on ${localOnly ? 'http://127.0.0.1' : 'https://0.0.0.0'}:${app.address.port}`);
}
