/**
 * 视频叠加层：把"源图像坐标系"里的检测结果，正确画到"屏幕坐标系"上。
 *
 * 这里是整个前端最容易出错的地方，必须守住三条：
 *  1. bbox 的坐标单位永远是源图像像素（image_width × image_height），
 *     而 <img> 的显示尺寸由 CSS 决定，两者没有任何必然关系。
 *  2. <img> 用的是 object-fit: contain，画面会出现上下或左右黑边（letterbox），
 *     映射时必须把这段偏移算进去，否则框会整体漂移。
 *  3. Canvas 必须按 devicePixelRatio 放大位图尺寸，否则在 Retina 上是糊的。
 *
 * computeContainFit 是纯函数，由 tests/core.test.js 覆盖。
 */
import { CONFIG } from './config.js';

/**
 * 计算 object-fit: contain 的实际绘制区域。
 * @returns {{ox:number, oy:number, dw:number, dh:number, scale:number}}
 *          ox/oy 是内容左上角相对容器的偏移（即黑边宽度），dw/dh 是内容显示尺寸。
 */
export function computeContainFit(imgW, imgH, boxW, boxH) {
  if (!(imgW > 0 && imgH > 0 && boxW > 0 && boxH > 0)) {
    return { ox: 0, oy: 0, dw: boxW || 0, dh: boxH || 0, scale: 1 };
  }
  const scale = Math.min(boxW / imgW, boxH / imgH);
  const dw = imgW * scale;
  const dh = imgH * scale;
  return { ox: (boxW - dw) / 2, oy: (boxH - dh) / 2, dw, dh, scale };
}

/** 源图像坐标 → 容器 CSS 坐标（点）。 */
export function mapImagePointToCanvas(x, y, imageWidth, imageHeight, boxW, boxH) {
  const f = computeContainFit(imageWidth, imageHeight, boxW, boxH);
  return { x: f.ox + x * f.scale, y: f.oy + y * f.scale };
}

/** 源图像坐标 → 容器 CSS 坐标（矩形）。 */
export function mapImageRectToCanvas(x, y, w, h, imageWidth, imageHeight, boxW, boxH) {
  const f = computeContainFit(imageWidth, imageHeight, boxW, boxH);
  return { x: f.ox + x * f.scale, y: f.oy + y * f.scale, w: w * f.scale, h: h * f.scale };
}

// Cosmetic only: telemetry arrives at ~10 Hz while the canvas redraws at up
// to 60 Hz. Follow each new displayed box over the intervening paint frames.
// Larger corrections catch up faster so smoothing does not hide a moved face.
export function smoothDisplayRect(previous, target, elapsedMs) {
  if (!previous || elapsedMs >= 250) return { ...target };
  const dt = Math.max(0, Number.isFinite(elapsedMs) ? elapsedMs : 0);
  const distance = Math.hypot(
    target.x + target.w / 2 - previous.x - previous.w / 2,
    target.y + target.h / 2 - previous.y - previous.h / 2
  );
  const tauMs = distance > Math.hypot(target.w, target.h) * 0.45 ? 60 : 85;
  const alpha = 1 - Math.exp(-dt / tauMs);
  return {
    x: previous.x + (target.x - previous.x) * alpha,
    y: previous.y + (target.y - previous.y) * alpha,
    w: previous.w + (target.w - previous.w) * alpha,
    h: previous.h + (target.h - previous.h) * alpha
  };
}

const COLOR = {
  box: '#0a84ff',
  boxStale: 'rgba(255,255,255,.28)',
  guide: 'rgba(255,255,255,.16)',
  center: '#32d74b',
  text: '#ffffff',
  chipBg: 'rgba(8,9,10,.78)'
};

export class VideoOverlay {
  /**
   * @param {HTMLCanvasElement} canvas 叠加画布
   * @param {HTMLElement} stage 决定尺寸的容器（与 <img> 同一个盒子）
   */
  constructor(canvas, stage) {
    this.canvas = canvas;
    this.stage = stage;
    this.ctx = canvas.getContext('2d');
    this.cssW = 0; this.cssH = 0; this.dpr = 1;
    this.displayRect = null; this.lastBoxFrameMs = 0;

    this._ro = new ResizeObserver(() => this._resize());
    this._ro.observe(stage);
    this._resize();

    // 显示器 DPI 变化（拖到外接屏 / 系统缩放变化）时重新适配
    this._mql = null;
    this._watchDpr();
  }

  destroy() {
    this._ro.disconnect();
    if (this._mql) this._mql.removeEventListener('change', this._onDpr);
  }

  _watchDpr() {
    this._onDpr = () => { this._resize(); this._watchDpr(); };
    if (window.matchMedia) {
      this._mql = window.matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
      this._mql.addEventListener('change', this._onDpr, { once: true });
    }
  }

  _resize() {
    const rect = this.stage.getBoundingClientRect();
    // dpr 封顶 2：3x 屏上再往上加只是白烧 GPU，肉眼无差别
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.max(1, Math.round(rect.width));
    const h = Math.max(1, Math.round(rect.height));
    if (w === this.cssW && h === this.cssH && dpr === this.dpr) return;
    this.cssW = w; this.cssH = h; this.dpr = dpr;
    this.displayRect = null; this.lastBoxFrameMs = 0;
    this.canvas.width = Math.round(w * dpr);
    this.canvas.height = Math.round(h * dpr);
    // 之后所有绘制都用 CSS 像素为单位，无需在业务代码里到处乘 dpr
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  /**
   * 绘制一帧叠加层。
   * @param {object} vision state.vision
   * @param {{personStale:boolean, gestureStale:boolean, showGuide:boolean}} flags
   */
  render(vision, flags) {
    const ctx = this.ctx;
    const W = this.cssW, H = this.cssH;
    ctx.clearRect(0, 0, W, H);
    if (W < 8 || H < 8) return;

    const iw = vision.image_width || CONFIG.DEFAULT_IMAGE_WIDTH;
    const ih = vision.image_height || CONFIG.DEFAULT_IMAGE_HEIGHT;
    const fit = computeContainFit(iw, ih, W, H);

    // 画面中心竖直参考线：跟随模式下用来判断目标偏移
    if (flags.showGuide) {
      const cx = fit.ox + fit.dw / 2;
      ctx.save();
      ctx.strokeStyle = COLOR.guide;
      ctx.lineWidth = 1;
      ctx.setLineDash([4, 6]);
      ctx.beginPath();
      ctx.moveTo(Math.round(cx) + .5, fit.oy);
      ctx.lineTo(Math.round(cx) + .5, fit.oy + fit.dh);
      ctx.stroke();
      ctx.restore();
    }

    const p = vision.person;
    // bbox 超过 VISION_STALE_MS 未更新就直接不画：宁可没有，也不能显示过期位置
    if (p && p.found && !flags.personStale) {
      const target = mapImageRectToCanvas(p.x, p.y, p.w, p.h, iw, ih, W, H);
      const now = performance.now();
      const r = smoothDisplayRect(this.displayRect, target, this.lastBoxFrameMs ? now - this.lastBoxFrameMs : 250);
      this.displayRect = r; this.lastBoxFrameMs = now;
      this._drawBox(r, `${p.predicted ? 'PREDICTED' : 'PERSON'} ${Math.round((p.confidence ?? 0) * 100)}%`, COLOR.box, p.predicted);
      this._drawCenter(r);
      if (flags.showGuide) this._drawOffset(r, fit);
    } else {
      this.displayRect = null; this.lastBoxFrameMs = 0;
    }
  }

  /** AF 取景框式的四角标记，比整圈矩形更像仪器，也不遮挡目标。 */
  _drawBox(r, label, color, predicted=false) {
    const ctx = this.ctx;
    const corner = Math.max(10, Math.min(r.w, r.h) * 0.22);

    ctx.save();
    ctx.strokeStyle = color;
    if(predicted)ctx.setLineDash([5,4]);
    ctx.lineWidth = 2;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';

    ctx.globalAlpha = 0.22;
    ctx.strokeRect(r.x, r.y, r.w, r.h);

    ctx.globalAlpha = 1;
    ctx.beginPath();
    // 左上
    ctx.moveTo(r.x, r.y + corner);           ctx.lineTo(r.x, r.y); ctx.lineTo(r.x + corner, r.y);
    // 右上
    ctx.moveTo(r.x + r.w - corner, r.y);     ctx.lineTo(r.x + r.w, r.y); ctx.lineTo(r.x + r.w, r.y + corner);
    // 右下
    ctx.moveTo(r.x + r.w, r.y + r.h - corner); ctx.lineTo(r.x + r.w, r.y + r.h); ctx.lineTo(r.x + r.w - corner, r.y + r.h);
    // 左下
    ctx.moveTo(r.x + corner, r.y + r.h);     ctx.lineTo(r.x, r.y + r.h); ctx.lineTo(r.x, r.y + r.h - corner);
    ctx.stroke();
    ctx.restore();

    this._chip(label, r.x, r.y - 8, color);
  }

  _drawCenter(r) {
    const ctx = this.ctx;
    const cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    ctx.save();
    ctx.strokeStyle = COLOR.center;
    ctx.fillStyle = COLOR.center;
    ctx.lineWidth = 1.5;
    ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2); ctx.fill();
    ctx.globalAlpha = .55;
    ctx.beginPath();
    ctx.moveTo(cx - 9, cy); ctx.lineTo(cx - 4, cy);
    ctx.moveTo(cx + 4, cy); ctx.lineTo(cx + 9, cy);
    ctx.moveTo(cx, cy - 9); ctx.lineTo(cx, cy - 4);
    ctx.moveTo(cx, cy + 4); ctx.lineTo(cx, cy + 9);
    ctx.stroke();
    ctx.restore();
  }

  /** 目标中心相对画面中心的水平偏移条：跟随模式的控制误差可视化。 */
  _drawOffset(r, fit) {
    const ctx = this.ctx;
    const cx = fit.ox + fit.dw / 2;
    const px = r.x + r.w / 2;
    const y = fit.oy + fit.dh - 16;
    ctx.save();
    ctx.strokeStyle = COLOR.box;
    ctx.globalAlpha = .8;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(cx, y);
    ctx.lineTo(px, y);
    ctx.stroke();
    ctx.globalAlpha = .35;
    ctx.beginPath(); ctx.arc(cx, y, 2.5, 0, Math.PI * 2); ctx.fill();
    ctx.restore();
  }

  /** 半透明标签片，避免文字压在复杂背景上看不清。 */
  _chip(text, x, bottomY, color) {
    const ctx = this.ctx;
    ctx.save();
    ctx.font = '600 12px ui-monospace, "SF Mono", Menlo, monospace';
    const padX = 7, h = 20;
    const w = ctx.measureText(text).width + padX * 2;
    const y = Math.max(2, bottomY - h);
    const cx = Math.min(Math.max(x, 2), Math.max(2, this.cssW - w - 2));

    ctx.fillStyle = COLOR.chipBg;
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    roundRect(ctx, cx, y, w, h, 5);
    ctx.fill();
    ctx.globalAlpha = .5; ctx.stroke(); ctx.globalAlpha = 1;

    ctx.fillStyle = COLOR.text;
    ctx.textBaseline = 'middle';
    ctx.fillText(text, cx + padX, y + h / 2 + .5);
    ctx.restore();
  }
}

export function roundRect(ctx, x, y, w, h, r) {
  const rr = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + rr, y);
  ctx.arcTo(x + w, y,     x + w, y + h, rr);
  ctx.arcTo(x + w, y + h, x,     y + h, rr);
  ctx.arcTo(x,     y + h, x,     y,     rr);
  ctx.arcTo(x,     y,     x + w, y,     rr);
  ctx.closePath();
}
