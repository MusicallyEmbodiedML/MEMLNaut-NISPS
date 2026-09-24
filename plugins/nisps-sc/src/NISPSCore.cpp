#include "NISPSCore.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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

} // namespace

NISPSCore::NISPSCore(size_t n_inputs, size_t n_outputs)
    : n_inputs_(n_inputs),
      n_outputs_(n_outputs),
      mlp_({ n_inputs, 16, 16, n_outputs }, { RELU, RELU, HARDSIGMOID }, loss::LOSS_FUNCTIONS::LOSS_MSE),
      controlInput_(n_inputs, 0.f),
      action_(n_outputs, 0.5f),
      mappingOutput_(n_outputs, 0.5f) {
    // Matches InterfaceRL<N>::setup()'s initial weight range (distinct from
    // the wider range randomiseNetwork() uses for the "scramble" gesture).
    mlp_.RandomiseWeightsAndBiasesLin(-1.2f, 0.9f, 0.f, 0.5f);

    replayMem_.setMemoryLimit(kMemoryLimit);

    ouNoises_.reserve(n_outputs);
    for (size_t i = 0; i < n_outputs; ++i) {
        ouNoises_.push_back(std::make_unique<OrnsteinUhlenbeckNoise>(kOuTheta, 0.0f, 0.0f, kOuDt, 0.0f));
    }

    itemsToRemove_.reserve(kMemoryLimit);
    joltWeightLoc_.reserve(kJoltNumWeights);
    joltTarget_.reserve(kJoltNumWeights);
}

void NISPSCore::setInput(size_t index, float value) {
    if (index < controlInput_.size()) {
        controlInput_[index] = value;
        newInput_ = true;
    }
}

void NISPSCore::setInputs(const std::vector<float>& values) {
    const size_t n = std::min(values.size(), controlInput_.size());
    for (size_t i = 0; i < n; ++i) controlInput_[i] = values[i];
    newInput_ = true;
}

void NISPSCore::setNoiseLevel(float level) {
    noiseLevel_ = level;
    float amplitude = level * kNoiseMaxAmplitude;
    if (amplitude < 0.01f) amplitude = 0.f;
    for (auto& n : ouNoises_) n->setStationaryStd(amplitude);
}

void NISPSCore::process() {
    if (joltActive_) {
        stepJolt(); // weights morph; learning paused while held
    } else {
        // Ramp learning rate back up after a jolt (0 -> full over ~5s) so
        // training doesn't immediately drag the net off the jolted sound.
        if (joltLRRamp_ < 1.f) joltLRRamp_ = std::min(1.f, joltLRRamp_ + kJoltLRRampStep);

        if (optimiseCounter_ >= optimiseDivisor_) {
            optimise();
            optimiseCounter_ = 0;
            newInput_ = true; // the mapping changed even if the input didn't
        } else {
            optimiseCounter_++;
        }
    }
    generateAction(false);
}

void NISPSCore::generateAction(bool force) {
    if (!(newInput_ || force)) return;
    newInput_ = false;

    mlp_.GetOutput(controlInput_, &mappingOutput_);
    for (size_t i = 0; i < mappingOutput_.size(); ++i) {
        const float noise = ouNoises_[i]->sample();
        mappingOutput_[i] += noise;
        if (mappingOutput_[i] < 0.f) {
            mappingOutput_[i] = std::fmod(-mappingOutput_[i], 1.f); // reflect at 0
        } else if (mappingOutput_[i] > 1.f) {
            mappingOutput_[i] = 1.f - std::fmod(mappingOutput_[i], 1.f); // reflect at 1
        }
    }
    action_ = mappingOutput_;
}

void NISPSCore::like() {
    storeExperience(1.f, controlInput_, action_);
}

void NISPSCore::dislike() {
    storeExperience(-1.f, controlInput_, action_);
}

void NISPSCore::randomiseNetwork() {
    // Matches InterfaceRL::randomiseTheNetwork()'s wider weight/bias range —
    // a deliberately different range from the constructor's initial one.
    mlp_.RandomiseWeightsAndBiasesLin(-0.9f, 1.1f, -0.9f, 0.3f);
    generateAction(true);
}

void NISPSCore::clearMemory() {
    replayMem_.clear();
}

float NISPSCore::randomJoltTarget() const {
    return kJoltWeightMin + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (kJoltWeightMax - kJoltWeightMin);
}

void NISPSCore::startJolt() {
    joltActive_ = true;
    joltWeightLoc_.clear();
    joltTarget_.clear();

    const size_t numLayers = mlp_.GetNumLayers();
    if (numLayers == 0) return;

    for (size_t i = 0; i < kJoltNumWeights; ++i) {
        const size_t layer = static_cast<size_t>(rand()) % numLayers;
        Layer<float>& L = mlp_.GetLayerRef(layer);
        const size_t numNodes = static_cast<size_t>(L.GetOutputSize());
        const size_t numInputsForLayer = static_cast<size_t>(L.GetInputSize());
        if (numNodes == 0 || numInputsForLayer == 0) continue;

        const size_t node = static_cast<size_t>(rand()) % numNodes;
        const size_t input = static_cast<size_t>(rand()) % numInputsForLayer;
        joltWeightLoc_.push_back({ layer, node, input });
        joltTarget_.push_back(randomJoltTarget());
    }
}

void NISPSCore::stepJolt() {
    for (size_t i = 0; i < joltWeightLoc_.size(); ++i) {
        const JoltWeightRef& ref = joltWeightLoc_[i];
        float& w = mlp_.GetLayerRef(ref.layer).weight(ref.node, ref.input);
        w += kJoltMorphRate * (joltTarget_[i] - w);

        float gap = joltTarget_[i] - w;
        if (gap < 0.f) gap = -gap;
        if (gap < kJoltTargetEpsilon) joltTarget_[i] = randomJoltTarget(); // keep drifting while held
    }
    newInput_ = true;
}

void NISPSCore::stopJolt() {
    joltActive_ = false; // weights stay where they morphed to (permanent)
    joltLRRamp_ = 0.f; // learning resumes gently, ramping back to full over ~5s
}

// Ports InterfaceRL<N>::optimise() (see InterfaceRL.tpp) verbatim in
// algorithm terms. Two points where the shipped code has drifted from
// docs/NISPS_workflow.md — implemented here per the code, not the doc:
//   - dislikes hard-expire after kDislikeLifetimeMs with no decay multiplier
//     (the doc's dislikeMultiplier_ does not exist in the source).
//   - the geometric push has no 1/(1+len) taper, and negLRRatio's base is
//     1.5, not 0.5 (per an explicit code comment: the taper was removed on
//     purpose).
void NISPSCore::optimise() {
    const float effLR = learningRateScaled_ * joltLRRamp_;

    // Positive batch: random sample (diversity for generalisation), trained
    // as imitation of the performer's own liked actions.
    std::vector<size_t> sample = replayMem_.sampleIndices(kBatchSize);
    if (sample.size() > 1) {
        size_t batchSizePos = 0;
        float avgRewardPos = 0.f;
        MLP<float>::training_pair_t tsPositive;
        tsPositive.first.reserve(sample.size());
        tsPositive.second.reserve(sample.size());

        for (auto& i : sample) {
            const auto& item = replayMem_.getItem(i);
            if (item.reward > 0.f) {
                tsPositive.first.push_back(item.input);
                tsPositive.second.push_back(item.action);
                batchSizePos++;
                avgRewardPos += item.reward;
            }
        }
        if (batchSizePos > 0) {
            avgRewardPos /= static_cast<float>(batchSizePos);
            mlp_.TrainBatch(tsPositive, effLR * avgRewardPos, 1, kBatchSize, 0.f, false);
        }
    }

    // Negative full scan: every dislike is guaranteed to push. No decay — a
    // 'no' pushes at full strength until it's lived kDislikeLifetimeMs, then
    // it's removed outright.
    MLP<float>::training_pair_t tsNegative;
    tsNegative.first.reserve(sample.size());
    tsNegative.second.reserve(sample.size());
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
            itemsToRemove_.push_back(i); // lived its lifetime -> stop pushing, remove
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
        MLP<float>::training_pair_t tsGeometric;
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
                const bool active = activeDims_.empty() || (j < activeDims_.size() && activeDims_[j]);
                if (!active) continue;
                const float d = useRandom ? (static_cast<float>(rand() & 0xFF) / 127.5f - 1.f) : (dir[j] / len);
                target[j] = std::clamp(negAction[j] + d * pushStep, 0.f, 1.f);
            }
            tsGeometric.second.push_back(std::move(target));
        }

        const float negFraction =
            static_cast<float>(batchSizeNeg) / static_cast<float>(std::max(batchSizeNeg + totalPosCount, size_t { 1 }));
        const float negLRRatio = kNegLRBase - 0.4f * negFraction;
        mlp_.TrainBatch(tsGeometric, effLR * negLRRatio, 1, batchSizeNeg, 0.f, false);
    }

    replayMem_.removeItems(itemsToRemove_);
    itemsToRemove_.clear();
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


// --- persistence -----------------------------------------------------------
//
// Deliberately not built on memlp's MLP::Serialise()/FromSerialised(): those
// go through Layer::GetWeights2D()/SetWeights(), which touch m_weights only,
// so the biases in m_biases would be dropped and a restored network would not
// reproduce the sound that was saved. The per-layer accessors used here cover
// both.
void NISPSCore::captureState(NISPSState& out) {
    out.version = NISPSState::kVersion;
    out.n_inputs = n_inputs_;
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
    if (in.n_inputs != n_inputs_ || in.n_outputs != n_outputs_) return false;

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
        if (mi.input.size() != n_inputs_ || mi.action.size() != n_outputs_) return false;
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

    if (in.input.size() == n_inputs_) controlInput_ = in.input;
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

    // Jolt state belongs to a gesture in progress, not to the saved mapping.
    joltActive_ = false;
    joltWeightLoc_.clear();
    joltTarget_.clear();
    joltLRRamp_ = 1.f;

    newInput_ = true; // regenerate the action from the restored network
    return true;
}
