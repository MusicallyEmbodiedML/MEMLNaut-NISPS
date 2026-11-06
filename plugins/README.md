# plugins

Desktop ports of the MEMLNaut's NISPS engine — the same interactive
reinforcement-learning mapper the firmware runs in
`src/memllib/examples/InterfaceRL.{hpp,tpp}`, rebuilt so it can be played
from a laptop.

- **`nisps-sc/`** — the portable engine (`src/NISPSCore.{h,cpp}`, no Arduino
  or display dependencies) plus the `NISPS` SuperCollider UGen that wraps it,
  its sclang class, a host smoke test and scsynth integration tests.
- **`nisps-max/`** — a Max package containing the `nisps` external, which
  compiles `NISPSCore` straight out of `nisps-sc/` (one copy of the engine)
  and exposes it as a control-rate Max object with a help patcher.
- **`memlnaut-vcv/`** — a VCV Rack 2 plugin: the MEMLNaut as a 24 HP module,
  `NISPSCore` mapping 4 inputs (an on-screen XY pad plus Z/W, or CV) to 32 CV
  outputs, with the hardware's screen redrawn on the panel and the gestures
  as buttons and gate inputs. Also compiles the engine out of `nisps-sc/`.
- **`memlcelium-web/`** — MODE_MEMLCELIUM in the browser: the firmware's own
  `MEMLCeliumAudioApp.hpp`, compiled unchanged against small Pico/Arduino
  stand-ins, driven by `NISPSCore`, built to WebAssembly with Emscripten and
  run in an AudioWorklet behind an HTML panel. Compiles the engine out of
  `nisps-sc/`.
- **`memlrhythm-max/`** — a Max package with the firmware's two rhythm
  generators, `meml.euclid~` (from MODE_ELYSIAMORFS / MODE_MEMLCELIUM) and
  `meml.ratioseq~` (the RatioSeqEngine behind MODE_MEMLCELIUM), plus a host
  test that checks them against the firmware's own expressions. Independent
  of the NISPS engine, though `nisps` is the obvious thing to drive them
  with — every parameter is an attribute.

`nisps-sc/` is where the engine lives; `nisps-max/` and `memlnaut-vcv/` are
glue. A change to the algorithm belongs in `nisps-sc/src/NISPSCore.cpp` and
reaches every host. The engine tracks the firmware's `InterfaceRL` as of
memllib `b3a1788`: weight-space exploration noise, jolt as planted
pseudo-likes, the five dislike modes, focus, drag and train-on-demand.

`nisps-sc/memlp/` is vendored (a plain copy, not a submodule) at memlp
`6dd5a4f`, which carries a Release-mode crash fix in `MLP::CreateMLP` that
was never pushed upstream — the firmware's own `src/memlp` submodule is
still at `ea77750`, without it. Reconcile the two once that fix is on the
memlp remote. Its `OrnsteinUhlenbeckNoise.h` has since been brought up to the
firmware's memlp `5273f17` (`get()`/`set()`, and `minstd_rand`), which the
engine needs to jump the exploration noise on a dislike.

Each folder's README (and `nisps-sc/CMakeLists.txt`) has its own build
instructions; both need a C++20 compiler, and each needs its host's SDK
(a SuperCollider source checkout, max-sdk-base cloned as
`plugins/max-sdk-base`, or the VCV Rack SDK).
