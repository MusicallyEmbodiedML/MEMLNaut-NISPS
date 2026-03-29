#ifndef __VERBFX_AUDIO_APP_HPP__
#define __VERBFX_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp" // Added missing include

#include <cstddef>
#include <cstdint>
#include <memory> 

//#include "src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>
#include "../../voicespaces/VoiceSpaces.hpp"
#include "../../voicespaces/VerbFX/basic.hpp"
#include "../../src/memllib/synth/OnePoleSmoother.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/daisysp/Effects/pitchshifter.h"





template<size_t NPARAMS=46> 
class VerbFXAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces=1;


    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;
    
    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    enum class controlMessages {
        MSG_ENABLE_FILTERBANK=0,
        MSG_ENABLE_REVERB,
        MSG_ENABLE_SHORT_DELAY,
        MSG_ENABLE_MEDIUM_DELAY,
        MSG_ENABLE_LONG_DELAY,
        MSG_ENABLE_DELAY_TO_REVERB,
    };

    queue_t controlMessageQueue;

    bool enableFilterbank=true;
    bool enableReverb=true;
    bool enableShortDelay=true;
    bool enableMediumDelay=true;
    bool enableLongDelay=true;
    bool enableDelayToReverb=true;


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
        auto voiceSpaceDefault = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_VERBFX_DEFAULT_BODY
        };
        voiceSpaces[0] = {"Default", voiceSpaceDefault};
        currentVoiceSpace = voiceSpaces[0].mappingFunction;
        queue_init(&controlMessageQueue, sizeof(controlMessages), 1);
    };


    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        static float verbFB = 0.f;
        static float delaysFB = 0.f;

        float mix = x.L + x.R;

        smoother.Process(neuralNetOutputs.data(), smoothParams.data());

        //mapping
        wetdry_mix_ = (smoothParams[0] * 0.9f) + 0.1f;
        
        lp0fb = smoothParams[1] * 0.9f;
        lp0cutoff = (smoothParams[2] * 0.5f) + 0.05f;

        lp1fb = smoothParams[3] * 0.9f;
        lp1cutoff = (smoothParams[4] * 0.5f) + 0.05f;

        lp2fb = smoothParams[5] * 0.9f;
        lp2cutoff = (smoothParams[6] * 0.5f) + 0.05f;

        lp3fb = smoothParams[7] * 0.9f;
        lp3cutoff = (smoothParams[8] * 0.5f) + 0.05f;

        lp4fb = smoothParams[9] * 0.9f;
        lp4cutoff = (smoothParams[10] * 0.5f) + 0.05f;

        lp5fb = smoothParams[11] * 0.98f;
        lp5cutoff = (smoothParams[12] * 0.5f) + 0.05f;

        lp6fb = smoothParams[13] * 0.9;
        lp6cutoff = (smoothParams[14] * 0.5f) + 0.05f;

        lp7fb = smoothParams[15] * 0.9f;
        lp7cutoff = (smoothParams[16] * 0.5f) + 0.05f;

        allp0fb = smoothParams[17] * 0.9f;
        allp1fb = smoothParams[18] * 0.9f;
        allp2fb = smoothParams[19] * 0.9f;
        allp3fb = smoothParams[20] * 0.9f;

        filterBankF0 = 40.f + (smoothParams[21] * 40.f);
        filterBankF1 = 80.f + (smoothParams[22] * 80.f);
        filterBankF2 = 160.f + (smoothParams[23] * 160.f);
        filterBankF3 = 320.f + (smoothParams[24] * 320.f);
        filterBankF4 = 640.f + (smoothParams[25] * 640.f);
        filterBankF5 = 1280.f + (smoothParams[26] * 1280.f);
        filterBankF6 = 2560.f + (smoothParams[27] * 2560.f);
        filterBankF7 = 5120.f + (smoothParams[28] * 5120.f);

        filterBankRes0 = 1.f + (smoothParams[29] * 19.f);
        filterBankRes1 = 1.f + (smoothParams[30] * 19.f);
        filterBankRes2 = 1.f + (smoothParams[31] * 19.f);
        filterBankRes3 = 1.f + (smoothParams[32] * 19.f);
        filterBankRes4 = 1.f + (smoothParams[33] * 19.f);
        filterBankRes5 = 1.f + (smoothParams[34] * 19.f);
        filterBankRes6 = 1.f + (smoothParams[35] * 19.f);
        filterBankRes7 = 1.f + (smoothParams[36] * 19.f);

        ddelayTime = 10.f + (smoothParams[37] * 16373.f);
        ddelayFeedback = (smoothParams[38] * 0.98f);

        ddelayTime1 = 10.f + (smoothParams[39] * 2037.f);
        ddelayFeedback1 = (smoothParams[40] * 0.98f);

        ddelayTime2 = 10.f + (smoothParams[41] * 501.f);
        ddelayFeedback2 = (smoothParams[42] * 0.98f);

        verbVsDelayLevel = smoothParams[43];
        delayToVerbLevel = smoothParams[44] * 0.99f;
        filterBankDelayXFade = smoothParams[45];

        //XFADE

        const float filterBankDelayFBLevel = sqrtf(filterBankDelayXFade);
        const float filterBankDelayFBLevelInv = sqrtf(1.f - filterBankDelayXFade);

        /////////////////// FILTERBANK

        float filterBankIn = mix + (filterBankDelayFBLevel * ddelayFeedback); 
        float filterBankOut=mix;

        if (enableFilterbank) {
            filterBankOut = filterBank0.bandpassChamberlain(filterBankIn,  filterBankF0, filterBankRes0);
            filterBankOut += filterBank1.bandpassChamberlain(filterBankIn, filterBankF1, filterBankRes1);
            filterBankOut += filterBank2.bandpassChamberlain(filterBankIn, filterBankF2, filterBankRes2);
            filterBankOut += filterBank3.bandpassChamberlain(filterBankIn, filterBankF3, filterBankRes3);
            filterBankOut += filterBank4.bandpassChamberlain(filterBankIn, filterBankF4, filterBankRes4);
            filterBankOut += filterBank5.bandpassChamberlain(filterBankIn, filterBankF5, filterBankRes5);
            filterBankOut += filterBank6.bandpassChamberlain(filterBankIn, filterBankF6, filterBankRes6);
            filterBankOut += filterBank7.bandpassChamberlain(filterBankIn, filterBankF7, filterBankRes7);

            filterBankOut *= 0.5f;
        }

        ////////////// DELAYS
        float delayIn = filterBankOut;

        float delayed = enableLongDelay ? ddelay.read(ddelayTime) : 0.f;
        ddelay.write((delayIn * filterBankDelayFBLevelInv) + ((ddelayFeedback + (delayIn * filterBankDelayFBLevel)) * delayed));

        float delayed1 = enableMediumDelay ? ddelay1.read(ddelayTime1) : 0.f;
        ddelay1.write(delayIn + (ddelayFeedback1 * delayed1));

        float delayed2 = enableShortDelay ? ddelay2.read(ddelayTime2) : 0.f;
        ddelay2.write(delayIn + (ddelayFeedback2 * delayed2));

        float delaySum = delayed + delayed1 + delayed2;

        //////////////// VERB
        float verbIn = enableReverb ? filterBankOut : 0.f;
        if (enableDelayToReverb && enableReverb) {
            verbIn += (delayToVerbLevel * delaySum);
        }
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

        float y= (sqrtf(verbVsDelayLevel) * delaySum) + (sqrtf(1.f - verbVsDelayLevel) * verbOut);

        //feedback 
        delaysFB = delaySum;
        verbFB = verbOut;




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
        controlMessages msg;
        while (queue_try_remove(&controlMessageQueue, &msg)) {
            switch(msg) {
                case controlMessages::MSG_ENABLE_FILTERBANK:
                    enableFilterbank = !enableFilterbank;
                    break;
                case controlMessages::MSG_ENABLE_REVERB:
                    enableReverb = !enableReverb;
                    break;
                case controlMessages::MSG_ENABLE_SHORT_DELAY:
                    enableShortDelay = !enableShortDelay;
                    break;
                case controlMessages::MSG_ENABLE_MEDIUM_DELAY:
                    enableMediumDelay = !enableMediumDelay;
                    break;
                case controlMessages::MSG_ENABLE_LONG_DELAY:
                    enableLongDelay = !enableLongDelay;
                    break;
                case controlMessages::MSG_ENABLE_DELAY_TO_REVERB:
                    enableDelayToReverb = !enableDelayToReverb;
                    break;
            }
        }
        currentVoiceSpace(params);
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

    DynamicDelay<16384> ddelay;
    DynamicDelay<2048> ddelay1;
    DynamicDelay<512> ddelay2;

    maxiDCBlocker dcb;

    float wetdry_mix_{0.5f};

    // mapping
    float lp0fb{0}, lp0cutoff{0};
    float lp1fb{0}, lp1cutoff{0};
    float lp2fb{0}, lp2cutoff{0};
    float lp3fb{0}, lp3cutoff{0};
    float lp4fb{0}, lp4cutoff{0};
    float lp5fb{0}, lp5cutoff{0};
    float lp6fb{0}, lp6cutoff{0};
    float lp7fb{0}, lp7cutoff{0};
    float allp0fb{0}, allp1fb{0}, allp2fb{0}, allp3fb{0};
    float filterBankF0{0}, filterBankF1{0}, filterBankF2{0}, filterBankF3{0};
    float filterBankF4{0}, filterBankF5{0}, filterBankF6{0}, filterBankF7{0};
    float filterBankRes0{0}, filterBankRes1{0}, filterBankRes2{0}, filterBankRes3{0};
    float filterBankRes4{0}, filterBankRes5{0}, filterBankRes6{0}, filterBankRes7{0};
    float ddelayTime{0}, ddelayFeedback{0};
    float ddelayTime1{0}, ddelayFeedback1{0};
    float ddelayTime2{0}, ddelayFeedback2{0};
    float verbVsDelayLevel{0}, delayToVerbLevel{0}, filterBankDelayXFade{0};

    OnePoleSmoother<kN_Params> smoother{150.f, kSampleRate};
    
    maxiDynamicsLite limiter;


};

#endif  
