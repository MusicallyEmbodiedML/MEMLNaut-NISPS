# NISPS — Neural Interactive Shaping of Parameter Spaces

> Workflow reference for documentation generation.
> Project page: https://musicallyembodiedml.github.io/memlnaut/approaches/nisps

## 1. Concept

NISPS lets a musician **sculpt the parameter space of a sound engine by feel**, without
ever editing parameters directly. A small neural network maps a low-dimensional gesture
input (joystick, machine-listening features, or MIDI CCs) to the full high-dimensional
parameter vector of a synth, effect, or external instrument. The performer explores by
moving the input, and *shapes* the mapping in real time using lightweight feedback —
**like / dislike**, **drag-to-place**, **randomise**, and **jolt** — instead of tweaking
knobs.

It is a form of **interactive reinforcement learning with human feedback (RLHF)**,
deliberately simplified for an embedded (RP2350 / Pico-class) target. There is no critic,
no value function, no Bellman update. The single MLP is simultaneously the policy and the
thing being trained; supervised targets are synthesised directly from human feedback. See
`docs/dislike_system_analysis.md` for the deep ML analysis of the feedback algorithm.

## 2. System architecture

- **Hardware platform:** MEMLNaut board (RP2350 dual-core), TFT display, joystick,
  momentary/toggle switches, three assignable knobs (RV X1/Y1/Z1), optional gain knob,
  MIDI I/O, SD card, and onboard flash (LittleFS).
- **Dual-core split** (`MEMLNaut-NISPS.ino`):
  - **Core 0** — interface + ML. Runs the periodic ML inference/training loop
    (`ML_INFERENCE_PERIOD_US = 5000`, i.e. ~200 Hz), reads the UI, drives the display.
  - **Core 1** — real-time audio. Runs the audio block callback, per-block parameter
    processing, and polls MIDI every 1 ms.
  - Cores hand off via volatile ready-flags with explicit memory barriers; the MLP is
    guarded by a `spin_lock` (`mlpActive`) so training never races audio-thread reads.
- **Mode system:** Exactly one `MODE_*` is selected at compile time in the `.ino`. Each
  mode is a self-contained header satisfying the `MEMLNautMode` C++20 concept
  (`modes/MEMLNautMode.hpp`) — it must provide audio `process()`, `setupInterface()`,
  `addViews()`, `setupAudio()`, MIDI setup, etc. ~20 modes exist (PAFSynth, ChannelStrip,
  XIASRI, VerbFX, DJFX, D50, TR6S, **TR8S**, Mopho, MicroQ, …), each pairing the shared
  RL engine with a particular sound engine or external MIDI instrument.
- **Shared engine:** Every mode embeds one `InterfaceRL`
  (`src/memllib/examples/InterfaceRL.{hpp,cpp}`) — the neural mapper, replay memory,
  feedback logic, exploration noise, display views, and save/load.

## 3. The neural mapping

- **Network:** a fully-connected MLP, `layers = { n_inputs, 16, 16, n_outputs }`,
  activations `RELU, RELU, HARDSIGMOID` (output bounded to [0,1]).
- **Inputs (`n_inputs = kMaxNNInputs = 10`):** the active input source fills the leading
  dims; unused tail dims are padded with a non-zero constant `unusedInputDefault_ =
  1.1 / n_unused` (a deliberate per-unit bias shift that makes the mapping more non-linear
  across a single-input sweep — see `copyAndZero()`).
- **Outputs (`n_outputs`):** mode-specific; each output is one synth/MIDI parameter in
  [0,1]. For TR8S this is 32 MIDI CC values.
- **Inference (`generateAction()`):** runs whenever the input changes or a refresh is
  forced. It assembles inputs → runs the MLP → adds per-output **Ornstein–Uhlenbeck
  exploration noise** (reflected at the [0,1] boundaries) → applies optional mode hooks →
  pushes the result to the audio queue and updates the display.

### Input sources (`INPUT_SOURCE`, runtime-selectable, persisted to flash)

`3D Joystick`, `4D Joystick`, `Machine Listening` (6 audio-analysis features),
`MIDI 1CC` (mod wheel), `MIDI 3CC`, `MIDI 8CC`, or `Combined` (joystick + ML). The
selection is chosen via an on-screen rotary view and saved to `/input_source.bin`.

## 4. The interaction workflow (core loop)

Each Core-0 loop tick (`InterfaceRL`'s loop callback, `bind_RL_interface()`):

1. **Drain deferred feedback** queued from ISR/switch callbacks (like, dislike,
   drag-store). These are flagged in the ISR and executed here so they never touch the
   replay memory concurrently.
2. **`optimiseSometimes()`** — run one training step every `optimiseDivisor` cycles
   (knob-adjustable; can be paused entirely).
3. **`generateAction()`** — regenerate and send the current action.

The performer's interaction vocabulary:

| Gesture | Control | Effect |
|---|---|---|
| **Explore** | Joystick / ML / MIDI input | Moves through the parameter space via the current mapping. |
| **Positive reward** | Momentary A1 up (or MIDI CC1) | Stores `(input, action, reward=+1)`; resets dislike multiplier. |
| **Negative reward** | Momentary A2 down (or MIDI CC2) | Stores/accumulates a negative experience at the current state; doubles dislike multiplier (cap 16). |
| **Drag sound** | Joystick switch / Toggle A1 | Freezes the action, lets the performer move the input to a new location, then on release stores the held action there as a positive example ("put this sound *here*"). |
| **Randomise / Scramble** | Momentary B1 up (or MIDI CC3) | Draws fresh random network weights — a whole new parameter landscape. |
| **Jolt** | Momentary B2 down | Perturbs existing weights slightly — a nudge, not a reset. |
| **Clear replay memory** | Toggle B1 (or MIDI CC4) | Forgets all stored experiences; resets multiplier. |
| **Learning rate** | Knob RV Y1 | Scales training step size. |
| **Reward scale** | Knob RV X1 | Scales stored reward magnitude (some modes repurpose this knob). |
| **Exploration noise** | Knob RV Z1 | Sets OU noise sigma (0 = off). |

## 5. The learning algorithm (`optimise()`)

Runs every `optimiseDivisor` cycles. Experiences live in a 64-item `ReplayMemory` of
`{input, action, reward}` tuples. One optimise step:

1. **k-NN positive centroid** — average the `kCentroidK = 4` positive memories whose
   stored input is nearest the current input. This is the context-relevant "good" target.
2. **Positive batch** — draw 8 random memories; train the network on the positive ones
   toward their own stored actions, with `lr = learningRateScaled · avgRewardPos`
   (imitation of liked outputs).
3. **Negative batch (full scan)** — every memory with `reward ≤ 0` is processed every
   cycle (no sampling miss). Each negative reward is **decayed** proportionally
   (`reward += 0.0025 · max(|reward|,1)`) so high-magnitude dislikes expire on the same
   wall-clock schedule as single ones; items crossing `-0.01` are removed.
4. **Geometric push** — for each disliked action, build a target that moves it *away* from
   itself and *toward* the positive centroid. Step size
   `pushStep = clamp(|avgRewardNeg|, 0.25, 1) · 0.5`, tapered by `1/(1+len)` so actions
   already far from liked territory are pushed less. Only **active dimensions**
   (`activeDims_`, set by the mode's focus system) are moved.
5. **Dynamic negative LR** — `negLRRatio = 0.5 − 0.4·negFraction`: push hard when dislikes
   are rare, gently when they flood the buffer (prevents policy collapse).
6. **Multiplier decay** — as negatives expire, `dislikeMultiplier_` halves per item and
   fully resets to 1 when no negatives remain.

This is closest to **imitation learning with negative (contrastive) examples** plus
metric-style geometric pushing — not actor-critic, Q-learning, or policy gradient.

## 6. Focus (per-dimension targeting)

Modes can group output parameters and let the performer **focus** on a subset. `FocusManager`
latches unfocused NN outputs and exposes an `activeDims_` mask to the RL engine, so feedback
only reshapes the parameters the performer currently cares about. In TR8S the 32 outputs are
grouped into 12 focus blocks (one per drum instrument BD/SD/LT/MT/HT/RS/HC/CH/OH/CC/RC, plus a
combined FX group), toggled from an on-screen `BlockSelectView`.

## 7. Worked example — TR8S mode (`MEMLNautModeTR8S.hpp`)

- **Target:** external Roland TR-8S drum machine over MIDI channel 10.
- **Outputs:** 32 NN outputs → 32 assignable MIDI CCs. A full CC catalogue (per-instrument
  Ctrl/Tune/Decay/Level + global FX, from the TR-8S MIDI Implementation v1.02) is selectable
  on a `CCSelectView`; a sensible 32-CC default set is provided.
- **Home + fade:** each CC has a stored **home** value (set with the gain knob). The RV X1
  knob is repurposed from "reward scale" to a **modulation fade** — output =
  `home + fade·(NN − home)`. Fade 0 = static home values, fade 1 = full NN modulation. This
  home/fade blend is applied *only* on the way out to MIDI; the NN-outputs display and the
  training targets see the raw network values.
- **Input:** 4D joystick by default.
- **Persistence:** CC assignments + home values are bundled into each saved model (so loading
  a model restores its own mapping) and also kept in a global flash default
  (`/tr8s_cc.bin`).

## 8. Persistence

- **Models** save/load to SD card via 8 named slots per mode
  (`_save_RL_to_SD` / `_load_RL_from_SD`). Files carry a `MEMLFileHeader` (magic `"MEML"`,
  format version, 16-byte mode tag) so a model can only be loaded back into the mode that
  wrote it; architecture mismatches trigger a clean rebuild.
- **Extra data hooks** (`setExtraSaveCallback` / `setExtraLoadCallback`) let a mode embed
  mode-specific state (e.g. TR8S CC mapping) inside the model file.
- **Settings** persisted to onboard flash: input source (`/input_source.bin`), MIDI CC-out
  numbers (`/<mode>_cc_numbers.bin`), slot names (`/<mode>/slots.txt`).

## 9. On-screen views

`InterfaceRL` builds a standard view stack: NN-outputs RL graph (with last-action label,
loss, like/dislike memory counts, dislike multiplier), NN-inputs bar graph, message log,
Save/Load model browsers, name-input dialog, input-source selector, and (optional) MIDI CC
selector. Modes insert their own views (focus blocks, CC-assign page, etc.).

---

### Quick mental model

> **Explore** with a gesture → the network turns it into a soundscape → **react** (like /
> dislike / drag / scramble / jolt) → the network continuously retrains so the
> regions you like attract and the regions you dislike repel — all in real time, all by
> feel.
