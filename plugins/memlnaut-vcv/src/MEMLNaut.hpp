// The MEMLNaut as a VCV Rack module: the NISPS engine (../nisps-sc NISPSCore, the
// firmware's InterfaceRL) mapping 4 inputs to 32 CV outputs, with the hardware's
// screen redrawn in NanoVG.
//
// Threads: process() runs the engine on the audio thread, ticking it every 5 ms like
// the firmware's 200 Hz loop. The screen (UI thread) never touches the engine. It
// reads a ScreenData snapshot the audio thread publishes under snapMutex (try_lock,
// so audio never waits), and talks back only through params, plain int settings that
// process() applies when they change, and the one-slot slotRequest. Rack itself keeps
// dataToJson / dataFromJson / onReset off the audio thread's toes (Engine locks).
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "plugin.hpp"

class NISPSCore;
struct NISPSState;

namespace memlnaut {

static constexpr int kNumOutputs = 32;
static constexpr int kNumInputs = 4;     // X, Y, Z, W: the firmware's 4D joystick source
static constexpr int kNetInputs = 10;    // the firmware's padded 10-input network
static constexpr int kNumSlots = 12;     // the firmware's save/load slots
static constexpr int kLossColumns = 60;  // RL screen loss trace: 60 columns...
static constexpr double kLossColumnMs = 200.0; // ...of 200 ms = 12 s of history
static constexpr int kMaxMemPoints = 64 + 8;   // replay memory + jolt points

struct MemPoint {
    float x, y, z, w;
    uint8_t kind; // 0 like, 1 dislike, 2 jolt
};

// Everything the screen draws, copied out of the audio thread.
struct ScreenData {
    std::array<float, kNumOutputs> action {};
    std::array<float, kNumInputs> input {};
    bool xyFromCV = false; // X/Y come from cables, not the on-screen pad
    size_t likes = 0, dislikes = 0;
    std::array<float, kLossColumns> lossLog {}; // log10(mean loss) per column; NaN = none
    int lossCursor = 0;
    float lastLoss = -1.f; // mean loss of the newest column (-1 = none yet)
    bool trainingIdle = false;
    bool noiseActive = false;
    bool dragging = false;
    std::string action_; // last gesture word ("yes", "no", "jolt", ...)
    std::string message;
    unsigned long eventSerial = 0;
    std::array<MemPoint, kMaxMemPoints> mem {};
    int memCount = 0;
    uint32_t slotMask = 0; // occupied slots
    int currentSlot = -1;
};

struct MEMLNautModule : Module {
    enum ParamId {
        LIKE_PARAM,
        DISLIKE_PARAM,
        JOLT_PARAM,
        SCRAMBLE_PARAM,
        DRAG_PARAM,
        FORGET_PARAM,
        LEARN_PARAM,
        REWARD_PARAM,
        EXPLORE_PARAM,
        Z_PARAM,
        W_PARAM,
        RANGE_PARAM,
        PAD_X_PARAM, // the on-screen pad (no panel control; MIDI-mappable)
        PAD_Y_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        LIKE_INPUT,
        DISLIKE_INPUT,
        JOLT_INPUT,
        SCRAMBLE_INPUT,
        DRAG_INPUT,
        FORGET_INPUT,
        X_INPUT,
        Y_INPUT,
        Z_INPUT,
        W_INPUT,
        EXPLORE_INPUT,
        LEARN_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(NN_OUTPUT, kNumOutputs),
        POLY_A_OUTPUT, // outputs 1-16
        POLY_B_OUTPUT, // outputs 17-32
        OUTPUTS_LEN
    };
    enum LightId {
        LIKE_LIGHT,
        DISLIKE_LIGHT,
        JOLT_LIGHT,
        SCRAMBLE_LIGHT,
        DRAG_LIGHT,
        FORGET_LIGHT,
        LIGHTS_LEN
    };
    static constexpr int kNumGestures = 6; // LIKE..FORGET, matching params/inputs/lights

    // Settings the UI may write; process() applies them when they change.
    int dislikeMode = 0;
    int storeMode = 2; // REPLACE_10_PERCENT
    bool trainOnDemand = false;
    int inputRange = 0; // CV inputs: 0 = 0..10 V, 1 = -5..5 V
    // A slot click from the screen: op * kNumSlots + slot, or -1. op 0 save, 1 load.
    std::atomic<int> slotRequest { -1 };

    std::mutex snapMutex;
    ScreenData snap; // guarded by snapMutex

    MEMLNautModule();
    ~MEMLNautModule() override;

    void process(const ProcessArgs& args) override;
    void onReset(const ResetEvent& e) override;
    void onRandomize(const RandomizeEvent& e) override;
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;

private:
    void newEngine();
    void tick(double elapsedMs);
    void applySettings();
    void handleSlotRequest();
    void publish();
    void postMessage(const char* action, const std::string& text);
    float readCV(int input, float fallback);

    std::unique_ptr<NISPSCore> core_;
    std::array<std::unique_ptr<NISPSState>, kNumSlots> slots_; // empty = free slot
    int currentSlot_ = -1;

    // applied settings (to spot changes)
    int appliedDislikeMode_ = -1;
    int appliedStoreMode_ = -1;
    int appliedTrainOnDemand_ = -1;

    // gestures: edges caught per sample, acted on at the next tick
    std::array<dsp::SchmittTrigger, kNumGestures> gateTriggers_;
    std::array<dsp::BooleanTrigger, kNumGestures> buttonTriggers_;
    std::array<bool, kNumGestures> pending_ {};
    std::array<float, kNumGestures> flash_ {};
    bool dragHeld_ = false;

    // tick timing and the output ramp between ticks
    int sampleCounter_ = 0;
    int tickSamples_ = 1;
    std::array<float, kNumOutputs> out_ {};
    std::array<float, kNumOutputs> outTarget_ {};
    std::array<float, kNumOutputs> outStep_ {};

    // RL screen loss trace
    std::array<float, kLossColumns> lossLog_ {};
    int lossCursor_ = 0;
    double lossSum_ = 0.0;
    int lossCount_ = 0;
    double lossColumnMs_ = 0.0;
    float lastLossMean_ = -1.f;
    unsigned long long seenLossPasses_ = 0;

    // event line
    unsigned long seenCoreSerial_ = 0;
    std::string msgAction_, msgText_;
    unsigned long msgSerial_ = 0;

    int memRefreshCounter_ = 0;
    ScreenData local_; // built here, copied into snap
};

} // namespace memlnaut
