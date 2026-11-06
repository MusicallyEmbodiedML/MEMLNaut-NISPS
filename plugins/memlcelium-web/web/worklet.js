// AudioWorklet processor: runs memlcelium.wasm on the audio thread. The page sends
// controls as messages; the processor sends back what the screens need, about 30
// times a second.

class MEMLCeliumProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.ready = false;
    this.queue = [];
    this.frames = 0;
    this.port.onmessage = (e) => (this.ready ? this.handle(e.data) : this.queue.push(e.data));
    this.start(options.processorOptions);
  }

  async start({ wasmBytes, seed }) {
    let memory = null;
    const imports = {
      wasi_snapshot_preview1: {
        fd_write: () => 0,
        // AudioWorkletGlobalScope has no crypto: Math.random is enough for weight init.
        random_get: (ptr, len) => {
          const b = new Uint8Array(memory.buffer, ptr, len);
          for (let i = 0; i < len; i++) b[i] = (Math.random() * 256) | 0;
          return 0;
        },
      },
    };
    const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
    this.w = instance.exports;
    memory = this.w.memory;
    this.w._initialize();
    this.w.celium_init(sampleRate, seed >>> 0);
    this.nParams = this.w.celium_num_params();
    this.ready = true;
    for (const m of this.queue) this.handle(m);
    this.queue = [];
    this.port.postMessage({ type: 'ready', nParams: this.nParams, sampleRate });
  }

  f32(ptr, n) { return new Float32Array(this.w.memory.buffer, ptr, n); }

  cstr(ptr) {
    const b = new Uint8Array(this.w.memory.buffer, ptr);
    let end = 0;
    while (b[end]) end++;
    let s = '';
    for (let i = 0; i < end; i++) s += String.fromCharCode(b[i]);
    return s;
  }

  handle(m) {
    const w = this.w;
    switch (m.type) {
      case 'input': w.celium_set_input(m.i, m.v); break;
      case 'gesture': w.celium_gesture(m.g); break;
      case 'drag': w.celium_set_drag(m.on ? 1 : 0); break;
      case 'knob': w.celium_set_knob(m.k, m.v); break;
      case 'playing': w.celium_set_playing(m.on ? 1 : 0); break;
      case 'focus': w.celium_toggle_focus(m.group); break;
      case 'enable': w.celium_set_voice_enable(m.mask); break;
      case 'dislikeMode': w.celium_set_dislike_mode(m.mode); break;
      case 'save': {
        const n = w.celium_state_save();
        const data = this.f32(w.celium_state_data(), n).slice();
        this.port.postMessage({ type: 'saved', slot: m.slot, data }, [data.buffer]);
        break;
      }
      case 'load': {
        const data = m.data;
        this.f32(w.celium_state_alloc(data.length), data.length).set(data);
        this.port.postMessage({ type: 'loaded', slot: m.slot, ok: !!w.celium_state_load() });
        break;
      }
    }
  }

  report() {
    const w = this.w;
    const action = this.f32(w.celium_action(), this.nParams).slice();
    const nMem = w.celium_memory_snapshot();
    const mem = this.f32(w.celium_memory_data(), nMem * 5).slice();
    const serial = w.celium_event_serial();
    const msg = {
      type: 'state', action, mem,
      likes: w.celium_likes(), dislikes: w.celium_dislikes(),
      loss: w.celium_loss(), exploring: !!w.celium_noise_active(),
      serial,
    };
    if (serial !== this.lastSerial) {
      this.lastSerial = serial;
      const [act, text] = this.cstr(w.celium_event_text()).split('\n');
      msg.event = { action: act, message: text };
    }
    this.port.postMessage(msg, [action.buffer, mem.buffer]);
  }

  process(_inputs, outputs) {
    const out = outputs[0];
    if (!this.ready || !out || out.length === 0) return true;
    const n = out[0].length;
    this.w.celium_render(n);
    out[0].set(this.f32(this.w.celium_out_l(), n));
    if (out[1]) out[1].set(this.f32(this.w.celium_out_r(), n));
    this.frames += n;
    if (this.frames >= sampleRate / 30) {
      this.frames = 0;
      this.report();
    }
    return true;
  }
}

registerProcessor('memlcelium', MEMLCeliumProcessor);
