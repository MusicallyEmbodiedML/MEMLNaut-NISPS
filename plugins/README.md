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

`nisps-sc/` is where the engine lives; `nisps-max/` is glue. A change to the
algorithm belongs in `nisps-sc/src/NISPSCore.cpp` and reaches both hosts.

`nisps-sc/memlp/` is vendored (a plain copy, not a submodule) at memlp
`6dd5a4f`, which carries a Release-mode crash fix in `MLP::CreateMLP` that
was never pushed upstream — the firmware's own `src/memlp` submodule is
still at `ea77750`, without it. Reconcile the two once that fix is on the
memlp remote.

Each folder's README (and `nisps-sc/CMakeLists.txt`) has its own build
instructions; both need a C++20 compiler, and each needs its host's SDK
(a SuperCollider source checkout, or max-sdk-base cloned as
`plugins/max-sdk-base`).
