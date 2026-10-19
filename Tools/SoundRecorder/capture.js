class TallowCapture extends AudioWorkletProcessor {
  constructor() {
    super(); this.buffer = new Float32Array(2048); this.index = 0; this.frames = 0; this.recording = true;
    this.port.onmessage = event => { if (event.data === 'stop') this.finish(); };
  }
  flush() {
    if (!this.index) return;
    const chunk = this.buffer.slice(0, this.index);
    this.port.postMessage({type: 'chunk', samples: chunk}, [chunk.buffer]); this.index = 0;
  }
  finish(reason = 'stop') {
    if (!this.recording) return;
    this.recording = false; this.flush(); this.port.postMessage({type: 'done', reason});
  }
  process(inputs) {
    const channels = inputs[0];
    if (this.recording && channels?.length) {
      for (let i = 0; i < channels[0].length; i++) {
        let value = 0; for (const channel of channels) value += channel[i] / channels.length;
        this.buffer[this.index++] = value; this.frames++;
        if (this.index === this.buffer.length) this.flush();
        if (this.frames >= sampleRate * 60) { this.finish('limit'); break; }
      }
    }
    return true;
  }
}
registerProcessor('tallow-capture', TallowCapture);
