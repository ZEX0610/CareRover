/** In-browser simulator; the same JSON boundary as the hardware transport. */
import { CONFIG, LINK } from './config.js';
import { Transport } from './transport.js';
import { RobotSim } from './sim.js';
export class MockTransport extends Transport {
  constructor(opts = {}) {
    super();
    this.sim = new RobotSim();
    this.connectDelayMs = opts.connectDelayMs ?? 260;
    this.fakeLatencyMs = opts.fakeLatencyMs ?? 8;
    this.pending = new Set();
    this.intervals = [];
  }
  get name() { return 'mock'; }
  later(fn, ms) {
    const id = setTimeout(() => { this.pending.delete(id); fn(); }, ms);
    this.pending.add(id);
  }
  connect() {
    if (this.isOpen || this.state === LINK.CONNECTING) return;
    this._setState(LINK.CONNECTING);
    this.later(() => { this._setState(LINK.CONNECTED); this.start(); }, this.connectDelayMs);
  }
  disconnect() { this.stop(); this._setState(LINK.DISCONNECTED); }
  send(message) {
    if (!this.isOpen) return false;
    const msg = JSON.parse(JSON.stringify(message));
    this.later(() => {
      if (!this.isOpen) return;
      this.sim.handleCommand(msg).forEach(r => this.deliver(r));
      if (msg.type !== 'ping' && msg.type !== 'cmd_vel') this.deliver(this.sim.telemetry());
    }, this.fakeLatencyMs);
    return true;
  }
  simulateDrop(ms = 2600) {
    if (!this.isOpen) return;
    this.stop();
    this._setState(LINK.RECONNECTING);
    this.later(() => { this._setState(LINK.CONNECTED); this.start(); }, ms);
  }
  start() {
    let previous = performance.now();
    this.intervals.push(setInterval(() => {
      const current = performance.now();
      this.sim.step(Math.max(0, (current - previous) / 1000));
      previous = current;
    }, 20));
    this.intervals.push(setInterval(() => {
      this.deliver(this.sim.telemetry());
      this.deliver({ type: 'ppg_batch', ts: Date.now(), sample_rate_hz: CONFIG.PPG_EXPECTED_RATE_HZ,
        samples: this.sim.ppgSamples(5, 50).map(Math.round) });
    }, 100));
    this.deliver(this.sim.telemetry());
  }
  stop() {
    this.pending.forEach(clearTimeout); this.pending.clear();
    this.intervals.forEach(clearInterval); this.intervals = [];
    this.sim.cmd = { vx: 0, vy: 0, wz: 0 };
    this.sim.vel = { vx: 0, vy: 0, wz: 0 };
    this.sim.mode = 'IDLE';
    this.sim.front.cancel('network_down');
  }
  deliver(msg) { if (this.isOpen) this._emitMessage(JSON.stringify(msg)); }
}
