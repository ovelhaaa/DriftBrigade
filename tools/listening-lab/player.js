"use strict";

class ComparisonPlayer {
  duration = 10;
  offset = 0;
  active = "level_matched/X";
  volume = 0.75;
  loop = false;
  run = null;
  buffers = new Map();
  revision = 0;
  playRequest = 0;
  ramp = 0.008;

  init() {
    if (!this.context) {
      this.context = new AudioContext({ sampleRate: 48000 });
      this.monitor = this.context.createGain();
      this.monitor.gain.value = this.volume;
      this.monitor.connect(this.context.destination);
    }
    return this.context;
  }

  async load(entries, get) {
    this.clear();
    const revision = this.revision;
    const context = this.init();
    const buffers = new Map();
    for (const [slot, path] of entries) {
      try {
        const bytes = await get(path).arrayBuffer();
        if (revision !== this.revision) return false;
        inspectWav(bytes.slice(0, 44), bytes.byteLength);
        const buffer = await context.decodeAudioData(bytes);
        if (revision !== this.revision) return false;
        if (buffer.numberOfChannels !== 2 || buffer.length !== 480000) throw new Error();
        buffers.set(slot, buffer);
      } catch {
        if (revision !== this.revision) return false;
        throw new Error(`Não foi possível decodificar ${path}. Selecione a pasta novamente.`);
      }
    }
    this.buffers = buffers;
    return true;
  }

  clear() {
    this.revision++;
    this.stop();
    this.offset = 0;
    this.buffers.clear();
  }

  get position() {
    if (!this.run) return this.offset;
    const elapsed = Math.max(0, this.context.currentTime - this.run.start);
    const position = this.offset + elapsed;
    return this.loop ? position % this.duration : Math.min(position, this.duration);
  }

  async play() {
    if (this.run || !this.buffers.size) return;
    const request = ++this.playRequest;
    await this.context.resume();
    if (request !== this.playRequest || this.run) return;
    if (this.offset >= this.duration) this.offset = 0;
    this.start();
  }

  start() {
    const start = this.context.currentTime + 0.015;
    const run = { start, nodes: new Map() };
    this.run = run;
    for (const [slot, buffer] of this.buffers) {
      const source = this.context.createBufferSource();
      const gain = this.context.createGain();
      source.buffer = buffer;
      source.loop = this.loop;
      source.connect(gain).connect(this.monitor);
      gain.gain.value = 0;
      gain.gain.setValueAtTime(0, start);
      gain.gain.linearRampToValueAtTime(Number(slot === this.active), start + this.ramp);
      run.nodes.set(slot, { source, gain });
      source.onended = () => {
        source.disconnect();
        gain.disconnect();
        if (this.run === run) {
          this.offset = this.duration;
          this.run = null;
        }
      };
      source.start(start, this.offset);
    }
  }

  fade(param, target, now) {
    param.cancelAndHoldAtTime(now);
    param.linearRampToValueAtTime(target, now + this.ramp);
  }

  stop() {
    this.playRequest++;
    if (!this.run) return;
    this.offset = this.position;
    const now = this.context.currentTime;
    for (const { source, gain } of this.run.nodes.values()) {
      this.fade(gain.gain, 0, now);
      source.stop(now + this.ramp);
    }
    this.run = null;
  }

  select(slot) {
    if (slot === this.active || !this.buffers.has(slot)) return;
    this.active = slot;
    if (!this.run) return;
    const now = this.context.currentTime;
    for (const [name, { gain }] of this.run.nodes) this.fade(gain.gain, Number(name === slot), now);
  }

  seek(seconds) {
    const playing = Boolean(this.run);
    this.stop();
    this.offset = Math.max(0, Math.min(seconds, this.duration));
    if (playing && this.offset < this.duration) this.start();
  }

  setLoop(loop) {
    const playing = Boolean(this.run);
    this.stop();
    this.loop = loop;
    if (playing && this.offset < this.duration) this.start();
  }

  setVolume(volume) {
    this.volume = volume;
    if (this.monitor) this.fade(this.monitor.gain, volume, this.context.currentTime);
  }
}

function drawReference(canvas, buffer) {
  const width = Math.round(canvas.clientWidth * devicePixelRatio);
  const height = Math.round(canvas.clientHeight * devicePixelRatio);
  if (!width || !height) return;
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
  const ctx = canvas.getContext("2d");
  const padding = 26 * devicePixelRatio;
  const span = width - padding * 2;
  ctx.clearRect(0, 0, width, height);
  ctx.strokeStyle = "#263540";
  ctx.beginPath();
  for (let i = 0; i <= 5; i++) {
    const x = padding + span * i / 5;
    ctx.moveTo(x, 0);
    ctx.lineTo(x, height);
  }
  for (const y of [height / 4, 3 * height / 4]) {
    ctx.moveTo(padding, y);
    ctx.lineTo(width - padding, y);
  }
  ctx.stroke();
  ctx.fillStyle = "#a6b7c4";
  ctx.font = `${12 * devicePixelRatio}px monospace`;
  ctx.fillText("L", 8 * devicePixelRatio, height / 4 + 4 * devicePixelRatio);
  ctx.fillText("R", 8 * devicePixelRatio, 3 * height / 4 + 4 * devicePixelRatio);
  if (buffer) {
    ctx.strokeStyle = "#83aba7";
    for (const [channel, y] of [[0, height / 4], [1, 3 * height / 4]]) {
      const data = buffer.getChannelData(channel);
      ctx.beginPath();
      for (let i = 0; i < span; i += 2 * devicePixelRatio) {
        const first = Math.floor(i * data.length / span);
        const last = Math.min(data.length, Math.ceil((i + 2 * devicePixelRatio) * data.length / span));
        let low = 0, high = 0;
        for (let j = first; j < last; j++) {
          low = Math.min(low, data[j]);
          high = Math.max(high, data[j]);
        }
        ctx.moveTo(padding + i, y - high * height * 1.2);
        ctx.lineTo(padding + i, y - low * height * 1.2);
      }
      ctx.stroke();
    }
  }
}
