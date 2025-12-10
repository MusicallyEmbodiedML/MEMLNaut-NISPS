#ifndef __CHANNEL_STRIP_AUDIO_APP_HPP__
#define __CHANNEL_STRIP_AUDIO_APP_HPP__

#include "src/memllib/audio/AudioAppBase.hpp" // Added missing include
#include "src/memllib/synth/maximilian.h" // Added missing include for maxiSettings, maxiOsc, maxiTrigger, maxiDelayline, maxiEnvGen, maxiLine

#include <cstddef>
#include <cstdint>
#include <memory> // Added for std::shared_ptr

#include "src/memllib/synth/maxiPAF.hpp"
#include "src/memllib/synth/ADSRLite.hpp"
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

class maxiDynamicsLite {

    public:

        enum ANALYSERS {PEAK, RMS};

        static constexpr float maxRMSSizeMS = 300.f;

        maxiDynamicsLite() {
            //define detector functions
            inputPeak = [](float sig) {
                return abs(sig);
            };

            rms.setup(maxRMSSizeMS,300);
            inputRMS = [&](float sig) {
                return rms.play(sig);
            };

            //default RMS
            inputAnalyser = inputRMS;

            //setup envelopes
            arEnvHigh.setup(10,0,1.f,10.f, maxiSettings::sampleRate);
            arEnvLow.setup(10,0,1.f,10.f, maxiSettings::sampleRate);

            lookAheadDelay.setup(maxiSettings::sampleRate * 0.1); //max 0.1s
        }


        /**
         * This functions compands the signal, providing download compression or upward expansion above an upper thresold, and
         * upward compression or downward expansion below a lower threshold.
         * \param sig The input signal to be companded
         * \param control This signal is used to trigger the compander. Use it for sidechaining, or if no sidechain is needed, use the same signal for this and the input signal
         * \param thresholdHigh The high threshold, in Dbs
         * \param ratioHigh The ratio for companding above the high threshold
         * \param kneeHigh The size of the knee for companding above the high threshold (in Dbs)
         * \param thresholdLow The low threshold, in Dbs
         * \param ratioLow The ratio for companding below the low threshold
         * \param kneeLow The size of the knee for companding below the low threshold (in Dbs)
         * \returns a companded signal
         */
        __attribute__((always_inline)) __attribute__((hot)) float play(float sig, float control,
            float thresholdHigh, float ratioHigh, float kneeHigh,
            float thresholdLow, float ratioLow, float kneeLow
        ) {
            const float inputEnv = inputAnalyser(control) + 0.00001f; //avoid log of zero   
            const float controlDB = maxiConvert::ampToDbs(inputEnv);
            float outDB = controlDB; 
            const float halfKneeHigh = kneeHigh * 0.5f;
            //companding above the high threshold
            if (ratioHigh > 0) {
                if (kneeHigh > 0) {
                    float lowerKnee = thresholdHigh - (kneeHigh*0.5f);
                    float higherKnee = thresholdHigh  +(kneeHigh*0.5f);
                    //attack/release
                    float envRatio = 1.f;
                    if (controlDB >= lowerKnee) {
                        arEnvHigh.triggerIfReady(1.f);
                        float envVal = arEnvHigh.play();
                        envRatio = envToRatio(envVal, ratioHigh);
                    }else {
                        arEnvHigh.release();
                    }
                    if ((controlDB >= lowerKnee) && (controlDB < higherKnee)) {
                        float kneeHighOut = ((higherKnee - thresholdHigh) / envRatio) + thresholdHigh;
                        float kneeRange = (kneeHighOut - lowerKnee);
                        float t = (controlDB - lowerKnee) / kneeHigh;
                        //bezier on x only
                        float curve =  ratioHigh > 1.f ? 0.8f : 0.2f;
                        float kneex = (2.f * (1.f-t) * t * curve) + (t*t);
                        outDB = lowerKnee + (kneex * kneeRange);
                    }
                    else if (controlDB >= higherKnee) {
                        outDB = ((controlDB - thresholdHigh) / envRatio) + thresholdHigh;
                    }else{
                        outDB = controlDB;
                    }
                }
                else {
                    //no knee
                    if (controlDB > thresholdHigh) {
                        arEnvHigh.trigger(1.f);
                    }else {
                        arEnvHigh.release();
                    }
                    float envVal = arEnvHigh.play();
                    // const float envVal = arEnvHigh.play(controlDB > thresholdHigh ? 1.f : 0.f);
                    const float envRatio = envToRatio(envVal, ratioHigh);
                    outDB = ((controlDB - thresholdHigh) / envRatio) + thresholdHigh;
  
                }
            }
            // //companding below the low threshold
            // if (ratioLow > 0) {
            //     if (kneeLow > 0) {
            //         float lowerKnee = thresholdLow - (kneeLow*0.5f);
            //         float higherKnee = thresholdLow  +(kneeLow*0.5f);
            //         //attack/release
            //         float envRatio = 1;
            //         if (controlDB < lowerKnee) {
            //             float envVal = arEnvLow.play(1.f);
            //             envRatio = envToRatio(envVal, ratioLow);
            //         }else {
            //             float envVal = arEnvLow.play(-1.f);
            //         }
            //         if ((controlDB >= lowerKnee) && (controlDB < higherKnee)) {
            //             float kneeLowOut = thresholdLow - ((thresholdLow-lowerKnee) / ratioLow);
            //             float kneeRange = (higherKnee - kneeLowOut);
            //             float t = (controlDB - lowerKnee) / kneeLow;
            //             //bezier on x only
            //             float curve =  ratioLow > 1.f ? 0.2f : 0.8f;
            //             float kneex = (2.f * (1.f-t) * t * curve) + (t*t);
            //             outDB = kneeLowOut + (kneex * kneeRange);
            //         }
            //         else if (controlDB < lowerKnee) {
            //             outDB = thresholdLow - ((thresholdLow-controlDB) / ratioLow);
            //         }
            //     }
            //     else {
            //         //no knee
            //         if (controlDB < thresholdLow) {
            //             float envVal = arEnvLow.play(1.f);
            //             // float envRatio = envToRatio(envVal, ratioLow);
            //             outDB = thresholdLow - ((thresholdLow-controlDB) / ratioLow);
            //         }else {
            //             float envVal = arEnvLow.play(-1.f);
            //             outDB = maxiConvert::ampToDbs(fabsf(sig));                        
            //         }
            //     }
            // }
            //scale the signal according to the amount of compansion on the control signal
            float outAmp = maxiConvert::dbsToAmp(outDB);
            // float ctrlAmp = maxiConvert::dbsToAmp(controlDB);
            float sigOut = sig;
              if (outAmp > 0.f) {
                if (lookAheadSize > 0.f) {
                    lookAheadDelay.push(sig);
                    sigOut = lookAheadDelay.tail(lookAheadSize);
                }
                // sigOut = sigOut * fabsf(control / outAmp);
                float gainReduction = outAmp / inputEnv;
                sigOut = sig * gainReduction;    
                // PERIODIC_DEBUG(1000,
                //     Serial.printf("%f %f %f %f %f\n",controlDB, outDB, control, outAmp, gainReduction);
                // )
            }else{
                // printf("Warning: maxiDynamicsLite output amplitude is zero or negative!\n");
            }
            return sigOut;
        }

        /**
         * Compress a signal (using downward compression)
         * \param sig The input signal to be compressed
         * \param threshold The threshold, in Dbs
         * \param ratio The compression ratio (>1 provides compression, <1 provides expansion)
         * \param knee The size of the knee (in Dbs)
         * \returns a compressed signal
         */
        __attribute__((always_inline)) __attribute__((hot)) float compress(float sig, float threshold, float ratio, float knee) {
            return play(sig, sig, threshold, ratio, knee, 0.f, 0.f, 0.f);
        }
        /**
         * Compress a signal with sidechaining (using downward compression)
         * \param sig The input signal to be compressed
         * \param control The sidechain signal
         * \param threshold The threshold, in Dbs
         * \param ratio The compression ratio (>1 provides compression, <1 provides expansion)
         * \param knee The size of the knee (in Dbs)
         * \returns a compressed signal
         */
        float sidechainCompress(float sig, float control, float threshold, float ratio, float knee) {
            return play(sig, control, threshold, ratio, knee, 0, 0, 0);
        }
        /**
         * Compand a signal, using detection above a threshold (provides downward compression or upward expansion)
         * \param sig The input signal to be compressed
         * \param control The sidechain signal
         * \param threshold The threshold, in Dbs
         * \param ratio The compression ratio (>1 provides compression, <1 provides expansion)
         * \param knee The size of the knee (in Dbs)
         * \returns a companded signal
         */
        float compandAbove(float sig, float control, float threshold, float ratio, float knee) {
            return play(sig, control, threshold, ratio, knee, 0, 0, 0);
        }
        /**
         * Compand a signal, using detection below a threshold (provides upward compression or downward expansion)
         * \param sig The input signal to be compressed
         * \param control The sidechain signal
         * \param threshold The threshold, in Dbs
         * \param ratio The compression ratio (>1 provides compression, <1 provides expansion)
         * \param knee The size of the knee (in Dbs)
         * \returns a companded signal
         */
        float compandBelow(float sig, float control, float threshold, float ratio, float knee) {
            return play(sig, control, 0, 0, 0, threshold, ratio, knee);
        }

        /**
         * Set the attack time for the high threshold. This is the amount of time over which the ratio moves from 1 to its full value, following the input analyser going over the threshold.
         * \param attack The attack time (in milliseconds)
         */
        __attribute__((always_inline)) void setAttackHigh(float attack) {
            arEnvHigh.setAttackTime(attack, maxiSettings::sampleRate);
        }
        /**
         * Set the release time for the high threshold. This is the amount of time over which the ratio moves from its full value to 1, following the input analyser going under the threshold.
         * \param release The release time (in milliseconds)
         */
        __attribute__((always_inline)) void setReleaseHigh(float release) {
            arEnvHigh.setReleaseTime(release, maxiSettings::sampleRate);
        }
        /**
         * Set the attack time for the low threshold. This is the amount of time over which the ratio moves from 1 to its full value, following the input analyser going under the threshold.
         * \param attack The attack time (in milliseconds)
         */
        __attribute__((always_inline)) void setAttackLow(float attack) {
            arEnvLow.setAttackTime(attack, maxiSettings::sampleRate);
        }
        /**
         * Set the release time for the low threshold. This is the amount of time over which the ratio moves from its full value to 1, following the input analyser going over the threshold.
         * \param release The release time (in milliseconds)
         */
        __attribute__((always_inline)) void setReleaseLow(float release) {
            arEnvLow.setReleaseTime(release, maxiSettings::sampleRate);
        }

        /**
         * The look ahead creates a delay on the input signal, meaning that that the signal is compressed according to event that have already happened in the control signal.  This can be useful for limiting and catching fast transients.
         * \param length The amount of time the compressor looks ahead (in milliseconds)
         */
        void setLookAhead(float length) {
            lookAheadSize = maxiConvert::msToSamps(length);
            lookAheadSize = std::min(lookAheadSize, lookAheadDelay.size());
        }
        /**
         * \returns the look ahead time (in milliseconds)
         */
        float getLookAhead() {
            return maxiConvert::sampsToMs(lookAheadSize);
        }

        /**
         * Set the size of the RMS window.  Longer times give a slower response
         * \param winSize The size of the window (in milliseconds)
         */
        void setRMSWindowSize(float winSize) {
            rms.setWindowSize(std::min(winSize, maxRMSSizeMS));
        }

        /**
         * Set the method by which the compressor analyses the control input
         * \mode maxiDynamics::PEAK for peak analysis, maxiDynamics::RMS for rms analysis
         */
        void setInputAnalyser(ANALYSERS mode) {
            if (mode == PEAK) {
                inputAnalyser = inputPeak;
            }else{
                inputAnalyser = inputRMS;
            }
        }


    private:
        ADSRLite arEnvHigh, arEnvLow;
        maxiRingBuf lookAheadDelay;
        size_t lookAheadSize = 0;
        maxiRMS rms;
        std::function<float(float)> inputPeak;
        std::function<float(float)> inputRMS;
        std::function<float(float)> inputAnalyser;
        // maxiPoll poll;

        //mapping from attack/release envelope to ratio
        inline float envToRatio(float envVal, float ratio) {
            float envRatio = 1.f;
            if (ratio > 1.f) {
                envRatio = 1.f + ((ratio-1.f) * envVal);
            }else {
                envRatio = 1.f - ((1.f-ratio) * envVal);
            }
            return envRatio;
        }

};


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
        if (!bypassAll) {

            if (!bypassPrePostGain) {
                y = tanhf(y * preGain);
            }
            if (!bypassInFilters)
            {
                y = inLowPass.loresChamberlain(y, inLowPassCutoff, 1.f);
                y = inHighPass.hiresChamberlain(y, inHighPassCutoff, 1.f);
            }
            if (!bypassEQ) {
                y = peak0.play(y);
                y = peak1.play(y);
                y = lowshelf.play(y);
                y = highshelf.play(y);
            }
            if (!bypassComp) {
                y = dyn.compress(y, compThreshold, compRatio, 0.f);
            }
            if (!bypassPrePostGain) {
                y = tanhf(y * postGain);
            }
        }
        stereosample_t ret { y, y};
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
    }
    

protected:

    float sampleRatef = maxiSettings::getSampleRate();

    float preGain=1.f;
    float postGain=1.f;

    maxiFilter inHighPass, inLowPass;

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

    maxiDynamicsLite dyn;

    maxiBiquad lowshelf;
    maxiBiquad peak0;
    maxiBiquad peak1;
    maxiBiquad highshelf;

};

#endif  
