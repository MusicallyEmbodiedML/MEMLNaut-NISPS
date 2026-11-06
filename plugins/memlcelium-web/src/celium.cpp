// MEMLCelium in the browser: the firmware's MEMLCeliumAudioApp (unchanged, compiled
// against the stand-ins in ../shim and prelude.hpp) driven by NISPSCore, the portable
// port of InterfaceRL that the VCV, SuperCollider and Max versions use.
//
// Everything runs on the AudioWorklet's thread. render() produces audio in blocks and
// runs one engine tick every 5 ms of audio, standing in for the firmware's 200 Hz
// control loop on core 0. The tick follows MEMLNautModeMEMLCelium + InterfaceRL:
// gestures, engine step, focus latch on the way out, then the audio app's
// ProcessParams().

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include "../../../modes/AudioApps/MEMLCeliumAudioApp.hpp"
#include "../../../src/memllib/audio/FocusManager.hpp"
#include "NISPSCore.h"
#include "NISPSStateFlat.h"

namespace {

using App = MEMLCeliumAudioApp<>;
constexpr size_t kNParams = App::kN_Params;
constexpr size_t kNInputs = 4;      // 4D joystick: X, Y, Z (twist), W
constexpr size_t kNetInputs = 10;   // InterfaceRLBase::kMaxNNInputs
constexpr size_t kNGroups = 6;      // Seq, Synth, Env, Voice 1-3
constexpr double kTickMs = 5.0;     // ML_INFERENCE_PERIOD_US
constexpr int kMaxBlock = 128;

App app;
std::unique_ptr<NISPSCore> core;
FocusManager<kNParams, kNGroups> focus;

float sampleRate = 48000.f;
double samplesPerTick = 240.0;
double tickPhase = 0.0;

float inputs[kNInputs] = {0.5f, 0.5f, 0.5f, 0.5f};
bool inputsChanged = true;
float learnKnob = 0.5f, exploreKnob = 0.f, volume = 0.7f;

enum Gesture { LIKE, DISLIKE, SCRAMBLE, JOLT, FORGET, NUM_GESTURES };
bool pending[NUM_GESTURES] = {};
bool dragWanted = false, dragHeld = false;

std::array<float, kNParams> params{};   // what the audio app last received
std::vector<float> sent;                // focus-latched action, for change detection
std::array<float, kNParams> uiAction{}; // the raw network output, for the RL screen

float outL[kMaxBlock], outR[kMaxBlock];
std::vector<float> memSnapshot;         // per memory item: x, y, z, w, reward
std::vector<float> stateBuf;            // save/load scratch

void updateActiveDims() {
    const uint32_t mask = focus.getSelectedMask();
    std::vector<bool> active(kNParams);
    for (size_t i = 0; i < kNParams; i++)
        active[i] = (mask == 0) || ((App::kParamGroupMask[i] & mask) != 0);
    core->setActiveDims(active);
}

void tick() {
    if (inputsChanged) {
        core->setInputs(std::vector<float>(inputs, inputs + kNInputs));
        inputsChanged = false;
    }
    core->setLearningRateScale(learnKnob);
    core->setNoiseLevel(exploreKnob);

    // The firmware's loop order: jolt, like, dislike; then the rest.
    if (pending[JOLT]) core->jolt();
    if (pending[LIKE]) core->like();
    if (pending[DISLIKE]) core->dislike();
    if (pending[SCRAMBLE]) core->randomiseNetwork();
    if (pending[FORGET]) core->clearMemory();
    for (bool& p : pending) p = false;
    if (dragWanted && !dragHeld) core->startDrag();
    if (!dragWanted && dragHeld) core->endDrag();
    dragHeld = dragWanted;

    core->advanceClock(kTickMs);
    core->process();

    const std::vector<float>& action = core->getAction();
    std::copy(action.begin(), action.end(), uiAction.begin());
    std::vector<float> p(action);
    focus.applyInPlace(p);  // MEMLCelium's paramTransformHook
    if (p != sent) {
        sent = p;
        std::copy(p.begin(), p.end(), params.begin());
        app.ProcessParams(params);
    }
    app.loop();  // picks up tempo changes
}

void pushFlat(std::vector<float>& out, const std::vector<float>& v) {
    out.push_back(static_cast<float>(v.size()));
    out.insert(out.end(), v.begin(), v.end());
}

bool popFlat(const float* data, size_t len, size_t& pos, std::vector<float>& v) {
    if (pos >= len) return false;
    const size_t n = static_cast<size_t>(data[pos++]);
    if (pos + n > len) return false;
    v.assign(data + pos, data + pos + n);
    pos += n;
    return true;
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE void celium_init(float sr, unsigned seed) {
    srand(seed);
    sampleRate = sr;
    samplesPerTick = sr * kTickMs / 1000.0;
    core = std::make_unique<NISPSCore>(kNInputs, kNParams, kNetInputs);
    focus.setParamGroups(App::kParamGroupMask);
    updateActiveDims();
    app.Setup(sr, nullptr);
    sent.clear();
    tick();
}

EMSCRIPTEN_KEEPALIVE float* celium_out_l() { return outL; }
EMSCRIPTEN_KEEPALIVE float* celium_out_r() { return outR; }

EMSCRIPTEN_KEEPALIVE void celium_render(int n) {
    if (n > kMaxBlock) n = kMaxBlock;
    for (int i = 0; i < n; i++) {
        if (tickPhase <= 0.0) {
            tick();
            tickPhase += samplesPerTick;
        }
        tickPhase -= 1.0;
        const stereosample_t y = app.Process({0.f, 0.f});
        outL[i] = y.L * volume;
        outR[i] = y.R * volume;
    }
}

// --- controls ---
EMSCRIPTEN_KEEPALIVE void celium_set_input(int i, float v) {
    if (i < 0 || i >= static_cast<int>(kNInputs)) return;
    if (inputs[i] != v) { inputs[i] = v; inputsChanged = true; }
}
EMSCRIPTEN_KEEPALIVE void celium_gesture(int g) {
    if (g >= 0 && g < NUM_GESTURES) pending[g] = true;
}
EMSCRIPTEN_KEEPALIVE void celium_set_drag(int on) { dragWanted = on != 0; }

// 0 = knob Y (tempo, 30-200 BPM), 1 = knob X (learning rate), 2 = Z (exploration), 3 = G (volume)
EMSCRIPTEN_KEEPALIVE void celium_set_knob(int k, float v) {
    switch (k) {
        case 0: { float bpm = 30.f + v * 170.f; queue_try_add(&app.bpmControlQueue, &bpm); } break;
        case 1: learnKnob = v; break;
        case 2: exploreKnob = v; break;
        case 3: volume = v; break;
    }
}

// The audio app reads this queue as an int (the firmware sends a bool; see the notes).
EMSCRIPTEN_KEEPALIVE void celium_set_playing(int on) {
    int v = on ? 1 : 0;
    queue_try_add(&app.sequencerControlQueue, &v);
    sent.clear();  // ProcessParams is where the audio app reads the queue
}

EMSCRIPTEN_KEEPALIVE uint32_t celium_toggle_focus(int group) {
    if (group < 0 || group >= static_cast<int>(kNGroups)) return focus.getSelectedMask();
    const uint32_t mask = focus.getSelectedMask() ^ (1u << group);
    focus.setFocus(mask, core->getAction());
    updateActiveDims();
    return mask;
}

EMSCRIPTEN_KEEPALIVE void celium_set_voice_enable(uint32_t mask) { app.voiceEnableMask_ = mask; }

EMSCRIPTEN_KEEPALIVE void celium_set_dislike_mode(int m) {
    if (m >= 0 && m < static_cast<int>(NISPSCore::DislikeMode::COUNT))
        core->setDislikeMode(static_cast<NISPSCore::DislikeMode>(m));
}

// --- what the screens show ---
EMSCRIPTEN_KEEPALIVE float* celium_action() { return uiAction.data(); }
EMSCRIPTEN_KEEPALIVE int celium_num_params() { return static_cast<int>(kNParams); }
EMSCRIPTEN_KEEPALIVE int celium_likes() { return static_cast<int>(core->likeCount()); }
EMSCRIPTEN_KEEPALIVE int celium_dislikes() { return static_cast<int>(core->dislikeCount()); }
EMSCRIPTEN_KEEPALIVE float celium_loss() { return core->lastLikeLoss(); }
EMSCRIPTEN_KEEPALIVE int celium_noise_active() { return core->noiseActive() ? 1 : 0; }

// Fills a buffer with x, y, z, w, kind (0 like, 1 dislike, 2 jolt point) for each
// memory, as the firmware's map draws them; returns the point count.
EMSCRIPTEN_KEEPALIVE int celium_memory_snapshot() {
    std::vector<NISPSCore::MemoryPoint> pts;
    core->memoryPoints(pts);
    memSnapshot.clear();
    for (const auto& m : pts) {
        for (size_t i = 0; i < kNInputs; i++) memSnapshot.push_back(i < m.input.size() ? m.input[i] : 0.5f);
        memSnapshot.push_back(static_cast<float>(m.kind));
    }
    return static_cast<int>(pts.size());
}
EMSCRIPTEN_KEEPALIVE float* celium_memory_data() { return memSnapshot.data(); }

// The last gesture, as the firmware reports it: a serial that counts up per event,
// then "action\nmessage" in a C string.
std::string eventText;
EMSCRIPTEN_KEEPALIVE int celium_event_serial() { return static_cast<int>(core->lastEvent().serial); }
EMSCRIPTEN_KEEPALIVE const char* celium_event_text() {
    eventText = core->lastEvent().action + "\n" + core->lastEvent().message;
    return eventText.c_str();
}

// --- save / load: the learned state as a flat float array ---
EMSCRIPTEN_KEEPALIVE int celium_state_save() {
    NISPSState st;
    core->captureState(st);
    nisps_flat::Flat f;
    nisps_flat::flatten(st, f);
    stateBuf.clear();
    stateBuf.push_back(static_cast<float>(f.version));
    stateBuf.push_back(static_cast<float>(f.inputs));
    stateBuf.push_back(static_cast<float>(f.outputs));
    std::vector<float> shape(f.layerShape.begin(), f.layerShape.end());
    pushFlat(stateBuf, shape);
    pushFlat(stateBuf, f.weights);
    pushFlat(stateBuf, f.biases);
    stateBuf.push_back(static_cast<float>(f.memoryCount));
    pushFlat(stateBuf, f.memInputs);
    pushFlat(stateBuf, f.memActions);
    pushFlat(stateBuf, f.memRewards);
    pushFlat(stateBuf, f.memAges);
    pushFlat(stateBuf, f.input);
    pushFlat(stateBuf, f.action);
    stateBuf.push_back(f.lrscale);
    stateBuf.push_back(f.rewardscale);
    stateBuf.push_back(f.noise);
    stateBuf.push_back(static_cast<float>(f.traindivisor));
    stateBuf.push_back(static_cast<float>(f.storemode));
    return static_cast<int>(stateBuf.size());
}
EMSCRIPTEN_KEEPALIVE float* celium_state_data() { return stateBuf.data(); }
EMSCRIPTEN_KEEPALIVE float* celium_state_alloc(int n) {
    stateBuf.assign(n > 0 ? n : 0, 0.f);
    return stateBuf.data();
}

// Loads what celium_state_alloc's buffer holds. Returns 1 on success.
EMSCRIPTEN_KEEPALIVE int celium_state_load() {
    const float* d = stateBuf.data();
    const size_t len = stateBuf.size();
    size_t pos = 0;
    if (len < 3) return 0;
    nisps_flat::Flat f;
    f.version = static_cast<int>(d[pos++]);
    f.inputs = static_cast<long>(d[pos++]);
    f.outputs = static_cast<long>(d[pos++]);
    std::vector<float> shape;
    if (!popFlat(d, len, pos, shape)) return 0;
    f.layerShape.assign(shape.begin(), shape.end());
    if (!popFlat(d, len, pos, f.weights) || !popFlat(d, len, pos, f.biases)) return 0;
    if (pos >= len) return 0;
    f.memoryCount = static_cast<long>(d[pos++]);
    if (!popFlat(d, len, pos, f.memInputs) || !popFlat(d, len, pos, f.memActions) ||
        !popFlat(d, len, pos, f.memRewards) || !popFlat(d, len, pos, f.memAges) ||
        !popFlat(d, len, pos, f.input) || !popFlat(d, len, pos, f.action)) return 0;
    if (pos + 5 > len) return 0;
    f.lrscale = d[pos++];
    f.rewardscale = d[pos++];
    f.noise = d[pos++];
    f.traindivisor = static_cast<long>(d[pos++]);
    f.storemode = static_cast<long>(d[pos++]);

    NISPSState st;
    std::string error;
    if (!nisps_flat::unflatten(f, st, error)) return 0;
    if (!core->restoreState(st)) return 0;
    // The knobs stay where the page has them, not where the saved model had them.
    core->setLearningRateScale(learnKnob);
    core->setNoiseLevel(exploreKnob);
    sent.clear();
    return 1;
}

}  // extern "C"
