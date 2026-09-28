const labels = {
  face_absent: ['30 秒未识别到人脸', '请查看实时画面确认孩子情况。', 'No face for 30 seconds', 'Check the live video.'],
  face_restored: ['重新识别到人脸', '无人脸提醒已解除。', 'Face detected again', 'No-face alert cleared.'],
  seat_left: ['离座提醒', '前方距离变化提示可能离座，请查看画面确认。', 'Possible seat departure', 'The distance changed; check the live video.'],
  seat_returned: ['入座提醒', '前方距离恢复，提示可能已经入座。', 'Possible seat return', 'The distance returned to its seated range.'],
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
    this.ringTimer = null;
    this.audio = null;
    this.unlock = () => { if (this.ringTimer) this.audio?.resume(); };
    document.addEventListener('pointerdown', this.unlock);
  }

  message(kind, active) {
    if (kind === 'face_restored') this.clear('face_absent');
    if (kind === 'seat_returned') this.clear('seat_left');
    if (kind === 'call_cancelled' || kind === 'call_connected' || kind === 'call_ended') {
      this.clear('call_invite'); this.stopRing();
    }
    if (kind === 'face_absent' && active === false) { this.clear(kind); return; }
    if (!labels[kind]) return;
    const sticky = kind === 'face_absent' || kind === 'seat_left' || kind === 'call_invite';
    if (sticky && this.active.has(kind)) return;
    const [zhTitle, zhBody, enTitle, enBody] = labels[kind];
    const english = this.language() === 'en';
    const card = document.createElement('article');
    card.className = `care-banner care-banner--${kind.startsWith('call') ? 'call' : kind.startsWith('face') ? 'face' : 'seat'}`;
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
    if (sticky) this.active.set(kind, card);
    else setTimeout(() => card.remove(), 6500);
  }

  snapshot(state) {
    if (state?.face_alert_active) this.message('face_absent', true);
    else this.clear('face_absent');
    if (state?.seat === 'vacant') this.message('seat_left', true);
    else this.clear('seat_left');
    if (state?.call_requested && !state.audio_paired) this.message('call_invite', true);
    else { this.clear('call_invite'); this.stopRing(); }
  }

  clear(kind) { this.active.get(kind)?.remove(); this.active.delete(kind); }

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

  destroy() { this.stopRing(); document.removeEventListener('pointerdown', this.unlock); this.root.remove(); }
}
