/** Input produces normalized targets only. Network ownership stays in app.js. */
import { CONFIG } from './config.js';
export function joystickVector(dx, dy, radius, deadzone = CONFIG.JOYSTICK_DEADZONE) {
  const distance = Math.hypot(dx, dy);
  const magnitude = Math.min(1, distance / radius);
  if (!distance || magnitude <= deadzone) return { vx: 0, vy: 0, x: 0, y: 0 };
  const output = (magnitude - deadzone) / (1 - deadzone);
  return { vx: -dy / distance * output, vy: dx / distance * output,
    x: dx / distance * magnitude * radius, y: dy / distance * magnitude * radius };
}
export class MotionInput {
  constructor({ pad, knob, left, right, stop, enabled, change, emergency }) {
    Object.assign(this, { pad, knob, left, right, enabled, change, emergency });
    this.abort = new AbortController();
    this.pointer = null; this.rotationPointer = null; this.keys = new Set();
    this.vector = { vx: 0, vy: 0 }; this.wz = 0;
    const on = (el, event, fn) => el.addEventListener(event, fn, { signal: this.abort.signal });
    on(pad, 'pointerdown', e => {
      if (!enabled() || e.button !== 0 || this.pointer !== null) return;
      e.preventDefault(); this.keys.clear(); this.pointer = e.pointerId;
      pad.setPointerCapture(e.pointerId); pad.dataset.active = 'true'; this.move(e);
    });
    on(pad, 'pointermove', e => { if (e.pointerId === this.pointer) this.move(e); });
    for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) {
      on(pad, event, e => { if (e.pointerId === this.pointer) this.reset(true); });
    }
    for (const [button, value] of [[left, -1], [right, 1]]) {
      on(button, 'pointerdown', e => {
        if (!enabled() || e.button !== 0 || this.rotationPointer !== null) return;
        e.preventDefault(); this.keys.clear(); this.rotationPointer = e.pointerId;
        button.setPointerCapture(e.pointerId); button.dataset.held = 'true';
        this.wz = value; this.publish();
      });
      for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) {
        on(button, event, e => { if (e.pointerId === this.rotationPointer) this.reset(true); });
      }
    }
    on(stop, 'click', () => this.reset(true));
    on(window, 'blur', () => this.reset(true));
    on(document, 'visibilitychange', () => { if (document.hidden) this.reset(true); });
    on(window, 'keydown', e => {
      if (e.key === 'Escape') { e.preventDefault(); emergency(); return; }
      if (e.code === 'Space' && !this.editing(e.target)) { e.preventDefault(); this.reset(true); return; }
      if (this.editing(e.target) || e.metaKey || e.ctrlKey || e.altKey || !enabled()) return;
      const key = e.key.toLowerCase();
      if (!'wasdqe'.includes(key) || key.length !== 1) return;
      e.preventDefault(); if (e.repeat) return;
      this.keys.add(key); this.keyboard();
    });
    on(window, 'keyup', e => {
      if (this.keys.delete(e.key.toLowerCase())) { e.preventDefault(); this.keyboard(true); }
    });
  }
  editing(target) { return target.closest('input, select, textarea, dialog, [contenteditable="true"]'); }
  move(e) {
    if (!this.enabled()) { this.reset(true); return; }
    const box = this.pad.getBoundingClientRect();
    const v = joystickVector(e.clientX - box.left - box.width / 2,
      e.clientY - box.top - box.height / 2, box.width * .34);
    this.vector = { vx: v.vx, vy: v.vy };
    this.knob.style.transform = `translate(${v.x}px, ${v.y}px)`;
    this.publish();
  }
  keyboard(immediate = false) {
    const axis = (a, b) => Number(this.keys.has(a)) - Number(this.keys.has(b));
    const vx = axis('w', 's'), vy = axis('d', 'a');
    const norm = Math.max(1, Math.hypot(vx, vy));
    this.vector = { vx: vx / norm, vy: vy / norm }; this.wz = axis('e', 'q');
    const radius = this.pad.getBoundingClientRect().width * .34;
    this.knob.style.transform = `translate(${this.vector.vy * radius}px, ${-this.vector.vx * radius}px)`;
    this.pad.dataset.active = String(this.keys.size > 0); this.publish(immediate);
  }
  publish(immediate = false) { this.change({ ...this.vector, wz: this.wz }, immediate); }
  reset(immediate = false) {
    const p = this.pointer, r = this.rotationPointer;
    this.pointer = null; this.rotationPointer = null; this.keys.clear();
    for (const [el, id] of [[this.pad, p], [this.left, r], [this.right, r]]) {
      if (id !== null && el.hasPointerCapture(id)) el.releasePointerCapture(id);
      el.dataset.active = 'false'; el.dataset.held = 'false';
    }
    this.vector = { vx: 0, vy: 0 }; this.wz = 0;
    this.knob.style.transform = ''; this.publish(immediate);
  }
  destroy() { this.reset(true); this.abort.abort(); }
}
