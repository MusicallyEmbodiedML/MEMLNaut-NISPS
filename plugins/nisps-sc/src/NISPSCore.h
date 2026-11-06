// NISPSCore — portable RL engine behind the NISPS SuperCollider UGen, the
// nisps Max object and the MEMLNaut VCV Rack module.
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
//
// Time: every millis() in the firmware becomes the engine clock, advanced by
// the caller through advanceClock(). The firmware's control loop runs at
// 200 Hz; process() is meant to be called at about that rate, though the
// time-based parts (exploration drift, dislike lifetimes, the jolt burst)
// follow the clock rather than the call rate.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "MLP.h"
#include "OrnsteinUhlenbeckNoise.h"
#include "ReplayMemory.hpp"

// A complete, portable snapshot of an engine's learned state: the network
// (weights AND biases - memlp's own MLP::Serialise() covers only weights, see
// captureState) plus the replay memory and the continuous controls. Plain
// data, no file format and no host dependency: NISPS.cpp / nisps.cpp decide
// how to store it.
struct NISPSState {
    static constexpr int kVersion = 1;

    struct Layer {
        size_t inputs = 0;              // inputs per node
        size_t nodes = 0;               // nodes in this layer
        std::vector<float> weights;     // nodes * inputs, node-major
        std::vector<float> biases;      // nodes
    };

    struct MemoryItem {
        std::vector<float> input;
        std::vector<float> action;
        float reward = 0.f;
        // Milliseconds this item had already lived when it was captured.
        // Stored as an age rather than a timestamp because the engine clock
        // restarts at zero in a new instance.
        double ageMs = 0.0;
    };

    int version = kVersion;
    size_t n_inputs = 0;   // the network's input width (see NISPSCore::netInputs)
    size_t n_outputs = 0;
    std::vector<Layer> layers;
    std::vector<MemoryItem> memory;

    // The current position and its mapped action, so a restored engine picks
    // up where it left off - like/dislike act on exactly this pair.
    std::vector<float> input;
    std::vector<float> action;

    // Continuous controls, as last set through the setters below.
    float learningRateScale = 1.f;
    float rewardScale = 1.f;
    float noiseLevel = 0.f;
    size_t optimiseDivisor = 1;
    int memoryStoreMode = static_cast<int>(2); // REPLACE_10_PERCENT
};

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

    // What a 'no' means (InterfaceRLBase::DISLIKE_MODES).
    //  CURRENT        an unbounded push away from the liked centroid
    //  BOUNDED_BLAME  move a bounded distance away, mostly reverting the params that differ
    //                 most from the nearest like
    //  REROLL         jump to a fresh sound here
    //  ESCALATE       repeated 'no's in one place: nudge, then back to a like, then re-roll
    //  BORROW         rebuild the sound here from qualities of nearby liked sounds
    enum class DislikeMode : uint8_t {
        CURRENT = 0,
        BOUNDED_BLAME,
        REROLL,
        ESCALATE,
        BORROW,
        COUNT
    };
    static const char* dislikeModeName(DislikeMode m);

    // n_inputs: the inputs the caller drives. n_net_inputs: the network's input width,
    // when wider than that (0 = the same). The firmware runs a 10-input net whatever the
    // input source, padding the unused inputs with a constant (see updateUnusedInputDefault
    // in InterfaceRL.hpp), which shapes how the mapping bends; pass 10 to match it.
    NISPSCore(size_t n_inputs, size_t n_outputs, size_t n_net_inputs = 0);

    size_t numInputs() const { return n_inputs_; }
    size_t netInputs() const { return n_net_inputs_; }
    size_t numOutputs() const { return n_outputs_; }

    // --- input / output ---
    void setInput(size_t index, float value);
    void setInputs(const std::vector<float>& values);
    const std::vector<float>& getInput() const { return controlInput_; }  // netInputs() wide
    const std::vector<float>& getAction() const { return action_; }
    // Number of like/dislike experiences currently held (0..kMemoryLimit).
    size_t memorySize() const { return replayMem_.size(); }
    size_t likeCount() const;
    // Disliked memories plus the live targets of the non-CURRENT dislike modes, the
    // "no" half of the firmware's RL screen counter.
    size_t dislikeCount() const { return replayMem_.size() - likeCount() + dislikeTargets_.size(); }

    // Mean training loss (MSE) from the most recent optimise() pass, with a
    // counter of how many passes have run: a caller polling at its own rate
    // uses the counter to tell a fresh value from the previous one, since a
    // loss that happens to repeat is not the same as no training at all.
    float lastTrainingError() const { return lastTrainingError_; }
    unsigned long long trainingPasses() const { return trainingPasses_; }
    // The loss on the liked memories alone (what the firmware's RL screen plots),
    // with its own pass counter.
    float lastLikeLoss() const { return lastLikeLoss_; }
    unsigned long long likeLossPasses() const { return likeLossPasses_; }

    // Advance the engine's notion of wall-clock time by the elapsed
    // milliseconds since the previous call. Replaces the embedded code's
    // calls to millis() — the caller (SCUnit wrapper) derives this from its
    // control-block duration.
    void advanceClock(double elapsedMs) { nowMs_ += elapsedMs; }
    double nowMs() const { return nowMs_; }

    // Mirrors InterfaceRL::optimiseSometimes() + generateAction(): call once
    // per control block, after advanceClock()/setInput(s)/any gestures.
    void process();

    // --- gestures (edge-detected by the caller) ---
    void like();
    void dislike();
    void randomiseNetwork();
    void clearMemory();
    // Jolt (the firmware's A2): plant a fresh set of synthetic pseudo-likes, spread
    // through the input range away from the likes, with target sounds far from what the
    // net plays there now, and train hard towards them. One-shot.
    void jolt();
    // Drag (the joystick switch / toggle B1): hold the current sound while the input
    // moves, then store it as a like wherever the input is on release.
    void startDrag();
    void endDrag();
    bool isDragging() const { return dragging_; }

    // Jolt used to be a held gate that morphed the weights; the firmware's jolt is now
    // a one-shot. These keep the gate API: startJolt() jolts, the rest only track it.
    void startJolt() { joltHeld_ = true; jolt(); }
    void stepJolt() {}
    void stopJolt() { joltHeld_ = false; }
    bool isJoltActive() const { return joltHeld_; }

    // --- continuous controls ---
    void setLearningRateScale(float scale) {
        learningRateScale_ = scale;
        learningRateScaled_ = learningRate_ * scale; // 0 -> training off (intended)
    }
    void setRewardScale(float scale) { rewardScale_ = scale; }
    // level is the [0,1] "noise" knob (the firmware's RV Z1): the depth of the
    // weight-space exploration noise. 0 turns it off.
    void setNoiseLevel(float level);
    float noiseLevel() const { return noiseLevel_; }
    bool noiseActive() const { return noiseAmp_ > 0.f; }
    // SC's control rate is normally much faster than the embedded 200 Hz
    // training loop; this divisor throttles optimise() to every Nth call to
    // process(), independent of the plugin's control-block rate.
    void setOptimiseDivisor(size_t divisor) { optimiseDivisor_ = divisor ? divisor : 1; }
    void setMemoryStoreMode(MemoryStoreMode mode) { memoryStoreMode_ = mode; }
    void setDislikeMode(DislikeMode m);
    DislikeMode dislikeMode() const { return dislikeMode_; }
    // Focus: which outputs the gestures may change (empty = all). See the focus notes
    // in InterfaceRL.hpp: with some outputs frozen, a gesture is a local edit of the rest.
    void setActiveDims(std::vector<bool> dims);
    const std::vector<bool>& activeDims() const { return activeDims_; }
    // Train on demand: stop training once the liked error settles, wake when it rises
    // or the data changes. Off by default, as in the firmware.
    void setTrainOnDemand(bool on) { trainOnDemand_ = on; wakeTraining(); }
    bool trainOnDemand() const { return trainOnDemand_; }
    bool isTrainingSettled() const { return trainOnDemand_ && trainSettled_; }

    // --- feedback for a UI ---
    // What the last gesture did, as the firmware's screens report it: an action word
    // ("yes", "no", "jolt", "scramble", "forget", "drag", "drop") and a message line.
    // The serial counts up on every event, so a repeated word still registers.
    struct Event {
        std::string action;
        std::string message;
        unsigned long serial = 0;
    };
    const Event& lastEvent() const { return event_; }

    // Positions of the memories in input space (first two inputs and on), for a map.
    struct MemoryPoint {
        std::vector<float> input;
        enum class Kind : uint8_t { LIKE, DISLIKE, JOLT } kind;
    };
    void memoryPoints(std::vector<MemoryPoint>& out) const;

    // --- persistence ---
    // Snapshot every piece of learned state, and put one back. restoreState()
    // refuses a snapshot whose topology does not match this engine (different
    // input/output width or layer shape) rather than loading it partially;
    // it returns false and leaves the engine untouched. On success the next
    // process() regenerates the action from the restored network.
    void captureState(NISPSState& out);
    bool restoreState(const NISPSState& in);

private:
    struct TrainItem {
        std::vector<float> input;
        std::vector<float> action;
        float reward;
    };
    using training_pair_t = MLP<float>::training_pair_t;

    void optimise();
    void generateAction(bool force);
    void storeExperience(float reward, const std::vector<float>& state, const std::vector<float>& takenAction);
    bool removeItemsAtDistance(const std::vector<float>& state, float distThreshold, float reward);
    void decayItemsAtDistance(const std::vector<float>& state, float distThreshold);
    void post(const char* action, const std::string& message);

    bool isActiveDim(size_t j) const {
        return activeDims_.empty() || (j < activeDims_.size() && activeDims_[j]);
    }
    bool focusActive() const {
        for (bool a : activeDims_)
            if (!a) return true;
        return false;
    }
    float inputScale() const; // sqrt(active inputs), for per-sqrt-dim radii
    bool nearCurrentInput(const std::vector<float>& x) const;
    const std::vector<float>& likeTarget(size_t i);

    // Train on demand
    void wakeTraining();
    float likedError();
    void updateTrainState(bool pending);

    // Dislike modes other than CURRENT: bounded, self-retiring training targets.
    struct DislikeTarget {
        enum class Kind : uint8_t { REPEL, SEEK };
        std::vector<float> input;
        std::vector<float> origin; // the disliked output
        std::vector<float> target;
        double t0;
        Kind kind; // REPEL: done once kRepelMargin from origin; SEEK: within kSeekEpsilon of target
    };
    float rmsDistActive(const std::vector<float>& a, const std::vector<float>& b) const;
    int nearestLikeIndex(const std::vector<float>& input) const;
    std::vector<float> stepTarget(const std::vector<float>& origin, std::vector<float> dir, float rmsStep) const;
    std::vector<float> randomUnitDir(size_t n) const;
    void addDislikeTarget(DislikeTarget&& t);
    void removeEndorsingLikes();
    void dislikeNudge(bool blame);
    void dislikeRetreat();
    void dislikeReroll();
    void dislikeBorrow();
    void handleDislike();
    void trainDislikeTargets(float lr);

    // Exploration noise: the last layer perturbed along fixed random weight directions.
    void initExploreNoise();
    float noiseLocality(const std::vector<float>& x);
    void rebuildNoiseW();
    void noisyForward(std::vector<float>& out);
    void jumpNoise();

    // Jolt: synthetic pseudo-likes
    struct SynthPoint {
        std::vector<float> input;
        std::vector<float> target;
    };
    void shiftSynthTargets();
    void rollSynthTarget(SynthPoint& sp);
    bool rerollNearbySynth();
    bool trainSynth(float lr);
    float synthError();

    size_t n_inputs_;
    size_t n_net_inputs_;
    size_t n_outputs_;
    float unusedInputDefault_ = 0.f; // pad value for the net inputs beyond n_inputs_

    MLP<float> mlp_;
    ReplayMemory<TrainItem> replayMem_;

    std::vector<float> controlInput_;
    std::vector<float> action_;
    std::vector<float> mappingOutput_;
    std::vector<float> savedAction_; // held while dragging
    std::vector<float> evalOut_;
    std::vector<float> likeTargetBuf_;
    bool newInput_ = true;
    bool dragging_ = false;
    bool joltHeld_ = false;

    double nowMs_ = 0.0;

    float learningRate_ = 1e-3f;
    float learningRateScale_ = 1.0f; // as given to setLearningRateScale, for captureState
    float learningRateScaled_ = learningRate_;
    float rewardScale_ = 1.0f;
    float noiseLevel_ = 0.0f;        // as given to setNoiseLevel, for captureState
    float noiseAmp_ = 0.0f;

    size_t optimiseDivisor_ = 1;
    size_t optimiseCounter_ = 0;

    float lastTrainingError_ = 0.f;
    unsigned long long trainingPasses_ = 0;
    float lastLikeLoss_ = 0.f;
    unsigned long long likeLossPasses_ = 0;

    std::vector<size_t> itemsToRemove_;
    MemoryStoreMode memoryStoreMode_ = MemoryStoreMode::REPLACE_10_PERCENT;
    std::vector<bool> activeDims_;

    Event event_;

    // Dislike modes
    std::vector<DislikeTarget> dislikeTargets_;
    DislikeMode dislikeMode_ = DislikeMode::ESCALATE;  // the firmware's fixed mode
    size_t escalateStage_ = 0;
    double lastDislikeMs_ = 0.0;
    std::vector<float> lastDislikeInput_;

    // Exploration noise
    std::vector<OrnsteinUhlenbeckNoise> exploreOU_; // unit-std drift per direction
    std::vector<int8_t> weightDirs_;                // [kWeightDirs][n_outputs][hidden], +-1
    std::vector<float> noiseW_;                     // [n_outputs][hidden]
    size_t hidden_ = 0;                             // width of the last hidden layer
    double noiseWMs_ = 0.0;
    bool noiseWValid_ = false;
    std::vector<float> locInput_;
    size_t locMemSize_ = SIZE_MAX;
    float locValue_ = 1.f;
    bool locDirty_ = true;
    std::vector<float> h1_, h2_;

    // Jolt
    std::vector<SynthPoint> synth_;
    float synthSeedError_ = 0.f;
    bool synthBurst_ = false;
    double synthSeedMs_ = 0.0;
    double synthLastCheckMs_ = 0.0;

    // Train on demand
    bool trainOnDemand_ = false;
    bool trainSettled_ = false;
    int settleGoodChecks_ = 0;
    float plateauRefError_ = 1e9f;
    double plateauRefMs_ = 0.0;
    float settledError_ = 0.f;
    double lastErrorEvalMs_ = 0.0;

    static constexpr size_t kMemoryLimit = 64;
    static constexpr size_t kBatchSize = 8;
    static constexpr size_t kCentroidK = 4;
    static constexpr float kGeometricPushScale = 1.0f;
    static constexpr float kNegLRBase = 1.5f;
    static constexpr double kDislikeLifetimeMs = 2500.0;

    static constexpr float kTargetLo = 0.05f;
    static constexpr float kTargetHi = 0.95f;
    static constexpr float kRepelMargin = 0.12f;
    static constexpr float kRepelStep = 0.16f;
    static constexpr float kSeekEpsilon = 0.03f;
    static constexpr float kRetreatJitter = 0.03f;
    static constexpr float kRerollMinDist = 0.25f;
    static constexpr float kRerollSpread = 0.2f;
    static constexpr float kBorrowKeepFraction = 0.4f;
    static constexpr float kBorrowInputSigma = 0.3f;
    static constexpr float kBorrowJitter = 0.02f;
    static constexpr float kDislikeLR = 1.5f;
    static constexpr double kDislikeTargetLifetimeMs = 4000.0;
    static constexpr size_t kMaxDislikeTargets = 8;
    static constexpr float kDislikeInputRadius = 0.10f;
    static constexpr double kEscalateWindowMs = 6000.0;
    static constexpr float kEscalateInputRadius = 0.15f;

    static constexpr float kFocusEditRadius = 0.15f;
    static constexpr int kFocusLocalSeeds = 3;
    static constexpr float kFocusSeedJitter = 0.08f;

    static constexpr float kNoiseMaxAmplitude = 0.65f;
    static constexpr size_t kWeightDirs = 6;
    static constexpr float kNearFloor = 0.2f;
    static constexpr float kNearRadius = 0.25f;
    static constexpr double kNoiseRebuildMs = 50.0;
    static constexpr double kControlPeriodMs = 5.0;
    static constexpr float kNoiseTheta = 0.02f;
    static constexpr float kNoiseDt = 0.004f;
    static constexpr float kNoiseDislikeJump = 0.5f;

    static constexpr int kSynthPoints = 8;
    static constexpr int kSynthCandidates = 32;
    static constexpr float kSynthInputLo = 0.1f;
    static constexpr float kSynthInputHi = 0.9f;
    static constexpr float kSynthClearance = 0.2f;
    static constexpr float kSynthTargetLo = 0.02f;
    static constexpr float kSynthTargetHi = 0.98f;
    static constexpr float kJoltShiftMin = 0.25f;
    static constexpr float kJoltShiftMax = 0.5f;
    static constexpr float kSynthBurstLR = 3.f;
    static constexpr float kSynthBurstDone = 0.25f;
    static constexpr double kSynthBurstMaxMs = 8000.0;
    static constexpr float kSynthHoldLR = 1.f;
    static constexpr size_t kSynthLikeAnchors = 8;  // likes trained with each jolt step
    static constexpr float kSynthDislikeRadius = 0.2f;

    static constexpr float kSettleError = 1e-4f;
    static constexpr int kSettleChecks = 3;
    static constexpr float kPlateauGain = 0.005f;
    static constexpr double kPlateauWindowMs = 2000.0;
    static constexpr double kSettleEvalMs = 100.0;
    static constexpr float kWakeRatio = 1.5f;
    static constexpr float kWakeMargin = 5e-4f;
    static constexpr double kWatchdogMs = 1000.0;
};
