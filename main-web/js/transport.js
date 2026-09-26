/**
 * Transport 抽象层 —— 本阶段最重要的工程约束。
 *
 * UI 永远只和 Transport 接口打交道，绝不知道自己连的是 Mock 还是真实 CAM。
 * 未来接真机时，唯一要改的只是 URL，页面代码一行不动。
 *
 * 两个实现都以"JSON 文本"为唯一出入口（Mock 也真的做序列化），
 * 这样解析、校验、日志、录制这些路径在 Mock 阶段就被完整验证过。
 */
import { CONFIG, LINK } from './config.js';

export class Transport {
  constructor() {
    this._msgCbs = new Set();
    this._stateCbs = new Set();
    this._state = LINK.DISCONNECTED;
  }

  /* 子类实现 */
  connect() { throw new Error('not implemented'); }
  disconnect() { throw new Error('not implemented'); }
  /** @returns {boolean} 是否真的发出去了 */
  send(_message) { return false; }
  get name() { return 'base'; }
  get isOpen() { return this._state === LINK.CONNECTED; }

  onMessage(cb) { this._msgCbs.add(cb); return () => this._msgCbs.delete(cb); }
  onStateChange(cb) { this._stateCbs.add(cb); return () => this._stateCbs.delete(cb); }

  /* 供子类调用 */
  _emitMessage(raw) {
    for (const cb of this._msgCbs) {
      try { cb(raw); } catch (e) { console.error('[transport] message handler failed', e); }
    }
  }
  _setState(s) {
    if (this._state === s) return;
    this._state = s;
    for (const cb of this._stateCbs) {
      try { cb(s); } catch (e) { console.error('[transport] state handler failed', e); }
    }
  }
  get state() { return this._state; }
}

/* ── 真实 WebSocket ───────────────────────────────────────────────────── */

export class WebSocketTransport extends Transport {
  /** @param {string} url 形如 ws://192.168.4.1/ws */
  constructor(url) {
    super();
    this.url = url;
    this.ws = null;
    this._retry = 0;
    this._timer = null;
    this._manualClose = false;
    this._handshake = null;
  }

  get name() { return 'websocket'; }

  connect() {
    this._manualClose = false;
    this._clearTimer();
    // 已有连接在途就不重复开
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) return;

    this._setState(this._retry === 0 ? LINK.CONNECTING : LINK.RECONNECTING);

    let ws;
    try {
      ws = new WebSocket(this.url);
    } catch (err) {
      // URL 非法之类的同步错误
      console.error('[ws] construct failed', err);
      this._setState(LINK.ERROR);
      this._scheduleReconnect();
      return;
    }
    this.ws = ws;
    this._handshake = setTimeout(() => { if (this.ws === ws && ws.readyState === WebSocket.CONNECTING) ws.close(); }, 5000);

    ws.onopen = () => {
      if (this.ws !== ws) return;
      clearTimeout(this._handshake);
      this._retry = 0;
      this._setState(LINK.CONNECTED);
    };
    ws.onmessage = ev => { if (this.ws === ws) this._emitMessage(ev.data); };
    ws.onerror = () => {
      // onerror 之后浏览器一定会再触发 onclose，重连逻辑统一放在 onclose
      if (this._state === LINK.CONNECTING) this._setState(LINK.ERROR);
    };
    ws.onclose = () => {
      if (this.ws !== ws) return;
      clearTimeout(this._handshake);
      this.ws = null;
      if (this._manualClose) { this._setState(LINK.DISCONNECTED); return; }
      this._setState(LINK.RECONNECTING);
      this._scheduleReconnect();
    };
  }

  disconnect() {
    this._manualClose = true;
    clearTimeout(this._handshake);
    this._clearTimer();
    if (this.ws) { const ws = this.ws; this.ws = null; try { ws.close(); } catch { /* 已经关了 */ } }
    this._setState(LINK.DISCONNECTED);
  }

  send(message) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return false;
    if (this.ws.bufferedAmount > 65536) { this.ws.close(); return false; }
    try {
      this.ws.send(JSON.stringify(message));
      return true;
    } catch (err) {
      console.error('[ws] send failed', err);
      return false;
    }
  }

  /** 指数退避：0.5s → 1s → 2s → 4s → 封顶 5s */
  _scheduleReconnect() {
    this._clearTimer();
    const delay = Math.min(CONFIG.WS_RECONNECT_BASE_MS * (2 ** this._retry), CONFIG.WS_RECONNECT_MAX_MS);
    this._retry++;
    this._timer = setTimeout(() => { this._timer = null; this.connect(); }, delay);
  }

  _clearTimer() {
    if (this._timer) { clearTimeout(this._timer); this._timer = null; }
  }

  /** 调试用：强制断开一次，验证重连逻辑（不影响自动重连）。 */
  simulateDrop() {
    if (this.ws) { try { this.ws.close(); } catch { /* ignore */ } }
  }
}

/* ── 工厂 ─────────────────────────────────────────────────────────────── */

/**
 * 根据 URL 参数决定用哪个 Transport。
 *   ?transport=mock  浏览器内模拟（默认，无需任何服务端）
 *   ?transport=ws    连接真实 WebSocket
 *   ?ws=ws://ip/ws   显式指定地址；不指定时用当前页面同源地址
 */
export async function createTransport(search = location.search) {
  const params = new URLSearchParams(search);
  const kind = (params.get('transport') || 'mock').toLowerCase();

  if (kind === 'ws' || kind === 'websocket') {
    return new WebSocketTransport(params.get('ws') || defaultWsUrl());
  }
  // 动态载入：WebSocket 模式不下载浏览器 Mock 模块。
  const { MockTransport } = await import('./mock-transport.js');
  return new MockTransport();
}

/** 页面由 CAM 自己提供时，WebSocket 就在同源同端口。 */
export function defaultWsUrl() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = location.host || 'localhost:8080';
  return `${proto}//${host}${CONFIG.WS_PATH}`;
}

/** MJPEG 地址同理，同源即可。 */
export function defaultStreamUrl() {
  return new URL(CONFIG.STREAM_PATH, location.href).href;
}
