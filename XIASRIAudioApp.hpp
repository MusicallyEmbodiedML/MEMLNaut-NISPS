#ifndef __XIASRI_AUDIO_APP_HPP__
#define __XIASRI_AUDIO_APP_HPP__

#include "src/memllib/audio/AudioAppBase.hpp" // Added missing include

#include <cstddef>
#include <cstdint>
#include <memory> 

//#include "src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>
#include "voicespaces/VoiceSpaces.hpp"
#include "src/memllib/synth/OnePoleSmoother.hpp"
#include "src/memllib/synth/maximilian.h"




template<size_t NPARAMS=12> //params just go out as MIDI for this one
class XIASRIAudioApp : public AudioAppBase<NPARAMS>
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

    XIASRIAudioApp() : AudioAppBase<NPARAMS>() {
        // auto voiceSpaceXIASRI = [this](const std::array<float, NPARAMS>& params) {
        // };

        // voiceSpaces[0] = {"XIASRI", voiceSpaceXIASRI};

        // currentVoiceSpace = voiceSpaces[0].mappingFunction;   

    };


    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float mix = x.L + x.R;

        smoother.Process(neuralNetOutputs.data(), smoothParams.data());

        dl1mix = smoothParams[0] * smoothParams[0] * 0.4f;
        dl2mix = smoothParams[1] * smoothParams[1] * 0.4f;
        dl3mix = smoothParams[2] * smoothParams[2] * 0.8f;
        allp1fb = smoothParams[4] * 0.99f;
        allp2fb = smoothParams[5] * 0.99f;
        float comb1fb = (smoothParams[6] * 0.95f);
        float comb2fb = (smoothParams[7] * 0.95f);

        float dl1fb = (smoothParams[8] * 0.95f);
        float dl2fb = (smoothParams[9] * 0.95f);
        float dl3fb = (smoothParams[10] * 0.95f);

        float y = dcb.play(mix, 0.99f) * 3.f;
        float y1 = allp1.allpass(y, 30, allp1fb);
        y1 = comb1.combfb(y1, 127, comb1fb);

        float y2 = allp2.allpass(y, 500, allp2fb);
        y2 = comb2.combfb(y2, 808, comb2fb);

        y = y1 + y2;
        float d1 = (dl1.play(y, 3500, dl1fb) * dl1mix);
        float d2 = (dl2.play(y, 9000, dl2fb) * dl2mix);
        float d3 = (dl3.play(y, 1199, dl3fb) * dl3mix);


        y = y + d1 + d2 + d3;

        y = tanhf(y);

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

    maxiDelayline<5000> dl1;
    maxiDelayline<15100> dl2;
    maxiDelayline<1201> dl3;

    maxiReverbFilters<300> allp1;
    maxiReverbFilters<1000> allp2;
    maxiReverbFilters<300> comb1;
    maxiReverbFilters<1000> comb2;

    float dl1mix = 0.0f;
    float dl2mix = 0.0f;
    float dl3mix = 0.0f;
    float allp1fb=0.5f;
    float allp2fb=0.5f;

    maxiDCBlocker dcb;

    OnePoleSmoother<kN_Params> smoother{150.f, kSampleRate};

};

#endif  
