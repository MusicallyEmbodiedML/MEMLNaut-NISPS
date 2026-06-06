#ifndef __DJFX_AUDIO_APP_HPP__
#define __DJFX_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/GrainDelayI16.hpp"
#include "../../src/memllib/synth/ModFXI16.hpp"
#include "../../src/memllib/synth/ReverbI16.hpp"

// 46 original FX params + 11 reverb params (46-56) = 57.
template<size_t NPARAMS=57>
class DJFXAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;

    // Effect enable bitmask — written by UI (Core 0), read by audio (Core 1).
    // 32-bit aligned volatile write/read is atomic on Cortex-M0+.
    static constexpr uint32_t kFX_Allpass    = 1u << 0;
    static constexpr uint32_t kFX_Flanger    = 1u << 1;
    static constexpr uint32_t kFX_Chorus     = 1u << 2;
    static constexpr uint32_t kFX_Grain1     = 1u << 3;
    static constexpr uint32_t kFX_Grain2     = 1u << 4;
    static constexpr uint32_t kFX_Delay      = 1u << 5;
    static constexpr uint32_t kFX_Downsample = 1u << 6;
    static constexpr uint32_t kFX_Stutter    = 1u << 7;
    static constexpr uint32_t kFX_RingMod    = 1u << 8;
    static constexpr uint32_t kFX_Reverb     = 1u << 9;
    static constexpr uint32_t kFX_All        = (1u << 10) - 1;  // all 10 FX enabled

    volatile uint32_t enableMask_{kFX_All};

    queue_t wetdryQueue;
    queue_t bpmQueue;
    queue_t rvx1Queue;

    void setWetDryQueued(float value) {
        queue_try_add(&wetdryQueue, &value);
    }

    void setBPMQueued(float bpm) {
        queue_try_add(&bpmQueue, &bpm);
    }

    void setRVX1Queued(float value) {
        queue_try_add(&rvx1Queue, &value);
    }

    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input = false,
            .line_level = 3,
            .mic_gain_dB = 0,
            .output_volume = 0.97f
        };
    }

    DJFXAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&wetdryQueue, sizeof(float), 1);
        queue_init(&bpmQueue, sizeof(float), 1);
        queue_init(&rvx1Queue, sizeof(float), 1);
    };

    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        const uint32_t en = enableMask_;
        float mix = (x.L + x.R) * 0.5f;

        // RVX1 DJ filter — on the mono input, before any FX
        if (rvx1Val_ < 0.499f)
            mix = rvLP_.play(mix);
        else if (rvx1Val_ > 0.501f)
            mix = rvHP_.play(mix);

        // Tone shapers — at the head so they colour the source before any time-based FX
        if ((en & kFX_Downsample) && downsampleMix_ > 0.f)
            mix = downSampler_.process(mix, 16.f, downsampleRate_, downsampleMix_);
        if ((en & kFX_RingMod) && ringModMix_ > 0.f)
            mix = ringMod_.process(mix, ringModFreq_, ringModMix_);

        // Modulation
        if ((en & kFX_Allpass) && apMix_ > 0.f) {
            const float apLfo = apLFO_.sinewave(apLFORate_);
            const float apOut = allpass_.process(mix, fmaxf(1.f, apDelay_ + apLFODepth_ * apLfo), apG_);
            mix = mix * (1.f - apMix_) + apOut * apMix_;
        }
        if ((en & kFX_Flanger) && flangerMix_ > 0.f)
            mix = flanger_.process(mix, flangerBase_, flangerDepth_, flangerRate_, flangerFeedback_, flangerMix_);
        if ((en & kFX_Chorus) && chorusMix_ > 0.f)
            mix = chorus_.process(mix, chorusBase_, chorusDepth_, chorusRate_, chorusFeedback_, chorusMix_);

        // Grain delays — parallel, blended by crossfade, then grain wet/dry
        if (grainMix_ > 0.f) {
            const bool g1en = (en & kFX_Grain1) != 0;
            const bool g2en = (en & kFX_Grain2) != 0;
            if (g1en || g2en) {
                const float g1 = g1en ? grainDelay.process(mix) : 0.f;
                const float g2 = g2en ? grainDelay2.process(mix) : 0.f;
                const float blended = (g1en && g2en)
                    ? g1 * (1.f - grainCrossfade_) + g2 * grainCrossfade_
                    : (g1en ? g1 : g2);
                mix = mix * (1.f - grainMix_) + blended * grainMix_;
            }
        }

        // Delay with feedback bandpass
        if ((en & kFX_Delay) && delayLevel_ > 0.f) {
            const float delayOut = delay500ms_.read(delayTimeSamples_);
            const float fb = delayOut * delayFeedback_;
            const float fbFiltered = delayBP_.play(fb);
            delay500ms_.write(mix + fb * (1.f - delayBPMix_) + fbFiltered * delayBPMix_);
            mix += delayOut * delayLevel_;
        }

        // Stutter
        if ((en & kFX_Stutter) && stutterMix_ > 0.f)
            mix = stutterGate_.process(mix, stutterPeriod_, stutterDuty_, stutterMix_);

        // Makeup gain
        mix = tanhf(mix * 4.0f);

        // Reverb — on the wet FX bus, produces a stereo pair
        float wetL = mix, wetR = mix;
        if ((en & kFX_Reverb) && reverbMix_ > 0.f) {
            auto [revL, revR] = reverb_.process(mix);
            const float rWet = sqrtf(reverbMix_);
            const float rDry = sqrtf(1.f - reverbMix_);
            wetL = mix * rDry + revL * rWet;
            wetR = mix * rDry + revR * rWet;
        }

        // Wet/dry crossfade — stereo wet added to stereo dry
        const float dryAmt = sqrtf(1.f - wetdry_mix_);
        const float wetAmt = sqrtf(wetdry_mix_);
        return { wetL * wetAmt + x.L * dryAmt, wetR * wetAmt + x.R * dryAmt };
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;

        // Delay feedback bandpass — start centred, broad
        delayBP_.set(maxiBiquad::filterTypes::BANDPASS, 1000.f, 1.f, 0.f);

        // RVX1 filter — start transparent (LP well above audible, HP well below)
        rvLP_.set(maxiBiquad::filterTypes::LOWPASS,  20000.f, 0.707f, 0.f);
        rvHP_.set(maxiBiquad::filterTypes::HIGHPASS,    20.f, 0.707f, 0.f);

        grainDelay.setup(sample_rate);
        grainDelay2.setup(sample_rate);
        reverb_.setup(sample_rate);
        sampleRate_ = sample_rate;
    }

    static float __force_inline effectMix(float raw) {
        return (raw > 0.3f) ? (raw - 0.3f) * (1.f / 0.7f) : 0.f;
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        {
            float v;
            if (queue_try_remove(&wetdryQueue, &v)) wetdry_mix_ = v;
        }
        {
            float newBPM;
            if (queue_try_remove(&bpmQueue, &newBPM)) bpm_ = newBPM;
        }
        {
            float rv;
            if (queue_try_remove(&rvx1Queue, &rv)) {
                rvx1Val_ = rv;
                if (rv < 0.499f) {
                    // Knob left of centre → LP sweeps 20kHz→40Hz (exponential)
                    const float frac   = (0.5f - rv) / 0.5f;
                    const float cutoff = 20000.f * powf(40.f / 20000.f, frac);
                    rvLP_.set(maxiBiquad::filterTypes::LOWPASS, cutoff, 0.707f, 0.f);
                } else if (rv > 0.501f) {
                    // Knob right of centre → HP sweeps 20Hz→20kHz (exponential)
                    const float frac   = (rv - 0.5f) / 0.5f;
                    const float cutoff = 20.f * powf(20000.f / 20.f, frac);
                    rvHP_.set(maxiBiquad::filterTypes::HIGHPASS, cutoff, 0.707f, 0.f);
                }
            }
        }

        const float beatMs      = 60000.f / bpm_;
        const float beatSamples = sampleRate_ * 60.f / bpm_;

        // Beat subdivisions shared by grain start-time and tap delay selectors.
        // 8 steps: 1/16, 1/8, 1/4, 3/8, 1/2, 3/4, 1, 3/2 beats
        static const float kBeatSubdivs[] = {
            1.f/16, 1.f/8, 1.f/4, 3.f/8, 1.f/2, 3.f/4, 1.f, 3.f/2
        };

        // params 0-7: grainDelay  (~682ms buffer at 48kHz)
        grainDelay.setGrainLengthMs(30.f + params[0] * 220.f);  // linear 30-250ms (texture)
        {
            const int idx = static_cast<int>(params[1] * 7.99f);
            grainDelay.setStartTimeMs(fminf(beatMs * kBeatSubdivs[idx], 670.f));
        }
        grainDelay.setFeedback(          params[2]);
        grainDelay.setPitch(             powf(2.f, params[3] * 2.f - 1.f));
        grainDelay.setPitchSpread(       params[4] * 0.3f);
        grainDelay.setFreeze(            params[7] > 0.9f);

        // params 8-15: grainDelay2  (~682ms buffer at 48kHz)
        grainDelay2.setGrainLengthMs(10.f + params[8]  * 90.f);  // linear 10-100ms
        {
            const int idx = static_cast<int>(params[9] * 7.99f);
            grainDelay2.setStartTimeMs(fminf(beatMs * kBeatSubdivs[idx], 670.f));
        }
        grainDelay2.setFeedback(           params[10]);
        grainDelay2.setPitch(              powf(2.f, params[11] * 2.f - 1.f));
        grainDelay2.setPitchSpread(        params[12]);
        grainDelay2.setFreeze(             params[15] > 0.9f);

        // param 16: grain crossfade (Gr1 → Gr2)
        grainCrossfade_ = params[16];
        // param 17: grain section wet/dry mix
        grainMix_       = effectMix(params[17]);

        // params 18-20: dynamic delay — 5 BPM subdivisions (1/16 to 1/2 beat, ~341ms max)
        {
            static const float kDelaySubdivs[] = {1.f/16, 1.f/8, 1.f/4, 3.f/8, 1.f/2};
            const int idx = static_cast<int>(params[18] * 4.99f);
            delayTimeSamples_ = fminf(beatSamples * kDelaySubdivs[idx], 16383.f);
        }
        delayFeedback_    = params[19] * 0.95f;
        delayLevel_       = effectMix(params[20]);

        // params 21-25: flanger (delayBase 5-240 samples, depth, rate 0.1-5 Hz, feedback, mix)
        flangerBase_     = 5.f + params[21] * 235.f;
        flangerDepth_    = params[22];
        flangerRate_     = 0.1f + params[23] * 4.9f;
        flangerFeedback_ = params[24] * 0.9f;
        flangerMix_      = effectMix(params[25]);

        // params 26-30: chorus (delayBase 240-1440 samples, depth, rate 0.1-3 Hz, feedback, mix)
        chorusBase_     = 240.f + params[26] * 1200.f;
        chorusDepth_    = params[27] * 0.5f;
        chorusRate_     = 0.1f + params[28] * 2.9f;
        chorusFeedback_ = params[29] * 0.5f;
        chorusMix_      = effectMix(params[30]);

        // params 31-33: allpass (delay 100-2000 samples, g 0.3-0.8, mix)
        apDelay_ = 100.f + params[31] * 1900.f;
        apG_     = 0.3f  + params[32] * 0.5f;
        apMix_   = effectMix(params[33]);

        // params 34-35: allpass LFO (rate 0.1-10 Hz, depth 0-500 samples)
        apLFORate_  = 0.1f + params[34] * 9.9f;
        apLFODepth_ = params[35] * 500.f;

        // params 36-37: downsampler (rateDiv 1-32, mix)
        downsampleRate_ = 1.f + params[36] * 31.f;
        downsampleMix_  = effectMix(params[37]);

        // params 38-40: stutter gate (subdivision, duty cycle, mix)
        {
            static const float kSubdivisions[] = {1.f, 2.f, 4.f, 8.f, 16.f};
            const int idx = static_cast<int>(params[38] * 4.99f);
            const float subdiv = kSubdivisions[idx];
            stutterPeriod_ = (sampleRate_ * 60.f) / (bpm_ * subdiv);
        }
        stutterDuty_ = 0.05f + params[39] * 0.9f;
        stutterMix_  = effectMix(params[40]);

        // params 41-42: ring mod (freq 1-2000 Hz, mix)
        ringModFreq_ = powf(2000.f, params[41]);
        ringModMix_  = effectMix(params[42]);

        // params 43-45: delay feedback bandpass (freq 40–16kHz exp, Q 0.5–10, mix)
        {
            const float freq = 40.f * powf(400.f, params[43]);
            const float q    = 0.5f + params[44] * 9.5f;
            delayBPMix_      = effectMix(params[45]);
            delayBP_.set(maxiBiquad::filterTypes::BANDPASS, freq, q, 0.f);
        }

        // params 46-56: reverb (copied from SaxFX)
        reverb_.setSize(       params[46]);
        reverb_.setDecay(      params[47]);
        reverb_.setDamping(    params[48]);
        reverb_.setDiffusion(  params[49]);
        reverb_.setModDepth(   params[50]);
        reverb_.setModRate(    params[51]);
        reverb_.setPreDelay(   params[52]);
        reverb_.setLowCut(     params[53]);
        reverb_.setStereoWidth(params[54]);
        reverb_.setSaturation( params[55]);
        reverbMix_ = effectMix(params[56]);
    }

protected:
    // RVX1 DJ filter — mono, applied before all FX
    maxiBiquad rvLP_;
    maxiBiquad rvHP_;
    float rvx1Val_{0.5f};

    maxiBiquad delayBP_;

    GrainDelayI16<16384*2> grainDelay;
    GrainDelayI16<8192*4>  grainDelay2;
    DynamicDelayI16<16384> delay500ms_;

    FlangerI16<1024>  flanger_;
    ChorusI16<4096>   chorus_;
    AllpassI16<4096>  allpass_;
    BitCrusher        downSampler_;
    RingMod           ringMod_;
    StutterGate       stutterGate_;
    maxiOsc           apLFO_;
    ReverbI16<4096>   reverb_;   // reverted from ReverbI16Large (startup crash — likely RAM)
    float             reverbMix_{0.f};

    float wetdry_mix_{0.5f};
    float grainCrossfade_{0.5f};
    float grainMix_{1.f};
    float bpm_{100.f};
    float delayTimeSamples_{8192.f};
    float delayFeedback_{0.f};
    float delayLevel_{0.f};
    float delayBPMix_{0.f};
    float flangerBase_{48.f}, flangerDepth_{0.f}, flangerRate_{0.5f};
    float flangerFeedback_{0.f}, flangerMix_{0.f};
    float chorusBase_{480.f}, chorusDepth_{0.f}, chorusRate_{0.5f};
    float chorusFeedback_{0.f}, chorusMix_{0.f};
    float sampleRate_{48000.f};
    float apDelay_{500.f}, apG_{0.5f}, apMix_{0.f};
    float apLFORate_{1.f}, apLFODepth_{0.f};
    float downsampleRate_{1.f}, downsampleMix_{0.f};
    float stutterPeriod_{12000.f}, stutterDuty_{0.5f}, stutterMix_{0.f};
    float ringModFreq_{100.f}, ringModMix_{0.f};
};

#endif
