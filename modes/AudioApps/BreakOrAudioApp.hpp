#ifndef __BREAKOR_AUDIO_APP_HPP__
#define __BREAKOR_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp" 
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/interface/MIDIInOut.hpp"


#include <cstddef>
#include <cstdint>
#include <memory> // Added for std::shared_ptr

#include "../../src/memllib/interface/InterfaceBase.hpp" 

#include <span>

#include "../../voicespaces/VoiceSpaces.hpp"

struct ratioSeqState {
    std::array<float, 3> ratios{1.f};
    float phasor=0.f;
    float phasorInc=0.f;
    bool lastTrig = false;
    float phasorMul = 1.f;
    float ratioSum=1.f;
    int midiNote = 36;
    float pulseWidth = 0.5f;
};

template<size_t seqLength>
inline bool __not_in_flash_func(ratioSeq)(ratioSeqState &seq) {
    bool trig     = 0;
    float phaseAdj           = seq.ratioSum * seq.phasor;
    float accumulatedSum     = 0;
    float lastAccumulatedSum = 0;
    for (size_t v : seq.ratios)
    {
        accumulatedSum += v;
        if (phaseAdj <= accumulatedSum)
        {
            // check pulse width
            //TODO: could precalc acc-lastacc
            float beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= seq.pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    return trig;
}


template<size_t NPARAMS=50, size_t NSEQUENCES=10>
class BreakOrAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces=0;

    std::shared_ptr<MIDIInOut> midiIO;

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
        if (sequencingSampleCounter==0) {
            for(auto &seq: ratioSeqStates) {
                //update phasor
                seq.phasor += (seq.phasorInc * seq.phasorMul);
                if (seq.phasor >= 1.f) {
                    seq.phasor -= 1.f;
                }
                bool trig = ratioSeq<3>(seq);
                if (trig && !seq.lastTrig) {
                    if (trig) {
                        midiIO->queueNoteOn(seq.midiNote, 127);
                    }else{
                        midiIO->queueNoteOff(seq.midiNote, 0);
                    }
                }
                seq.lastTrig = trig;
            }
            midiIO->flushQueue();
        }

        sequencingSampleCounter ++;
        if (sequencingSampleCounter >= sequencingSampleDiv) {
            sequencingSampleCounter = 0;
        }

        stereosample_t ret { 0.f,0.f };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        sampleRatef = static_cast<float>(sample_rate);
        updateBPM(90.f);
        size_t midiNote = 36;
        for(auto &seq: ratioSeqStates) {
            seq.midiNote = midiNote++;
        }
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midiIO = new_midi_interf;
    }

    void loop() override {
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        firstParamsReceived = true;
        // currentVoiceSpace(params);
        size_t paramIdx = 0;
        for(auto &v: ratioSeqStates) {
            float sum=0.f;
            for(size_t i=0; i < v.ratios.size(); i++) {
                v.ratios[i] = (float)(int)(params[paramIdx++] * 7.f) + 1.f; 
                sum += v.ratios[i];
            }
            v.ratioSum = sum;
            v.phasorMul = ((float)(int)(params[paramIdx++] * 2.f) * 0.5f)+ 1.f;
        }

    }

    void updateBPM(float newBPM) {
        bpm = newBPM;
        float beatLengthInSeconds = 60.f / bpm;
        float barLengthInSeconds = beatLengthInSeconds * timeSigBeats;
        float barLengthInSamples = barLengthInSeconds * (sampleRatef/ sequencingSampleDiv); 
        float barPhasorInc = 1.f/ barLengthInSamples;
        for(auto &v: ratioSeqStates) {
            v.phasorInc = barPhasorInc;
        }
    }

    void setTimeSignature(float beats, float division) {
        timeSigBeats = beats;
        timeSigDivision = division;
        updateBPM(bpm); // Recalculate phasor increment with new time signature
    }

    

protected:

    float sampleRatef;
    bool firstParamsReceived = false;

    float bpm=90.f;
    float timeSigBeats=4.f;
    float timeSigDivision=4.f;
    
    size_t sequencingSampleDiv = 50; // How often the sequencer should update (in Hz)
    size_t sequencingSampleCounter = 0;

    std::array<ratioSeqState, NSEQUENCES> ratioSeqStates;
    
};

#endif  // __BREAKOR_AUDIO_APP_HPP__

