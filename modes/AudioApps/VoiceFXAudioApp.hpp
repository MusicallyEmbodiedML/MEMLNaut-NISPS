#ifndef __VOICEFX_AUDIO_APP_HPP__
#define __VOICEFX_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/GrainDelayI16.hpp"
#include "../../src/memllib/synth/ModFXI16.hpp"
#include "../../src/memllib/synth/ReverbI16.hpp"

// VoiceFX — a clone of LiveFX (DJFXAudioApp) without the downsampler, stutter gate
// and ring mod, and without the DJ filter / DJ Mode. Wet/dry is set directly (knob Y).
// The remaining effects and their parameter mappings are unchanged from LiveFX.
//
// Chain: wet = mono effects bus (allpass, chorus, grains, delay, saturation,
// reverb -> stereo) -> stereo flanger -> compressor (stereo-linked) -> peaking EQ in
// the vocal range (with its own wet/dry). Out = wet/dry (knob Y) of that wet signal
// and the untouched input, so knob Y fully dry bypasses every effect.
//
// Params (59):
//   0-7   grain delay 1            8-15  grain delay 2
//   16-17 grain crossfade, grain wet/dry
//   18-20 delay (time, feedback, level)
//   21-25 stereo flanger (max delay, depth, rate, feedback, mix)
//   26-30 chorus
//   31-35 allpass + LFO
//   36-38 delay feedback bandpass (freq, Q, mix)
//   39-49 reverb
//   50-54 compressor (threshold, ratio, attack, release, makeup)
//   55-58 EQ (frequency, gain, Q, wet/dry)
// Stereo flanger: float delay lines, exponential sweep (notches move evenly in pitch)
// from kMinDelay up to a set maximum, the right LFO 90 degrees behind the left,
// bipolar feedback. mix 1 = 50/50, the deepest notches.
class VoiceFlanger {
public:
    static constexpr int kN = 1024;            // > 10 ms at 48 kHz
    static constexpr float kMinDelayS = 0.0002f;

    void setup(float sampleRate) {
        fs_ = sampleRate;
        minOct_ = log2f(kMinDelayS * fs_);
        centreOct_ = centreOctT_ = minOct_ + 1.f;
    }
    // maxDelayS: 1..10 ms; depth 0..1 (share of the min..max range swept);
    // rateHz; feedback -0.9..0.9; mix 0..1
    void setParams(float maxDelayS, float depth, float rateHz, float feedback, float mix) {
        const float maxOct = log2f(maxDelayS * fs_);
        centreOctT_ = 0.5f * (minOct_ + maxOct);
        spanT_      = 0.5f * depth * (maxOct - minOct_);
        rateInc_    = rateHz / fs_;
        fb_         = feedback;
        mix_        = mix;
    }

    __force_inline void process(float& L, float& R) {
        centreOct_ += glide_ * (centreOctT_ - centreOct_);
        span_      += glide_ * (spanT_ - span_);
        phase_ += rateInc_;
        if (phase_ >= 1.f) phase_ -= 1.f;
        const float dL = FastMath::exp2(centreOct_ + span_ * FastMath::sinTurns(phase_));
        const float dR = FastMath::exp2(centreOct_ + span_ * FastMath::sinTurns(phase_ + 0.25f));
        const float oL = read(bufL_, dL);
        const float oR = read(bufR_, dR);
        bufL_[w_] = L + oL * fb_;
        bufR_[w_] = R + oR * fb_;
        w_ = (w_ + 1) & (kN - 1);
        const float dry = 1.f - mix_ * 0.5f, wet = mix_ * 0.5f;
        L = L * dry + oL * wet;
        R = R * dry + oR * wet;
    }

private:
    __force_inline float read(const float* buf, float d) const {
        float pos = static_cast<float>(w_) - d;
        if (pos < 0.f) pos += static_cast<float>(kN);
        const int i0 = static_cast<int>(pos);
        const float f = pos - static_cast<float>(i0);
        const float a = buf[i0 & (kN - 1)], b = buf[(i0 + 1) & (kN - 1)];
        return a + f * (b - a);
    }
    float bufL_[kN]{}, bufR_[kN]{};
    int w_{0};
    float fs_{48000.f}, minOct_{0.f};
    float centreOct_{0.f}, centreOctT_{0.f}, span_{0.f}, spanT_{0.f};
    float phase_{0.f}, rateInc_{0.f}, fb_{0.f}, mix_{0.f};
    float glide_{0.001f};                      // ~20 ms smoothing of sweep changes
};

template<size_t NPARAMS=59>
class VoiceFXAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;

    // Effect enable bitmask — written by UI (Core 0), read by audio (Core 1).
    static constexpr uint32_t kFX_Allpass = 1u << 0;
    static constexpr uint32_t kFX_Flanger = 1u << 1;
    static constexpr uint32_t kFX_Chorus  = 1u << 2;
    static constexpr uint32_t kFX_Grain1  = 1u << 3;
    static constexpr uint32_t kFX_Grain2  = 1u << 4;
    static constexpr uint32_t kFX_Delay   = 1u << 5;
    static constexpr uint32_t kFX_Reverb  = 1u << 6;
    static constexpr uint32_t kFX_Comp    = 1u << 7;
    static constexpr uint32_t kFX_EQ      = 1u << 8;
    static constexpr uint32_t kNFX        = 9;
    static constexpr uint32_t kFX_All     = (1u << kNFX) - 1;  // all FX enabled

    volatile uint32_t enableMask_{kFX_All};

    queue_t wetdryQueue;
    queue_t bpmQueue;

    void setWetDryQueued(float value) {
        queue_try_add(&wetdryQueue, &value);
    }

    void setBPMQueued(float bpm) {
        queue_try_add(&bpmQueue, &bpm);
    }

    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input = false,
            .line_level = 3,
            .mic_gain_dB = 0,
            .output_volume = 0.97f
        };
    }

    VoiceFXAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&wetdryQueue, sizeof(float), 1);
        queue_init(&bpmQueue, sizeof(float), 1);
    };

    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        const uint32_t en = enableMask_;
        // Wet bus is mono (sums L+R); the dry path stays stereo (x.L/x.R passthrough).
        float mix = (x.L + x.R) * 0.5f;

        // Modulation
        if ((en & kFX_Allpass) && apMix_ > 0.f) {
            const float apLfo = apLFO_.sinewave(apLFORate_);
            const float apOut = allpass_.process(mix, fmaxf(1.f, apDelay_ + apLFODepth_ * apLfo), apG_);
            mix = mix * (1.f - apMix_) + apOut * apMix_;
        }
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

        // Makeup gain (inline tanh: tanhf is a wrapped float routine in flash)
        mix = FastMath::tanh(mix * 4.0f);

        // Reverb — on the wet FX bus, produces a stereo pair
        float wetL = mix, wetR = mix;
        if ((en & kFX_Reverb) && reverbMix_ > 0.f) {
            auto [revL, revR] = reverb_.process(mix);
            if (reverbMix_ != revMixCached_) {   // sqrt only when the mix changes
                revMixCached_ = reverbMix_;
                rWet_ = sqrtf(reverbMix_);
                rDry_ = sqrtf(1.f - reverbMix_);
            }
            wetL = mix * rDry_ + revL * rWet_;
            wetR = mix * rDry_ + revR * rWet_;
        }

        // Stereo flanger on the wet signal, after the reverb (so the later bus effects
        // don't smear its notches).
        if ((en & kFX_Flanger) && flangerMix_ > 0.f) flanger_.process(wetL, wetR);

        // Compressor on the wet signal: stereo-linked, feed-forward, peak level with
        // attack/release. Above the threshold, gain = (level / threshold)^(1/ratio - 1),
        // via inline exp2/log2 (no flash calls).
        if (en & kFX_Comp) {
            const float aL = wetL < 0.f ? -wetL : wetL;
            const float aR = wetR < 0.f ? -wetR : wetR;
            const float lvl = aL > aR ? aL : aR;
            compEnv_ += ((lvl > compEnv_) ? compAtk_ : compRel_) * (lvl - compEnv_);
            float g = compMakeup_;
            if (compEnv_ > compThr_)
                g *= FastMath::exp2(compSlope_ * FastMath::log2(compEnv_ * compInvThr_));
            wetL *= g;
            wetR *= g;
        }

        // EQ after the compressor: one peaking band per channel, with its own wet/dry.
        if (en & kFX_EQ) {
            if (++eqTick_ >= kEQUpdateInterval) { eqTick_ = 0; updateEQ(); }
            if (eqMix_ > 0.f) {
                wetL += eqMix_ * (eqL_.play(wetL) - wetL);
                wetR += eqMix_ * (eqR_.play(wetR) - wetR);
            }
        }

        // Wet/dry crossfade (knob Y), last: dry is the untouched input, wet has every
        // effect (equal power).
        if (wetdry_mix_ != wetdryCached_) {      // sqrt only when wet/dry changes
            wetdryCached_ = wetdry_mix_;
            wetAmt_ = sqrtf(wetdry_mix_);
            dryAmt_ = sqrtf(1.f - wetdry_mix_);
        }
        return { wetL * wetAmt_ + x.L * dryAmt_, wetR * wetAmt_ + x.R * dryAmt_ };
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;

        // Delay feedback bandpass — start centred, broad
        delayBP_.set(maxiBiquad::filterTypes::BANDPASS, 1000.f, 1.f, 0.f);

        // EQ starts flat at 1 kHz.
        eqL_.set(maxiBiquad::filterTypes::PEAK, 1000.f, 1.f, 0.f);
        eqR_.set(maxiBiquad::filterTypes::PEAK, 1000.f, 1.f, 0.f);

        flanger_.setup(sample_rate);
        grainDelay.setup(sample_rate);
        grainDelay2.setup(sample_rate);
        reverb_.setup(sample_rate);
        sampleRate_ = sample_rate;
    }

    // Glide the EQ towards its targets (frequency in octaves) and recompute the
    // coefficients, only while it is moving. Every kEQUpdateInterval samples: set()
    // calls tanf/powf, so it must not run per sample.
    void updateEQ() {
        const float dOct  = eqOctT_ - eqOct_;
        const float dGain = eqGainT_ - eqGain_;
        const float dQ    = eqQT_ - eqQ_;
        if (fabsf(dOct) < 1e-3f && fabsf(dGain) < 1e-2f && fabsf(dQ) < 1e-3f) return;
        eqOct_  += eqGlide_ * dOct;
        eqGain_ += eqGlide_ * dGain;
        eqQ_    += eqGlide_ * dQ;
        const float freq = FastMath::exp2(eqOct_);
        eqL_.set(maxiBiquad::filterTypes::PEAK, freq, eqQ_, eqGain_);
        eqR_.set(maxiBiquad::filterTypes::PEAK, freq, eqQ_, eqGain_);
    }

    static float __force_inline effectMix(float raw) {
        return (raw > 0.3f) ? (raw - 0.3f) * (1.f / 0.7f) : 0.f;
    }

    // Wet/dry (knob Y) and BPM (tap tempo) arrive on their own queues from Core 0 and
    // are drained every audio-core iteration, not only when a new NN action arrives.
    void pollControls()
    {
        {
            float v;
            if (queue_try_remove(&wetdryQueue, &v)) wetdry_mix_ = v;
        }
        {
            float newBPM;
            if (queue_try_remove(&bpmQueue, &newBPM)) bpm_ = newBPM;
        }
    }

    // Runs every Core 1 iteration (loop1), independent of NN action generation.
    void loop() override
    {
        pollControls();
        AudioAppBase<NPARAMS>::loop();      // FX params only when a new NN action arrives
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
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

        // params 21-25: stereo flanger (max delay 1-10 ms exp, depth, rate 0.05-2 Hz exp,
        // feedback -0.9..0.9, mix)
        flangerMix_ = effectMix(params[25]);
        flanger_.setParams(0.001f * powf(10.f, params[21]),
                           params[22],
                           0.05f * powf(40.f, params[23]),
                           params[24] * 1.8f - 0.9f,
                           flangerMix_);

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

        // params 36-38: delay feedback bandpass (freq 40–16kHz exp, Q 0.5–10, mix)
        {
            const float freq = 40.f * powf(400.f, params[36]);
            const float q    = 0.5f + params[37] * 9.5f;
            delayBPMix_      = effectMix(params[38]);
            delayBP_.set(maxiBiquad::filterTypes::BANDPASS, freq, q, 0.f);
        }

        // params 39-49: reverb
        reverb_.setSize(       params[39]);
        reverb_.setDecay(      params[40]);
        reverb_.setDamping(    params[41]);
        reverb_.setDiffusion(  params[42]);
        reverb_.setModDepth(   params[43]);
        reverb_.setModRate(    params[44]);
        reverb_.setPreDelay(   params[45]);
        reverb_.setLowCut(     params[46]);
        reverb_.setStereoWidth(params[47]);
        reverb_.setSaturation( params[48]);
        reverbMix_ = effectMix(params[49]);

        // params 50-54: master compressor
        {
            const float thrDb = -40.f + params[50] * 40.f;                 // -40 .. 0 dB
            compThr_    = powf(10.f, thrDb * 0.05f);
            compInvThr_ = 1.f / compThr_;
            const float ratio = powf(10.f, params[51]);                    // 1 .. 10 (exp)
            compSlope_  = 1.f / ratio - 1.f;
            const float atkS = 0.0005f * powf(100.f, params[52]);          // 0.5 .. 50 ms
            const float relS = 0.02f   * powf(25.f,  params[53]);          // 20 .. 500 ms
            compAtk_    = 1.f - expf(-1.f / (atkS * sampleRate_));
            compRel_    = 1.f - expf(-1.f / (relS * sampleRate_));
            compMakeup_ = powf(10.f, params[54] * 12.f * 0.05f);           // 0 .. +12 dB
        }

        // params 55-58: EQ in the vocal range (targets; updateEQ() glides to them)
        eqOctT_  = log2f(100.f) + params[55] * log2f(8000.f / 100.f);       // 100 Hz .. 8 kHz
        eqGainT_ = params[56] * 20.f - 10.f;                                // -10 .. +10 dB
        eqQT_    = 0.5f * powf(8.f, params[57]);                            // Q 0.5 .. 4
        eqMix_   = effectMix(params[58]);
    }

protected:
    maxiBiquad delayBP_;

    GrainDelayI16<16384*2> grainDelay;
    GrainDelayI16<8192*4>  grainDelay2;
    DynamicDelayI16<16384> delay500ms_;

    VoiceFlanger      flanger_;
    ChorusI16<4096>   chorus_;
    AllpassI16<4096>  allpass_;
    FastSineOsc       apLFO_;
    ReverbI16<4096>   reverb_;
    float             reverbMix_{0.f};
    float revMixCached_{-1.f}, rWet_{0.f}, rDry_{1.f};
    float wetdryCached_{-1.f}, wetAmt_{0.f}, dryAmt_{1.f};

    float wetdry_mix_{0.5f};
    float grainCrossfade_{0.5f};
    float grainMix_{1.f};
    float bpm_{100.f};
    float delayTimeSamples_{8192.f};
    float delayFeedback_{0.f};
    float delayLevel_{0.f};
    float delayBPMix_{0.f};
    float flangerMix_{0.f};
    float chorusBase_{480.f}, chorusDepth_{0.f}, chorusRate_{0.5f};
    float chorusFeedback_{0.f}, chorusMix_{0.f};
    float sampleRate_{48000.f};
    float apDelay_{500.f}, apG_{0.5f}, apMix_{0.f};
    float apLFORate_{1.f}, apLFODepth_{0.f};

    // master compressor
    float compEnv_{0.f};
    float compThr_{1.f}, compInvThr_{1.f}, compSlope_{0.f};
    float compAtk_{0.1f}, compRel_{0.001f}, compMakeup_{1.f};

    // EQ: targets from ProcessParams, glided values used by updateEQ()
    static constexpr int kEQUpdateInterval = 32;
    maxiBiquad eqL_, eqR_;
    int   eqTick_{0};
    float eqOctT_{9.9658f}, eqGainT_{0.f}, eqQT_{1.f};   // 1 kHz, flat
    float eqOct_{9.9658f},  eqGain_{0.f},  eqQ_{1.f};
    float eqGlide_{0.15f};                               // per update (~7 ms time constant)
    float eqMix_{0.f};
};

#endif
