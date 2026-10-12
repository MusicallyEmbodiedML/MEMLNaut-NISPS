// Drives MEMLNautModule outside Rack (linked against the SDK's libRack): ticks the
// engine, plays gestures through buttons and gate inputs, checks the outputs, the
// screen snapshot, slots and the patch JSON round trip, and times process().
//   make RACK_DIR=... test
#include <chrono>
#include <cmath>
#include <cstdio>

#include "../src/MEMLNaut.hpp"

using namespace memlnaut;
using M = MEMLNautModule;

static int failures = 0;
static void check(bool ok, const char* what) {
    std::printf("%-44s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

static Module::ProcessArgs args() {
    Module::ProcessArgs a;
    a.sampleRate = 48000.f;
    a.sampleTime = 1.f / 48000.f;
    a.frame = 0;
    return a;
}

static void run(M& m, int samples) {
    auto a = args();
    for (int i = 0; i < samples; ++i) m.process(a);
}

// What the engine does when a cable goes in (setChannels() is a no-op on an empty port).
static void plug(Port& p, int channels = 1) { p.channels = static_cast<uint8_t>(channels); }
static void unplug(Port& p) { p.channels = 0; }
static void connect(Port& p, float v) {
    plug(p);
    p.setVoltage(v);
}

static void press(M& m, int param) {
    m.params[param].setValue(1.f);
    run(m, 48);
    m.params[param].setValue(0.f);
    run(m, 480);
}

static ScreenData snap(M& m) {
    std::lock_guard<std::mutex> lock(m.snapMutex);
    return m.snap;
}

int main() {
    M m;
    for (int o = 0; o < M::OUTPUTS_LEN; ++o) plug(m.outputs[o]);
    run(m, 48000);

    bool inRange = true;
    for (int i = 0; i < kNumOutputs; ++i) {
        const float v = m.outputs[M::NN_OUTPUT + i].getVoltage();
        inRange &= std::isfinite(v) && v >= 0.f && v <= 10.f;
    }
    check(inRange, "outputs in 0..10 V");
    check(m.outputs[M::POLY_A_OUTPUT].getChannels() == 16 && m.outputs[M::POLY_B_OUTPUT].getChannels() == 16,
          "poly outs carry 16 channels each");
    bool polyMatch = true;
    for (int i = 0; i < kNumOutputs; ++i)
        polyMatch &= m.outputs[i < 16 ? M::POLY_A_OUTPUT : M::POLY_B_OUTPUT].getVoltage(i & 15) ==
                     m.outputs[M::NN_OUTPUT + i].getVoltage();
    check(polyMatch, "poly channels match the mono outs");

    // The pad moves the sound.
    const float before = m.outputs[M::NN_OUTPUT].getVoltage();
    float maxMove = 0.f;
    m.params[M::PAD_X_PARAM].setValue(0.05f);
    m.params[M::PAD_Y_PARAM].setValue(0.95f);
    run(m, 4800);
    for (int i = 0; i < kNumOutputs; ++i) maxMove = std::max(maxMove, std::fabs(m.outputs[M::NN_OUTPUT + i].getVoltage() - before));
    check(maxMove > 0.01f, "moving the pad changes the outputs");
    check(std::fabs(snap(m).input[0] - 0.05f) < 1e-6f, "snapshot shows the pad position");

    // CV overrides the pad.
    connect(m.inputs[M::X_INPUT], 7.5f);
    run(m, 480);
    check(std::fabs(snap(m).input[0] - 0.75f) < 1e-6f && snap(m).xyFromCV, "X CV overrides the pad (7.5 V -> 0.75)");
    m.inputRange = 1;
    run(m, 480);
    check(std::fabs(snap(m).input[0] - 1.f) < 1e-6f, "bipolar input range (7.5 V clamps to 1)");
    m.inputRange = 0;
    unplug(m.inputs[M::X_INPUT]);

    // Gestures through the buttons.
    press(m, M::LIKE_PARAM);
    check(snap(m).likes == 1 && snap(m).action_ == "yes", "YES button stores a like");
    m.params[M::PAD_X_PARAM].setValue(0.8f);
    run(m, 480);
    press(m, M::DISLIKE_PARAM);
    check(snap(m).dislikes == 1 && snap(m).action_ == "no", "NO button stores a dislike");
    press(m, M::JOLT_PARAM);
    run(m, 48 * 20 * 6); // two memory-point refreshes
    int jolts = 0;
    for (int i = 0; i < snap(m).memCount; ++i) jolts += snap(m).mem[i].kind == 2;
    check(jolts == 8 && snap(m).action_ == "jolt", "JOLT plants 8 points on the map");

    // A 1 ms trigger on a gate input is not missed.
    const size_t likes = snap(m).likes;
    m.params[M::PAD_X_PARAM].setValue(0.3f);
    m.params[M::PAD_Y_PARAM].setValue(0.2f);
    run(m, 480);
    connect(m.inputs[M::LIKE_INPUT], 10.f);
    run(m, 48);
    m.inputs[M::LIKE_INPUT].setVoltage(0.f);
    run(m, 480);
    check(snap(m).likes == likes + 1, "1 ms trigger on the YES gate is caught");

    // Drag: hold the sound, move, release -> a like at the new place.
    m.params[M::DRAG_PARAM].setValue(1.f);
    run(m, 480);
    const float held = m.outputs[M::NN_OUTPUT + 3].getVoltage();
    m.params[M::PAD_X_PARAM].setValue(0.6f);
    m.params[M::PAD_Y_PARAM].setValue(0.65f);
    run(m, 4800);
    check(std::fabs(m.outputs[M::NN_OUTPUT + 3].getVoltage() - held) < 1e-4f && snap(m).dragging, "DRAG holds the sound while moving");
    m.params[M::DRAG_PARAM].setValue(0.f);
    run(m, 480);
    check(snap(m).action_ == "drop" && !snap(m).dragging, "releasing DRAG drops it as a like");

    // Bipolar output range.
    m.params[M::RANGE_PARAM].setValue(1.f);
    run(m, 480);
    bool bip = true;
    for (int i = 0; i < kNumOutputs; ++i) {
        const float v = m.outputs[M::NN_OUTPUT + i].getVoltage();
        bip &= v >= -5.f && v <= 5.f;
    }
    check(bip, "RANGE switch gives -5..5 V");
    m.params[M::RANGE_PARAM].setValue(0.f);

    // Explore noise moves a still input.
    m.params[M::EXPLORE_PARAM].setValue(1.f);
    run(m, 480);
    const float e0 = m.outputs[M::NN_OUTPUT + 5].getVoltage();
    run(m, 48000);
    check(std::fabs(m.outputs[M::NN_OUTPUT + 5].getVoltage() - e0) > 1e-3f && snap(m).noiseActive, "EXPLORE drifts the sound");
    m.params[M::EXPLORE_PARAM].setValue(0.f);

    // Dislike modes from the setting.
    bool allReport = true;
    for (int mode = 0; mode < 5; ++mode) {
        m.dislikeMode = mode;
        m.params[M::PAD_X_PARAM].setValue(0.1f + 0.15f * mode);
        run(m, 480);
        const unsigned long s0 = snap(m).eventSerial;
        press(m, M::DISLIKE_PARAM);
        allReport &= snap(m).eventSerial != s0 && snap(m).action_ == "no";
    }
    check(allReport, "every dislike mode acts and reports");

    // Slots: save, change, load back.
    m.params[M::PAD_X_PARAM].setValue(0.4f);
    m.params[M::PAD_Y_PARAM].setValue(0.4f);
    run(m, 48000); // let dislike targets retire
    m.trainOnDemand = false;
    m.params[M::LEARN_PARAM].setValue(0.f); // freeze learning so outputs only depend on the weights
    run(m, 480);
    m.slotRequest = 0 * kNumSlots + 3;
    run(m, 480);
    check(snap(m).slotMask == (1u << 3) && snap(m).currentSlot == 3, "save to slot 4");
    float saved[kNumOutputs];
    for (int i = 0; i < kNumOutputs; ++i) saved[i] = m.outputs[M::NN_OUTPUT + i].getVoltage();
    press(m, M::SCRAMBLE_PARAM);
    run(m, 480);
    m.slotRequest = 1 * kNumSlots + 3;
    run(m, 4800);
    float diff = 0.f;
    for (int i = 0; i < kNumOutputs; ++i) diff = std::max(diff, std::fabs(m.outputs[M::NN_OUTPUT + i].getVoltage() - saved[i]));
    check(diff < 1e-3f, "load slot 4 brings the sound back after a scramble");

    // Patch JSON round trip into a fresh module.
    json_t* j = m.dataToJson();
    M m2;
    for (int o = 0; o < M::OUTPUTS_LEN; ++o) plug(m2.outputs[o]);
    m2.params[M::PAD_X_PARAM].setValue(0.4f);
    m2.params[M::PAD_Y_PARAM].setValue(0.4f);
    m2.params[M::LEARN_PARAM].setValue(0.f);
    m2.dataFromJson(j);
    json_decref(j);
    run(m2, 4800);
    diff = 0.f;
    for (int i = 0; i < kNumOutputs; ++i)
        diff = std::max(diff, std::fabs(m2.outputs[M::NN_OUTPUT + i].getVoltage() - m.outputs[M::NN_OUTPUT + i].getVoltage()));
    check(diff < 1e-3f, "patch JSON restores the model");
    check(snap(m2).slotMask == (1u << 3) && m2.dislikeMode == m.dislikeMode && snap(m2).likes == snap(m).likes,
          "patch JSON restores slots, settings, memory");

    // CPU: mean and worst process() call (ticks included) with memory full.
    m.params[M::LEARN_PARAM].setValue(1.f);
    for (int i = 0; i < 70; ++i) {
        m.params[M::PAD_X_PARAM].setValue((i * 37 % 100) / 100.f);
        m.params[M::PAD_Y_PARAM].setValue((i * 61 % 100) / 100.f);
        run(m, 480);
        press(m, i % 3 ? M::LIKE_PARAM : M::DISLIKE_PARAM);
    }
    m.params[M::EXPLORE_PARAM].setValue(0.5f);
    press(m, M::JOLT_PARAM);
    auto a = args();
    double worst = 0.0, total = 0.0;
    const int N = 48000 * 5;
    for (int i = 0; i < N; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        m.process(a);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        total += us;
        worst = std::max(worst, us);
    }
    const double meanUs = total / N;
    std::printf("CPU: mean %.3f us/sample (%.1f%% of one core at 48 kHz), worst call %.1f us, memory %zu\n", meanUs,
                meanUs * 48000.0 / 1e6 * 100.0, worst, snap(m).likes + snap(m).dislikes);

    std::printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
