#ifndef __THRU_AUDIO_APP_HPP__
#define __THRU_AUDIO_APP_HPP__

#include "src/memllib/audio/AudioAppBase.hpp" // Added missing include

#include <cstddef>
#include <cstdint>
#include <memory> 

//#include "src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>
#include "voicespaces/VoiceSpaces.hpp"
#include "src/memllib/synth/maximilian.h" // maxiBiquad, maxiSettings




template<size_t NPARAMS=8> //params just go out as MIDI for this one
class ThruAudioApp : public AudioAppBase<NPARAMS>
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

    ThruAudioApp() : AudioAppBase<NPARAMS>() {
        auto voiceSpaceThru = [this](const std::array<float, NPARAMS>& params) {
        };

        voiceSpaces[0] = {"thru", voiceSpaceThru};

        currentVoiceSpace = voiceSpaces[0].mappingFunction;   

    };


    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        // float y = x.L + x.R;
        // y = filt.play(y);
        // return stereosample_t{y, y};
        return x;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        filt.set(maxiBiquad::filterTypes::HIGHPASS, 20.f, 0.707f, 0);
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        currentVoiceSpace(params);
    }
    

protected:

    maxiBiquad filt;

};

#endif  
