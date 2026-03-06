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
    float phaseOffset = 0.f;
    bool lastTrig = false;
    float phasorMul = 1.f;
    float ratioSum=1.f;
    int midiNote = 36;
    float pulseWidth = 0.5f;

    std::array<float, 2> ampRatios{1.f};
    float ampRatioSum=1.f;

};

template<size_t seqLength>
inline bool __not_in_flash_func(ratioSeq)(float phasor, float phaseOffset, float ratioSum, const std::array<float, seqLength> &ratios, float pulseWidth) {
    bool trig     = 0;
    float offsetPhase  = phaseOffset + phasor;
    if (offsetPhase >= 1.f) {
        offsetPhase -= 1.f;
    }
    float phaseAdj           = ratioSum * offsetPhase;
    float accumulatedSum     = 0;
    float lastAccumulatedSum = 0;
    for (size_t v : ratios)
    {
        accumulatedSum += v;
        if (phaseAdj <= accumulatedSum)
        {
            // check pulse width
            float beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    return trig;
}


template<size_t NPARAMS=56, size_t NSEQUENCES=8>
class BreakOrAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces=0;

    queue_t bpmQueue;
    queue_t sequencerControlQueue;
    queue_t i2cOutQueue;
    queue_t barPhaseResetQueue;

    enum SequencerClockModes {
        INTERNAL,
        MIDI_CLOCK
    } sequencerClockMode = INTERNAL;


    bool sequencerPlaying = false;

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
        queue_init(&bpmQueue, sizeof(float), 1);
        queue_init(&sequencerControlQueue, sizeof(int), 1);
        queue_init(&i2cOutQueue, sizeof(float) * 8, 1);
        queue_init(&barPhaseResetQueue, sizeof(int), 1);
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
        if (sequencerPlaying) {
            if (sequencerClockMode == INTERNAL){
                midiClockPhasor += midiClockPhasorInc;
                if (midiClockPhasor >= 1.f) {
                    midiClockPhasor -= 1.f;
                    midiIO->queueClock();
                    midiIO->flushQueue();
                }
            }
            if (sequencingSampleCounter==0) {
                
                barPhasor += barPhasorInc;
                if (barPhasor >= 1.f) {
                    barPhasor -= 1.f;
                }
                if (sequencerClockMode == MIDI_CLOCK) {
                    int phasorResetValue=0;
                    if (queue_try_remove(&barPhaseResetQueue, &phasorResetValue)) {
                        barPhasor = 0.f;
                        Serial.println("Bar phase reset triggered by queue");
                    }
                }
                int i2cIdx=0;
                for(auto &seq: ratioSeqStates) {
                    //update phasor
                    float seqPhasor = barPhasor * seq.phasorMul;
                    seqPhasor = fmodf(seqPhasor + seq.phaseOffset, 1.f); // Wrap phasor to [0,1]

                    bool trig = ratioSeq<3>(seqPhasor, seq.phaseOffset, seq.ratioSum, seq.ratios, seq.pulseWidth);
                    bool highAmp = ratioSeq<2>(seqPhasor, seq.phaseOffset, seq.ampRatioSum, seq.ampRatios, 0.5f);
                    if (trig && !seq.lastTrig) {
                        if (trig) {
                            midiIO->queueNoteOn(seq.midiNote, highAmp ? 127 : 64);
                        }else{
                            midiIO->queueNoteOff(seq.midiNote, 0);
                        }
                    }
                    seq.lastTrig = trig;
                    i2cValues[i2cIdx++] = trig;
                }
                midiIO->flushQueue();
                queue_try_add(&i2cOutQueue, &i2cValues);
            }

            sequencingSampleCounter ++;
            if (sequencingSampleCounter >= sequencingSampleDiv) {
                sequencingSampleCounter = 0;
            }


        }
        stereosample_t ret { 0.f,0.f };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface, SequencerClockModes clockMode) 
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        sampleRatef = static_cast<float>(sample_rate);
        sequencerClockMode = clockMode;
        updateBPM(90.f);
        const size_t midiNotes[NSEQUENCES] = {36,37,38,39,40, 42,43,45/*,47,48*/};
        size_t midiNote = 0;
        for(auto &seq: ratioSeqStates) {
            seq.midiNote = midiNotes[midiNote++];
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
        // if (sequencerPlaying) {
        // }
        firstParamsReceived = true;
        if (queue_try_remove(&bpmQueue, &bpm)) {
            // bpm = 30.f + (bpm * 200.f); // Scale BPM from [0,1] to [30,230]
            updateBPM(bpm);
        }
        int sequencerControl;
        if (queue_try_remove(&sequencerControlQueue, &sequencerControl)) {
            sequencerPlaying = sequencerControl == 1;
            if (!sequencerPlaying) {
                midiIO->queueClockStop();
                // Send note offs for all sequences when stopping
                for(auto &seq: ratioSeqStates) {
                    midiIO->queueNoteOff(seq.midiNote, 0);
                    seq.phasor = 0.f; // Reset phasor to start when stopping
                }
                midiIO->flushQueue();
                midiClockPhasor = 0.f; // Reset MIDI clock phasor when stopping
                sequencingSampleCounter = 0; // Reset sequencing sample counter
                
            }else{
                midiIO->queueClockStart();
                midiIO->flushQueue();
            }
            Serial.printf("Sequencer %s\n", sequencerPlaying ? "Playing" : "Stopped");
        }
        // currentVoiceSpace(params);
        size_t paramIdx = 0;
        for(auto &v: ratioSeqStates) {
            float sum=0.f;
            for(size_t i=0; i < v.ratios.size(); i++) {
                v.ratios[i] = (float)(int)(params[paramIdx++] * 3.f) + 1.f; 
                sum += v.ratios[i];
            }
            v.ratioSum = sum;
            // static float muls[7] = {0.25f, 0.33f, 0.5f, 1.f, 1.5f, 2.f, 3.f};
            // v.phasorMul = muls[(int)(params[paramIdx++] * 6.999999f)];
            static float muls[4] = {1.f, 2.f, 4.f, 8.f};
            v.phasorMul = muls[(int)(params[paramIdx++] * 3.999999f)];
            v.phaseOffset = ((int)(params[paramIdx++] * timeSigBeats)) * timeSigBeatsInv;

            sum=0.f;
            for(size_t i=0; i < v.ampRatios.size(); i++) {
                v.ampRatios[i] = (float)(int)(params[paramIdx++] * 3.f) + 1.f; 
                sum += v.ampRatios[i];
            }
            v.ampRatioSum = sum;
        }
        // Serial.printf("pm: %f", ratioSeqStates[0].phasorMul);

    }

    void updateBPM(float newBPM) {
        bpm = newBPM;
        float beatLengthInSeconds = 60.f / bpm;
        float barLengthInSeconds = beatLengthInSeconds * timeSigBeats;
        float barLengthInSamples = barLengthInSeconds * (sampleRatef/ sequencingSampleDiv); 
        barPhasorInc = 1.f/ barLengthInSamples;
        // for(auto &v: ratioSeqStates) {
        //     v.phasorInc = barPhasorInc;
        // }
        float midiClockLengthInSeconds = beatLengthInSeconds / 24.f;
        float midiClockLengthInSamples = midiClockLengthInSeconds * sampleRatef;
        midiClockPhasorInc = 1.f / midiClockLengthInSamples;
    }

    void setTimeSignature(float beats, float division) {
        timeSigBeats = beats;
        timeSigDivision = division;
        updateBPM(bpm); // Recalculate phasor increment with new time signature
    }

    

protected:

    float i2cValues[8] = {0,0,0,0,0,0,0,0};

    float sampleRatef;
    bool firstParamsReceived = false;

    float bpm=90.f;
    float timeSigBeats=4.f;
    float timeSigBeatsInv=1.f/timeSigBeats;
    float timeSigDivision=4.f;

    float barPhasorInc=0.f;
    float barPhasor;
    
    size_t sequencingSampleDiv = 400; 
    size_t sequencingSampleCounter = 0;

    std::array<ratioSeqState, NSEQUENCES> ratioSeqStates;

    float midiClockPhasor=0;
    float midiClockPhasorInc=0;
    
};

#endif  // __BREAKOR_AUDIO_APP_HPP__

