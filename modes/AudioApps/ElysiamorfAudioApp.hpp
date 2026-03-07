#ifndef __ELYSIAMORF_AUDIO_APP_HPP__
#define __ELYSIAMORF_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/interface/MIDIInOut.hpp"


#include <cstddef>
#include <cstdint>
#include <memory> // Added for std::shared_ptr

#include "../../src/memllib/interface/InterfaceBase.hpp"


#include <span>

#include "../../voicespaces/VoiceSpaces.hpp"


struct FMOp {
    float prevOutput = 0.f;

    // phase:    external phasor 0-1 (e.g. barPhasor * phasorMul)
    // modIn:    signal from another operator [-1, 1], 0 = none
    // freqMul:  cycles per phase period (1 = once per bar, 2 = twice, 0.5 = half, etc.)
    // modIndex: depth of modulation — scales how much modIn shifts the phase
    // fbLevel:  feedback 0-1 — feeds prevOutput back into own phase
    float __force_inline process(float phase, float modIn,
                                        float freqMul, float modIndex,
                                        float fbLevel) {
        float p = fmodf(phase * freqMul + modIndex * modIn + fbLevel * prevOutput, 1.f);
        if (p < 0.f) p += 1.f;
        float out = sinf(TWO_PI * p);
        prevOutput = out;
        return out;
    }
};

// Convenience: 2-op FM pair matching the (phase, carrierFreq, modFreq, modIndex, fbLevel) signature.
// fbLevel sits on the modulator (DX7 style). carrier and modulator are caller-owned FMOp instances.
float __force_inline fmPair(float phase,
                                   float carrierFreq, float modFreq,
                                   float modIndex, float fbLevel,
                                   FMOp& carrier, FMOp& modulator) {
    float modOut = modulator.process(phase, 0.f, modFreq, 0.f, fbLevel);
    return carrier.process(phase, modOut, carrierFreq, modIndex, 0.f);
}

struct fmSeqState {
    float phaseOffset = 0.f;
    float phasorMul = 1.f;
    float ratio=1.f;
    float carrierFreq=1.f;
    float modFreq = 2.f;
    float modIndex = 2.f;
    float fbLevel = 0.f;
    FMOp carrier;
    FMOp modulator;
};


#include "RatioSeq.hpp"

template<size_t NPARAMS=56, size_t NSEQUENCES=8>
class ElysiamorfAudioApp : public AudioAppBase<NPARAMS>
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

    ElysiamorfAudioApp() : AudioAppBase<NPARAMS>() {
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
                    }
                }
                // int i2cIdx=0;
                int ccIndex=0;
                static int ccNumbers[8] = {1,2,3,4,5,9,11,12};
                // for(auto &seq: ratioSeqStates) {
                //     //update phasor
                //     float seqPhasor = barPhasor * seq.phasorMul;
                //     seqPhasor = fmodf(seqPhasor + seq.phaseOffset, 1.f); // Wrap phasor to [0,1]

                //     bool trig = ratioSeq<3>(seqPhasor, seq.phaseOffset, seq.ratioSum, seq.ratios, seq.pulseWidth);
                //     bool highAmp = ratioSeq<2>(seqPhasor, seq.phaseOffset, seq.ampRatioSum, seq.ampRatios, 0.5f);
                //     if (trig != seq.lastTrig) {
                //         midiIO->queueCC(ccNumbers[ccIndex++], trig ? highAmp ? 127 : 64 : 0);
                //         // Serial.printf("Seq %d - Phasor: %f, Trig: %d, HighAmp: %d\n", &seq - &ratioSeqStates[0], seqPhasor, trig, highAmp);
                //     }
                //     seq.lastTrig = trig;
                //     // i2cValues[i2cIdx++] = trig;
                // }
                for(auto &seq: fmSeqStates) {
                    //update phasor
                    float seqPhasor = barPhasor * seq.phasorMul;
                    seqPhasor = fmodf(seqPhasor + seq.phaseOffset, 1.f); // Wrap phasor to [0,1]
                    float fmVal = fmPair(seqPhasor, seq.carrierFreq, seq.modFreq, seq.modIndex, seq.fbLevel, seq.carrier, seq.modulator);
                    fmVal = (fmVal + 1.f) * 0.5f; // Normalize to [0,1]
                    fmVal *= 127.f; // Scale to MIDI range
                    // if (ccIndex==0) {
                    //     Serial.printf("Seq %d - Phasor: %f, FM Val: %f\n", &seq - &fmSeqStates[0], seqPhasor, fmVal);
                    // }
                    midiIO->queueCC(ccNumbers[ccIndex++], static_cast<uint8_t>(fmVal));
                }
                midiIO->flushQueue();
                // queue_try_add(&i2cOutQueue, &i2cValues);
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
        if (queue_try_remove(&bpmQueue, &bpm)) {
            updateBPM(bpm);
            Serial.printf("BPM updated: %f\n", bpm);
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
        // size_t paramIdx = 0;
        // for(auto &v: ratioSeqStates) {
        //     float sum=0.f;
        //     for(size_t i=0; i < v.ratios.size(); i++) {
        //         v.ratios[i] = (float)(int)(params[paramIdx++] * 3.f) + 1.f;
        //         sum += v.ratios[i];
        //     }
        //     v.ratioSum = sum;
        //     // static float muls[7] = {0.25f, 0.33f, 0.5f, 1.f, 1.5f, 2.f, 3.f};
        //     // v.phasorMul = muls[(int)(params[paramIdx++] * 6.999999f)];
        //     static float muls[4] = {1.f, 2.f, 4.f, 8.f};
        //     v.phasorMul = muls[(int)(params[paramIdx++] * 3.999999f)];
        //     v.phaseOffset = ((int)(params[paramIdx++] * timeSigBeats)) * timeSigBeatsInv;

        //     sum=0.f;
        //     for(size_t i=0; i < v.ampRatios.size(); i++) {
        //         v.ampRatios[i] = (float)(int)(params[paramIdx++] * 3.f) + 1.f;
        //         sum += v.ampRatios[i];
        //     }
        //     v.ampRatioSum = sum;
        // }
        size_t paramIdx = 0;
        for(auto &v: fmSeqStates) {
            v.carrierFreq = (0.25f + params[paramIdx++] * 0.75f) * 0.125f; // 1 to 10
            v.modFreq = (0.25f + params[paramIdx++] * 0.75f) * 0.25f; // 1 to 10
            v.modIndex = params[paramIdx++] * 4.f; // 0 to 10
            v.fbLevel = 0.f;//params[paramIdx++] ; // 0 to 1
            static float muls[4] = {1.f};
            v.phasorMul = 1.f;//muls[(int)(params[paramIdx++])];
            v.phaseOffset = ((int)(params[paramIdx++] * timeSigBeats)) * timeSigBeatsInv;
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

    size_t sequencingSampleDiv = 500;
    size_t sequencingSampleCounter = 0;

    std::array<ratioSeqState, NSEQUENCES> ratioSeqStates;
    std::array<fmSeqState, NSEQUENCES> fmSeqStates;


    float midiClockPhasor=0;
    float midiClockPhasorInc=0;

};

#endif  // __ELYSIAMORF_AUDIO_APP_HPP__

