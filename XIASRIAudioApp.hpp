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
#include "src/daisysp/Effects/pitchshifter.h"





template<size_t NPARAMS=24> 
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
        mix *= 2.0f;

        smoother.Process(neuralNetOutputs.data(), smoothParams.data());

        // float dl1mix = smoothParams[0] * 0.6f;
        // float dl2mix = smoothParams[1] * 0.6f;
        // float dl3mix = smoothParams[2] * 0.8f;
        // float ymix = smoothParams[12] * 1.0f;
        
        // float allp1fb = smoothParams[3] * 0.96f;
        // float allp2fb = smoothParams[4] * 0.96f;
        // float allp3fb = smoothParams[5] * 0.96f;

        // float comb1fb = (smoothParams[6] * 0.95f);
        // float comb2fb = (smoothParams[7] * 0.95f);
        // float comb3fb = (smoothParams[8] * 0.95f);

        // float dl1fb = (smoothParams[9] * 0.95f);
        // float dl2fb = (smoothParams[10] * 0.95f);
        // float dl3fb = (smoothParams[11] * 0.95f);

        // float y = dcb.play(mix, 0.99f) * 3.f;

        // float y1 = allp1.allpass(y, 73, allp1fb);
        // y1 = comb1.combfb(y1, 255, comb1fb);

        // float y2 = allp2.allpass(y, 987, allp2fb);
        // y2 = comb2.combfb(y2, 1616, comb2fb);

        // float y3 = comb3.combfb(y3, 847, comb3fb);
        // y3 = allp3.allpass(y, 707, allp3fb);

        // y = y1 + y2 + y3;

        // float d1 = (dl1.play(y, 7000, dl1fb) * dl1mix);
        // float d2 = (dl2.play(y, 18000, dl2fb) * dl2mix);
        // float d3 = (dl3.play(y, 2399, dl3fb) * dl3mix);


        // y = (y * ymix) + d1 + d2 + d3;

        // y = y * 1.9f;

        // y = tanhf(y);

        float dl1mix = smoothParams[0] * 0.4f;
        float dl2mix = smoothParams[1] * 0.4f;
        float dl3mix = smoothParams[2] * 0.8f;
        float dl4mix = smoothParams[22] * smoothParams[22] * 0.41f;
        
        float allp1fb = smoothParams[4] * 0.99f;
        float allp2fb = smoothParams[5] * 0.99f;
        float comb1fb = (smoothParams[6] * 0.95f);
        float comb2fb = (smoothParams[7] * 0.95f);

        float dl1fb = (smoothParams[8] * 0.95f);
        float dl2fb = (smoothParams[9] * 0.95f);
        float dl3fb = (smoothParams[10] * 0.95f);
        float dl4fb = (smoothParams[23] * 0.95f);
        // Wet-dry mix between 0.2 and 1
        wetdry_mix_ = (smoothParams[11] * 0.7f) + 0.3f;
        // Pitch shift transposition between -12 and +12 semitones
        // float pitchshift_transpose = (smoothParams[12] * 24.f) - 12.f; // Scale to -12 to +12 semitones
        pitchshifter_.SetTransposition(12.f + smoothParams[12]);
        //pitchshifter_.SetTransposition(-5.f);
        // Set pitch shifter mix
        pitchshifter_mix_ = smoothParams[13] * 0.99f;

        float allp3fb = smoothParams[14] * 0.99f;
        float allp4fb = smoothParams[15] * 0.99f;
        float allp5fb = smoothParams[16] * 0.99f;
        float allp6fb = smoothParams[17] * 0.99f;
        float allp3fbmix = smoothParams[18];
        float allp4fbmix = smoothParams[19];
        float allp5fbmix = smoothParams[20];
        float allp6fbmix = smoothParams[21];

        // // PROCESS
        float pitchshifted = pitchshifter_.Process(mix);
        // // Mix the original signal with the pitch-shifted signal
        pitchshifted = (mix * (1.f - pitchshifter_mix_)) + (pitchshifted * pitchshifter_mix_);

        float y = dcb.play(pitchshifted, 0.99f) * 2.f;
        // float y = dcb.play(pitchshifted, 0.99f) * 3.f;
        float y1 = allp1.allpass(y, 30, allp1fb);
        y1 = comb1.combfb(y1, 127, comb1fb);

        float y2 = allp2.allpass(y, 482, allp2fb);
        y2 = comb2.combfb(y2, 808, comb2fb);

        float y3 = allp3.allpass(y, 19, allp3fb) * allp3fbmix;
        float y4 = allp4.allpass(y, 69, allp4fb) * allp4fbmix;
        float y5 = allp5.allpass(y, 131, allp5fb) * allp5fbmix;
        float y6 = allp6.allpass(y, 287, allp6fb) * allp6fbmix;
        y = y1 + y2 + y3 + y4 +y5 +y6;
        float d1 = (dl1.play(y, 3500, dl1fb) * dl1mix);
        float d2 = (dl2.play(y, 7886, dl2fb) * dl2mix);
        float d3 = (dl3.play(y, 299, dl3fb) * dl3mix);
        float d4 = (dl4.play(y, 15873, dl4fb) * dl4mix);


        y = y + d1 + d2 + d3;

        // Mix dry
        y = (y * wetdry_mix_) + (mix * (1.f - wetdry_mix_));

        y = tanhf(y*1.2f);

        stereosample_t ret { y, y };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        pitchshifter_.Init(sample_rate);
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        // currentVoiceSpace(params);
        neuralNetOutputs = params;
    }
    

protected:

    std::array<float,NPARAMS> neuralNetOutputs{0}, smoothParams{0};

    // maxiDelayline<10000> dl1;
    // maxiDelayline<30100> dl2;
    // maxiDelayline<2401> dl3;

    // maxiReverbFilters<300> allp1;
    // maxiReverbFilters<1000> allp2;
    // maxiReverbFilters<1000> allp3;

    // maxiReverbFilters<300> comb1;
    // maxiReverbFilters<2000> comb2;
    // maxiReverbFilters<1000> comb3;

    // maxiDCBlocker dcb;

    maxiDelayline<5000> dl1;
    maxiDelayline<8000> dl2;
    maxiDelayline<1201> dl3;
    maxiDelayline<16000> dl4;

    maxiReverbFilters<300> allp1;
    maxiReverbFilters<500> allp2;
    maxiReverbFilters<500> allp3;
    maxiReverbFilters<500> allp4;
    maxiReverbFilters<500> allp5;
    maxiReverbFilters<500> allp6;
    maxiReverbFilters<200> comb1;
    maxiReverbFilters<900> comb2;





    maxiDCBlocker dcb;

    daisysp::PitchShifter pitchshifter_;
    float pitchshifter_mix_{0.5f};
    float wetdry_mix_{0.5f};

    OnePoleSmoother<kN_Params> smoother{150.f, kSampleRate};

};

#endif  
