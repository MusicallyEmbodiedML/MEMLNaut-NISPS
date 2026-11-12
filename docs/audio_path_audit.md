# Audio-path audit: code that runs from flash

The audio callback runs from SRAM on core 1. Anything it calls that lives in flash
(XIP) is fetched through the flash cache, which core 0 is also using while it runs
the neural network, so those calls are slow and their cost varies. On the RP2350
Arduino-Pico core this includes the SDK's wrapped float routines (`sinf`, `tanhf`,
`expf`, `exp2f`, `powf`, `fmodf`, ...), software double-precision arithmetic
(`__aeabi_dmul`, `__aeabi_dadd`, ...), `memmove`/`memset`, and every non-inlined
function compiled from a `.cpp` file.

Measured on the device: NautLinear went from ~6100 to ~560 us per 1000 us block
when its oversampler stopped calling `memmove`, and LiveFX from ~911 to ~487 us
(worst case, all effects on) when six `sinf`/`tanhf` calls per sample were replaced.

## How the audit works

Build each mode, then walk the call graph from `audio_block_callback` and
`dma_i2s_in_handler` (following `bl`/`b.w` and veneers) and list every reachable
function whose address is in flash (0x10000000-0x11ffffff). Error-only paths
(`abort`, `raise`, `__errno`, `__throw_bad_function_call`, `__assert_func`, ...) are
ignored. Calls through function pointers (`std::function`, virtuals) are not
followed. "Reachable" is not the same as "runs every sample": some of these are on
rare paths.

## Fixed

- `src/memllib/synth/FastMath.hpp`: inline `sinTurns`/`sin`, `exp2`/`exp`, `tanh`,
  `fmod1`, accurate enough to replace the libm calls in sound-producing code.
- `maxiOsc::sinewave` uses `FastMath::sinTurns` (was `sinf`): affects every mode
  that uses maxiOsc sine oscillators.
- `ModFXI16.hpp` (LiveFX) uses `FastSineOsc`; LiveFX's makeup `tanhf` and per-sample
  `sqrtf`s are gone.
- `maxiFilter` Chamberlin SVFs (`loresChamberlain`, `hiresChamberlain`,
  `bandpassChamberlain` in `maximilian.h`) computed `2*sinf(omega/2)` every sample;
  now `FastMath::sin` (VerbFX, ChannelStrip, BreakOr, Elysiamorfs, ...).
- Per-sample `sinf`/`tanhf`/`fmodf` replaced in MEMLCelium, MixMasterMEML,
  MEMLAciiid, PAF Synth, Bunty, ChannelStrip, Braykore, BreakOr, Elysiamorfs,
  GlitchSeqEngine, RatioSeqEngine, FMPatternGen, and the vendored DaisySP
  `PitchShifter` crossfade gains (`src/daisysp/Effects/pitchshifter.h`).
- `maxiFilter::lopass`/`hipass` are inline in `maximilian.h` (were flash calls from
  `maximilian.cpp`; VerbFX's reverb called lopass 8 times per sample).
- VerbFX ran its parameter smoother, voice-space mapping (`std::function`) and eleven
  `sqrtf` mix weights every sample: ~1.8 ms per 1 ms block, constantly dropping
  audio. Now every 16 samples (smoother retimed to match): ~390 us.
- Result: no per-sample float-library calls left in any mode's audio path. Left:
  ChannelStrip's `maxiConvert::dbsToAmp` (`powf`, below), Braykore's per-trigger
  `powf`, and NautLinear's control-rate update.

## Still to do

### memllib
- `maximilian.cpp` functions used per sample live in flash because they are not
  inline: `maxiRingBuf::push`,
  `maxiDynamicsLite::envToRatio`, `maxiConvert::ampToDbs`/`dbsToAmp` (ChannelStrip,
  which also use double `log10`/`pow`). Inline them in the header, or mark them
  `__not_in_flash_func`.
- `maxiFilter::bandpass` uses double `sqrt`/`pow`.

### Modes
- **Braykore** (worst): `Serial.printf` on every trigger (and its `powf(2, crushBits)`
  there), SD card file calls
  (`fs::File::stat`/`seek` via `BreakbeatPlayer`), `malloc`/`free` and double
  arithmetic are reachable from the audio callback. `BreakbeatPlayer::process` and
  `::trigger` and `RatioSeqEngine::setPlaying` are themselves in flash.
- **BreakOr, Elysiamorfs**: MIDI is sent from the audio callback
  (`MIDIInOut::queueCC`/`queueClock`/`flushQueue`, which can block in `waitForDMA`
  and takes the UART mutex). BreakOr also calls `millis()` there.
- **Software double arithmetic** in the audio path: MEMLCelium, MixMasterMEML,
  Braykore, BreakOr, Elysiamorfs (double literals or double-returning helpers in
  float code; add `f` suffixes / use float helpers).
- **Pico SDK queues** (`queue_try_remove`/`queue_try_add`, in flash, with spin
  locks) polled from the audio callback: Braykore, BreakOr, Elysiamorfs.
- **Bunty**: `daisysp::PitchShifter` and `daisysp::DelayLine` run from flash;
  `ADSRLite::setup` is called from the audio path.
- **Bunty bug?** `BuntyAudioApp::Process` overwrites the sine-shaper output with a
  constant (`shape = sin(0.2*TWOPI*2 + 0.1)`, about 0.50), adding ~+0.3 DC to voice
  0 every sample. Behaviour kept as is; probably meant to use `shape`.
- **Event-rate libm calls** (per note or step, not per sample): `mtof` uses
  `exp2f`/`powf` in MEMLCelium, MixMasterMEML, PAF Synth and ChunkyBits (ChunkyBits'
  `powf` goes through double `exp`/`log`); MEMLAciiid/AcidVoice `exp2f` at the voice
  control rate. Cheap to switch to `FastMath::exp2`.
- **NautLinear**: `updateControl()` (every 64 samples) is in flash and calls
  `expf`/`powf`/`tanhf`. Minor; could move to RAM.
- **std::function** invokers reachable from the callback in most modes (flash);
  check which ones run per sample.

## Core 0 (control loop: network, controls, display)

Profiled on the device with a model loaded (LiveFX, 2026-11-12):

- **Fixed — touch polling**: TFT_eSPI `getTouch()` blocked for ~33 ms per call (five
  samples, each with `delay()` debouncing), and the panel's idle pressure (~150-190)
  is above the threshold of 20 that was passed, so it always ran in full. The control
  loop managed ~35 passes per second instead of 200. `DisplayDriver::PollTouch` now
  reads the pressure once (~30 us) and only calls `getTouch()` above 500 (pressed
  reads ~1000-3000). Loop: ~30 ms -> ~3 ms average, ~185 passes per second.
- **Fixed — memlp training**: RMSProp uses the FPU `vsqrt` instead of a `sqrtf` call
  per weight, and `TrainBatch` takes pointers so `InterfaceRL` no longer copies the
  replay memory into new vectors every tick. Training ~0.77 ms per tick with a full
  memory; inference ~80 us.
- **Still to do — display redraw**: `DisplayDriver::Draw()` takes ~13 ms per redraw
  (30 per second) on a busy screen, ~40% of core 0 and the cause of 16 ms worst-case
  loop passes. Candidates: draw only what changed, or push the frame with DMA without
  blocking.
