#include "NISPSCore.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

// Ported from src/memllib/examples/InterfaceRL.tpp. Function by function the
// algorithm is the firmware's; what differs is plumbing: millis() is nowMs_,
// the display views become lastEvent(), and the StaticMLP's compile-time
// layers are the dynamic MLP's GetLayerRef().

namespace {

float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.f;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const float d = a[i] - b[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

float uni() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }

const char* pick(const char* const* list, size_t n) { return list[static_cast<size_t>(rand()) % n]; }

const char* const kLikeMsgs[] = {
    "Wow, incredible", "Awesome", "That's amazing", "Unbelievable+",
    "I love it!!", "More of this", "Yes!!!!", "A-M-A-Z-I-N-G",
    "Keep going!", "In flow", "I believe in you",
    "Absolutely brilliant!", "This is perfection!",
    "Stunning work!", "Pure genius!",
    "Keep shining!", "Fantastic!", "Incredible vibes!", "Love this journey!",
    "Super cool!"
};
const char* const kDislikeMsgs[] = {
    "oh no!", "Get rid of this sound",
    "Why even bother?", "New sound please!", "No, please no!!!",
    "Thumbs down", "I'm so sorry", "I'm trying my hardest...",
    "I'm doing the best I can", "Learning...",
    "I'll try to do better.", "Still figuring things out.",
    "Thanks for the feedback.", "Working on it!", "Oops, my bad.",
    "Learning from this.", "I'll adjust, promise.", "Noted", "Rearranging",
    "Let's move on!"
};
const char* const kForgetMsgs[] = {
    "Erasing my memory", "Forgetting everything", "Memory wiped", "Thank you Susan?",
    "Starting afresh", "Why care about the past?", "Living in the moment"
};

template <size_t N>
constexpr size_t countOf(const char* const (&)[N]) { return N; }

} // namespace

const char* NISPSCore::dislikeModeName(DislikeMode m) {
    static const char* names[] = { "Push away", "Nudge + blame", "Re-roll", "Nudge>Back>Re-roll",
                                   "Borrow from likes" };
    const size_t i = static_cast<size_t>(m);
    return i < static_cast<size_t>(DislikeMode::COUNT) ? names[i] : "?";
}

NISPSCore::NISPSCore(size_t n_inputs, size_t n_outputs, size_t n_net_inputs)
    : n_inputs_(n_inputs),
      n_net_inputs_(std::max(n_inputs, n_net_inputs)),
      n_outputs_(n_outputs),
      mlp_({ std::max(n_inputs, n_net_inputs), 16, 16, n_outputs }, { RELU, RELU, HARDSIGMOID },
           loss::LOSS_FUNCTIONS::LOSS_MSE),
      controlInput_(std::max(n_inputs, n_net_inputs), 0.f),
      action_(n_outputs, 0.5f),
      mappingOutput_(n_outputs, 0.5f) {
    // Unused net inputs: 1.1/n_unused, so the constant injected into layer 1 stays
    // bounded however many there are (InterfaceRL::updateUnusedInputDefault).
    const size_t unused = n_net_inputs_ - n_inputs_;
    unusedInputDefault_ = unused > 0 ? 1.1f / static_cast<float>(unused) : 0.f;
    for (size_t i = n_inputs_; i < n_net_inputs_; ++i) controlInput_[i] = unusedInputDefault_;

    // Matches InterfaceRL<N>::setup()'s initial weight range (distinct from
    // the wider range randomiseNetwork() uses for the "scramble" gesture).
    mlp_.RandomiseWeightsAndBiasesLin(-1.2f, 0.9f, 0.f, 0.5f);

    replayMem_.setMemoryLimit(kMemoryLimit);
    itemsToRemove_.reserve(kMemoryLimit);
    initExploreNoise();
}

void NISPSCore::post(const char* action, const std::string& message) {
    if (action) event_.action = action;
    event_.message = message;
    event_.serial++;
}

void NISPSCore::setInput(size_t index, float value) {
    if (index < n_inputs_) {
        controlInput_[index] = value;
        newInput_ = true;
    }
}

void NISPSCore::setInputs(const std::vector<float>& values) {
    const size_t n = std::min(values.size(), n_inputs_);
    for (size_t i = 0; i < n; ++i) controlInput_[i] = values[i];
    newInput_ = true;
}

size_t NISPSCore::likeCount() const {
    size_t n = 0;
    for (size_t i = 0; i < replayMem_.size(); ++i)
        if (replayMem_.getItem(i).reward > 0.f) n++;
    return n;
}

void NISPSCore::setNoiseLevel(float level) {
    // Knob [0,1] -> how far the mapping roams (InterfaceRL::setNoiseLevel).
    noiseLevel_ = level;
    const float amplitude = level * kNoiseMaxAmplitude;
    noiseAmp_ = amplitude < 0.01f ? 0.f : amplitude;
}

void NISPSCore::setDislikeMode(DislikeMode m) {
    if (m >= DislikeMode::COUNT) return;
    dislikeMode_ = m;
    dislikeTargets_.clear(); // targets from the previous mode would keep training
    escalateStage_ = 0;
    lastDislikeInput_.clear();
}

void NISPSCore::setActiveDims(std::vector<bool> dims) {
    activeDims_ = std::move(dims);
    locDirty_ = true;
    wakeTraining();
}

float NISPSCore::inputScale() const {
    return std::sqrt(static_cast<float>(std::max<size_t>(1, n_inputs_)));
}

void NISPSCore::process() {
    if (optimiseCounter_ >= optimiseDivisor_) {
        optimise();
        optimiseCounter_ = 0;
        newInput_ = true; // the mapping changed even if the input didn't
    } else {
        optimiseCounter_++;
    }
    generateAction(false);
}

void NISPSCore::generateAction(bool force) {
    // While noise is on, regenerate every cycle so it keeps moving even when the input is
    // still and training has settled.
    if (!(newInput_ || force || noiseAmp_ > 0.f)) return;
    newInput_ = false;
    if (!dragging_) {
        if (noiseAmp_ > 0.f) noisyForward(mappingOutput_);
        else mlp_.GetOutput(controlInput_, &mappingOutput_);
    }
    action_ = mappingOutput_;
}

// --- gestures ----------------------------------------------------------------

void NISPSCore::like() {
    storeExperience(1.f, controlInput_, action_);
    escalateStage_ = 0;
    lastDislikeInput_.clear(); // a like ends any run of 'no's
    post("yes", pick(kLikeMsgs, countOf(kLikeMsgs)));
}

void NISPSCore::dislike() {
    if (noiseAmp_ > 0.f) jumpNoise(); // the noise took us here: move it away too
    if (rerollNearbySynth()) {        // a disliked jolt sound: try another there
        post("no", "No: new jolt sound here");
        return;
    }
    if (dislikeMode_ != DislikeMode::CURRENT) {
        handleDislike(); // posts its own message
        return;
    }
    storeExperience(-1.f, controlInput_, action_);
    post("no", pick(kDislikeMsgs, countOf(kDislikeMsgs)));
}

void NISPSCore::randomiseNetwork() {
    // Matches InterfaceRL::randomiseTheNetwork()'s wider weight/bias range —
    // a deliberately different range from the constructor's initial one.
    mlp_.RandomiseWeightsAndBiasesLin(-0.9f, 1.1f, -0.9f, 0.3f);
    synth_.clear();
    wakeTraining();
    generateAction(true);
    post("scramble", "Scrambling the network");
}

void NISPSCore::clearMemory() {
    replayMem_.clear();
    synth_.clear();
    locDirty_ = true;
    wakeTraining();
    dislikeTargets_.clear();
    escalateStage_ = 0;
    lastDislikeInput_.clear();
    post("forget", pick(kForgetMsgs, countOf(kForgetMsgs)));
}

void NISPSCore::startDrag() {
    if (dragging_) return;
    savedAction_ = action_;
    dragging_ = true;
    post("drag", "Where do you want it?");
}

void NISPSCore::endDrag() {
    if (!dragging_) return;
    dragging_ = false;
    storeExperience(1.f, controlInput_, savedAction_);
    escalateStage_ = 0;
    lastDislikeInput_.clear();
    newInput_ = true;
    post("drop", "Here!");
}

// --- training ----------------------------------------------------------------

bool NISPSCore::nearCurrentInput(const std::vector<float>& x) const {
    return euclideanDistance(x, controlInput_) < kFocusEditRadius * inputScale();
}

// Training target for liked memory i: its action, except (under focus) a like near the
// current input releases its focused dims to the net's current output there.
const std::vector<float>& NISPSCore::likeTarget(size_t i) {
    const auto& item = replayMem_.getItem(i);
    if (!focusActive() || !nearCurrentInput(item.input)) return item.action;
    mlp_.GetOutput(item.input, &likeTargetBuf_);
    for (size_t j = 0; j < likeTargetBuf_.size() && j < item.action.size(); j++)
        if (!isActiveDim(j)) likeTargetBuf_[j] = item.action[j]; // frozen dims: still held
    return likeTargetBuf_;
}

void NISPSCore::wakeTraining() {
    trainSettled_ = false;
    settleGoodChecks_ = 0;
    plateauRefError_ = 1e9f;
    plateauRefMs_ = nowMs_;
}

float NISPSCore::likedError() {
    float sum = 0.f;
    size_t n = 0;
    for (size_t i = 0; i < replayMem_.size(); i++) {
        if (replayMem_.getItem(i).reward <= 0.f) continue;
        const std::vector<float> target = likeTarget(i);
        mlp_.GetOutput(replayMem_.getItem(i).input, &evalOut_);
        const size_t m = std::min(evalOut_.size(), target.size());
        float se = 0.f;
        for (size_t j = 0; j < m; j++) {
            const float d = target[j] - evalOut_[j];
            se += d * d;
        }
        sum += m ? se / static_cast<float>(m) : 0.f;
        n++;
    }
    for (const auto& sp : synth_) { // jolt points count as likes here
        mlp_.GetOutput(sp.input, &evalOut_);
        const size_t m = std::min(evalOut_.size(), sp.target.size());
        float se = 0.f;
        for (size_t j = 0; j < m; j++) {
            const float d = sp.target[j] - evalOut_[j];
            se += d * d;
        }
        sum += m ? se / static_cast<float>(m) : 0.f;
        n++;
    }
    return n ? sum / static_cast<float>(n) : 0.f;
}

void NISPSCore::updateTrainState(bool pending) {
    const double t = nowMs_;
    if (pending) { // something time-limited still working: restart the settle tests
        settleGoodChecks_ = 0;
        plateauRefError_ = 1e9f;
        plateauRefMs_ = t;
        return;
    }
    if (t - lastErrorEvalMs_ < kSettleEvalMs) return;
    lastErrorEvalMs_ = t;
    const float e = likedError();

    settleGoodChecks_ = (e < kSettleError) ? settleGoodChecks_ + 1 : 0;
    bool settle = settleGoodChecks_ >= kSettleChecks;
    if (!settle && t - plateauRefMs_ >= kPlateauWindowMs) {
        settle = e > plateauRefError_ * (1.f - kPlateauGain);
        plateauRefError_ = e;
        plateauRefMs_ = t;
    }
    if (!settle) return;
    trainSettled_ = true;
    settledError_ = e;
}

// Ports InterfaceRL<N>::optimise() (see InterfaceRL.tpp). Where the shipped
// code has drifted from docs/NISPS_workflow.md it follows the code, not the doc:
//   - dislikes hard-expire after kDislikeLifetimeMs with no decay multiplier.
//   - the geometric push has no 1/(1+len) taper, and negLRRatio's base is 1.5.
void NISPSCore::optimise() {
    // Train on demand: settled -> no training; just an occasional watchdog check.
    if (trainOnDemand_ && trainSettled_) {
        if (nowMs_ - lastErrorEvalMs_ < kWatchdogMs) return;
        lastErrorEvalMs_ = nowMs_;
        const float e = likedError();
        if (e <= settledError_ * kWakeRatio + kWakeMargin) return;
        wakeTraining();
    }

    const float effLR = learningRateScaled_;
    float lossSum = 0.f;
    int lossCount = 0;

    // Positive batch: random sample (diversity for generalisation), trained
    // as imitation of the performer's own liked actions.
    std::vector<size_t> sample = replayMem_.sampleIndices(kBatchSize);
    if (sample.size() > 1) {
        size_t batchSizePos = 0;
        float avgRewardPos = 0.f;
        training_pair_t tsPositive;
        tsPositive.first.reserve(sample.size());
        tsPositive.second.reserve(sample.size());
        for (auto& i : sample) {
            if (replayMem_.getItem(i).reward > 0.f) {
                tsPositive.first.push_back(replayMem_.getItem(i).input);
                tsPositive.second.push_back(likeTarget(i));
                batchSizePos++;
                avgRewardPos += replayMem_.getItem(i).reward;
            }
        }
        if (batchSizePos > 0) {
            avgRewardPos /= static_cast<float>(batchSizePos);
            const float loss = mlp_.TrainBatch(tsPositive, effLR * avgRewardPos, 1, kBatchSize, 0.f, false);
            lossSum += loss;
            lossCount++;
            lastLikeLoss_ = loss;
            likeLossPasses_++;
        }
    }

    // Negative full scan: every dislike is guaranteed to push, at full strength
    // until it has lived kDislikeLifetimeMs, then it is removed outright.
    training_pair_t tsNegative;
    float avgRewardNeg = 0.f;
    size_t totalPosCount = 0;
    size_t batchSizeNeg = 0;
    for (size_t i = 0; i < replayMem_.size(); ++i) {
        const float reward = replayMem_.getItem(i).reward;
        if (reward > 0.f) {
            totalPosCount++;
            continue;
        }
        if ((nowMs_ - static_cast<double>(replayMem_.getTimestamp(i))) >= kDislikeLifetimeMs) {
            itemsToRemove_.push_back(i);
            continue;
        }
        const auto& item = replayMem_.getItem(i);
        tsNegative.first.push_back(item.input);
        tsNegative.second.push_back(item.action);
        batchSizeNeg++;
        avgRewardNeg += reward;
    }

    if (batchSizeNeg > 0) {
        // k-NN positive-action centroid, nearest to the *current* input.
        struct PosCandidate {
            float dist;
            size_t idx;
        };
        std::vector<PosCandidate> candidates;
        candidates.reserve(replayMem_.size());
        for (size_t i = 0; i < replayMem_.size(); ++i) {
            const auto& item = replayMem_.getItem(i);
            if (item.reward > 0.f) candidates.push_back({ euclideanDistance(item.input, controlInput_), i });
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const PosCandidate& a, const PosCandidate& b) { return a.dist < b.dist; });

        std::vector<float> meanPositiveAction(action_.size(), 0.f);
        size_t posMemCount = 0;
        const size_t kUsed = std::min(candidates.size(), kCentroidK);
        for (size_t ci = 0; ci < kUsed; ++ci) {
            const auto& item = replayMem_.getItem(candidates[ci].idx);
            for (size_t j = 0; j < meanPositiveAction.size() && j < item.action.size(); ++j)
                meanPositiveAction[j] += item.action[j];
            posMemCount++;
        }
        if (posMemCount > 0) {
            for (auto& v : meanPositiveAction) v /= static_cast<float>(posMemCount);
        }
        avgRewardNeg /= static_cast<float>(batchSizeNeg);

        // Push each disliked action's training target away from the liked
        // centroid — or in a random direction when there are no likes yet.
        const bool havePositives = (posMemCount > 0);
        training_pair_t tsGeometric;
        tsGeometric.first = tsNegative.first;
        tsGeometric.second.reserve(tsNegative.second.size());

        const float pushStep = std::clamp(std::fabs(avgRewardNeg), 0.25f, 1.0f) * kGeometricPushScale;

        for (const auto& negAction : tsNegative.second) {
            const size_t dimCount = std::min(negAction.size(), meanPositiveAction.size());
            float len = 0.f;
            std::vector<float> dir(dimCount);
            for (size_t j = 0; j < dimCount; ++j) {
                dir[j] = negAction[j] - meanPositiveAction[j];
                len += dir[j] * dir[j];
            }
            len = std::sqrt(len);
            const bool useRandom = !havePositives || (len <= 1e-4f);

            std::vector<float> target(negAction); // copy keeps out-of-range dims intact
            for (size_t j = 0; j < dimCount; ++j) {
                if (!isActiveDim(j)) continue;
                const float d = useRandom ? (static_cast<float>(rand() & 0xFF) / 127.5f - 1.f) : (dir[j] / len);
                target[j] = std::clamp(negAction[j] + d * pushStep, 0.f, 1.f);
            }
            tsGeometric.second.push_back(std::move(target));
        }

        const float negFraction =
            static_cast<float>(batchSizeNeg) / static_cast<float>(std::max(batchSizeNeg + totalPosCount, size_t { 1 }));
        const float negLRRatio = kNegLRBase - 0.4f * negFraction;
        lossSum += mlp_.TrainBatch(tsGeometric, effLR * negLRRatio, 1, batchSizeNeg, 0.f, false);
        lossCount++;
    }

    if (lossCount > 0) {
        lastTrainingError_ = lossSum / static_cast<float>(lossCount);
        trainingPasses_++;
    }

    replayMem_.removeItems(itemsToRemove_);
    itemsToRemove_.clear();

    // Non-CURRENT dislike modes: bounded, self-retiring targets.
    trainDislikeTargets(effLR * kDislikeLR);

    // Jolt: train towards the synthetic points (strongly just after a jolt).
    const bool synthPending = trainSynth(effLR);

    if (trainOnDemand_) updateTrainState(batchSizeNeg > 0 || !dislikeTargets_.empty() || synthPending);
}

bool NISPSCore::removeItemsAtDistance(const std::vector<float>& state, float distThreshold, float reward) {
    std::vector<size_t> indicesToRemove;
    bool accumulated = false;
    for (size_t i = 0; i < replayMem_.size(); ++i) {
        TrainItem& item = replayMem_.getItem(i);
        const float dist = euclideanDistance(item.input, state);
        if (dist < distThreshold) {
            if (reward < 0.f && item.reward < 0.f) {
                // Strengthen the existing dislike rather than adding a duplicate.
                item.reward = std::max(item.reward + reward, -1.0f);
                accumulated = true;
            } else if (reward < 0.f && item.reward > 0.f) {
                // A dislike near a like: remove the like so it stops pulling
                // the mapping back toward the disliked region.
                indicesToRemove.push_back(i);
            } else if (item.reward > 0.f && reward > 0.f) {
                indicesToRemove.push_back(i); // duplicate like
            }
        }
    }
    replayMem_.removeItems(indicesToRemove);
    return accumulated;
}

void NISPSCore::decayItemsAtDistance(const std::vector<float>& state, float distThreshold) {
    std::vector<size_t> indicesToRemove;
    for (size_t i = 0; i < replayMem_.size(); ++i) {
        TrainItem& item = replayMem_.getItem(i);
        const float dist = euclideanDistance(item.input, state);
        if (dist < distThreshold) {
            const float decayFactor = dist / distThreshold; // closer items decay more toward zero
            item.reward *= decayFactor;
            if (item.reward < 0.05f) indicesToRemove.push_back(i);
        }
    }
    replayMem_.removeItems(indicesToRemove);
}

void NISPSCore::storeExperience(float reward, const std::vector<float>& state, const std::vector<float>& takenAction) {
    wakeTraining(); // new data
    locDirty_ = true;
    TrainItem item { state, takenAction, reward * rewardScale_ };
    bool skipAdd = false;
    switch (memoryStoreMode_) {
        case MemoryStoreMode::ADD:
            break;
        case MemoryStoreMode::REPLACE_5_PERCENT:
            skipAdd = removeItemsAtDistance(state, 0.05f, item.reward);
            break;
        case MemoryStoreMode::REPLACE_10_PERCENT:
            skipAdd = removeItemsAtDistance(state, 0.10f, item.reward);
            break;
        case MemoryStoreMode::REPLACE_15_PERCENT:
            skipAdd = removeItemsAtDistance(state, 0.15f, item.reward);
            break;
        case MemoryStoreMode::REWARD_DECAY_10_PERCENT:
            decayItemsAtDistance(state, 0.10f);
            break;
        case MemoryStoreMode::REWARD_DECAY_20_PERCENT:
            decayItemsAtDistance(state, 0.20f);
            break;
    }
    if (!skipAdd) replayMem_.add(item, static_cast<size_t>(nowMs_));
}

// --- dislike modes -----------------------------------------------------------

float NISPSCore::rmsDistActive(const std::vector<float>& a, const std::vector<float>& b) const {
    const size_t n = std::min(a.size(), b.size());
    float sum = 0.f;
    size_t count = 0;
    for (size_t j = 0; j < n; j++) {
        if (!isActiveDim(j)) continue;
        const float d = a[j] - b[j];
        sum += d * d;
        count++;
    }
    return count ? std::sqrt(sum / static_cast<float>(count)) : 0.f;
}

int NISPSCore::nearestLikeIndex(const std::vector<float>& input) const {
    int best = -1;
    float bestDist = 0.f;
    for (size_t i = 0; i < replayMem_.size(); i++) {
        const auto& item = replayMem_.getItem(i);
        if (item.reward <= 0.f) continue;
        const float d = euclideanDistance(item.input, input);
        if (best < 0 || d < bestDist) {
            best = static_cast<int>(i);
            bestDist = d;
        }
    }
    return best;
}

std::vector<float> NISPSCore::randomUnitDir(size_t n) const {
    std::vector<float> dir(n, 0.f);
    for (size_t j = 0; j < n; j++)
        if (isActiveDim(j)) dir[j] = uni() * 2.f - 1.f;
    return dir; // normalised by stepTarget
}

// origin + dir scaled to an RMS displacement of rmsStep over the active dims, reflected
// back inside [kTargetLo, kTargetHi] rather than clipped.
std::vector<float> NISPSCore::stepTarget(const std::vector<float>& origin, std::vector<float> dir, float rmsStep) const {
    float len = 0.f;
    size_t count = 0;
    for (size_t j = 0; j < dir.size(); j++) {
        if (!isActiveDim(j)) {
            dir[j] = 0.f;
            continue;
        }
        len += dir[j] * dir[j];
        count++;
    }
    len = std::sqrt(len);
    if (len < 1e-6f || count == 0) return origin;
    const float scale = rmsStep * std::sqrt(static_cast<float>(count)) / len;
    std::vector<float> target(origin);
    for (size_t j = 0; j < target.size() && j < dir.size(); j++) {
        if (!isActiveDim(j)) continue;
        float v = origin[j] + dir[j] * scale;
        if (v > kTargetHi) v = kTargetHi - (v - kTargetHi);
        if (v < kTargetLo) v = kTargetLo + (kTargetLo - v);
        target[j] = std::clamp(v, kTargetLo, kTargetHi);
    }
    return target;
}

void NISPSCore::addDislikeTarget(DislikeTarget&& t) {
    wakeTraining();
    // A new 'no' here supersedes older ones here, so successive presses don't fight.
    for (size_t i = dislikeTargets_.size(); i-- > 0;) {
        if (euclideanDistance(dislikeTargets_[i].input, t.input) < kDislikeInputRadius)
            dislikeTargets_.erase(dislikeTargets_.begin() + static_cast<long>(i));
    }
    if (dislikeTargets_.size() >= kMaxDislikeTargets) dislikeTargets_.erase(dislikeTargets_.begin());
    dislikeTargets_.push_back(std::move(t));
}

// Likes at this input whose sound is (nearly) the one just disliked go: the player has
// changed their mind about them. Likes of a different sound here are kept.
void NISPSCore::removeEndorsingLikes() {
    if (focusActive()) return; // a focused 'no' edits just those dims; keep the like
    std::vector<size_t> idx;
    for (size_t i = 0; i < replayMem_.size(); i++) {
        const auto& item = replayMem_.getItem(i);
        if (item.reward > 0.f && euclideanDistance(item.input, controlInput_) < kDislikeInputRadius &&
            rmsDistActive(item.action, action_) < kRepelMargin)
            idx.push_back(i);
    }
    if (!idx.empty()) replayMem_.removeItems(idx);
}

void NISPSCore::dislikeNudge(bool blame) {
    removeEndorsingLikes();
    const size_t n = action_.size();
    std::vector<float> dir(n, 0.f);
    bool haveDir = false;
    std::string msg = "No: nudging away";

    if (blame) {
        const int li = nearestLikeIndex(controlInput_);
        if (li >= 0) {
            const auto& ref = replayMem_.getItem(static_cast<size_t>(li)).action;
            for (size_t j = 0; j < n && j < ref.size(); j++) {
                const float diff = ref[j] - action_[j];
                dir[j] = diff * std::fabs(diff);
            }
            haveDir = true;
            msg = "No: reverting what changed";
        }
    } else {
        struct Cand {
            float dist;
            size_t idx;
        };
        std::vector<Cand> cands;
        for (size_t i = 0; i < replayMem_.size(); i++) {
            const auto& item = replayMem_.getItem(i);
            if (item.reward > 0.f) cands.push_back({ euclideanDistance(item.input, controlInput_), i });
        }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.dist < b.dist; });
        const size_t k = std::min(cands.size(), kCentroidK);
        if (k > 0) {
            std::vector<float> centroid(n, 0.f);
            for (size_t c = 0; c < k; c++) {
                const auto& a = replayMem_.getItem(cands[c].idx).action;
                for (size_t j = 0; j < n && j < a.size(); j++) centroid[j] += a[j];
            }
            for (size_t j = 0; j < n; j++) dir[j] = action_[j] - centroid[j] / static_cast<float>(k);
            haveDir = true;
        }
    }

    float len = 0.f;
    for (size_t j = 0; j < n; j++)
        if (isActiveDim(j)) len += dir[j] * dir[j];
    if (!haveDir || len < 1e-8f) dir = randomUnitDir(n);

    addDislikeTarget({ controlInput_, action_, stepTarget(action_, std::move(dir), kRepelStep), nowMs_,
                       DislikeTarget::Kind::REPEL });
    post("no", msg);
}

void NISPSCore::dislikeRetreat() {
    removeEndorsingLikes();
    const int li = nearestLikeIndex(controlInput_);
    if (li < 0) {
        dislikeReroll();
        return;
    }
    const auto& ref = replayMem_.getItem(static_cast<size_t>(li)).action;
    std::vector<float> target(action_);
    for (size_t j = 0; j < target.size() && j < ref.size(); j++) {
        if (!isActiveDim(j)) continue;
        const float jitter = (uni() * 2.f - 1.f) * kRetreatJitter;
        target[j] = std::clamp(ref[j] + jitter, kTargetLo, kTargetHi);
    }
    if (rmsDistActive(target, action_) < kRepelMargin) { // already there
        dislikeReroll();
        return;
    }
    addDislikeTarget({ controlInput_, action_, std::move(target), nowMs_, DislikeTarget::Kind::SEEK });
    post("no", "No again: going back");
}

void NISPSCore::dislikeReroll() {
    removeEndorsingLikes();
    std::vector<size_t> likes;
    for (size_t i = 0; i < replayMem_.size(); i++)
        if (replayMem_.getItem(i).reward > 0.f) likes.push_back(i);

    std::vector<float> best;
    float bestDist = -1.f;
    for (int attempt = 0; attempt < 8 && bestDist < kRerollMinDist; attempt++) {
        std::vector<float> cand(action_);
        const bool nearLike = !likes.empty() && (rand() & 1);
        const std::vector<float>* base =
            nearLike ? &replayMem_.getItem(likes[static_cast<size_t>(rand()) % likes.size()]).action : nullptr;
        for (size_t j = 0; j < cand.size(); j++) {
            if (!isActiveDim(j)) continue;
            float v;
            if (base && j < base->size()) {
                const float g = (uni() + uni() + uni() - 1.5f) * 2.f; // ~N(0,1)
                v = (*base)[j] + g * kRerollSpread;
            } else {
                v = 0.15f + 0.7f * uni();
            }
            cand[j] = std::clamp(v, kTargetLo, kTargetHi);
        }
        const float d = rmsDistActive(cand, action_);
        if (d > bestDist) {
            bestDist = d;
            best = std::move(cand);
        }
    }
    addDislikeTarget({ controlInput_, action_, std::move(best), nowMs_, DislikeTarget::Kind::SEEK });
    post("no", "No: trying something new");
}

void NISPSCore::dislikeBorrow() {
    removeEndorsingLikes();
    std::vector<size_t> likes;
    std::vector<float> w;
    float minD2 = 0.f;
    for (size_t i = 0; i < replayMem_.size(); i++) {
        const auto& item = replayMem_.getItem(i);
        if (item.reward <= 0.f) continue;
        const float d = euclideanDistance(item.input, controlInput_);
        const float d2 = d * d;
        if (likes.empty() || d2 < minD2) minD2 = d2;
        likes.push_back(i);
        w.push_back(d2);
    }
    if (likes.empty()) {
        dislikeReroll();
        return;
    }
    // Gaussian kernel on input distance, relative to the nearest like so it can't underflow.
    const float inv2s2 = 1.f / (kBorrowInputSigma * kBorrowInputSigma);
    float wSum = 0.f;
    for (auto& x : w) {
        x = std::exp(-(x - minD2) * inv2s2);
        wSum += x;
    }
    auto pickLike = [&]() -> const std::vector<float>& {
        float r = uni() * wSum;
        for (size_t k = 0; k < likes.size(); k++) {
            r -= w[k];
            if (r <= 0.f) return replayMem_.getItem(likes[k]).action;
        }
        return replayMem_.getItem(likes.back()).action;
    };

    // Keep fewer of the sound's own params on each retry until it has moved far enough.
    std::vector<float> best;
    float bestDist = -1.f;
    for (float keep = kBorrowKeepFraction; keep >= -0.01f && bestDist < kRepelMargin; keep -= 0.1f) {
        std::vector<float> cand(action_);
        for (size_t j = 0; j < cand.size(); j++) {
            if (!isActiveDim(j) || uni() < keep) continue;
            const auto& donor = pickLike();
            if (j >= donor.size()) continue;
            const float jitter = (uni() * 2.f - 1.f) * kBorrowJitter;
            cand[j] = std::clamp(donor[j] + jitter, kTargetLo, kTargetHi);
        }
        const float d = rmsDistActive(cand, action_);
        if (d > bestDist) {
            bestDist = d;
            best = std::move(cand);
        }
    }
    if (bestDist < kRepelMargin) { // the nearby likes all sound like this one
        dislikeReroll();
        return;
    }
    addDislikeTarget({ controlInput_, action_, std::move(best), nowMs_, DislikeTarget::Kind::SEEK });
    post("no", "No: borrowing from likes");
}

void NISPSCore::handleDislike() {
    switch (dislikeMode_) {
        case DislikeMode::BOUNDED_BLAME: dislikeNudge(true); break;
        case DislikeMode::REROLL: dislikeReroll(); break;
        case DislikeMode::BORROW: dislikeBorrow(); break;
        case DislikeMode::ESCALATE: {
            const bool continuing = lastDislikeInput_.size() == controlInput_.size() &&
                                    (nowMs_ - lastDislikeMs_) < kEscalateWindowMs &&
                                    euclideanDistance(lastDislikeInput_, controlInput_) < kEscalateInputRadius;
            escalateStage_ = continuing ? std::min<size_t>(escalateStage_ + 1, 2) : 0;
            lastDislikeMs_ = nowMs_;
            lastDislikeInput_ = controlInput_;
            if (escalateStage_ == 0) dislikeNudge(false);
            else if (escalateStage_ == 1) dislikeRetreat();
            else dislikeReroll();
            break;
        }
        default: break;
    }
}

// Train every live target toward its goal, retiring the ones that have got there (or
// outlived kDislikeTargetLifetimeMs). Checks the net's own (noise-free) output.
void NISPSCore::trainDislikeTargets(float lr) {
    if (dislikeTargets_.empty()) return;
    training_pair_t batch;
    std::vector<float> y;
    for (size_t i = dislikeTargets_.size(); i-- > 0;) {
        const auto& t = dislikeTargets_[i];
        bool done = (nowMs_ - t.t0) >= kDislikeTargetLifetimeMs;
        if (!done) {
            mlp_.GetOutput(t.input, &y);
            done = rmsDistActive(y, t.target) < kSeekEpsilon ||
                   (t.kind == DislikeTarget::Kind::REPEL && rmsDistActive(y, t.origin) >= kRepelMargin);
        }
        if (done) {
            dislikeTargets_.erase(dislikeTargets_.begin() + static_cast<long>(i));
            continue;
        }
        batch.first.push_back(t.input);
        batch.second.push_back(t.target);
    }
    if (!batch.first.empty()) mlp_.TrainBatch(batch, lr, 1, batch.first.size(), 0.f, false);
}

// --- jolt --------------------------------------------------------------------

// Plant a fresh set of kSynthPoints synthetic points (replacing the last). Inputs: random
// in the playable range, clear of the liked inputs and of each other. Targets: each
// focused output goes to the opposite half of its range from what the net plays there.
void NISPSCore::jolt() {
    synth_.clear();
    const size_t nAct = std::min(n_inputs_, controlInput_.size());
    const float clearance = kSynthClearance * inputScale();
    std::vector<float> cand(controlInput_.size(), unusedInputDefault_);
    const int localSeeds = focusActive() ? kFocusLocalSeeds : 0;
    for (int p = 0; p < kSynthPoints; p++) {
        std::vector<float> best;
        float bestDist = -1.f;
        bool found = p < localSeeds;
        if (found) {
            best = controlInput_;
            if (p > 0)
                for (size_t i = 0; i < nAct; i++)
                    best[i] = std::clamp(best[i] + (uni() * 2.f - 1.f) * kFocusSeedJitter, 0.f, 1.f);
        }
        for (int c = 0; c < kSynthCandidates && !found; c++) {
            for (size_t i = 0; i < nAct; i++) cand[i] = kSynthInputLo + uni() * (kSynthInputHi - kSynthInputLo);
            float dLike = 1e9f, dSynth = 1e9f;
            for (size_t i = 0; i < replayMem_.size(); i++) {
                const auto& item = replayMem_.getItem(i);
                if (item.reward > 0.f) dLike = std::min(dLike, euclideanDistance(item.input, cand));
            }
            for (const auto& sp : synth_) dSynth = std::min(dSynth, euclideanDistance(sp.input, cand));
            const float d = std::min(dLike, dSynth * 2.f);
            if (dLike >= clearance && dSynth >= clearance * 0.5f) found = true; // take it
            if (found || d > bestDist) {
                bestDist = d;
                best = cand;
            }
        }
        SynthPoint sp;
        sp.input = best;
        rollSynthTarget(sp);
        synth_.push_back(std::move(sp));
    }
    synthSeedMs_ = nowMs_;
    synthSeedError_ = synthError();
    synthBurst_ = true;
    wakeTraining();
    post("jolt", "Jolt: new sounds in the gaps");
}

void NISPSCore::rollSynthTarget(SynthPoint& sp) {
    mlp_.GetOutput(sp.input, &evalOut_);
    sp.target = evalOut_;
    const float mid = 0.5f * (kSynthTargetLo + kSynthTargetHi);
    for (size_t j = 0; j < sp.target.size(); j++) {
        if (!isActiveDim(j)) continue;
        sp.target[j] = (sp.target[j] < mid) ? mid + 0.05f + uni() * (kSynthTargetHi - mid - 0.05f)
                                            : kSynthTargetLo + uni() * (mid - 0.05f - kSynthTargetLo);
    }
}

bool NISPSCore::rerollNearbySynth() {
    if (synth_.empty()) return false;
    int nearest = -1;
    float best = kSynthDislikeRadius * inputScale();
    for (size_t i = 0; i < synth_.size(); i++) {
        const float d = euclideanDistance(synth_[i].input, controlInput_);
        if (d < best) {
            best = d;
            nearest = static_cast<int>(i);
        }
    }
    if (nearest < 0) return false;
    rollSynthTarget(synth_[static_cast<size_t>(nearest)]);
    synthSeedMs_ = nowMs_;
    synthSeedError_ = synthError();
    synthBurst_ = true;
    wakeTraining();
    return true;
}

// A burst at kSynthBurstLR after a jolt, until the synthetic error is down to
// kSynthBurstDone of where it began (or kSynthBurstMaxMs), then kSynthHoldLR as anchors.
bool NISPSCore::trainSynth(float lr) {
    if (synth_.empty()) return false;
    if (synthBurst_ && nowMs_ - synthLastCheckMs_ >= 200.0) {
        synthLastCheckMs_ = nowMs_;
        if (synthError() <= synthSeedError_ * kSynthBurstDone || nowMs_ - synthSeedMs_ >= kSynthBurstMaxMs)
            synthBurst_ = false;
    }
    training_pair_t ts;
    ts.first.reserve(synth_.size());
    ts.second.reserve(synth_.size());
    for (const auto& sp : synth_) {
        ts.first.push_back(sp.input);
        ts.second.push_back(sp.target);
    }
    mlp_.TrainBatch(ts, lr * (synthBurst_ ? kSynthBurstLR : kSynthHoldLR), 1, synth_.size(), 0.f, false);
    return synthBurst_;
}

float NISPSCore::synthError() {
    float sum = 0.f;
    for (const auto& sp : synth_) {
        mlp_.GetOutput(sp.input, &evalOut_);
        float se = 0.f;
        for (size_t j = 0; j < sp.target.size(); j++) {
            const float d = sp.target[j] - evalOut_[j];
            se += d * d;
        }
        sum += se / static_cast<float>(sp.target.size());
    }
    return synth_.empty() ? 0.f : sum / static_cast<float>(synth_.size());
}

// --- exploration noise (weight-space) ------------------------------------------

void NISPSCore::initExploreNoise() {
    const size_t numLayers = mlp_.GetNumLayers();
    hidden_ = numLayers > 0 ? static_cast<size_t>(mlp_.GetLayerRef(numLayers - 1).GetInputSize()) : 0;
    exploreOU_.clear();
    exploreOU_.reserve(kWeightDirs);
    for (size_t i = 0; i < kWeightDirs; i++) {
        // Correlation time ~1/(theta*dt) control cycles (~60s at 200 Hz). Unit std.
        exploreOU_.emplace_back(kNoiseTheta, 0.0f, 0.0f, kNoiseDt, 0.0f);
        exploreOU_.back().setStationaryStd(1.f);
    }
    // Fixed random +-1 weight directions, from the firmware's LCG and seed.
    weightDirs_.assign(kWeightDirs * n_outputs_ * hidden_, 1);
    uint32_t r = 0x12345678u;
    for (auto& d : weightDirs_) {
        r = r * 1664525u + 1013904223u;
        d = (r >> 31) ? 1 : -1;
    }
    noiseW_.assign(n_outputs_ * hidden_, 0.f);
    h1_.assign(16, 0.f);
    h2_.assign(hidden_, 0.f);
}

// Noise scale at input x: kNearFloor right on a liked input, rising to 1 at kNearRadius.
float NISPSCore::noiseLocality(const std::vector<float>& x) {
    if (focusActive()) return 1.f; // focus protects the rest: explore right here
    if (!locDirty_ && locMemSize_ == replayMem_.size() && locInput_ == x) return locValue_;
    locDirty_ = false;
    locMemSize_ = replayMem_.size();
    locInput_ = x;
    locValue_ = 1.f;
    const float radius = kNearRadius * inputScale();
    float d = 1e9f;
    for (size_t i = 0; i < replayMem_.size(); i++) {
        const auto& item = replayMem_.getItem(i);
        if (item.reward > 0.f) d = std::min(d, euclideanDistance(item.input, x));
    }
    if (d < radius) locValue_ = kNearFloor + (1.f - kNearFloor) * (d / radius);
    return locValue_;
}

// dW = sum_k c_k D_k, with each drift stepped once per control period elapsed since the
// last rebuild (so the morphing speed doesn't depend on the rebuild or call rate).
void NISPSCore::rebuildNoiseW() {
    long steps = noiseWValid_ ? static_cast<long>((nowMs_ - noiseWMs_) / kControlPeriodMs) : 1;
    steps = std::min<long>(std::max<long>(steps, 1), 100);
    noiseWMs_ = nowMs_;
    noiseWValid_ = true;
    float c[kWeightDirs];
    for (size_t k = 0; k < kWeightDirs; k++) {
        for (long i = 1; i < steps; i++) exploreOU_[k].sample();
        c[k] = exploreOU_[k].sample();
    }
    for (size_t o = 0; o < n_outputs_; o++)
        for (size_t j = 0; j < hidden_; j++) {
            float w = 0.f;
            for (size_t k = 0; k < kWeightDirs; k++) w += c[k] * weightDirs_[(k * n_outputs_ + o) * hidden_ + j];
            noiseW_[o * hidden_ + j] = w;
        }
}

// The forward pass by hand (ReLU, ReLU, hard sigmoid) with the last layer's weights
// perturbed by dW. Normalised by |h2| and sqrt(K) to ~unit std per output, then x6 (the
// hard sigmoid's slope is 1/6) so each output moves by ~noiseAmp_.
void NISPSCore::noisyForward(std::vector<float>& out) {
    if (mlp_.GetNumLayers() != 3) {
        mlp_.GetOutput(controlInput_, &out);
        return;
    }
    const float amp = noiseAmp_ * noiseLocality(controlInput_);
    Layer<float>& L0 = mlp_.GetLayerRef(0);
    Layer<float>& L1 = mlp_.GetLayerRef(1);
    Layer<float>& L2 = mlp_.GetLayerRef(2);
    const size_t n0 = static_cast<size_t>(L0.GetOutputSize());
    const size_t n1 = static_cast<size_t>(L1.GetOutputSize());
    h1_.resize(n0);
    h2_.resize(n1);

    for (size_t j = 0; j < n0; j++) {
        float z = L0.bias(j);
        for (size_t i = 0; i < controlInput_.size(); i++) z += L0.weight(j, i) * controlInput_[i];
        h1_[j] = z > 0.f ? z : 0.f;
    }
    float hn = 0.f;
    for (size_t j = 0; j < n1; j++) {
        float z = L1.bias(j);
        for (size_t i = 0; i < n0; i++) z += L1.weight(j, i) * h1_[i];
        h2_[j] = z > 0.f ? z : 0.f;
        hn += h2_[j] * h2_[j];
    }
    hn = std::sqrt(hn);
    const float wScale = hn > 1e-6f ? 6.f * amp / (hn * std::sqrt(static_cast<float>(kWeightDirs))) : 0.f;
    if (!noiseWValid_ || nowMs_ - noiseWMs_ >= kNoiseRebuildMs) rebuildNoiseW();

    out.resize(n_outputs_);
    for (size_t o = 0; o < n_outputs_; o++) {
        float z = L2.bias(o);
        float dz = 0.f;
        const float* nw = &noiseW_[o * hidden_];
        for (size_t i = 0; i < n1; i++) {
            z += L2.weight(o, i) * h2_[i];
            dz += nw[i] * h2_[i];
        }
        z += dz * wScale;
        out[o] = z <= -3.f ? 0.f : (z >= 3.f ? 1.f : (z + 3.f) / 6.f);
    }
}

// Dislike while exploring: each noise coefficient jumps to the other side of zero, at
// least kNoiseDislikeJump away, so the mapping moves somewhere audibly different.
void NISPSCore::jumpNoise() {
    for (auto& ou : exploreOU_) {
        const float x = ou.get();
        ou.set((x >= 0.f ? -1.f : 1.f) * (std::fabs(x) + kNoiseDislikeJump));
    }
    noiseWValid_ = false; // rebuild now, so the jump is heard at once
}

void NISPSCore::memoryPoints(std::vector<MemoryPoint>& out) const {
    out.clear();
    out.reserve(replayMem_.size() + synth_.size());
    for (size_t i = 0; i < replayMem_.size(); i++) {
        const auto& item = replayMem_.getItem(i);
        out.push_back({ item.input, item.reward > 0.f ? MemoryPoint::Kind::LIKE : MemoryPoint::Kind::DISLIKE });
    }
    for (const auto& sp : synth_) out.push_back({ sp.input, MemoryPoint::Kind::JOLT });
}


// --- persistence -----------------------------------------------------------
//
// Deliberately not built on memlp's MLP::Serialise()/FromSerialised(): those
// go through Layer::GetWeights2D()/SetWeights(), which touch m_weights only,
// so the biases in m_biases would be dropped and a restored network would not
// reproduce the sound that was saved. The per-layer accessors used here cover
// both.
void NISPSCore::captureState(NISPSState& out) {
    out.version = NISPSState::kVersion;
    out.n_inputs = n_net_inputs_;
    out.n_outputs = n_outputs_;

    out.layers.clear();
    const size_t numLayers = mlp_.GetNumLayers();
    out.layers.reserve(numLayers);
    for (size_t l = 0; l < numLayers; ++l) {
        Layer<float>& L = mlp_.GetLayerRef(l);
        NISPSState::Layer ls;
        ls.nodes = static_cast<size_t>(L.GetOutputSize());
        ls.inputs = static_cast<size_t>(L.GetInputSize());
        ls.weights.reserve(ls.nodes * ls.inputs);
        ls.biases.reserve(ls.nodes);
        for (size_t node = 0; node < ls.nodes; ++node) {
            for (size_t in = 0; in < ls.inputs; ++in) ls.weights.push_back(L.weight(node, in));
            ls.biases.push_back(L.bias(node));
        }
        out.layers.push_back(std::move(ls));
    }

    out.memory.clear();
    out.memory.reserve(replayMem_.size());
    for (size_t i = 0; i < replayMem_.size(); ++i) {
        const TrainItem& item = replayMem_.getItem(i);
        NISPSState::MemoryItem mi;
        mi.input = item.input;
        mi.action = item.action;
        mi.reward = item.reward;
        const double stamp = static_cast<double>(replayMem_.getTimestamp(i));
        mi.ageMs = (nowMs_ > stamp) ? (nowMs_ - stamp) : 0.0;
        out.memory.push_back(std::move(mi));
    }

    out.input = controlInput_;
    out.action = action_;
    out.learningRateScale = learningRateScale_;
    out.rewardScale = rewardScale_;
    out.noiseLevel = noiseLevel_;
    out.optimiseDivisor = optimiseDivisor_;
    out.memoryStoreMode = static_cast<int>(memoryStoreMode_);
}

bool NISPSCore::restoreState(const NISPSState& in) {
    // Validate everything before writing anything, so a rejected snapshot
    // leaves the engine exactly as it was.
    if (in.version != NISPSState::kVersion) return false;
    if (in.n_inputs != n_net_inputs_ || in.n_outputs != n_outputs_) return false;

    const size_t numLayers = mlp_.GetNumLayers();
    if (in.layers.size() != numLayers) return false;
    for (size_t l = 0; l < numLayers; ++l) {
        Layer<float>& L = mlp_.GetLayerRef(l);
        const size_t nodes = static_cast<size_t>(L.GetOutputSize());
        const size_t inputs = static_cast<size_t>(L.GetInputSize());
        if (in.layers[l].nodes != nodes || in.layers[l].inputs != inputs) return false;
        if (in.layers[l].weights.size() != nodes * inputs) return false;
        if (in.layers[l].biases.size() != nodes) return false;
    }
    for (const auto& mi : in.memory) {
        if (mi.input.size() != n_net_inputs_ || mi.action.size() != n_outputs_) return false;
    }

    for (size_t l = 0; l < numLayers; ++l) {
        Layer<float>& L = mlp_.GetLayerRef(l);
        const NISPSState::Layer& ls = in.layers[l];
        size_t w = 0;
        for (size_t node = 0; node < ls.nodes; ++node) {
            for (size_t i = 0; i < ls.inputs; ++i) L.weight(node, i) = ls.weights[w++];
            L.bias(node) = ls.biases[node];
        }
    }

    // Ages are relative, so rebase the clock on the oldest item: that keeps
    // every item's remaining lifetime (kDislikeLifetimeMs) intact and keeps
    // the timestamps non-negative.
    double oldest = 0.0;
    for (const auto& mi : in.memory) oldest = std::max(oldest, mi.ageMs);
    nowMs_ = std::max(nowMs_, oldest);

    replayMem_.clear();
    for (const auto& mi : in.memory) {
        TrainItem item { mi.input, mi.action, mi.reward };
        const double stamp = nowMs_ - mi.ageMs;
        replayMem_.add(item, static_cast<size_t>(stamp > 0.0 ? stamp : 0.0));
    }

    if (in.input.size() == n_net_inputs_) controlInput_ = in.input;
    if (in.action.size() == n_outputs_) {
        action_ = in.action;
        mappingOutput_ = in.action;
    }

    setLearningRateScale(in.learningRateScale);
    setRewardScale(in.rewardScale);
    setNoiseLevel(in.noiseLevel);
    setOptimiseDivisor(in.optimiseDivisor);
    if (in.memoryStoreMode >= static_cast<int>(MemoryStoreMode::ADD) &&
        in.memoryStoreMode <= static_cast<int>(MemoryStoreMode::REWARD_DECAY_20_PERCENT)) {
        setMemoryStoreMode(static_cast<MemoryStoreMode>(in.memoryStoreMode));
    }

    // Gestures in progress belong to the old mapping, not the saved one.
    synth_.clear();
    synthBurst_ = false;
    dislikeTargets_.clear();
    escalateStage_ = 0;
    lastDislikeInput_.clear();
    dragging_ = false;
    joltHeld_ = false;
    locDirty_ = true;
    wakeTraining();

    newInput_ = true; // regenerate the action from the restored network
    return true;
}
