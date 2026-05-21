#ifndef __BREAKOR_AUDIO_APP_HPP__
#define __BREAKOR_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/interface/MIDIInOut.hpp"


#include <cstddef>
#include <cstdint>
#include <memory>

#include "../../src/memllib/interface/InterfaceBase.hpp"


#include <span>

#include "../../voicespaces/VoiceSpaces.hpp"

#include "RatioSeq.hpp"
#include "RatioSeqEngine.hpp"
#include "FMPatternGen.hpp"


template<size_t NPARAMS=56, size_t NSEQUENCES=8>
class BreakOrAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params    = NPARAMS;
    static constexpr size_t nVoiceSpaces = 0;
    static constexpr size_t kNRatioSeqs  = NSEQUENCES / 2;  // 4 — trigger outputs
    static constexpr size_t kNFMSeqs     = NSEQUENCES / 2;  // 4 — continuous outputs

    static constexpr uint32_t kFocusS0 = (1u << 0);
    static constexpr uint32_t kFocusS1 = (1u << 1);
    static constexpr uint32_t kFocusS2 = (1u << 2);
    static constexpr uint32_t kFocusS3 = (1u << 3);
    static constexpr uint32_t kFocusS4 = (1u << 4);
    static constexpr uint32_t kFocusS5 = (1u << 5);
    static constexpr uint32_t kFocusS6 = (1u << 6);
    static constexpr uint32_t kFocusS7 = (1u << 7);

    static constexpr std::array<uint32_t, NPARAMS> kParamGroupMask = {
        kFocusS0, kFocusS0, kFocusS0, kFocusS0, kFocusS0, kFocusS0, kFocusS0,  // 0-6
        kFocusS1, kFocusS1, kFocusS1, kFocusS1, kFocusS1, kFocusS1, kFocusS1,  // 7-13
        kFocusS2, kFocusS2, kFocusS2, kFocusS2, kFocusS2, kFocusS2, kFocusS2,  // 14-20
        kFocusS3, kFocusS3, kFocusS3, kFocusS3, kFocusS3, kFocusS3, kFocusS3,  // 21-27
        kFocusS4, kFocusS4, kFocusS4, kFocusS4, kFocusS4, kFocusS4, kFocusS4,  // 28-34
        kFocusS5, kFocusS5, kFocusS5, kFocusS5, kFocusS5, kFocusS5, kFocusS5,  // 35-41
        kFocusS6, kFocusS6, kFocusS6, kFocusS6, kFocusS6, kFocusS6, kFocusS6,  // 42-48
        kFocusS7, kFocusS7, kFocusS7, kFocusS7, kFocusS7, kFocusS7, kFocusS7,  // 49-55
    };

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

    RatioSeqEngine<kNRatioSeqs> seqEngine;
    std::array<FMPatternGenState, kNFMSeqs> fmStates;

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
        queue_init(&bpmQueue, sizeof(float), 1);
        queue_init(&sequencerControlQueue, sizeof(int), 1);
        queue_init(&i2cOutQueue, sizeof(float) * 8, 1);
        queue_init(&barPhaseResetQueue, sizeof(int), 1);
    };

    bool __force_inline euclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth)
    {
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
                seqEngine.midiClockPhasor += seqEngine.midiClockPhasorInc;
                if (seqEngine.midiClockPhasor >= 1.f) {
                    seqEngine.midiClockPhasor -= 1.f;
                    midiIO->queueClock();
                    midiIO->flushQueue();
                }
            }
            if (sequencerClockMode == MIDI_CLOCK) {
                int phasorResetValue=0;
                if (queue_try_remove(&barPhaseResetQueue, &phasorResetValue)) {
                    seqEngine.resetBar();
                }
            }

            if (seqEngine.tick()) {
                int ccIndex=0;
                // static int ccNumbers[8] = {1,2,3,4,5,9,11,12};
                static int ccNumbers[8] = {1,2,3,4,5,9,11,12};
                // Trigger outputs (S1–S4)
                for (size_t i = 0; i < kNRatioSeqs; i++) {
                    const float trigVal = seqEngine.states[i].lastTrig ? 1.f : 0.f;
                    i2cValues[i] = trigVal;
                    midiIO->queueCC(ccNumbers[ccIndex++], static_cast<uint8_t>(trigVal * 127.f));
                }
                // Continuous FM outputs (S5–S8)
                float bp = seqEngine.getBarPhasor();
                for (size_t i = 0; i < kNFMSeqs; i++) {
                    auto& s = fmStates[i];
                    float p = fmodf(bp * s.phasorMul + s.phaseOffset, 1.f);
                    if (p < 0.f) p += 1.f;
                    float v = fmPair(p, s.carrierFreq, s.modFreq, s.modIndex, s.fbLevel,
                                     s.carrier, s.modulator);
                    const float fmVal = (v + 1.f) * 0.5f;
                    i2cValues[kNRatioSeqs + i] = fmVal;
                    midiIO->queueCC(ccNumbers[ccIndex++], static_cast<uint8_t>(fmVal * 127.f));
                }
                midiIO->flushQueue();
                queue_try_add(&i2cOutQueue, &i2cValues);
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

        const size_t midiNotes[kNRatioSeqs] = {36, 37, 38, 39};
        for (size_t i = 0; i < kNRatioSeqs; i++) {
            seqEngine.states[i].midiNote = midiNotes[i];
        }

        seqEngine.setup(sample_rate);
        seqEngine.updateBPM(90.f);

        seqEngine.onNoteOn = [this](size_t seqIdx, int velocity) {
            midiIO->queueNoteOn(seqEngine.states[seqIdx].midiNote, velocity);
        };
        seqEngine.onNoteOff = [this](size_t seqIdx) {
            midiIO->queueNoteOff(seqEngine.states[seqIdx].midiNote, 0);
        };
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
        float newBPM;
        if (queue_try_remove(&bpmQueue, &newBPM)) {
            seqEngine.updateBPM(newBPM);
        }
        int sequencerControl;
        if (queue_try_remove(&sequencerControlQueue, &sequencerControl)) {
            sequencerPlaying = sequencerControl == 1;
            seqEngine.setPlaying(sequencerPlaying);
            if (!sequencerPlaying) {
                midiIO->queueClockStop();
                midiIO->flushQueue();
            } else {
                midiIO->queueClockStart();
                midiIO->flushQueue();
            }
        }

        seqEngine.updateParams(params, 0);  // params 0–27 → 4 ratio seqs

        size_t pi = kNRatioSeqs * 7;        // = 28
        for (auto& s : fmStates) {
            s.carrierFreq = (0.25f + params[pi++] * 0.75f) * 0.125f;
            s.modFreq     = (0.25f + params[pi++] * 0.75f) * 0.25f;
            s.modIndex    = params[pi++] * 4.f;
            s.fbLevel     = params[pi++];
            static const float fmMuls[4] = {1.f, 2.f, 3.f, 4.f};
            s.phasorMul   = fmMuls[(int)(params[pi++] * 3.999f)];
            s.phaseOffset = ((int)(params[pi++] * timeSigBeats)) * timeSigBeatsInv;
            pi++;  // 7th param reserved
        }
    }

    void setTimeSignature(float beats, float division) {
        seqEngine.setTimeSignature(beats, division);
    }

protected:

    float i2cValues[8] = {0,0,0,0,0,0,0,0};

    float sampleRatef;
    float timeSigBeats = 4.f;
    float timeSigBeatsInv = 0.25f;
    bool firstParamsReceived = false;
};

#endif  // __BREAKOR_AUDIO_APP_HPP__
