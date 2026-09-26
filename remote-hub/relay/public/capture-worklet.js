class CapturePcm16 extends AudioWorkletProcessor {
  constructor() {
    super(); this.phase = 0; this.buffer = new Int16Array(320); this.used = 0;
  }
  process(inputs) {
    const channel = inputs[0]?.[0];
    if (!channel) return true;
    for (const x of channel) {
      this.phase += 16000 / sampleRate;
      if (this.phase < 1) continue;
      this.phase -= 1;
      this.buffer[this.used++] = Math.max(-32768, Math.min(32767, Math.round(x * 32767)));
      if (this.used === 320) {
        this.port.postMessage(this.buffer.buffer, [this.buffer.buffer]);
        this.buffer = new Int16Array(320); this.used = 0;
      }
    }
    return true;
  }
}
registerProcessor('capture-pcm16', CapturePcm16);
