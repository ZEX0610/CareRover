import { VideoOverlay, computeContainFit, roundRect } from './video-overlay.js';
import { streamUrl } from './protocol.js';
import { defaultStreamUrl } from './transport.js';
/** Video sources and metadata share a stage, never a transport implementation. */
export class VideoPanel {
  constructor({ stage, source, overlay, image, video, select, input, openFile, notice }) {
    Object.assign(this, { stage, source, image, video, select, input, openFile, notice });
    this.overlay = new VideoOverlay(overlay, stage); this.ctx = source.getContext('2d');
    this.streamUrl = null; this.retryTimer = null; this.retryMs = 1000; this.streamConnected = false;
    this.kind = 'canvas'; this.objectUrl = null; this.ready = false;
    this.abort = new AbortController(); const signal = this.abort.signal;
    select.addEventListener('change', () => {
      this.setSource(select.value);
    }, { signal });
    openFile.addEventListener('click', () => input.click(), { signal });
    input.addEventListener('change', () => {
      const file = input.files[0]; if (!file) return;
      input.value = '';
      this.setSource('file'); this.objectUrl = URL.createObjectURL(file); video.src = this.objectUrl;
      video.play().catch(() => { this.notice('video.failed'); this.setSource('canvas'); });
    }, { signal });
    video.addEventListener('loadeddata', () => { this.ready = true; }, { signal });
    video.addEventListener('error', () => { this.ready = false; this.notice('video.failed'); }, { signal });
    image.addEventListener('load', () => { this.ready = true; this.streamFailed = false; this.retryMs = 1000; }, { signal });
    image.addEventListener('error', () => { this.ready = false; this.streamFailed = true; this.scheduleRetry(); }, { signal });
    this.setSource('canvas');
  }
  setSource(kind) {
    clearTimeout(this.retryTimer); this.retryTimer = null;
    this.video.pause(); this.video.removeAttribute('src'); this.video.load();
    this.image.removeAttribute('src');
    if (this.objectUrl) { URL.revokeObjectURL(this.objectUrl); this.objectUrl = null; }
    this.kind = kind; this.ready = kind === 'canvas'; this.streamFailed = false; this.select.value = kind;
    this.openFile.hidden = kind !== 'file';
    this.source.hidden = kind !== 'canvas'; this.image.hidden = kind !== 'mjpeg'; this.video.hidden = kind !== 'file';
    this.stage.dataset.source = kind;
    if (kind === 'mjpeg') this.connection(true);
  }
  setStreamUrl(url) {
    const next = url || null;
    if (next === this.streamUrl) return;
    this.streamUrl = next;
    if (this.kind === 'mjpeg') this.connection(true);
  }
  scheduleRetry() {
    if (this.retryTimer || this.kind !== 'mjpeg' || !this.streamConnected) return;
    this.retryTimer = setTimeout(() => {
      this.retryTimer = null;
      if (this.kind === 'mjpeg' && this.streamConnected) this.connection(true);
    }, this.retryMs);
    this.retryMs = Math.min(5000, this.retryMs * 2);
  }
  connection(connected) {
    this.streamConnected = connected;
    clearTimeout(this.retryTimer); this.retryTimer = null;
    if (this.kind !== 'mjpeg') return;
    this.ready = false; this.streamFailed = !connected;
    this.image.removeAttribute('src');
    if (connected) this.image.src = streamUrl(new URLSearchParams(location.search).get('stream'), this.streamUrl, defaultStreamUrl());
  }
  render(vision, flags) {
    if (this.kind === 'mjpeg' && !this.streamFailed && this.image.naturalWidth > 0) this.ready = true;
    const width = this.kind === 'file' ? this.video.videoWidth : this.kind === 'mjpeg' ? this.image.naturalWidth || vision.image_width : vision.image_width;
    const height = this.kind === 'file' ? this.video.videoHeight : this.kind === 'mjpeg' ? this.image.naturalHeight || vision.image_height : vision.image_height;
    const aspect = width > 0 && height > 0 ? `${width} / ${height}` : '4 / 3';
    if (this.stage.style.aspectRatio !== aspect) this.stage.style.aspectRatio = aspect;
    if (this.kind === 'canvas') this.drawScene(vision, flags.personStale);
    let mapped = vision;
    if (this.kind === 'file' && this.video.videoWidth && this.video.videoHeight) {
      // Local clips may have a different aspect ratio from synthetic metadata.
      // Preserve normalized positions inside the actual video's contain rectangle.
      const sx = this.video.videoWidth / vision.image_width, sy = this.video.videoHeight / vision.image_height;
      const p = vision.person;
      mapped = { ...vision, image_width: this.video.videoWidth, image_height: this.video.videoHeight,
        person: p?.found ? { ...p, x: p.x * sx, y: p.y * sy, w: p.w * sx, h: p.h * sy } : p };
    }
    this.overlay.render(mapped, { ...flags, personStale: flags.personStale || !this.ready });
  }
  drawScene(vision, stale) {
    const { source: canvas, ctx, stage } = this;
    const box = stage.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
    if (canvas.width !== Math.round(box.width * dpr) || canvas.height !== Math.round(box.height * dpr)) {
      canvas.width = Math.round(box.width * dpr); canvas.height = Math.round(box.height * dpr);
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const W = box.width, H = box.height, iw = vision.image_width, ih = vision.image_height;
    ctx.fillStyle = '#171a1b'; ctx.fillRect(0, 0, W, H);
    const f = computeContainFit(iw, ih, W, H);
    ctx.save(); ctx.translate(f.ox, f.oy); ctx.scale(f.scale, f.scale);
    ctx.beginPath(); ctx.rect(0, 0, iw, ih); ctx.clip();
    const wall = ctx.createLinearGradient(0, 0, iw, ih); wall.addColorStop(0, '#777c7b'); wall.addColorStop(.65, '#42494a'); wall.addColorStop(1, '#242d30');
    ctx.fillStyle = wall; ctx.fillRect(0, 0, iw, ih);
    // Architectural room: a broad light aperture and a quiet receding floor.
    ctx.fillStyle = '#9ba5a1'; ctx.fillRect(iw * .09, ih * .1, iw * .18, ih * .48);
    const light = ctx.createLinearGradient(0, 0, iw * .4, 0); light.addColorStop(0, '#c2c8c0'); light.addColorStop(1, '#929e98');
    ctx.fillStyle = light; ctx.fillRect(iw * .10, ih * .11, iw * .16, ih * .46);
    ctx.strokeStyle = 'rgba(29,39,37,.35)'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(iw * .18, ih * .11); ctx.lineTo(iw * .18, ih * .57); ctx.stroke();
    ctx.fillStyle = '#434c4c'; ctx.beginPath(); ctx.moveTo(0, ih * .72); ctx.lineTo(iw, ih * .67); ctx.lineTo(iw, ih); ctx.lineTo(0, ih); ctx.fill();
    ctx.strokeStyle = 'rgba(200,215,210,.10)'; ctx.lineWidth = .5;
    for (let n = -3; n < 8; n++) { ctx.beginPath(); ctx.moveTo(iw * .55, ih * .66); ctx.lineTo(n * iw / 4, ih); ctx.stroke(); }
    ctx.fillStyle = '#303839'; roundRect(ctx, iw * .77, ih * .54, iw * .15, ih * .13, 2); ctx.fill();
    ctx.fillStyle = '#8c9690'; ctx.fillRect(iw * .76, ih * .53, iw * .17, ih * .012);
    const p = vision.person;
    if (p?.found && !stale) {
      ctx.save(); ctx.translate(p.x, p.y); ctx.scale(p.w / 100, p.h / 220);
      ctx.fillStyle = 'rgba(5,12,14,.25)'; ctx.beginPath(); ctx.ellipse(50, 216, 46, 4, 0, 0, Math.PI * 2); ctx.fill();
      const suit = ctx.createLinearGradient(15, 0, 85, 0); suit.addColorStop(0, '#27343b'); suit.addColorStop(.55, '#48585d'); suit.addColorStop(1, '#26353c');
      ctx.fillStyle = suit;
      roundRect(ctx, 28, 47, 44, 97, 17); ctx.fill();
      roundRect(ctx, 11, 54, 15, 82, 7); ctx.fill(); roundRect(ctx, 74, 54, 15, 82, 7); ctx.fill();
      roundRect(ctx, 29, 132, 19, 83, 7); ctx.fill(); roundRect(ctx, 53, 132, 19, 83, 7); ctx.fill();
      ctx.fillStyle = '#acaaa0'; ctx.beginPath(); ctx.ellipse(50, 24, 16, 21, 0, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = '#263137'; ctx.beginPath(); ctx.ellipse(50, 15, 16, 13, 0, Math.PI, Math.PI * 2); ctx.fill();
      ctx.restore();
    }
    const shade = ctx.createLinearGradient(0, 0, 0, ih); shade.addColorStop(0, 'rgba(5,10,12,.12)'); shade.addColorStop(.7, 'rgba(5,10,12,0)'); shade.addColorStop(1, 'rgba(5,10,12,.35)');
    ctx.fillStyle = shade; ctx.fillRect(0, 0, iw, ih); ctx.restore();
  }
  destroy() { clearTimeout(this.retryTimer); this.streamConnected = false; this.abort.abort(); this.overlay.destroy(); this.video.pause(); this.image.removeAttribute('src'); if (this.objectUrl) URL.revokeObjectURL(this.objectUrl); }
}
