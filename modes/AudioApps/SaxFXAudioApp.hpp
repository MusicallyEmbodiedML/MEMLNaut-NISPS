#ifndef __SAXFX_AUDIO_APP_HPP__
#define __SAXFX_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/GrainDelayI16.hpp"
#include "../../src/memllib/synth/ReverbI16.hpp"
#include "../../voicespaces/VoiceSpaces.hpp"

// Audio input source, selectable at compile time:
//   1 = on-board mic (default), 0 = line-in (handy for bench testing with a line source).
// Override with a build flag (e.g. -DSAXFX_USE_MIC=0) or by editing this default.
#ifndef SAXFX_USE_MIC
#define SAXFX_USE_MIC 1
#endif

template<size_t NPARAMS=47>
class SaxFXAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces = 4;

    enum class controlMessages {
        MSG_ENABLE_REVERB = 0,
        MSG_ENABLE_GRAIN1,
        MSG_ENABLE_GRAIN2,
        MSG_ENABLE_GRAIN3,
    };

    queue_t controlMessageQueue;

    bool enableReverb = true;
    bool enableGrain1 = true;
    bool enableGrain2 = true;
    bool enableGrain3 = true;

    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;
    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    std::array<String, nVoiceSpaces> getVoiceSpaceNames() {
        std::array<String, nVoiceSpaces> names;
        for (size_t i = 0; i < voiceSpaces.size(); i++)
            names[i] = voiceSpaces[i].name;
        return names;
    }

    void setVoiceSpace(size_t i) {
        if (i < voiceSpaces.size()) {
            currentVoiceSpace = voiceSpaces[i].mappingFunction;
            currentVoiceSpaceIdx_ = i;
        }
    }

    size_t getVoiceSpace() const { return currentVoiceSpaceIdx_; }

    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input    = (SAXFX_USE_MIC != 0),   // compile-time: mic (default) vs line-in
            .line_level   = 3,
            .mic_gain_dB  = 0,
            .output_volume = 0.97f
        };
    }

    SaxFXAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&controlMessageQueue, sizeof(controlMessages), 8);

        auto vsDefault = [this](const std::array<float, NPARAMS>& p) {
            mixScale_ = 0.33333f;
            grainDelay.setGrainLengthMs(10.f + p[0] * 490.f);
            grainDelay.setStartTimeMs(  20.f + p[1] * 980.f);
            grainDelay.setFeedback(          p[2]);
            grainDelay.setPitch(             powf(2.f, p[3] * 2.f - 1.f));
            grainDelay.setPitchSpread(       p[4]);
            grainDelay.setFreeze(            p[7] > 0.99f);

            grainDelay2.setGrainLengthMs(10.f + p[8]  * 190.f);
            grainDelay2.setStartTimeMs(  10.f + p[9]  * 240.f);
            grainDelay2.setFeedback(           p[10]);
            grainDelay2.setPitch(              powf(2.f, p[11] * 2.f - 1.f));
            grainDelay2.setPitchSpread(        p[12]);
            grainDelay2.setFreeze(             p[15] > 0.99f);

            grainDelay3.setGrainLengthMs(10.f + p[16] * 190.f);
            grainDelay3.setStartTimeMs(  10.f + p[17] * 240.f);
            grainDelay3.setFeedback(           p[18]);
            grainDelay3.setPitch(              powf(2.f, p[19] * 2.f - 1.f));
            grainDelay3.setPitchSpread(        p[20]);
            grainDelay3.setFreeze(             p[23] > 0.99f);

            reverb_.setSize(       p[24]);
            reverb_.setDecay(      p[25]);
            reverb_.setDamping(    p[26]);
            reverb_.setDiffusion(  p[27]);
            reverb_.setModDepth(   p[28]);
            reverb_.setModRate(    p[29]);
            reverb_.setPreDelay(   p[30]);
            reverb_.setLowCut(     p[31]);
            reverb_.setStereoWidth(p[32]);
            reverb_.setSaturation( p[33]);
            reverbMix_ = effectMix(p[34]);
        };
        voiceSpaces[0] = {"Default", vsDefault};

        auto vsOctaveSnap = [this](const std::array<float, NPARAMS>& p) {
            mixScale_ = 0.33333f;
            auto snap = [](float v) -> float {
                return (v < 0.333f) ? 0.5f : (v < 0.667f) ? 1.0f : 2.0f;
            };

            grainDelay.setGrainLengthMs(10.f + p[0] * 490.f);
            grainDelay.setStartTimeMs(  20.f + p[1] * 980.f);
            grainDelay.setFeedback(          p[2]);
            grainDelay.setPitch(             snap(p[3]));
            grainDelay.setPitchSpread(       p[4] * 0.03f);
            grainDelay.setFreeze(            p[7] > 0.99f);

            grainDelay2.setGrainLengthMs(10.f + p[8]  * 190.f);
            grainDelay2.setStartTimeMs(  10.f + p[9]  * 240.f);
            grainDelay2.setFeedback(           p[10]);
            grainDelay2.setPitch(              snap(p[11]));
            grainDelay2.setPitchSpread(        p[12] * 0.03f);
            grainDelay2.setFreeze(             p[15] > 0.99f);

            grainDelay3.setGrainLengthMs(10.f + p[16] * 190.f);
            grainDelay3.setStartTimeMs(  10.f + p[17] * 240.f);
            grainDelay3.setFeedback(           p[18]);
            grainDelay3.setPitch(              snap(p[19]));
            grainDelay3.setPitchSpread(        p[20] * 0.03f);
            grainDelay3.setFreeze(             p[23] > 0.99f);

            reverb_.setSize(       p[24]);
            reverb_.setDecay(      p[25]);
            reverb_.setDamping(    p[26]);
            reverb_.setDiffusion(  p[27]);
            reverb_.setModDepth(   p[28]);
            reverb_.setModRate(    p[29]);
            reverb_.setPreDelay(   p[30]);
            reverb_.setLowCut(     p[31]);
            reverb_.setStereoWidth(p[32]);
            reverb_.setSaturation( p[33]);
            reverbMix_ = effectMix(p[34]);
        };
        voiceSpaces[1] = {"OctaveSnap", vsOctaveSnap};

        auto vsGrainVerb = [this](const std::array<float, NPARAMS>& p) {
            mixScale_ = 1.f;
            grainDelay.setGrainLengthMs(10.f + p[0] * 490.f);
            grainDelay.setStartTimeMs(  20.f + p[1] * 980.f);
            grainDelay.setFeedback(          p[2]);
            grainDelay.setPitch(             powf(2.f, p[3] * 2.f - 1.f));
            grainDelay.setPitchSpread(       p[4]);
            grainDelay.setFreeze(            p[7] > 0.99f);

            grainDelay2.setFeedback(0.f);
            grainDelay3.setFeedback(0.f);

            reverb_.setSize(       p[24]);
            reverb_.setDecay(      p[25]);
            reverb_.setDamping(    p[26]);
            reverb_.setDiffusion(  p[27]);
            reverb_.setModDepth(   p[28]);
            reverb_.setModRate(    p[29]);
            reverb_.setPreDelay(   p[30]);
            reverb_.setLowCut(     p[31]);
            reverb_.setStereoWidth(p[32]);
            reverb_.setSaturation( p[33]);
            reverbMix_ = effectMix(p[34]);
        };
        voiceSpaces[2] = {"GrainVerb", vsGrainVerb};

        auto vsSqueaky = [this](const std::array<float, NPARAMS>& p) {
            mixScale_ = 0.33333f;
            grainDelay.setGrainLengthMs(10.f + p[0] * 490.f);
            grainDelay.setStartTimeMs(  20.f + p[1] * 980.f);
            grainDelay.setFeedback(          p[2]);
            grainDelay.setPitch(             powf(2.f, p[3] * 4.f));
            grainDelay.setPitchSpread(       p[4] * 0.3f);
            grainDelay.setFreeze(            p[7] > 0.9f);

            grainDelay2.setGrainLengthMs(10.f + p[8]  * 190.f);
            grainDelay2.setStartTimeMs(  10.f + p[9]  * 240.f);
            grainDelay2.setFeedback(           p[10]);
            grainDelay2.setPitch(              powf(2.f, p[11] * 4.f));
            grainDelay2.setPitchSpread(        p[12] * 0.3f);
            grainDelay2.setFreeze(             p[15] > 0.99f);

            grainDelay3.setGrainLengthMs(10.f + p[16] * 190.f);
            grainDelay3.setStartTimeMs(  10.f + p[17] * 240.f);
            grainDelay3.setFeedback(           p[18]);
            grainDelay3.setPitch(              powf(2.f, p[19] * 4.f));
            grainDelay3.setPitchSpread(        p[20] * 0.3f);
            grainDelay3.setFreeze(             p[23] > 0.99f);

            reverb_.setSize(       p[24]);
            reverb_.setDecay(      p[25]);
            reverb_.setDamping(    p[26]);
            reverb_.setDiffusion(  p[27]);
            reverb_.setModDepth(   p[28]);
            reverb_.setModRate(    p[29]);
            reverb_.setPreDelay(   p[30]);
            reverb_.setLowCut(     p[31]);
            reverb_.setStereoWidth(p[32]);
            reverb_.setSaturation( p[33]);
            reverbMix_ = effectMix(p[34]);
        };
        voiceSpaces[3] = {"Squeaky", vsSqueaky};

        currentVoiceSpace = voiceSpaces[0].mappingFunction;
    }

    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float mix = x.L;
        mix = hp.play(mix);
        mix = lp.play(mix);
        mix = notch.play(mix);

        const float grainOut  = enableGrain1 ? grainDelay.process(mix)  : 0.f;
        const float grainOut2 = enableGrain2 ? grainDelay2.process(mix) : 0.f;
        const float grainOut3 = enableGrain3 ? grainDelay3.process(mix) : 0.f;
        const float out = (grainOut + grainOut2 + grainOut3) * mixScale_;

        float outL = out;
        float outR = out;
        // float outL = mix;
        // float outR = mix;

        if (enableReverb && reverbMix_ > 0.f) {
            auto [revL, revR] = reverb_.process(outL);
            const float wet = sqrtf(reverbMix_);
            const float dry = sqrtf(1.f - reverbMix_);
            outL = outL * dry + revL * wet;
            outR = outR * dry + revR * wet;
        }

        outL *= 2.f;
        outR *= 2.f;
        return { bassCut2.play(bassCut.play(outL)), bassCut2R.play(bassCutR.play(outR)) };
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;

        notch.set(maxiBiquad::filterTypes::HIGHSHELF, 9000.f, 0.707f, -12.f);
        hp.set(maxiBiquad::filterTypes::HIGHPASS,       50.f, 0.707f,  0.f);
        lp.set(maxiBiquad::filterTypes::LOWPASS,      9000.f, 0.707f,  0.f);
        bassCut.set(  maxiBiquad::filterTypes::HIGHPASS, 200.f, 0.541f, 0.f);
        bassCutR.set( maxiBiquad::filterTypes::HIGHPASS, 200.f, 0.541f, 0.f);
        bassCut2.set( maxiBiquad::filterTypes::HIGHPASS, 200.f, 1.307f, 0.f);
        bassCut2R.set(maxiBiquad::filterTypes::HIGHPASS, 200.f, 1.307f, 0.f);

        grainDelay.setup(sample_rate);
        grainDelay2.setup(sample_rate);
        grainDelay3.setup(sample_rate);
        reverb_.setup(sample_rate);
    }

    static float effectMix(float raw) {
        return (raw > 0.3f) ? (raw - 0.3f) * (1.f / 0.7f) : 0.f;
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        currentVoiceSpace(params);

        controlMessages msg;
        while (queue_try_remove(&controlMessageQueue, &msg)) {
            switch (msg) {
                case controlMessages::MSG_ENABLE_REVERB: enableReverb = !enableReverb; break;
                case controlMessages::MSG_ENABLE_GRAIN1: enableGrain1 = !enableGrain1; break;
                case controlMessages::MSG_ENABLE_GRAIN2: enableGrain2 = !enableGrain2; break;
                case controlMessages::MSG_ENABLE_GRAIN3: enableGrain3 = !enableGrain3; break;
            }
        }
    }

protected:
    maxiBiquad notch;
    maxiBiquad hp;
    maxiBiquad lp;
    maxiBiquad bassCut;
    maxiBiquad bassCutR;
    maxiBiquad bassCut2;
    maxiBiquad bassCut2R;

    GrainDelayI16<16384*4> grainDelay;
    GrainDelayI16<16384*1> grainDelay2;
    GrainDelayI16<16384*1> grainDelay3;
    ReverbI16Large<2048> reverb_;   // larger/denser stereo reverb (2048 combs to fit RAM)
    float reverbMix_  = 0.f;
    float mixScale_   = 0.33333f;
    size_t currentVoiceSpaceIdx_ = 0;
};

#endif
