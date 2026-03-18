#ifndef __VERBFX_AUDIO_APP_HPP__
#define __VERBFX_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp" // Added missing include

#include <cstddef>
#include <cstdint>
#include <memory> 

//#include "src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>
#include "../../voicespaces/VoiceSpaces.hpp"
#include "../../src/memllib/synth/OnePoleSmoother.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/daisysp/Effects/pitchshifter.h"





template<size_t NPARAMS=24> 
class VerbFXAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces=1;


    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;
    
    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    std::array<String, nVoiceSpaces> getVoiceSpaceNames() {
        std::array<String, nVoiceSpaces> names;
        for(size_t i=0; i < voiceSpaces.size(); i++) {
            names[i] = voiceSpaces[i].name;
        }
        return names;
    }

    void setVoiceSpace(size_t i) {
        if (i < voiceSpaces.size()) {
            currentVoiceSpace = voiceSpaces[i].mappingFunction;
        }
    }

    VerbFXAudioApp() : AudioAppBase<NPARAMS>() {
    };


    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float mix = x.L + x.R;

        smoother.Process(neuralNetOutputs.data(), smoothParams.data());

        
        // float allp1fb = smoothParams[4] * 0.99f;
        // float allp2fb = smoothParams[5] * 0.99f;
        // float allp3fb = smoothParams[14] * 0.99f;

        wetdry_mix_ = (smoothParams[0] * 0.9f) + 0.1f;
        const float lp0fb = smoothParams[1] * 0.98f;
        const float lp0cutoff = (smoothParams[2] * 0.5f) + 0.05f;

        const float lp1fb = smoothParams[3] * 0.98f;
        const float lp1cutoff = (smoothParams[4] * 0.5f) + 0.05f;

        const float lp2fb = smoothParams[5] * 0.98f;
        const float lp2cutoff = (smoothParams[6] * 0.5f) + 0.05f;

        const float lp3fb = smoothParams[7] * 0.98f;
        const float lp3cutoff = (smoothParams[8] * 0.5f) + 0.05f;

        const float lp4fb = smoothParams[9] * 0.98f;
        const float lp4cutoff = (smoothParams[10] * 0.5f) + 0.05f;

        const float lp5fb = smoothParams[11] * 0.98f;
        const float lp5cutoff = (smoothParams[12] * 0.5f) + 0.05f;

        const float lp6fb = smoothParams[13] * 0.98f;
        const float lp6cutoff = (smoothParams[14] * 0.5f) + 0.05f;

        const float lp7fb = smoothParams[15] * 0.98f;
        const float lp7cutoff = (smoothParams[16] * 0.5f) + 0.05f;

        float lp0 = lpcomb0.lpcombfb(mix, SIZE_comb0, lp0fb, lp0cutoff);
        float lp1 = lpcomb1.lpcombfb(mix, SIZE_comb1, lp1fb, lp1cutoff);
        float lp2 = lpcomb2.lpcombfb(mix, SIZE_comb2, lp2fb, lp2cutoff);
        float lp3 = lpcomb3.lpcombfb(mix, SIZE_comb3, lp3fb, lp3cutoff);
        float lp4 = lpcomb4.lpcombfb(mix, SIZE_comb4, lp4fb, lp4cutoff);
        float lp5 = lpcomb5.lpcombfb(mix, SIZE_comb5, lp5fb, lp5cutoff);
        float lp6 = lpcomb6.lpcombfb(mix, SIZE_comb6, lp6fb, lp6cutoff);
        float lp7 = lpcomb7.lpcombfb(mix, SIZE_comb7, lp7fb, lp7cutoff);

        float y = lp0 + lp1 + lp2 + lp3 + lp4 + lp5 + lp6 + lp7;


        const float allp0fb = smoothParams[17] * 0.98f;
        const float allp1fb = smoothParams[18] * 0.98f;
        const float allp2fb = smoothParams[19] * 0.98f;
        const float allp3fb = smoothParams[20] * 0.98f;

        y = allp0.allpass(y, SIZE_allp0, allp0fb);
        y = allp1.allpass(y, SIZE_allp1, allp1fb);
        y = allp2.allpass(y, SIZE_allp2, allp2fb);
        y = allp3.allpass(y, SIZE_allp3, allp3fb);


        // Mix dry
        y = (y * wetdry_mix_) + (mix * (1.f - wetdry_mix_));


        stereosample_t ret { y, y };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        // currentVoiceSpace(params);
        neuralNetOutputs = params;
    }
    

protected:

    std::array<float,NPARAMS> neuralNetOutputs{0}, smoothParams{0};

    // https://ccrma.stanford.edu/~jos/pasp/Freeverb.html
    static constexpr size_t SIZE_allp0=244;
    static constexpr size_t SIZE_allp1=605;
    static constexpr size_t SIZE_allp2=479;
    static constexpr size_t SIZE_allp3=371;

    maxiReverbFilters<SIZE_allp0> allp0;
    maxiReverbFilters<SIZE_allp1> allp1;
    maxiReverbFilters<SIZE_allp2> allp2;
    maxiReverbFilters<SIZE_allp3> allp3;

    static constexpr size_t SIZE_comb0=1694;
    static constexpr size_t SIZE_comb1=1759;
    static constexpr size_t SIZE_comb2=1622;
    static constexpr size_t SIZE_comb3=1547;
    static constexpr size_t SIZE_comb4=1379;
    static constexpr size_t SIZE_comb5=1464;
    static constexpr size_t SIZE_comb6=1283;
    static constexpr size_t SIZE_comb7=1205;

    maxiReverbFilters<SIZE_comb0> lpcomb0;
    maxiReverbFilters<SIZE_comb1> lpcomb1;
    maxiReverbFilters<SIZE_comb2> lpcomb2;
    maxiReverbFilters<SIZE_comb3> lpcomb3;
    maxiReverbFilters<SIZE_comb4> lpcomb4;
    maxiReverbFilters<SIZE_comb5> lpcomb5;
    maxiReverbFilters<SIZE_comb6> lpcomb6;
    maxiReverbFilters<SIZE_comb7> lpcomb7;

    maxiDCBlocker dcb;

    float wetdry_mix_{0.5f};

    OnePoleSmoother<kN_Params> smoother{150.f, kSampleRate};

};

#endif  
