// NISPSCore — portable RL engine behind the NISPS SuperCollider UGen.
//
// Reimplements the algorithm in MEMLNaut-NISPS's InterfaceRL<N> (see
// src/memllib/examples/InterfaceRL.{hpp,tpp} in that repo), stripped of every
// Arduino/display/MIDI/persistence dependency and sized at construction time
// by a runtime n_inputs/n_outputs pair rather than a compile-time template
// (the embedded version fixes the width at compile time via StaticMLP; this
// uses memlp's dynamic MLP so one plugin instance handles any width).
//
// Threading model: the embedded version needs a spin_lock_t because its ML
// loop (Core 0) and audio loop (Core 1) run concurrently on separate cores.
// A SuperCollider UGen has no such split — every inbound control value and
// every command a client sends is already serialized with calc() on the
// single audio thread — so no locking is needed here. The caller (the SCUnit
// wrapper) is expected to call every method below from that same thread.
#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "MLP.h"
#include "OrnsteinUhlenbeckNoise.h"
#include "ReplayMemory.hpp"

class NISPSCore {
public:
    enum class MemoryStoreMode {
        ADD,
        REPLACE_5_PERCENT,
        REPLACE_10_PERCENT,
        REPLACE_15_PERCENT,
        REWARD_DECAY_10_PERCENT,
        REWARD_DECAY_20_PERCENT
    };

    NISPSCore(size_t n_inputs, size_t n_outputs);

    size_t numInputs() const { return n_inputs_; }
    size_t numOutputs() const { return n_outputs_; }

    // --- input / output ---
    void setInput(size_t index, float value);
    void setInputs(const std::vector<float>& values);
    const std::vector<float>& getInput() const { return controlInput_; }
    const std::vector<float>& getAction() const { return action_; }
    // Number of like/dislike experiences currently held (0..kMemoryLimit).
    size_t memorySize() const { return replayMem_.size(); }

    // Advance the engine's notion of wall-clock time by the elapsed
    // milliseconds since the previous call. Replaces the embedded code's
    // calls to millis() — the caller (SCUnit wrapper) derives this from its
    // control-block duration.
    void advanceClock(double elapsedMs) { nowMs_ += elapsedMs; }

    // Mirrors InterfaceRL::optimiseSometimes() + generateAction(): call once
    // per control block, after advanceClock()/setInput(s)/any gestures.
    void process();

    // --- gestures (edge-detected by the caller) ---
    void like();
    void dislike();
    void randomiseNetwork();
    void clearMemory();

    // Jolt is a *gate*: call startJolt() on the rising edge, stepJolt() every
    // block while held, stopJolt() on the falling edge — mirrors the
    // embedded hardware's momentary-button-held behavior.
    void startJolt();
    void stepJolt();
    void stopJolt();
    bool isJoltActive() const { return joltActive_; }

    // --- continuous controls ---
    void setLearningRateScale(float scale) { learningRateScaled_ = learningRate_ * scale; }
    void setRewardScale(float scale) { rewardScale_ = scale; }
    // level is the [0,1] "noise" knob; internally scaled to an OU stationary
    // std the same way the embedded RV Z1 knob is (see setNoiseLevel in
    // InterfaceRL.hpp) — 0 turns exploration noise off.
    void setNoiseLevel(float level);
    // SC's control rate is normally much faster than the embedded 200 Hz
    // training loop; this divisor throttles optimise() to every Nth call to
    // process(), independent of the plugin's control-block rate.
    void setOptimiseDivisor(size_t divisor) { optimiseDivisor_ = divisor ? divisor : 1; }
    void setMemoryStoreMode(MemoryStoreMode mode) { memoryStoreMode_ = mode; }

private:
    struct TrainItem {
        std::vector<float> input;
        std::vector<float> action;
        float reward;
    };

    void optimise();
    void generateAction(bool force);
    void storeExperience(float reward, const std::vector<float>& state, const std::vector<float>& takenAction);
    bool removeItemsAtDistance(const std::vector<float>& state, float distThreshold, float reward);
    void decayItemsAtDistance(const std::vector<float>& state, float distThreshold);
    float randomJoltTarget() const;

    size_t n_inputs_;
    size_t n_outputs_;

    MLP<float> mlp_;
    ReplayMemory<TrainItem> replayMem_;
    std::vector<std::unique_ptr<OrnsteinUhlenbeckNoise>> ouNoises_;

    std::vector<float> controlInput_;
    std::vector<float> action_;
    std::vector<float> mappingOutput_;
    bool newInput_ = true;

    double nowMs_ = 0.0;

    float learningRate_ = 1e-3f;
    float learningRateScaled_ = learningRate_;
    float rewardScale_ = 1.0f;

    size_t optimiseDivisor_ = 1;
    size_t optimiseCounter_ = 0;

    std::vector<size_t> itemsToRemove_;
    MemoryStoreMode memoryStoreMode_ = MemoryStoreMode::REPLACE_10_PERCENT;

    // activeDims_ empty == every output dimension participates in the
    // geometric push (matches InterfaceRL's activeDims_ semantics). Not
    // wired to a control input in v1 — kept for a future "focus" input.
    std::vector<bool> activeDims_;

    // Jolt state. The embedded code picks random weights via a flat
    // SynthMLP::WeightPtrAt(i) view over a fixed-size StaticMLP. The dynamic
    // MLP used here exposes weights per layer instead
    // (MLP::GetLayerRef(i).weight(node,input)), so a jolt slot is an explicit
    // (layer, node, input) triple rather than one flat index.
    struct JoltWeightRef {
        size_t layer;
        size_t node;
        size_t input;
    };
    std::vector<JoltWeightRef> joltWeightLoc_;
    std::vector<float> joltTarget_;
    bool joltActive_ = false;
    float joltLRRamp_ = 1.0f;

    static constexpr size_t kMemoryLimit = 64;
    static constexpr size_t kBatchSize = 8;
    static constexpr size_t kCentroidK = 4;
    static constexpr float kGeometricPushScale = 1.0f;
    static constexpr float kNegLRBase = 1.5f;
    static constexpr double kDislikeLifetimeMs = 2500.0;

    // OU noise smoothness (see InterfaceRL.hpp): correlation time ~=
    // 1/(theta*dt) calls, independent of amplitude (set via setNoiseLevel).
    static constexpr float kOuTheta = 0.02f;
    static constexpr float kOuDt = 0.004f;
    static constexpr float kNoiseMaxAmplitude = 0.65f;

    static constexpr size_t kJoltNumWeights = 40;
    static constexpr float kJoltMorphRate = 0.017f;
    static constexpr float kJoltWeightMin = -1.2f;
    static constexpr float kJoltWeightMax = 0.9f;
    static constexpr float kJoltTargetEpsilon = 0.05f;
    static constexpr float kJoltLRRampStep = 0.001f;
};
