#ifndef __CHANNEL_STRIP_AUDIO_APP_HPP__
#define __CHANNEL_STRIP_AUDIO_APP_HPP__

#include "src/memllib/audio/AudioAppBase.hpp" // Added missing include
#include "src/memllib/synth/maximilian.h" // Added missing include for maxiSettings, maxiOsc, maxiTrigger, maxiDelayline, maxiEnvGen, maxiLine

#include <cstddef>
#include <cstdint>
#include <memory> // Added for std::shared_ptr

#include "src/memllib/synth/maxiPAF.hpp"
#include "src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>
#include "voicespaces/VoiceSpaces.hpp"

#include "voicespaces/ChannelStrip/basic.hpp"

template<size_t BUFSIZE>
class maxiRingBufLite {
public:
    maxiRingBufLite();

    /*!Add the latest value to the buffer \param x A value*/
    __force_inline void push(float x) {
        buf[idx] = x;
        idx++;
        if (idx==BUFSIZE) {
            idx=0;
        }
    }

    /*! \returns The size of the buffer*/
    size_t size() {return BUFSIZE;}

    /*! \returns the value at the front of the buffer*/
    __force_inline float head() {return idx == 0 ? buf[BUFSIZE-1] : buf[idx-1];}

    /*! \returns the oldest value in the buffer, for a particular window size \param N The size of the window, N < the size of the buffer*/
    __force_inline float tail(const size_t N) {
        float val=0;
        if (idx >= N) {
            val = buf[idx-N];
        }else{
            size_t tailIdx = BUFSIZE - (N-idx);
            val = buf[tailIdx];
        }
        return val;
    }

    using reduceFunction = std::function<float(float, float)>;
    /**
     * Apply a function of the previous N values in the buffer
     * \param N The number of values in the window
     * \param func A function in the form float func(float previousResult, float nextValue)
     * \param initval The initial value to pass into the function (usually 0)
     * \returns The last result of the function, after passing in all values from the window
     * Example: this function will sum the values in the window:
     *     auto sumfunc = [](float val, float n) {return val + n;};
     */
    float reduce(size_t N, reduceFunction func, float initval) {
        float val=0;
        if (idx >= N) {
            for(size_t i=idx-N; i < idx; i++) {
                val = func(val, buf[i]);
            }
        }else{
            //first chunk
            for(size_t i=F64_ARRAY_SIZE(buf)-(N-idx); i < buf.size(); i++) {
                val = func(val, buf[i]);
            }
            //second chunk
            for(int i=0; i < idx; i++) {
                val = func(val, buf[i]);
            }
        }
        return val;
    }



private:
    std::array<float, BUFSIZE> buf{};
    size_t idx=0;
};


// /**
//  * Calculate the Root Mean Square of a signal over a window of time
//  * This is a good measurement of the amount of power in a signal
//  */
// template<size_t BUFSIZE>
// class maxiRMSLite {
//     public:
//         maxiRMS();

//         /*!Configure the analyser \param maxLength The maximum length of time to analyse (ms) \param windowSize The size of the window of time to analyse initially (ms, <= maxLength) */
//         void setup(float maxLength, float windowSize) {
//             buf.setup(maxiConvert::msToSamps(maxLength));
//             setWindowSize(windowSize);
//         }

//         /*!Set the size of the analysis window \param newWindowSize the size of the analysis window (in ms). Large values will smooth out the measurement, and make it less responsive to transients*/
//         void setWindowSize(float newWindowSize) {
//             size_t windowSizeInSamples = maxiConvert::msToSamps(newWindowSize);
//             if (windowSizeInSamples <= buf.size() && windowSizeInSamples > 0) {
//                 windowSize = windowSizeInSamples;
//                 windowSizeInv = 1.f / static_cast<float>(windowSize);
//             }
//             runningRMS = 0;
//         }

//         /*!Find out the size of the analysis window (in ms)*/
//         float getWindowSize() {
//             return maxiConvert::sampsToMs(windowSize);
//         }

//         /*Analyse the signal \param signal a signal \returns RMS*/
//         float play(float signal) {
//             float sigPow2 = (signal * signal);
//             runningRMS -= buf.tail(windowSize);
//             buf.push(sigPow2);
//             runningRMS += sigPow2;
//             return sqrtf(runningRMS * windowSizeInv);
//         }

//     private:
//         maxiRingBuf<BUFSIZE> buf;
//         size_t windowSize=BUFSIZE; // in samples
//         float windowSizeInv=1.f/BUFSIZE;
//         float runningRMS=0;
// };



template<size_t NPARAMS=24>
class ChannelStripAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces=6;

    enum class controlMessages {
        MSG_BYPASS_ALL=0,
        MSG_BYPASS_EQ,
        MSG_BYPASS_COMP,
        MSG_BYPASS_PREPOSTGAIN,
        MSG_BYPASS_INFILTERS,
    };

    queue_t controlMessageQueue;

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

    ChannelStripAudioApp() : AudioAppBase<NPARAMS>() {

        auto voiceSpaceNeve66 = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_CHSTRIP_NEVE66_BODY
        };
        auto voiceSpaceSSL4K = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_CHSTRIP_SSL4KGIST_BODY
        };
        auto voiceSpaceMaleVox = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_CHSTRIP_MALE_VOX_BODY
        };
        auto voiceSpaceFemaleVox = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_CHSTRIP_FEMALE_VOX_BODY
        };
        auto voiceSpaceSSL9K = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_CHSTRIP_SSL9KINDA_BODY
        };
        auto voiceSpaceNeve80 = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_CHSTRIP_NEVE_80
        };
        voiceSpaces[0] = {"WannabeNeve66", voiceSpaceNeve66};
        voiceSpaces[1] = {"SSL 4K G-ist", voiceSpaceSSL4K};
        voiceSpaces[2] = {"SSL 9K-inda", voiceSpaceSSL9K};
        voiceSpaces[3] = {"MaleVox", voiceSpaceMaleVox};
        voiceSpaces[4] = {"FemaleVox", voiceSpaceFemaleVox};
        voiceSpaces[5] = {"Neve 80", voiceSpaceNeve80};

        currentVoiceSpace = voiceSpaces[0].mappingFunction;   
        
        queue_init(&controlMessageQueue, sizeof(controlMessages), 1);

    };


    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float y = x[0];
        float y1 = x[1];
        if (!bypassAll) {

            if (!bypassPrePostGain) {
                y = tanhf(y * preGain);
                y1 = tanhf(y1 * preGain);
            }
            if (!bypassInFilters)
            {
                y = inLowPass.loresChamberlain(y, inLowPassCutoff, 1.f);
                y = inHighPass.hiresChamberlain(y, inHighPassCutoff, 1.f);

                y1 = inLowPass1.loresChamberlain(y1, inLowPassCutoff, 1.f);
                y1 = inHighPass1.hiresChamberlain(y1, inHighPassCutoff, 1.f);
            }
            if (!bypassEQ) {
                y = peak0.play(y);
                y = peak1.play(y);
                y = lowshelf.play(y);
                y = highshelf.play(y);

                y1 = peak0_1.play(y1);
                y1 = peak1_1.play(y1);
                y1 = lowshelf1.play(y1);
                // y1 = highshelf1.play(y1);

            }
            if (!bypassComp) {
                y = dyn.compress(y, compThreshold, compRatio, 0.f);
                y1 = dyn1.compress(y1, compThreshold, compRatio, 0.f);
            }
            if (!bypassPrePostGain) {
                y = tanhf(y * postGain);
                y1 = tanhf(y1 * postGain);
            }
        }
        stereosample_t ret { y, y1};
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        dyn.setLookAhead(0);
        dyn.setAttackHigh(50);
        dyn.setReleaseHigh(200);

    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        controlMessages msg;
        while (queue_try_remove(&controlMessageQueue, &msg)) {
            Serial.printf("ChannelStripAudioApp: received control message %d\n", static_cast<int>(msg));
            switch(msg) {
                case controlMessages::MSG_BYPASS_ALL:
                    bypassAll = !bypassAll;
                    break;
                case controlMessages::MSG_BYPASS_EQ:
                    bypassEQ = !bypassEQ;
                    break;
                case controlMessages::MSG_BYPASS_COMP:
                    bypassComp = !bypassComp;
                    break;
                case controlMessages::MSG_BYPASS_PREPOSTGAIN:
                    bypassPrePostGain = !bypassPrePostGain;
                    break;
                case controlMessages::MSG_BYPASS_INFILTERS:
                    bypassInFilters = !bypassInFilters;
                    break;
            }
        }

        currentVoiceSpace(params);
        dyn.setAttackHigh(compAttack);
        dyn.setReleaseHigh(compRelease);
        peak0.set(maxiBiquad::PEAK, peak0Freq, peak0Q, peak0Gain);
        peak1.set(maxiBiquad::PEAK, peak1Freq, peak1Q, peak1Gain);
        lowshelf.set(maxiBiquad::LOWSHELF, 100.f, 2.f, 3.f);
        highshelf.set(maxiBiquad::HIGHSHELF, 1000.f, 2.f, 3.f);

        dyn1.setAttackHigh(compAttack);
        dyn1.setReleaseHigh(compRelease);
        peak0_1.set(maxiBiquad::PEAK, peak0Freq, peak0Q, peak0Gain);
        peak1_1.set(maxiBiquad::PEAK, peak1Freq, peak1Q, peak1Gain);
        lowshelf1.set(maxiBiquad::LOWSHELF, 100.f, 2.f, 3.f);
        highshelf1.set(maxiBiquad::HIGHSHELF, 1000.f, 2.f, 3.f);
    }
    

protected:

    float sampleRatef = maxiSettings::getSampleRate();

    float preGain=1.f;
    float postGain=1.f;

    maxiFilter inHighPass, inLowPass;
    maxiFilter inHighPass1, inLowPass1;

    float inLowPassCutoff=200.f;
    float inHighPassCutoff=2000.f;

    float compThreshold=0.f;
    float compRatio = 1.f;
    float compAttack=10.f;
    float compRelease=50.f;

    float peak0Freq=100.f;
    float peak0Q=1.f;
    float peak0Gain=1.f;

    float peak1Freq=1000.f;
    float peak1Q=1.f;
    float peak1Gain=1.f;

    float lowShelfFreq=1000.f;
    float lowShelfQ=1.f;
    float lowShelfGain=1.f;

    float highShelfFreq=1000.f;
    float highShelfQ=1.f;
    float highShelfGain=1.f;

    bool bypassAll = false;
    bool bypassEQ = false;
    bool bypassComp = false;
    bool bypassPrePostGain = false;
    bool bypassInFilters = false;

    maxiDynamicsLite dyn,   dyn1;

    maxiBiquad lowshelf;
    maxiBiquad peak0;
    maxiBiquad peak1;
    maxiBiquad highshelf;

    maxiBiquad lowshelf1;
    maxiBiquad peak0_1;
    maxiBiquad peak1_1;
    maxiBiquad highshelf1;
};

#endif  
