/** Bounded PPG rendering, independent of sample arrival frequency. */
import { CONFIG } from './config.js';
export class PpgChart {
  constructor(canvas) { this.canvas = canvas; this.ctx = canvas.getContext('2d'); this.low = 16000; this.high = 22000; }
  render(ppg, valid, now = Date.now()) {
    const c = this.canvas, ctx = this.ctx, box = c.getBoundingClientRect();
    const dpr = Math.min(devicePixelRatio || 1, 2), w = box.width, h = box.height;
    if (c.width !== Math.round(w * dpr) || c.height !== Math.round(h * dpr)) {
      c.width = Math.round(w * dpr); c.height = Math.round(h * dpr);
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, w, h);
    ctx.strokeStyle = 'rgba(255,255,255,.055)'; ctx.lineWidth = 1;
    for (let i = 1; i < 8; i++) { ctx.beginPath(); ctx.moveTo(w * i / 8, 0); ctx.lineTo(w * i / 8, h); ctx.stroke(); }
    ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();
    const windowMs = CONFIG.PPG_WINDOW_SECONDS * 1000, from = now - windowMs;
    let low = Infinity, high = -Infinity;
    ppg.ring.each(from, v => { low = Math.min(low, v); high = Math.max(high, v); });
    if (!Number.isFinite(low)) return;
    const pad = Math.max(100, (high - low) * .18);
    this.low += (low - pad - this.low) * .08; this.high += (high + pad - this.high) * .08;
    ctx.beginPath(); let previous = null; let last = null;
    ppg.ring.each(from, (v, time) => {
      const x = (time - from) / windowMs * w, y = h - (v - this.low) / (this.high - this.low) * h;
      if (previous === null || time - previous > 150) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      previous = time; last = { x, y };
    });
    ctx.strokeStyle = valid ? '#76d9c5' : '#62676d'; ctx.lineWidth = 1.8; ctx.lineJoin = 'round'; ctx.stroke();
    if (valid && last && now - ppg.lastTs < 500) {
      ctx.fillStyle = '#b4f4df'; ctx.beginPath(); ctx.arc(last.x, last.y, 3, 0, Math.PI * 2); ctx.fill();
    }
  }
}
