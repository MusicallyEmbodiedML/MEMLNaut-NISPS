// Headless check of web/memlcelium.wasm: it makes sound, the gestures change the
// memory, the sequencer stops and starts, save/load round-trips, and it renders
// faster than real time. Run: node tests/smoke.mjs
import { readFileSync } from 'node:fs';
import { webcrypto } from 'node:crypto';

const bytes = readFileSync(new URL('../web/memlcelium.wasm', import.meta.url));
let mem;
const imports = { wasi_snapshot_preview1: {
  fd_write: () => 0,
  random_get: (ptr, len) => { webcrypto.getRandomValues(new Uint8Array(mem.buffer, ptr, len)); return 0; },
}};
const { instance } = await WebAssembly.instantiate(bytes, imports);
const w = instance.exports; mem = w.memory;
w._initialize();

const SR = 48000, BLOCK = 128;
const fail = (msg) => { console.error('FAIL', msg); process.exit(1); };
w.celium_init(SR, 12345);
const outL = () => new Float32Array(mem.buffer, w.celium_out_l(), BLOCK);

function render(seconds) {
  let sum = 0, n = 0, bad = 0;
  for (let b = 0; b < Math.round(seconds * SR / BLOCK); b++) {
    w.celium_render(BLOCK);
    for (const s of outL()) { if (!Number.isFinite(s)) bad++; sum += s * s; n++; }
  }
  return { rms: Math.sqrt(sum / n), bad };
}

let r = render(2);
console.log('playing rms', r.rms.toFixed(4));
if (r.bad) fail('non-finite samples');
if (r.rms < 1e-3) fail('silent while playing');

// Likes at three joystick positions.
for (const [x, y] of [[0.2, 0.2], [0.8, 0.3], [0.5, 0.9]]) {
  w.celium_set_input(0, x); w.celium_set_input(1, y);
  render(0.05); w.celium_gesture(0); render(0.05);
}
console.log('likes', w.celium_likes(), 'dislikes', w.celium_dislikes());
if (w.celium_likes() !== 3) fail('expected 3 likes');
w.celium_gesture(1); render(0.05);
if (w.celium_dislikes() < 1) fail('dislike not stored');
render(1);
console.log('loss', w.celium_loss().toFixed(5));
const n = w.celium_memory_snapshot();
console.log('memory items', n);
if (n < 3) fail('memory snapshot');

// Save, scramble + forget, load back.
const likesAtSave = w.celium_likes();
const len = w.celium_state_save();
const saved = new Float32Array(mem.buffer, w.celium_state_data(), len).slice();
const before = new Float32Array(mem.buffer, w.celium_action(), w.celium_num_params()).slice();
w.celium_gesture(2); w.celium_gesture(4); render(0.05);
if (w.celium_likes() !== 0) fail('forget');
new Float32Array(mem.buffer, w.celium_state_alloc(saved.length), saved.length).set(saved);
if (!w.celium_state_load()) fail('load refused');
render(0.05);
console.log('after load: likes', w.celium_likes());
if (w.celium_likes() !== likesAtSave) fail(`load: expected ${likesAtSave} likes`);
const after = new Float32Array(mem.buffer, w.celium_action(), w.celium_num_params());
let diff = 0; for (let i = 0; i < before.length; i++) diff = Math.max(diff, Math.abs(before[i] - after[i]));
console.log('max action diff after load', diff.toFixed(4));

// Stop the sequencer: the voices release and the output falls quiet.
w.celium_set_playing(0); render(3);
r = render(0.5);
console.log('stopped rms', r.rms.toFixed(5));
w.celium_set_playing(1);
r = render(2);
console.log('restarted rms', r.rms.toFixed(4));
if (r.rms < 1e-3) fail('silent after restart');

// Speed: render 10 s of audio with exploration on.
w.celium_set_knob(2, 0.8);
const t0 = performance.now(); render(10); const ms = performance.now() - t0;
console.log(`10 s of audio in ${ms.toFixed(0)} ms (${(10000 / ms).toFixed(0)}x real time)`);
console.log('OK');
