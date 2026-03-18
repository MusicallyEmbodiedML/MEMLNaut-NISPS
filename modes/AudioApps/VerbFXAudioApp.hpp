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





template<size_t NPARAMS=41> 
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

        const float allp0fb = smoothParams[17] * 0.98f;
        const float allp1fb = smoothParams[18] * 0.98f;
        const float allp2fb = smoothParams[19] * 0.98f;
        const float allp3fb = smoothParams[20] * 0.98f;

        const float filterBankF0 = 40.f + (smoothParams[21] * 40.f);
        const float filterBankF1 = 80.f + (smoothParams[22] * 80.f);
        const float filterBankF2 = 160.f + (smoothParams[23] * 160.f);
        const float filterBankF3 = 320.f + (smoothParams[24] * 320.f);
        const float filterBankF4 = 640.f + (smoothParams[25] * 640.f);
        const float filterBankF5 = 1280.f + (smoothParams[26] * 1280.f);
        const float filterBankF6 = 2560.f + (smoothParams[27] * 2560.f);
        const float filterBankF7 = 5120.f + (smoothParams[28] * 5120.f);


        const float filterBankRes0 = 1.f + (smoothParams[29] * 18.f);
        const float filterBankRes1 = 1.f + (smoothParams[30] * 18.f);
        const float filterBankRes2 = 1.f + (smoothParams[31] * 18.f);
        const float filterBankRes3 = 1.f + (smoothParams[32] * 18.f);
        const float filterBankRes4 = 1.f + (smoothParams[33] * 18.f);
        const float filterBankRes5 = 1.f + (smoothParams[34] * 18.f);
        const float filterBankRes6 = 1.f + (smoothParams[35] * 18.f);
        const float filterBankRes7 = 1.f + (smoothParams[36] * 18.f);


        const float ddelayTime = 10.f + (smoothParams[37] * 8181.f);
        const float ddelayFeedback = (smoothParams[38] * 0.98f);

        const float filterBankLFOFreq = (smoothParams[39] * 0.5f);
        const float filterBankLFODepth = (smoothParams[40] * 0.3f);

        float filterBankIn = mix;
        float filterBankOut=0;

        const float filterBankFreqMod = 1.f + lfo1.sinebuf4(filterBankLFOFreq) * filterBankLFODepth;

        filterBankOut = filterBank0.bandpassChamberlain(filterBankIn,  filterBankF0 * filterBankFreqMod, filterBankRes0);
        filterBankOut += filterBank1.bandpassChamberlain(filterBankIn, filterBankF1 * filterBankFreqMod, filterBankRes1);
        filterBankOut += filterBank2.bandpassChamberlain(filterBankIn, filterBankF2 * filterBankFreqMod, filterBankRes2);
        filterBankOut += filterBank3.bandpassChamberlain(filterBankIn, filterBankF3 * filterBankFreqMod, filterBankRes3);
        filterBankOut += filterBank4.bandpassChamberlain(filterBankIn, filterBankF4 * filterBankFreqMod, filterBankRes4);
        filterBankOut += filterBank5.bandpassChamberlain(filterBankIn, filterBankF5 * filterBankFreqMod, filterBankRes5);
        filterBankOut += filterBank6.bandpassChamberlain(filterBankIn, filterBankF6 * filterBankFreqMod, filterBankRes6);
        filterBankOut += filterBank7.bandpassChamberlain(filterBankIn, filterBankF7 * filterBankFreqMod, filterBankRes7);

        filterBankOut *= 0.5f;

        float verbIn = filterBankOut;
        float verbOut=0.f;

        verbOut = lpcomb0.lpcombfb(filterBankOut, SIZE_comb0, lp0fb, lp0cutoff);
        verbOut += lpcomb1.lpcombfb(filterBankOut, SIZE_comb1, lp1fb, lp1cutoff);
        verbOut += lpcomb2.lpcombfb(filterBankOut, SIZE_comb2, lp2fb, lp2cutoff);
        verbOut += lpcomb3.lpcombfb(filterBankOut, SIZE_comb3, lp3fb, lp3cutoff);
        verbOut += lpcomb4.lpcombfb(filterBankOut, SIZE_comb4, lp4fb, lp4cutoff);
        verbOut += lpcomb5.lpcombfb(filterBankOut, SIZE_comb5, lp5fb, lp5cutoff);
        verbOut += lpcomb6.lpcombfb(filterBankOut, SIZE_comb6, lp6fb, lp6cutoff);
        verbOut += lpcomb7.lpcombfb(filterBankOut, SIZE_comb7, lp7fb, lp7cutoff);




        verbOut = allp0.allpass(verbOut, SIZE_allp0, allp0fb);
        verbOut = allp1.allpass(verbOut, SIZE_allp1, allp1fb);
        verbOut = allp2.allpass(verbOut, SIZE_allp2, allp2fb);
        verbOut = allp3.allpass(verbOut, SIZE_allp3, allp3fb);

        
        float delayed = ddelay.read(ddelayTime);
        ddelay.write(filterBankOut + (ddelayFeedback * delayed));

        float y= delayed + verbOut;




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

    maxiFilter filterBank0;
    maxiFilter filterBank1;
    maxiFilter filterBank2;
    maxiFilter filterBank3;
    maxiFilter filterBank4;
    maxiFilter filterBank5;
    maxiFilter filterBank6;
    maxiFilter filterBank7;

    DynamicDelay<8192> ddelay;

    maxiDCBlocker dcb;

    float wetdry_mix_{0.5f};

    OnePoleSmoother<kN_Params> smoother{150.f, kSampleRate};

    maxiOsc lfo1;

};

#endif  
