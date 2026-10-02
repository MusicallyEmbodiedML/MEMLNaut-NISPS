#ifndef __CHUNKYBITS_AUDIO_APP_HPP__
#define __CHUNKYBITS_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/GrainDelayI16.hpp"

template<size_t NPARAMS = 20>
class ChunkyBitsAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;

    static constexpr uint8_t kArpNotes[7] = {18, 30, 42, 54, 66, 78, 90};
    static constexpr size_t  kArpLen      = 7;
    static constexpr float   kArpBPM      = 147.f;

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

    queue_t noteOnQueue;
    queue_t feedbackFactorQueue;
    queue_t envQueue;
    queue_t waveformQueue;

    ChunkyBitsAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&noteOnQueue, 2, 4);
        queue_init(&feedbackFactorQueue, sizeof(float), 1);
        queue_init(&envQueue, sizeof(float), 1);
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
        // Advance arpeggiator phasor one sample at a time
        arpPhasor_ += arpPhaseInc_;
        if (arpPhasor_ >= 1.f) {
            arpPhasor_ -= 1.f;
            arpFreq_    = mtof(kArpNotes[arpStep_ % kArpLen]);
            ++arpStep_;
            arpTrigger_ = true;
        }

        envVal_ = env_.play();

        float g1 = grainDelay1_.process(0.f, grainDelay3Out_ * feedbackAmount1_);
        float g2 = grainDelay2_.process(0.f, grainDelay3Out_ * feedbackAmount2_);
        float mixed = g1 * (1.f - grainCrossfade_) + g2 * grainCrossfade_;

        // Grain3 captures the mixed output
        grainDelay3Out_ = grainDelay3_.process(mixed);

        float out = fasttanh(mixed * 2.f) * envVal_;
        return { out, out };
    }

    void Setup(float sr, std::shared_ptr<InterfaceBase> iface) override
    {
        AudioAppBase<NPARAMS>::Setup(sr, iface);
        maxiSettings::sampleRate = sr;
        sampleRate_ = sr;
        arpPhaseInc_ = kArpBPM / (60.f * sr);
        grainDelay1_.setup(sr);
        grainDelay2_.setup(sr);
        grainDelay3_.setup(sr);
        lastFillFreq_ = mtof(kArpNotes[0]);
        fillBuffers(lastFillFreq_);
        env_.setup(50.f, 700.f, 0.f, 1.f, sr);
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
                fillBuffers(lastFillFreq_);
            }
        }

        // Arpeggiator trigger — set by Process() ISR, consumed here
        if (arpTrigger_) {
            arpTrigger_ = false;
            lastFillFreq_ = arpFreq_;
            fillBuffers(lastFillFreq_);
            env_.trigger(1.f);
        }

        // External MIDI note-on overrides arp for this step
        uint8_t msg[2];
        while (queue_try_remove(&noteOnQueue, msg)) {
            lastFillFreq_ = mtof(msg[0]);
            fillBuffers(lastFillFreq_);
            env_.trigger(1.f);
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

        // params 5-7, 13-14: grain delay 3 (captures live output, feeds back into 1+2)
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
        // param 17: wet/dry mix (NN-controlled)
        wetdry_ = params[17];

        float dummy; queue_try_remove(&envQueue, &dummy);
        queue_try_add(&envQueue, &envVal_);
    }

protected:
    GrainDelayI16<16384, 2> grainDelay1_;
    GrainDelayI16<16384, 2> grainDelay2_;
    GrainDelayI16<16384, 2> grainDelay3_;  // captures output of 1+2, feeds back into them

    Waveform waveform_     {Waveform::Saw};
    float lastFillFreq_    {0.f};

    void fillBuffers(float freq) {
        switch (waveform_) {
            case Waveform::Saw:          grainDelay1_.fillWithSaw(freq);          grainDelay2_.fillWithSaw(freq);          break;
            case Waveform::Square:       grainDelay1_.fillWithSquare(freq);       grainDelay2_.fillWithSquare(freq);       break;
            case Waveform::Triangle:     grainDelay1_.fillWithTriangle(freq);     grainDelay2_.fillWithTriangle(freq);     break;
            case Waveform::FallingSaw:   grainDelay1_.fillWithFallingSaw(freq);   grainDelay2_.fillWithFallingSaw(freq);   break;
            case Waveform::Sine:         grainDelay1_.fillWithSine(freq);         grainDelay2_.fillWithSine(freq);         break;
            case Waveform::HalfRectSine: grainDelay1_.fillWithHalfRectSine(freq); grainDelay2_.fillWithHalfRectSine(freq); break;
            case Waveform::Trapezoid:    grainDelay1_.fillWithTrapezoid(freq);    grainDelay2_.fillWithTrapezoid(freq);    break;
            case Waveform::AsymTriangle: grainDelay1_.fillWithAsymTriangle(freq); grainDelay2_.fillWithAsymTriangle(freq); break;
            case Waveform::Staircase:    grainDelay1_.fillWithStaircase(freq);    grainDelay2_.fillWithStaircase(freq);    break;
            default: break;
        }
    }

    float grainDelay3Out_  {0.f};
    float feedbackAmount1_ {0.f};
    float feedbackAmount2_ {0.f};
    float feedbackFactor_  {1.f};
    float grainCrossfade_{0.5f};
    float wetdry_{0.5f};
    float sampleRate_{48000.f};

    // Arpeggiator state
    float        arpPhasor_   {0.f};
    float        arpPhaseInc_ {0.f};
    float        arpFreq_     {0.f};
    size_t       arpStep_     {0};
    volatile bool arpTrigger_ {false};

    // AR envelope (triggered on each note, value sent to Core 0 as NN input)
    ADSRLite     env_;
    float        envVal_      {0.f};

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

    static float mtof(uint8_t note) {
        return 440.f * powf(2.f, (static_cast<float>(note) - 69.f) / 12.f);
    }
};

#endif
