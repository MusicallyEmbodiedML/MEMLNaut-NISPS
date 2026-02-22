#ifndef __BREAKOR_AUDIO_APP_HPP__
#define __BREAKOR_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp" 
#include "../../src/memllib/synth/maximilian.h"

#include <cstddef>
#include <cstdint>
#include <memory> // Added for std::shared_ptr

#include "../../src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>

#include "../../voicespaces/VoiceSpaces.hpp"



template<size_t NPARAMS=33>

class BreakOrAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces=0;

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

    BreakOrAudioApp() : AudioAppBase<NPARAMS>() {
        // currentVoiceSpace = voiceSpaces[0].mappingFunction;   

    };

    bool __force_inline euclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth)
    {
        // Euclidean function
        const float fi = phase * n;
        int i = static_cast<int>(fi);
        const float rem = fi - i;
        if (i == n)
        {
            i--;
        }
        const int idx = ((i + n - offset) * k) % n;
        return (idx < k && rem < pulseWidth) ? 1 : 0;
    }

    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        stereosample_t ret { 0.f,0.f };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        sampleRatef = static_cast<float>(sample_rate);
    }


    void loop() override {
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        firstParamsReceived = true;
        // currentVoiceSpace(params);
    }

    

protected:

    float sampleRatef;
    bool firstParamsReceived = false;
};

#endif  // __BREAKOR_AUDIO_APP_HPP__

