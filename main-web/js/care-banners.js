// Same top-edge visual language as the private HTTPS parent console.
const messages = {
  seat_left: ['离座提醒', '已连续 30 秒未识别到人脸，请查看画面确认。'],
  seat_returned: ['入座提醒', '重新识别到人脸，请查看画面确认。'],
  call_invite: ['孩子请求通话', '请在私有 HTTPS 家长端确认接听。'],
  call_cancelled: ['孩子已取消通话', '来电请求已结束。']
};

export class CareBanners {
  constructor() {
    this.root = document.createElement('section');
    this.root.className = 'care-banners';
    this.root.setAttribute('aria-label', 'CareRover 消息');
    this.root.setAttribute('aria-live', 'polite');
    document.body.append(this.root);
    this.active = new Map();
    this.timers = new Map();
  }
  message(kind) {
    if (kind === 'seat_returned') this.clear('seat_left');
    if (kind === 'call_cancelled') this.clear('call_invite');
    if (!messages[kind] || this.active.has(kind)) return;
    const card = document.createElement('article');
    card.className = `care-banner care-banner--${kind.startsWith('call') ? 'call' : 'seat'}`;
    const words = document.createElement('div'); words.className = 'care-banner__words';
    const title = document.createElement('strong'); title.textContent = messages[kind][0];
    const body = document.createElement('span'); body.textContent = messages[kind][1];
    words.append(title, body); card.append(words); this.root.prepend(card);
    this.active.set(kind, card);
    if (kind !== 'call_invite') this.timers.set(kind, setTimeout(() => this.clear(kind), 6500));
  }
  clear(kind) {
    if (this.timers.has(kind)) clearTimeout(this.timers.get(kind));
    this.timers.delete(kind);
    this.active.get(kind)?.remove(); this.active.delete(kind);
  }
  destroy() { for (const kind of this.active.keys()) this.clear(kind); this.root.remove(); }
}
