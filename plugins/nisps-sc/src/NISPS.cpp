// NISPS — a SuperCollider control-rate UGen wrapping NISPSCore, the ported
// NISPS (Neural Interactive Shaping of Parameter Spaces) RL engine from the
// MEMLNaut-NISPS embedded firmware (see NISPSCore.h for the port notes).
//
// Interface: N control-rate inputs in, N control-rate outputs out (a mapped
// parameter vector), with the performer's gesture vocabulary — like,
// dislike, randomise, jolt, clear-memory — and the LR/reward/noise/training-
// rate knobs all exposed as ordinary kr UGen inputs. See NISPS.sc for the
// sclang-side argument list.
#include "SC_PlugIn.hpp"

#include <algorithm>
#include <vector>

#include "NISPSCore.h"

static InterfaceTable* ft;

struct NISPS : public SCUnit {
public:
    NISPS() {
        // numOutputs is consumed entirely on the language side (NISPS.sc's
        // init strips it before building `inputs`, the same way In/LocalIn
        // do in the SC class library) — the server already sized mNumOutputs
        // from that before constructing this Unit, so it's just numOutputs().
        const size_t nOut = static_cast<size_t>(std::max(1, numOutputs()));
        const size_t numInputsTotal = static_cast<size_t>(numInputs());
        n_inputs_ = (numInputsTotal > kNumFixedArgs) ? (numInputsTotal - kNumFixedArgs) : 1;

        // One-time allocation at Synth creation only — the RT-safety rule
        // this plugin follows forbids allocation in the per-block calc
        // function (next(), below), not in the constructor.
        core_ = new NISPSCore(n_inputs_, nOut);
        inputBuf_.resize(n_inputs_);

        set_calc_function<NISPS, &NISPS::next>();
        next(1);
    }

    ~NISPS() { delete core_; }

private:
    // Fixed control-input layout, indices 0..8; the trailing inputs
    // (index kNumFixedArgs..) are the N mapped-parameter inputs. numOutputs
    // is NOT one of these — it's consumed on the language side (NISPS.sc)
    // to size the output array and never becomes an actual Unit input. Keep
    // this in sync with NISPS.sc's *kr argument order.
    enum FixedArg {
        kLikeArg = 0,
        kDislikeArg,
        kRandomiseArg,
        kJoltArg,
        kClearMemoryArg,
        kLrScaleArg,
        kRewardScaleArg,
        kNoiseArg,
        kTrainDivisorArg,
        kNumFixedArgs
    };

    void next(int nSamples) {
        core_->advanceClock(1000.0 * static_cast<double>(nSamples) * sampleDur());

        // Discrete gestures: edge-detected triggers, except jolt which is a
        // gate (held for the duration of the morph, like the hardware's
        // momentary button).
        const float likeIn = in0(kLikeArg);
        const float dislikeIn = in0(kDislikeArg);
        const float randomiseIn = in0(kRandomiseArg);
        const float clearIn = in0(kClearMemoryArg);
        const float joltIn = in0(kJoltArg);

        if (likeIn > 0.f && prevLike_ <= 0.f) core_->like();
        if (dislikeIn > 0.f && prevDislike_ <= 0.f) core_->dislike();
        if (randomiseIn > 0.f && prevRandomise_ <= 0.f) core_->randomiseNetwork();
        if (clearIn > 0.f && prevClear_ <= 0.f) core_->clearMemory();

        if (joltIn > 0.f && prevJolt_ <= 0.f) {
            core_->startJolt();
        } else if (joltIn <= 0.f && prevJolt_ > 0.f) {
            core_->stopJolt();
        }

        prevLike_ = likeIn;
        prevDislike_ = dislikeIn;
        prevRandomise_ = randomiseIn;
        prevClear_ = clearIn;
        prevJolt_ = joltIn;

        // Continuous controls.
        core_->setLearningRateScale(in0(kLrScaleArg));
        core_->setRewardScale(in0(kRewardScaleArg));
        core_->setNoiseLevel(in0(kNoiseArg));
        core_->setOptimiseDivisor(static_cast<size_t>(std::max(1.f, in0(kTrainDivisorArg))));

        // Mapped-parameter inputs (the trailing, variable-length part of the
        // input list).
        for (size_t i = 0; i < n_inputs_; ++i) {
            inputBuf_[i] = in0(static_cast<int>(kNumFixedArgs + i));
        }
        core_->setInputs(inputBuf_);

        core_->process();

        const std::vector<float>& action = core_->getAction();
        const size_t n = std::min(action.size(), static_cast<size_t>(numOutputs()));
        for (size_t i = 0; i < n; ++i) {
            out0(static_cast<int>(i)) = action[i];
        }
    }

    NISPSCore* core_ = nullptr;
    std::vector<float> inputBuf_;
    size_t n_inputs_ = 0;

    float prevLike_ = 0.f;
    float prevDislike_ = 0.f;
    float prevRandomise_ = 0.f;
    float prevClear_ = 0.f;
    float prevJolt_ = 0.f;
};

PluginLoad(NISPSUGens) {
    ft = inTable;
    registerUnit<NISPS>(ft, "NISPS");
}
