# nisps for Max

Max/MSP port of the NISPS (Neural Interactive Shaping of Parameter Spaces)
RL mapper. Same engine as the `NISPS` SuperCollider UGen in `../nisps-sc`
(`NISPSCore` + the memlp subset it needs), which is itself a port of this
repo's firmware `InterfaceRL` (`../../src/memllib/examples/InterfaceRL.{hpp,tpp}`).
The engine sources are compiled straight from `../nisps-sc`, so there is one
copy to maintain.

    nisps [n_inputs=2] [n_outputs=4] [@attrs]

A control-rate object (no signals): the SC UGen runs at kr, and the Max
equivalent is scheduler-thread messages. An internal clock ticks the engine
at `@interval` ms (default 5 ms, the firmware's 200 Hz loop). Each tick
trains, maps the current input vector and emits the action list if it
changed.

| inlet 0 | |
|---|---|
| list | the `n_inputs` control values in 0..1 (joystick, listening features, CCs…) |
| float | same, for a 1-input object |
| bang | run one tick now (train + map + output); with `@active 0` this is how to drive it from your own `metro` |
| `like` / `dislike` | store the current (input, action) pair as a liked / disliked experience |
| `randomise` (`randomize`) | scramble the network weights |
| `clear` | empty the replay memory |
| `jolt 1` / `jolt 0` | hold / release the weight-morph gate (learning pauses while held) |
| `write [name]` | save the learned state to a JSON file; no name uses `@file`, else a save dialog |
| `read [name]` | load a file written by `write`; no name uses `@file`, else an open dialog |
| `store <n>` | snapshot the current state into slot `n` (1-based) |
| `recall <n>` | make slot `n` the live state |
| `erase <n>` (`delete <n>`) | empty slot `n` |
| `slots` | report the occupied slot numbers out the right outlet |

| outlet | |
|---|---|
| 0 (list) | the `n_outputs` mapped values in 0..1 |
| 1 (int) | replay-memory size, after every like / dislike / clear / read |
| 2 (any) | `slots <n> <n> …` in reply to `slots`, and `slot <n>` after a store or recall |
| 3 (float) | training error: the mean loss of the most recent training pass |

| attribute | default | |
|---|---|---|
| `@active` | 1 | run the internal clock (0 = tick only on bang) |
| `@interval` | 5. | tick period in ms |
| `@lrscale` | 1. | learning-rate multiplier |
| `@rewardscale` | 1. | reward multiplier applied to like / dislike |
| `@noise` | 0.2 | exploration noise level, 0..1 (0 = off) |
| `@traindivisor` | 1 | train once every N ticks |
| `@storemode` | 2 | memory dedup policy: 0 add, 1 replace5, 2 replace10, 3 replace15, 4 decay10, 5 decay20 |
| `@file` | | the state file this object belongs to |
| `@autoload` | 1 | read `@file` when the patcher finishes loading |

Mapping from the SC UGen: `numOutputs` and the input array's length became
the two creation arguments; every kr control input became an attribute; the
edge-detected gesture inputs became messages.

## Saving and loading

`write` captures everything the object has learned — the network's weights
**and biases**, the replay memory with each experience's age, the current
position and its mapped action, and the engine parameters — as a Max
dictionary written to JSON, the same file idea as `pattrstorage`. The result
opens in `[dict]` or any text editor:

    { "nisps" : { "version" : 1, "inputs" : 2, "outputs" : 4,
                  "layer_shape" : [2, 16, 16, 16, 16, 4],
                  "weights" : [...], "biases" : [...],
                  "memory_count" : 12, "memory_inputs" : [...], ... } }

With a filename it writes there and reads back along Max's search path; with
no argument it opens the standard save or open dialog. Both are deferred to
the main thread, so sending `read` from a message box in a running patch is
safe. On a successful `read` the loaded parameters are written into the
object's attributes (they are the source of truth — the tick pushes them into
the engine every time), and the memory-size outlet reports the new count.

A file is validated in full before anything is touched: wrong version, a
different input/output width, a layer shape that disagrees with the weight
array, or replay-memory arrays that do not match their own count are all
refused with a message in the Max console, leaving the running engine exactly
as it was.

The state lives in `NISPSCore::captureState()` / `restoreState()` and its file
shape in `NISPSStateFlat.{h,cpp}`, both in `../nisps-sc` — so this is plain
C++ covered by that package's host test, and the SuperCollider plugin can
gain the same persistence without duplicating anything. Note that memlp's own
`MLP::Serialise()` is deliberately **not** used: it goes through
`Layer::GetWeights2D()`, which covers `m_weights` but not `m_biases`, so a
network saved through it would not reproduce the sound that was saved.

These files are the Max object's own; they are not interchangeable with
models saved on MEMLNaut hardware, whose binary format stores raw `size_t`
values of a different width.

## Training error

The right outlet sends the mean loss (MSE) of each training pass as it
happens, so you can watch the mapping settle — patch it into a `number` or a
`plot~`. It is sent only when a pass actually runs, so it falls silent when
`@traindivisor` is holding training off or the replay memory is empty; the
object tracks a pass counter internally rather than watching the value, since
a loss that happens to repeat is not the same as no training.

Expect zero, or near it, when you have only been pressing `like` with
`@noise 0`: the training target then *is* the network's own output, so there
is nothing to correct. Dislikes push the target away from the action that was
taken, which is what makes the number move.

## Slots

`store 1`, `store 2`, … keep numbered snapshots in memory and `recall <n>`
makes one of them live again — the pattrstorage idea, except that a slot here
holds *everything*: the network, the replay memory and the engine parameters.
Recalling is a complete change of mapping, not a parameter set, so a set of
slots is a set of instruments to move between mid-performance. `slots` reports
which are occupied out the right outlet, as `slots 1 2 5`, and `store` /
`recall` echo `slot <n>` there so a patch can follow along.

Slots travel inside the same file as the live state, under a `slots` entry:

    { "nisps" : { …live state…,
                  "slots" : { "1" : { …a whole state… },
                              "2" : { …another… } } } }

A file with no `slots` entry is perfectly valid and simply has none. If any
slot in a file fails to validate the whole read is refused, naming the slot —
a file that silently loses half its presets would be worse than one that says
what is wrong with it.

## Autoload

    nisps 2 4 @file mymap.json

reads `mymap.json` when the patcher finishes loading, restoring the live state
and every slot in it. `@autoload 0` keeps the name without reading, which is
useful when you want a bare `write` to go back to the same file — with `@file`
set, `write` and `read` use it instead of opening a dialog, so a patch can
save with a single message box.

The read happens on the object's `loadbang`, which is when the file is
findable relative to the patcher. An object created by scripting never
receives a loadbang, so such a patch should send `read` itself.

## Building

Needs [max-sdk-base](https://github.com/Cycling74/max-sdk-base) — cloned
next to this folder, i.e. `plugins/max-sdk-base`, or pass
`-DMAX_SDK_BASE_PATH=/path/to/max-sdk-base` — and `../nisps-sc` (override with
`-DNISPS_SC_PATH`). C++20 (memlp uses `std::span`).

macOS:

    cmake -S source/nisps -B build -DMAX_SDK_BASE_PATH=/path/to/max-sdk-base \
          -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release        # -> externals/nisps.mxo

Windows (MSVC):

    cmake -S source/nisps -B build -G "Visual Studio 17 2022" -A x64 -DMAX_SDK_BASE_PATH=...
    cmake --build build --config Release       # -> externals/nisps.mxe64

Windows from Linux (MinGW-w64, `apt install g++-mingw-w64-x86-64`):

    cmake -S source/nisps -B build-win -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-mingw-w64.cmake \
          -DMAX_SDK_BASE_PATH=... -DCMAKE_BUILD_TYPE=Release
    cmake --build build-win                    # -> externals/nisps.mxe64

Install: this folder is a Max package — drop it into `~/Documents/Max 9/Packages/`
(or copy `externals/nisps.*` and `help/nisps.maxhelp` into `~/Documents/Max 9/Library/`),
restart Max, create `nisps`, alt-click it for the help patcher.

`help/nisps.maxhelp` is generated by `python3 source/make_help.py`. It uses
Max's tabbed help layout — `basic`, `storage`, `slots` — which is a convention
inside the patcher rather than a format of its own: a `js helpstarter.js
nisps` object in the root plus one `p <name>` subpatcher per tab, each with
the varname `<name>_tab` and `showontab: 1`. That is how the help files
shipped with the Max SDK are built.
