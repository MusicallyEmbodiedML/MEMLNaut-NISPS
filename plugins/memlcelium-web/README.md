# MEMLCelium for the browser

MODE_MEMLCELIUM running in a web page. The sound is the firmware's own
`modes/AudioApps/MEMLCeliumAudioApp.hpp`, compiled unchanged. The learning is
`NISPSCore` from `../nisps-sc`, the same port of `InterfaceRL` the VCV,
SuperCollider and Max versions use. Both are built to one standalone `.wasm`
that runs inside an AudioWorklet; the page draws the panel and the screens.

## Files

| Path | What it is |
|---|---|
| `src/celium.cpp` | Glue: the 5 ms engine tick, focus latch, gestures, save/load, and the C exports the worklet calls |
| `src/prelude.hpp` | Force-included first. Stands in for `AudioDriver.hpp` and `InterfaceBase.hpp` by defining their include guards |
| `shim/` | Host stand-ins for `pico.h`, `pico/util/queue.h`, Arduino's `String` |
| `web/index.html`, `style.css`, `app.js` | The panel: screen (RL, Map, Focus, Enable, No, Slots), switches, knobs, joystick |
| `web/worklet.js` | The AudioWorklet processor that runs the wasm |
| `web/memlcelium.wasm` | Build output |
| `tests/smoke.mjs` | Headless check in Node |

## Build and run

    ./build.sh                     # needs Emscripten (~/emsdk); writes web/memlcelium.wasm
    node tests/smoke.mjs           # sound, gestures, sequencer, save/load, speed
    python3 -m http.server -d web 8000   # then open http://localhost:8000

AudioWorklet needs a secure context, so serve over `https://` or from `localhost`;
opening `index.html` as a file will not work. The `web/` folder is static and
can be copied to any web server as it is. The MEML website serves it from
`www.meml/play/memlcelium/`: after a rebuild, copy `web/*` there.

## How it maps to the hardware

| Hardware | Page |
|---|---|
| Joystick X/Y, twist Z, ADC3 W | Drag the stick; Z and W sliders; or a gamepad (left stick X/Y, right stick Z/W) |
| Momentary A (left) up / down | Scramble / Jolt (`S` / `J`) |
| Momentary B (right) up / down | Like / Dislike (`L` / `D`) |
| Toggle A (left) up | Forget (`Shift`+`F`) |
| Toggle B (right) up, joystick press | Drag, held (`Space`, gamepad X) |
| Toggle B (right) down | Sequencer start/stop (`P`) |
| Knob X, Y, Z, G | Learning rate, tempo 30–200 BPM, exploration, volume |
| Encoder + screens | Tabs (`1`–`6`) |
| SD card slots | 12 slots in the browser's localStorage |

## Differences from the firmware

- **One thread.** On the MEMLNaut the network runs on core 0 and the audio on
  core 1. Here both run on the AudioWorklet thread: `celium_render()` runs one
  engine tick every 5 ms of audio, then hands the action straight to
  `ProcessParams()` instead of through a queue.
- **Models** are saved in the browser, not on an SD card, and are not
  interchangeable with the hardware's model files.
