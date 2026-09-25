# memlrhythm for Max

The MEMLNaut's two rhythm generators as MSP externals:

| object | ported from |
|---|---|
| `meml.euclid~` | the `euclidean()` function in `modes/AudioApps/ElysiamorfAudioApp.hpp` (MODE_ELYSIAMORFS) and `MEMLCeliumAudioApp.hpp` (MODE_MEMLCELIUM) |
| `meml.ratioseq~` | `ratioSeq()` in `modes/AudioApps/RatioSeq.hpp` as driven by `RatioSeqEngine::tick()` (MODE_MEMLCELIUM) |

Both generators are phase-driven in the firmware, so both objects take a
**phase signal** in their left inlet: patch a `phasor~` in and the patch owns
tempo, sync and swing. With nothing connected to that inlet an internal
phasor free-runs at `@bpm` / `@beats` (one cycle per bar), so either object
is usable on its own.

Every parameter is a Max attribute, which means all of them are settable by
name at control rate (`n 12`, `ratios 1 2 1 3`, `pw 0.25`) from any patch
cord — that is what makes the generators drivable from a mapping object such
as `nisps` in `../nisps-max`. Each object also takes a `norm` message
carrying a 0..1 vector mapped exactly the way the firmware maps NN output.

## meml.euclid~

    meml.euclid~ [n=8] [k=3] [@attrs]

`n` steps per cycle, `k` pulses spread evenly over them, rotated by `offset`,
each pulse lasting `pw` of one step.

| outlet | |
|---|---|
| 0 (signal) | gate, 1 while a pulse sounds |
| 1 (bang) | one bang per pulse onset |
| 2 (int) | the step index that fired, sent just before the bang |

| attribute | default | |
|---|---|---|
| `@n` | 8 | steps per cycle, 1..64 |
| `@k` | 3 | pulses, clamped to `n` |
| `@offset` | 0 | rotation in steps |
| `@pw` | 0.5 | pulse width, a fraction of one step |
| `@bpm` | 120. | internal tempo, used only when inlet 0 is unpatched |
| `@beats` | 4. | beats per cycle |
| `@nrange` | 1 16 | `n` limits used by `norm` |
| `@krange` | 1 16 | `k` limits used by `norm` |

`norm <n> <k> <offset>` takes three 0..1 floats and maps them as
`EuclideanAudioApp::VoiceOperator_` does: `n` snapped to a power of 2 or 3
inside `@nrange`, `k` inside `@krange`, `offset` over 0..n-1.

## meml.ratioseq~

    meml.ratioseq~ [ratios...] [@attrs]

A cycle is cut into slices proportional to `@ratios`, and each slice fires
for `@pw` of its own length — `1 2 1` gives short, long, short. `@ampratios`
is a second, independent pattern whose gate picks the velocity the firmware
sends: 127 where high, 64 where low. The firmware runs several of these in
one engine; here one object is one sequence, so make as many as you need.

| outlet | |
|---|---|
| 0 (signal) | trigger gate |
| 1 (signal) | amp gate, high = velocity 127 |
| 2 (int) | 127 or 64 at each onset, 0 at each release — wire to `makenote` |

| attribute | default | |
|---|---|---|
| `@ratios` | 1 2 1 | slice lengths, up to 32 |
| `@ampratios` | 1 3 | velocity pattern, up to 32 |
| `@mul` | 1. | cycles per bar (the firmware uses 1, 2, 4, 8) |
| `@offset` | 0. | phase offset |
| `@pw` | 0.5 | pulse width, a fraction of each slice |
| `@bpm` | 120. | internal tempo, used only when inlet 0 is unpatched |
| `@beats` | 4. | beats per bar |
| `@mute` | 0 | silence every outlet; settable as a `mute 1` message |

`mute 1` silences all three outlets and `mute 0` lets them sound again.
Muting part way through a note releases it, so nothing is left hanging
downstream, and unmuting part way through a slice waits for the next onset
rather than starting a note in the middle of one — the gate signal and the
velocity outlet come from one decision, so they cannot disagree about whether
a note is sounding. Time keeps running while muted, so unmuting lands
wherever the pattern has got to instead of restarting it.

`norm` takes the firmware's own parameter vector in
`RatioSeqEngine::updateParams()` order: one 0..1 float per ratio, then `mul`,
then `offset`, then one per amp ratio. Ratios snap to 1..4, `mul` to 1/2/4/8,
`offset` to the `@beats` grid.

## Port notes

The generators live in `source/common/RhythmCore.h`, with no Max or Arduino
dependency, and `source/tests/rhythm_test.cpp` checks them against the
firmware's expressions reimplemented verbatim — exhaustively for the
Euclidean one over every `n`, `k` and `offset` up to 16. Three deliberate
differences, all marked `PORT NOTE` in the header:

- **Guarded inputs.** The firmware guards nothing: `n == 0` divides by zero,
  `k > n` makes every step fire, `offset >= n` wraps through unsigned
  arithmetic. These are clamped here, which cannot change the result for any
  input the firmware itself produces.
- **Fractional ratios work.** The firmware's loop is `for (size_t v : ratios)`
  over a *float* array, so each ratio is truncated to an integer inside the
  loop while the sum is computed from the untruncated floats. Every ratio the
  firmware generates is already an integer 1..4, so this is invisible there;
  here the floats are used as written, so `ratios 1 2.5 1` does the sensible
  thing instead of an inconsistent one.
- **The phase offset is applied twice**, once when building the sequence
  phasor and again inside `ratioSeq()`, so an offset shifts the pattern by
  twice its value. That is the firmware's behaviour and it is kept.

One thing worth knowing about the Euclidean function: its index test is
Bresenham rather than Bjorklund. It always produces exactly `k` pulses with
the canonical Euclidean multiset of gaps, but for some `(n, k)` in a
different rotation — E(5,16) comes out `x...x..x..x..x..` where the textbook
pattern is `x..x..x..x..x...`, the same rhythm starting on the long gap.

## Building

Needs [max-sdk-base](https://github.com/Cycling74/max-sdk-base) — cloned next
to this folder as `plugins/max-sdk-base`, or pass `-DMAX_SDK_BASE_PATH=...`.
C++17. Each external builds on its own; point CMake at its source folder.

macOS:

    cmake -S "source/meml.euclid~" -B build -DMAX_SDK_BASE_PATH=/path/to/max-sdk-base \
          -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release      # -> externals/meml.euclid~.mxo

Windows (MSVC):

    cmake -S "source/meml.euclid~" -B build -G "Visual Studio 17 2022" -A x64 -DMAX_SDK_BASE_PATH=...
    cmake --build build --config Release      # -> externals/meml.euclid~.mxe64

Windows from Linux (MinGW-w64, `apt install g++-mingw-w64-x86-64`):

    cmake -S "source/meml.euclid~" -B build-win \
          -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-mingw-w64.cmake \
          -DMAX_SDK_BASE_PATH=... -DCMAKE_BUILD_TYPE=Release
    cmake --build build-win

Host test (anywhere, no SDK needed):

    cmake -S source/tests -B build-tests && cmake --build build-tests
    ./build-tests/rhythm_test

Install: this folder is a Max package — drop it into `~/Documents/Max 9/Packages/`,
restart Max, create `meml.euclid~`, alt-click it for the help patcher.

`help/*.maxhelp` is generated by `python3 source/make_help.py`.
