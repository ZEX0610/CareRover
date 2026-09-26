import { CONFIG } from './config.js';
import { t } from './i18n.js';
export class DebugPanel {
  constructor({ replay, resume }) {
    this.rows = []; this.record = null; this.saved = null; this.dirty = false;
    this.abort = new AbortController(); const signal = this.abort.signal;
    const on = (id, fn) => document.getElementById(id).addEventListener('click', fn, { signal });
    on('dbgClear', () => { this.rows = []; this.dirty = true; });
    on('dbgRec', () => {
      if (this.record) this.finishRecording();
      else { this.record = []; document.getElementById('dbgRec').textContent = t('dbg.recstop'); }
    });
    on('dbgSave', () => {
      if (!this.saved) return;
      const url = URL.createObjectURL(new Blob([JSON.stringify({ version: 1, messages: this.saved })], { type: 'application/json' }));
      const a = document.createElement('a'); a.href = url; a.download = 'carerover-session.json'; a.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    });
    on('dbgReplay', () => document.getElementById('dbgReplayInput').click());
    on('dbgResume', resume);
    document.getElementById('dbgReplayInput').addEventListener('change', async e => {
      const file = e.target.files[0]; if (!file) return;
      try {
        if (file.size > 32 * 1024 * 1024) throw new Error('Replay is larger than 32 MB');
        const data = JSON.parse(await file.text());
        if (data.version !== 1 || !Array.isArray(data.messages) || !data.messages.length || data.messages.length > CONFIG.DEBUG_RECORD_MAX) throw new Error('Invalid recording');
        const start = data.messages[0].at; let prev = start;
        for (const row of data.messages) {
          if (!Number.isFinite(row.at) || row.at < prev || row.at - start > 3600000 || typeof row.raw !== 'string' || row.raw.length > 65536) throw new Error('Invalid recording event');
          prev = row.at;
        }
        replay(data.messages);
      } catch { this.log('err', t('t.replay.bad')); }
      e.target.value = '';
    }, { signal });
  }
  finishRecording() {
    this.saved = this.record; this.record = null;
    document.getElementById('dbgRec').textContent = t('dbg.rec');
    document.getElementById('dbgSave').disabled = !this.saved?.length;
  }
  message(raw) {
    if (this.record) {
      this.record.push({ at: Date.now(), raw });
      if (this.record.length >= CONFIG.DEBUG_RECORD_MAX) this.finishRecording();
    }
  }
  log(kind, detail) {
    this.rows.push({ time: new Date().toLocaleTimeString('en-GB', { hour12: false }), kind, detail: String(detail).slice(0, 350) });
    if (this.rows.length > CONFIG.DEBUG_LOG_LINES) this.rows.shift(); this.dirty = true;
  }
  render(state, fps) {
    const $ = id => document.getElementById(id);
    $('dbgRec').textContent = t(this.record ? 'dbg.recstop' : 'dbg.rec');
    $('dbgBadge').textContent = `tx ${state.ui.txCount} · rx ${state.ui.rxCount}`;
    $('dbgResume').hidden = !state.ui.replaying;
    if (!$('debugPanel').open) return;
    const heap = performance.memory?.usedJSHeapSize;
    const memory = heap ? `  ·  ${(heap / 1048576).toFixed(1)} MiB JS` : '';
    $('dbgStats').textContent = `${state.ui.transportName.toUpperCase()}  ·  ${fps} FPS  ·  ${state.ppg.ring.size}/${CONFIG.PPG_RING_CAPACITY} PPG  ·  ${state.connection.lastTelemetryTs ? Date.now() - state.connection.lastTelemetryTs : '—'} ms${memory}`;
    if (!this.dirty) return; this.dirty = false;
    const fragment = document.createDocumentFragment();
    for (const row of this.rows) {
      const el = document.createElement('div'); el.className = `logline logline--${row.kind}`;
      el.textContent = `${row.time}  ${row.kind.toUpperCase()}  ${row.detail}`; fragment.append(el);
    }
    $('dbgLog').replaceChildren(fragment); $('dbgLog').scrollTop = $('dbgLog').scrollHeight;
  }
  destroy() { this.abort.abort(); }
}
