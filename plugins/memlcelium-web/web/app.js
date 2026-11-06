// MEMLCelium in the browser: page side. Starts the AudioWorklet, sends it the panel's
// controls, and draws the screens from what it reports back.
'use strict';

const GESTURE = { LIKE: 0, DISLIKE: 1, SCRAMBLE: 2, JOLT: 3, FORGET: 4 };
// Output sections on the RL screen (MEMLNautModeMEMLCelium::setupInterface).
const SECTIONS = [[0, 'Seq'], [21, 'V1'], [44, 'V2'], [63, 'V3']];
const FOCUS_GROUPS = ['Seq', 'Synth', 'Env', 'Voice 1', 'Voice 2', 'Voice 3'];
// Group membership per output, as in MEMLCeliumAudioApp::kParamGroupMask
// (bits: 0 Seq, 1 Synth, 2 Env, 3-5 Voice 1-3).
const GROUP_MASK = (() => {
  const m = [], add = (n, bits) => { for (let i = 0; i < n; i++) m.push(bits); };
  const S = 1, Y = 2, E = 4, V0 = 8, V1 = 16, V2 = 32;
  add(7, S | V0); add(7, S | V1); add(7, S | V2);
  add(12, Y | V0); add(7, Y | E | V0); add(4, Y | V0);
  add(12, Y | V1); add(7, Y | E | V1);
  add(12, Y | V2); add(7, Y | E | V2); add(1, Y | V2);
  return m;
})();
const DISLIKE_MODES = [
  ['Push away', 'Push the sound away from your likes'],
  ['Nudge + blame', 'Step away, mostly undoing what differs from the nearest like'],
  ['Re-roll', 'Jump to a fresh sound here'],
  ['Nudge > back > re-roll', 'Repeated no: nudge, then back to a like, then re-roll'],
  ['Borrow from likes', 'Rebuild from qualities of nearby likes'],
];
const NUM_SLOTS = 12;
const SLOT_KEY = (i) => `memlcelium.slot.${i}`;

const $ = (s) => document.querySelector(s);
const $$ = (s) => [...document.querySelectorAll(s)];

let node = null;
let state = null;            // last report from the worklet
const lossTrace = [];
let page = 'rl';
let focusMask = 0;
let voiceMask = 0b111;
let dislikeMode = 0;
let slotMode = 'save';
let currentSlot = -1;
let lastAction = '';
let lastActionAt = 0;
const input = [0.5, 0.5, 0.5, 0.5];

function send(msg) { if (node) node.port.postMessage(msg); }

// ---------------------------------------------------------------- audio
async function start() {
  const btn = $('#start-btn');
  btn.disabled = true;
  btn.textContent = 'Loading…';
  try {
    const ctx = new AudioContext({ latencyHint: 'interactive' });
    const [wasmBytes] = await Promise.all([
      fetch('memlcelium.wasm').then((r) => { if (!r.ok) throw new Error(`memlcelium.wasm: ${r.status}`); return r.arrayBuffer(); }),
      ctx.audioWorklet.addModule('worklet.js'),
    ]);
    node = new AudioWorkletNode(ctx, 'memlcelium', {
      numberOfInputs: 0, outputChannelCount: [2],
      processorOptions: { wasmBytes, seed: (Math.random() * 2 ** 32) >>> 0 },
    });
    node.connect(ctx.destination);
    node.port.onmessage = (e) => onWorklet(e.data);
    await ctx.resume();
    // Bring the engine to where the panel is.
    $$('.knob').forEach((k) => send({ type: 'knob', k: +k.dataset.knob, v: +k.dataset.value }));
    input.forEach((v, i) => send({ type: 'input', i, v }));
    send({ type: 'enable', mask: voiceMask });
    send({ type: 'dislikeMode', mode: dislikeMode });
    $('#start').hidden = true;
    say('Move the joystick.');
  } catch (err) {
    btn.disabled = false;
    btn.textContent = 'Start sound';
    say(`Could not start audio: ${err.message}`);
  }
}

function onWorklet(m) {
  if (m.type === 'state') {
    state = m;
    if (m.loss > 0 || lossTrace.length) { lossTrace.push(m.loss); if (lossTrace.length > 240) lossTrace.shift(); }
    if (m.event) {
      lastAction = m.event.action;
      lastActionAt = performance.now();
      if (m.event.message) say(m.event.message);
    }
  } else if (m.type === 'saved') {
    try {
      const bytes = new Uint8Array(m.data.buffer);
      let bin = '';
      for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
      localStorage.setItem(SLOT_KEY(m.slot), JSON.stringify({ when: Date.now(), data: btoa(bin) }));
      currentSlot = m.slot;
      say(`Saved to slot ${m.slot + 1}.`);
    } catch {
      say('Could not save: this browser is not keeping site data.');
    }
    renderSlots();
  } else if (m.type === 'loaded') {
    if (m.ok) { currentSlot = m.slot; say(`Loaded slot ${m.slot + 1}.`); } else say(`Slot ${m.slot + 1} could not be loaded.`);
    renderSlots();
  }
}

function say(text) { $('#message').textContent = text; }

// ---------------------------------------------------------------- switches
function gesture(g, el) {
  if (!node) return;
  send({ type: 'gesture', g });
  if (el) { el.classList.add('flash'); setTimeout(() => el.classList.remove('flash'), 120); }
}
$$('[data-gesture]').forEach((b) => b.addEventListener('click', () => gesture(+b.dataset.gesture, b)));

let dragging = false;
function setDrag(on) {
  if (on === dragging || !node) return;
  dragging = on;
  $('#drag').classList.toggle('down', on);
  send({ type: 'drag', on });
}
const dragBtn = $('#drag');
dragBtn.addEventListener('pointerdown', (e) => { dragBtn.setPointerCapture(e.pointerId); setDrag(true); });
dragBtn.addEventListener('pointerup', () => setDrag(false));
dragBtn.addEventListener('pointercancel', () => setDrag(false));
dragBtn.addEventListener('keydown', (e) => { if (e.key === 'Enter') setDrag(true); });
dragBtn.addEventListener('keyup', (e) => { if (e.key === 'Enter') setDrag(false); });

let playing = true;
function togglePlay() {
  if (!node) return;
  playing = !playing;
  const b = $('#play');
  b.classList.toggle('on', playing);
  b.setAttribute('aria-pressed', String(playing));
  send({ type: 'playing', on: playing });
  say(playing ? 'Sequencer running.' : 'Sequencer stopped.');
}
$('#play').addEventListener('click', togglePlay);

// ---------------------------------------------------------------- knobs
function knobText(k, v) {
  switch (k) {
    case 0: return `${Math.round(30 + v * 170)} BPM`;
    case 1: return `learn ${v.toFixed(2)}`;
    case 2: return v < 0.005 ? 'explore off' : `explore ${v.toFixed(2)}`;
    default: return `vol ${v.toFixed(2)}`;
  }
}
function setKnob(el, v) {
  v = Math.min(1, Math.max(0, v));
  el.dataset.value = v;
  const k = +el.dataset.knob;
  const dial = el.querySelector('.dial');
  dial.style.setProperty('--a', `${-135 + v * 270}deg`);
  dial.setAttribute('aria-valuenow', v.toFixed(2));
  dial.setAttribute('aria-valuetext', knobText(k, v));
  el.querySelector('.kv').textContent = knobText(k, v);
  send({ type: 'knob', k, v });
}
$$('.knob').forEach((el) => {
  const dial = el.querySelector('.dial');
  dial.setAttribute('aria-valuemin', '0');
  dial.setAttribute('aria-valuemax', '1');
  setKnob(el, +el.dataset.value);
  let startY = 0, startV = 0;
  dial.addEventListener('pointerdown', (e) => {
    dial.setPointerCapture(e.pointerId);
    startY = e.clientY; startV = +el.dataset.value;
  });
  dial.addEventListener('pointermove', (e) => {
    if (!dial.hasPointerCapture(e.pointerId)) return;
    setKnob(el, startV + (startY - e.clientY) / 180);
  });
  dial.addEventListener('wheel', (e) => { e.preventDefault(); setKnob(el, +el.dataset.value - Math.sign(e.deltaY) * 0.02); }, { passive: false });
  dial.addEventListener('keydown', (e) => {
    const step = e.shiftKey ? 0.1 : 0.02;
    if (e.key === 'ArrowUp' || e.key === 'ArrowRight') { setKnob(el, +el.dataset.value + step); e.preventDefault(); }
    if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') { setKnob(el, +el.dataset.value - step); e.preventDefault(); }
  });
});

// ---------------------------------------------------------------- joystick
const stick = $('#stick'), puck = $('#puck');
function setInput(i, v) {
  v = Math.min(1, Math.max(0, v));
  if (input[i] === v) return;
  input[i] = v;
  send({ type: 'input', i, v });
  if (i < 2) {
    puck.style.left = `${input[0] * 100}%`;
    puck.style.top = `${(1 - input[1]) * 100}%`;
    stick.setAttribute('aria-valuetext', `X ${input[0].toFixed(2)}, Y ${input[1].toFixed(2)}`);
  }
}
function stickFromPointer(e) {
  const r = stick.getBoundingClientRect();
  let dx = (e.clientX - r.left) / r.width - 0.5, dy = (e.clientY - r.top) / r.height - 0.5;
  const len = Math.hypot(dx, dy), max = 0.5;
  if (len > max) { dx *= max / len; dy *= max / len; }
  setInput(0, dx + 0.5);
  setInput(1, 0.5 - dy);
}
stick.addEventListener('pointerdown', (e) => { stick.setPointerCapture(e.pointerId); stick.classList.add('dragging'); stickFromPointer(e); });
stick.addEventListener('pointermove', (e) => { if (stick.hasPointerCapture(e.pointerId)) stickFromPointer(e); });
stick.addEventListener('pointerup', () => stick.classList.remove('dragging'));
stick.addEventListener('dblclick', () => { setInput(0, 0.5); setInput(1, 0.5); });
stick.addEventListener('keydown', (e) => {
  const s = e.shiftKey ? 0.1 : 0.02;
  const moves = { ArrowLeft: [0, -s], ArrowRight: [0, s], ArrowUp: [1, s], ArrowDown: [1, -s] };
  if (moves[e.key]) { setInput(moves[e.key][0], input[moves[e.key][0]] + moves[e.key][1]); e.preventDefault(); }
});
$('#z').addEventListener('input', (e) => setInput(2, +e.target.value));
$('#w').addEventListener('input', (e) => setInput(3, +e.target.value));

// Gamepad: left stick X/Y, right stick Z/W; A like, B dislike, X drag, Y jolt.
const padPrev = [];
function pollGamepad() {
  const pads = navigator.getGamepads ? navigator.getGamepads() : [];
  const gp = [...pads].find((p) => p && p.connected);
  if (!gp || !node) return;
  const ax = (i) => (Math.abs(gp.axes[i] ?? 0) < 0.06 ? 0 : gp.axes[i]);
  setInput(0, 0.5 + ax(0) / 2); setInput(1, 0.5 - ax(1) / 2);
  if (gp.axes.length >= 4) {
    setInput(2, 0.5 + ax(2) / 2); setInput(3, 0.5 - ax(3) / 2);
    $('#z').value = input[2]; $('#w').value = input[3];
  }
  const pressed = (i) => !!gp.buttons[i]?.pressed;
  const edge = (i) => pressed(i) && !padPrev[i];
  if (edge(0)) gesture(GESTURE.LIKE);
  if (edge(1)) gesture(GESTURE.DISLIKE);
  if (edge(3)) gesture(GESTURE.JOLT);
  setDrag(pressed(2));
  gp.buttons.forEach((b, i) => { padPrev[i] = b.pressed; });
}

// ---------------------------------------------------------------- keyboard
document.addEventListener('keydown', (e) => {
  if (e.target.matches('input, .dial') || e.metaKey || e.ctrlKey || e.altKey) return;
  if (e.repeat && e.code !== 'Space') return;
  const k = e.key.toLowerCase();
  if (k === 'l') gesture(GESTURE.LIKE, $('[data-gesture="0"]'));
  else if (k === 'd') gesture(GESTURE.DISLIKE, $('[data-gesture="1"]'));
  else if (k === 's') gesture(GESTURE.SCRAMBLE, $('[data-gesture="2"]'));
  else if (k === 'j') gesture(GESTURE.JOLT, $('[data-gesture="3"]'));
  else if (k === 'f' && e.shiftKey) gesture(GESTURE.FORGET, $('[data-gesture="4"]'));
  else if (k === 'p') togglePlay();
  else if (e.code === 'Space' && !e.target.matches('button')) { setDrag(true); e.preventDefault(); }
  else if (k >= '1' && k <= '6') showPage($$('#tabs button')[+k - 1].dataset.page);
});
document.addEventListener('keyup', (e) => { if (e.code === 'Space') setDrag(false); });

// ---------------------------------------------------------------- screen pages
function showPage(p) {
  page = p;
  $$('#tabs button').forEach((b) => b.classList.toggle('on', b.dataset.page === p));
  ['focus', 'enable', 'no', 'slots'].forEach((id) => { $(`#page-${id}`).hidden = id !== p; });
}
$$('#tabs button').forEach((b) => b.addEventListener('click', () => showPage(b.dataset.page)));

function makeBlocks(container, names, onClick) {
  container.replaceChildren(...names.map((n, i) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = n;
    b.addEventListener('click', () => onClick(i, b));
    return b;
  }));
}
makeBlocks($('#focus-blocks'), FOCUS_GROUPS, (i) => {
  focusMask ^= 1 << i;
  send({ type: 'focus', group: i });
  renderFocus();
});
function renderFocus() { $$('#focus-blocks button').forEach((b, i) => b.classList.toggle('on', !!(focusMask & (1 << i)))); }

makeBlocks($('#enable-blocks'), ['Voice 1', 'Voice 2', 'Voice 3'], (i) => {
  voiceMask ^= 1 << i;
  send({ type: 'enable', mask: voiceMask });
  renderEnable();
});
function renderEnable() { $$('#enable-blocks button').forEach((b, i) => b.classList.toggle('on', !!(voiceMask & (1 << i)))); }
renderEnable();

$('#dislike-list').replaceChildren(...DISLIKE_MODES.map(([name, desc], i) => {
  const b = document.createElement('button');
  b.type = 'button';
  b.innerHTML = `${name}<small>${desc}</small>`;
  b.addEventListener('click', () => { dislikeMode = i; send({ type: 'dislikeMode', mode: i }); renderDislike(); });
  return b;
}));
function renderDislike() { $$('#dislike-list button').forEach((b, i) => b.classList.toggle('on', i === dislikeMode)); }
renderDislike();

function readSlot(i) {
  try { return JSON.parse(localStorage.getItem(SLOT_KEY(i)) || 'null'); } catch { return null; }
}
makeBlocks($('#slot-blocks'), Array.from({ length: NUM_SLOTS }, (_, i) => String(i + 1)), (i) => {
  if (!node) { say('Start the sound first.'); return; }
  if (slotMode === 'save') { send({ type: 'save', slot: i }); return; }
  const s = readSlot(i);
  if (!s) { say(`Slot ${i + 1} is empty.`); return; }
  const bin = atob(s.data);
  const bytes = new Uint8Array(bin.length);
  for (let j = 0; j < bin.length; j++) bytes[j] = bin.charCodeAt(j);
  send({ type: 'load', slot: i, data: new Float32Array(bytes.buffer) });
});
function renderSlots() {
  $$('#slot-blocks button').forEach((b, i) => {
    const s = readSlot(i);
    b.classList.toggle('used', !!s);
    b.classList.toggle('current', i === currentSlot);
    b.textContent = s ? `${i + 1}\n${new Date(s.when).toLocaleDateString(undefined, { day: 'numeric', month: 'short' })}` : String(i + 1);
    b.style.whiteSpace = 'pre-line';
  });
}
function setSlotMode(m) {
  slotMode = m;
  $('#slot-save').classList.toggle('on', m === 'save');
  $('#slot-load').classList.toggle('on', m === 'load');
}
$('#slot-save').addEventListener('click', () => setSlotMode('save'));
$('#slot-load').addEventListener('click', () => setSlotMode('load'));
renderSlots();

// ---------------------------------------------------------------- canvas screens
const tft = $('#tft'), g = tft.getContext('2d');
const W = 320, H = 240;

function barColour(i, n, exploring, active) {
  const t = i / Math.max(1, n - 1);
  const [a, b] = exploring ? [[230, 70, 60], [240, 210, 70]] : [[63, 174, 108], [59, 143, 214]];
  const c = a.map((v, k) => Math.round(v + (b[k] - v) * t));
  return `rgba(${c[0]},${c[1]},${c[2]},${active ? 1 : 0.28})`;
}

function drawRL() {
  g.fillStyle = '#0d1215'; g.fillRect(0, 0, W, H);
  g.font = '11px ui-monospace, Menlo, monospace';
  if (!state) { g.fillStyle = '#8fa49a'; g.fillText('RL', 8, 16); return; }
  const a = state.action, n = a.length;
  const top = 22, base = 168, x0 = 8, bw = (W - 16) / n;
  g.fillStyle = '#8fa49a'; g.fillText(`RL · ${n} outputs`, 8, 14);
  g.fillStyle = '#1d2a25'; g.fillRect(x0, base, W - 16, 1);
  for (let i = 0; i < n; i++) {
    const active = focusMask === 0 || (GROUP_MASK[i] & focusMask) !== 0;
    const h = Math.max(1, a[i] * (base - top));
    g.fillStyle = barColour(i, n, state.exploring, active);
    g.fillRect(x0 + i * bw, base - h, Math.max(1, bw - 0.6), h);
  }
  SECTIONS.forEach(([start, name]) => {
    const x = x0 + start * bw;
    g.fillStyle = '#2f4038'; g.fillRect(x, top, 1, base - top + 12);
    g.fillStyle = '#8fa49a'; g.fillText(name, x + 3, base + 12);
  });
  // loss trace
  const ty = 186, th = 22;
  g.strokeStyle = '#2a3a33'; g.strokeRect(8.5, ty + 0.5, 140, th);
  if (lossTrace.length > 1) {
    const mx = Math.max(1e-6, ...lossTrace);
    g.beginPath();
    lossTrace.forEach((v, i) => {
      const x = 9 + (i / 239) * 139, y = ty + th - (v / mx) * (th - 2);
      i ? g.lineTo(x, y) : g.moveTo(x, y);
    });
    g.strokeStyle = '#5fd38b'; g.stroke();
  }
  g.fillStyle = '#8fa49a';
  g.fillText(`loss ${state.loss.toFixed(4)}`, 156, ty + 9);
  g.fillText(`yes ${state.likes} / no ${state.dislikes}`, 156, ty + 22);
  const age = performance.now() - lastActionAt;
  if (lastAction && age < 4000) {
    g.fillStyle = age < 400 ? '#ffffff' : '#c6d0c8';
    g.fillText(`last: ${lastAction}`, 8, 230);
  }
  if (focusMask) {
    g.fillStyle = '#b9f04a';
    g.fillText(`focus: ${FOCUS_GROUPS.filter((_, i) => focusMask & (1 << i)).join(', ')}`, 120, 230);
  }
}

function tri(x, y, size, rot, fill, stroke) {
  g.save(); g.translate(x, y); g.rotate(rot);
  g.beginPath(); g.moveTo(0, -size); g.lineTo(size * 0.87, size * 0.5); g.lineTo(-size * 0.87, size * 0.5); g.closePath();
  if (fill) { g.fillStyle = fill; g.fill(); }
  if (stroke) { g.strokeStyle = stroke; g.lineWidth = 1.5; g.stroke(); }
  g.restore();
}

function drawMap() {
  g.fillStyle = '#0d1215'; g.fillRect(0, 0, W, H);
  const s = 200, x0 = (W - s) / 2, y0 = 22;
  g.strokeStyle = '#2a3a33'; g.strokeRect(x0 + 0.5, y0 + 0.5, s, s);
  g.font = '11px ui-monospace, Menlo, monospace';
  g.fillStyle = '#8fa49a'; g.fillText('Map · X/Y, size Z, turn W', 8, 14);
  const px = (v) => x0 + v * s, py = (v) => y0 + (1 - v) * s;
  const size = (z) => 4 + z * 8, rot = (w) => (w - 0.5) * Math.PI * 2;
  if (state) {
    const m = state.mem;
    for (let i = 0; i + 4 < m.length; i += 5) {
      const kind = m[i + 4];
      const col = kind === 0 ? '#5fd38b' : kind === 1 ? '#ff7a6e' : '#7b8a84';
      tri(px(m[i]), py(m[i + 1]), size(m[i + 2]), rot(m[i + 3]), col, null);
    }
  }
  tri(px(input[0]), py(input[1]), size(input[2]) + 2, rot(input[3]), null, dragging ? '#6cb6f5' : '#ffffff');
  g.fillStyle = '#5fd38b'; g.fillText('like', 268, 40);
  g.fillStyle = '#ff7a6e'; g.fillText('no', 268, 56);
  g.fillStyle = '#7b8a84'; g.fillText('jolt', 268, 72);
}

function frame() {
  pollGamepad();
  if (page === 'map') drawMap(); else drawRL();
  requestAnimationFrame(frame);
}

$('#start-btn').addEventListener('click', start);
setInput(0, 0.5); setInput(1, 0.5);
puck.style.left = '50%'; puck.style.top = '50%';
requestAnimationFrame(frame);
