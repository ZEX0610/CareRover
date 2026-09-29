const labels = {
  seat_left: ['离座提醒', '已连续 30 秒未识别到人脸，请查看画面确认。', 'Seat absence reminder', 'No face detected for 30 seconds; check the live video.'],
  seat_returned: ['入座提醒', '重新识别到人脸，请查看画面确认。', 'Face detected again', 'A face is visible again; check the live video.'],
  call_invite: ['孩子请求通话', '请确认是否接听。', 'Incoming call from the child', 'Confirm to answer.'],
  call_cancelled: ['孩子已取消通话', '来电请求已结束。', 'Call request cancelled', 'The incoming call ended.'],
  call_connected: ['通话已接通', '双向音频连接已建立。', 'Call connected', 'Two-way audio is connected.'],
  call_ended: ['通话已结束', '双向音频连接已断开。', 'Call ended', 'Two-way audio disconnected.']
};

export class CareBanners {
  constructor({ language = () => 'zh', answer = () => {} } = {}) {
    this.language = language;
    this.answer = answer;
    this.root = document.createElement('section');
    this.root.className = 'care-banners';
    this.root.setAttribute('aria-label', 'CareRover 消息');
    this.root.setAttribute('aria-live', 'polite');
    document.body.append(this.root);
    this.active = new Map();
    this.timers = new Map();
    this.ringTimer = null;
    this.audio = null;
    this.unlock = () => { if (this.ringTimer) this.audio?.resume(); };
    document.addEventListener('pointerdown', this.unlock);
  }

  message(kind, active) {
    if (kind === 'seat_returned') this.clear('seat_left');
    if (kind === 'call_cancelled' || kind === 'call_connected' || kind === 'call_ended') {
      this.clear('call_invite'); this.stopRing();
    }
    if (!labels[kind]) return;
    if (this.active.has(kind)) return;
    const [zhTitle, zhBody, enTitle, enBody] = labels[kind];
    const english = this.language() === 'en';
    const card = document.createElement('article');
    card.className = `care-banner care-banner--${kind.startsWith('call') ? 'call' : 'seat'}`;
    const words = document.createElement('div'); words.className = 'care-banner__words';
    const title = document.createElement('strong'); title.textContent = english ? enTitle : zhTitle;
    const body = document.createElement('span'); body.textContent = english ? enBody : zhBody;
    words.append(title, body); card.append(words);
    if (kind === 'call_invite') {
      const button = document.createElement('button'); button.type = 'button';
      button.textContent = english ? 'Answer' : '接听';
      button.addEventListener('click', () => { this.stopRing(); this.answer(); });
      card.append(button); this.startRing();
    }
    this.root.prepend(card);
    this.active.set(kind, card);
    if (kind !== 'call_invite') this.timers.set(kind, setTimeout(() => this.clear(kind), 6500));
  }

  snapshot(state) {
    // Snapshots restore only actionable calls, never replay an expired seat toast.
    if (state?.call_requested && !state.audio_paired) this.message('call_invite', true);
    else { this.clear('call_invite'); this.stopRing(); }
  }

  clear(kind) {
    if (this.timers.has(kind)) clearTimeout(this.timers.get(kind));
    this.timers.delete(kind);
    this.active.get(kind)?.remove(); this.active.delete(kind);
  }

  startRing() {
    if (this.ringTimer) return;
    const Audio = window.AudioContext || window.webkitAudioContext;
    if (!Audio) return;
    try { this.audio = new Audio(); } catch { return; }
    const pulse = () => {
      if (this.audio?.state !== 'running') return; // Browsers may require a user gesture.
      const at = this.audio.currentTime;
      for (const [offset, pitch] of [[0, 660], [0.25, 880]]) {
        const osc = this.audio.createOscillator(); const gain = this.audio.createGain();
        osc.type = 'sine'; osc.frequency.value = pitch;
        gain.gain.setValueAtTime(0.0001, at + offset);
        gain.gain.exponentialRampToValueAtTime(0.035, at + offset + 0.025);
        gain.gain.exponentialRampToValueAtTime(0.0001, at + offset + 0.22);
        osc.connect(gain).connect(this.audio.destination);
        osc.start(at + offset); osc.stop(at + offset + 0.23);
      }
    };
    pulse(); this.ringTimer = setInterval(pulse, 1800);
  }

  stopRing() {
    if (this.ringTimer) clearInterval(this.ringTimer);
    this.ringTimer = null;
    this.audio?.close(); this.audio = null;
  }

  destroy() { this.stopRing(); for (const kind of this.active.keys()) this.clear(kind); document.removeEventListener('pointerdown', this.unlock); this.root.remove(); }
}
