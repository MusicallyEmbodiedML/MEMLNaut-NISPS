#ifndef __INFINIBITS_AUDIO_APP_HPP__
#define __INFINIBITS_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/GrainDelayI16.hpp"

// Infinibits: the ChunkyBits grain network as a drone. Instead of a single-cycle wave
// written into the grain buffers on each note (which decays), two detuned oscillators
// at a fixed 80 Hz feed grain delays 1 and 2 continuously, so the sound never runs out.
// No arpeggiator, no envelope. Grain writes are clamped to +-1, so feedback past unity
// saturates rather than wrapping the int16 buffers.
template<size_t NPARAMS = 20>
class InfinibitsAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;

    static constexpr float  kFundamentalHz = 80.f;
    static constexpr float  kMaxDetuneCents = 50.f;   // total spread between the oscillators
    static constexpr size_t kTableSize = 600;         // one cycle of 80 Hz at 48 kHz

    enum class Waveform : uint8_t {
        Saw = 0, Square, Triangle, FallingSaw, Sine,
        HalfRectSine, Trapezoid, AsymTriangle, Staircase,
        COUNT
    };
    static constexpr size_t kNumWaveforms = static_cast<size_t>(Waveform::COUNT);
    static constexpr const char* kWaveformNames[kNumWaveforms] = {
        "Saw", "Square", "Triangle", "Falling Saw", "Sine",
        "Half-rect Sine", "Trapezoid", "Asym Triangle", "Staircase"
    };

    queue_t feedbackFactorQueue;
    queue_t waveformQueue;

    InfinibitsAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&feedbackFactorQueue, sizeof(float), 1);
        queue_init(&waveformQueue, sizeof(uint8_t), 1);
    }

    void setFeedbackFactorQueued(float v) { queue_try_add(&feedbackFactorQueue, &v); }
    void setWaveformQueued(Waveform w) {
        uint8_t v = static_cast<uint8_t>(w);
        queue_try_add(&waveformQueue, &v);
    }

    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input    = false,
            .line_level   = 3,
            .mic_gain_dB  = 0,
            .output_volume = 0.97f
        };
    }

    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        // Two table oscillators, detune glided per sample so NN steps don't click
        incA_ += (targetIncA_ - incA_) * kIncSmooth_;
        incB_ += (targetIncB_ - incB_) * kIncSmooth_;
        const float osc = (readTable(phaseA_) + readTable(phaseB_)) * kOscGain_;
        phaseA_ += incA_; if (phaseA_ >= kTableSizeF_) phaseA_ -= kTableSizeF_;
        phaseB_ += incB_; if (phaseB_ >= kTableSizeF_) phaseB_ -= kTableSizeF_;

        float g1 = grainDelay1_.process(osc, grainDelay3Out_ * feedbackAmount1_);
        float g2 = grainDelay2_.process(osc, grainDelay3Out_ * feedbackAmount2_);
        float grains = g1 * (1.f - grainCrossfade_) + g2 * grainCrossfade_;

        // Grain3 captures the grain mix
        grainDelay3Out_ = grainDelay3_.process(grains);

        float mixed = osc * (1.f - wetdry_) + grains * wetdry_;

        // DC blocker (half-rect sine and asymmetric feedback both carry DC)
        const float dc = mixed - dcX1_ + kDCCoeff_ * dcY1_;
        dcX1_ = mixed;
        dcY1_ = dc;

        float out = fasttanh(dc * 2.f);
        return { out, out };
    }

    void Setup(float sr, std::shared_ptr<InterfaceBase> iface) override
    {
        AudioAppBase<NPARAMS>::Setup(sr, iface);
        maxiSettings::sampleRate = sr;
        sampleRate_ = sr;
        grainDelay1_.setup(sr);
        grainDelay2_.setup(sr);
        grainDelay3_.setup(sr);
        grainDelay1_.setClampWrite(true);
        grainDelay2_.setClampWrite(true);
        grainDelay3_.setClampWrite(true);
        fillTable();
        setDetune(0.f);
        incA_ = targetIncA_;
        incB_ = targetIncB_;
        if (!pitchLUTReady_) {
            for (size_t i = 0; i < kPitchLUTSize; ++i)
                pitchLUT_[i] = exp2f((float)i / (kPitchLUTSize - 1) * 2.f - 1.f);
            pitchLUTReady_ = true;
        }
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        {
            uint8_t w;
            if (queue_try_remove(&waveformQueue, &w)) {
                waveform_ = static_cast<Waveform>(w);
                fillTable();
            }
        }

        {
            float v;
            if (queue_try_remove(&feedbackFactorQueue, &v)) feedbackFactor_ = v;
        }

        // params 0-4: grain delay 1
        grainDelay1_.setGrainLengthMs(10.f + params[0] * 490.f);
        grainDelay1_.setStartTimeMs(  10.f + params[1] * 320.f);
        grainDelay1_.setFeedback(   fminf(params[2] * feedbackFactor_, 1.f) * 0.95f);
        grainDelay1_.setPitch(      pitchRatio(params[3]));
        grainDelay1_.setPitchSpread(params[4] * 0.3f);

        // params 5-7, 13-14: grain delay 3 (captures the grain mix, feeds back into 1+2)
        grainDelay3_.setGrainLengthMs(10.f + params[5] * 490.f);
        grainDelay3_.setStartTimeMs(  10.f + params[6] * 320.f);
        grainDelay3_.setFeedback(   fminf(params[7] * feedbackFactor_, 1.f) * 0.95f);
        grainDelay3_.setPitch(      pitchRatio(params[13]));
        grainDelay3_.setPitchSpread(params[14] * 0.3f);
        // param 15: grain3 → grain1 cross-feedback; param 18: grain3 → grain2 cross-feedback
        feedbackAmount1_ = params[15] * feedbackFactor_;
        feedbackAmount2_ = params[18] * feedbackFactor_;

        // params 8-12: grain delay 2
        grainDelay2_.setGrainLengthMs(10.f + params[8] * 490.f);
        grainDelay2_.setStartTimeMs(  10.f + params[9] * 320.f);
        grainDelay2_.setFeedback(   fminf(params[10] * feedbackFactor_, 1.f) * 0.95f);
        grainDelay2_.setPitch(      pitchRatio(params[11]));
        grainDelay2_.setPitchSpread(params[12] * 0.3f);

        // param 16: crossfade Gr1 ↔ Gr2
        grainCrossfade_ = params[16];
        // param 17: oscillators ↔ grains
        wetdry_ = params[17];
        // param 19: detune between the two oscillators
        setDetune(params[19]);
    }

protected:
    GrainDelayI16<16384, 2> grainDelay1_;
    GrainDelayI16<16384, 2> grainDelay2_;
    GrainDelayI16<16384, 2> grainDelay3_;  // captures output of 1+2, feeds back into them

    Waveform waveform_     {Waveform::Saw};

    float grainDelay3Out_  {0.f};
    float feedbackAmount1_ {0.f};
    float feedbackAmount2_ {0.f};
    float feedbackFactor_  {1.f};
    float grainCrossfade_{0.5f};
    float wetdry_{0.5f};
    float sampleRate_{48000.f};

    // Oscillators: phase in table samples. Hot-path constants live in SRAM (non-const).
    inline static float table_[kTableSize] = {};
    float phaseA_ {0.f};
    float phaseB_ {0.f};
    float incA_ {1.f}, incB_ {1.f};
    float targetIncA_ {1.f}, targetIncB_ {1.f};
    float kTableSizeF_ {static_cast<float>(kTableSize)};
    float kIncSmooth_ {0.001f};
    float kOscGain_ {0.4f};
    float kDCCoeff_ {0.995f};
    float dcX1_ {0.f}, dcY1_ {0.f};

    __force_inline float readTable(float phase) const {
        const size_t i = static_cast<size_t>(phase);
        const size_t j = (i + 1 < kTableSize) ? i + 1 : 0;
        const float frac = phase - static_cast<float>(i);
        return table_[i] + frac * (table_[j] - table_[i]);
    }

    // Oscillators sit symmetrically around the fundamental, so it stays at 80 Hz.
    void setDetune(float amount) {
        const float halfOctaves = amount * kMaxDetuneCents / 2400.f;
        const float base = kFundamentalHz * kTableSize / sampleRate_;
        targetIncA_ = base * exp2f(-halfOctaves);
        targetIncB_ = base * exp2f( halfOctaves);
    }

    // One cycle of the chosen shape. At 80 Hz the harmonics that fold back past
    // Nyquist are above the 300th, so naive shapes are clean enough here.
    void fillTable() {
        for (size_t i = 0; i < kTableSize; ++i) {
            const float p = static_cast<float>(i) / kTableSize;
            float v = 0.f;
            switch (waveform_) {
                case Waveform::Saw:          v = p * 2.f - 1.f; break;
                case Waveform::Square:       v = p < 0.5f ? 1.f : -1.f; break;
                case Waveform::Triangle:     v = 2.f * fabsf(2.f * p - 1.f) - 1.f; break;
                case Waveform::FallingSaw:   v = 1.f - p * 2.f; break;
                case Waveform::Sine:         v = sinf(p * 2.f * static_cast<float>(M_PI)); break;
                case Waveform::HalfRectSine: v = fmaxf(0.f, sinf(p * 2.f * static_cast<float>(M_PI))); break;
                case Waveform::Trapezoid:
                    if      (p < 0.2f) v = p / 0.2f * 2.f - 1.f;
                    else if (p < 0.5f) v = 1.f;
                    else if (p < 0.7f) v = 1.f - (p - 0.5f) / 0.2f * 2.f;
                    else               v = -1.f;
                    break;
                case Waveform::AsymTriangle: v = p < 0.25f ? p / 0.25f * 2.f - 1.f
                                                           : 1.f - (p - 0.25f) / 0.75f * 2.f; break;
                case Waveform::Staircase:    v = floorf(p * 8.f) * 2.f / 7.f - 1.f; break;
                default: break;
            }
            table_[i] = v;
        }
    }

    static constexpr size_t kPitchLUTSize = 256;
    inline static float pitchLUT_[kPitchLUTSize] = {};
    inline static bool  pitchLUTReady_ = false;

    static __force_inline float pitchRatio(float param) {
        const float idx = param * (kPitchLUTSize - 1);
        const size_t i  = (size_t)idx;
        if (i >= kPitchLUTSize - 1) return pitchLUT_[kPitchLUTSize - 1];
        return pitchLUT_[i] + (idx - (float)i) * (pitchLUT_[i + 1] - pitchLUT_[i]);
    }

    static __force_inline float fasttanh(float x) {
        const float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }
};

#endif
