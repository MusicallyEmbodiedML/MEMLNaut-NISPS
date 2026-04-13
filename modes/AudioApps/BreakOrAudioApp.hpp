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

    RatioSeqEngine<NSEQUENCES> seqEngine;

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
                // Capture trigger state for I2C after tick
                for (size_t i = 0; i < NSEQUENCES; i++) {
                    i2cValues[i] = seqEngine.states[i].lastTrig;
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

        const size_t midiNotes[NSEQUENCES] = {36,37,38,39,40, 42,43,45};
        for (size_t i = 0; i < NSEQUENCES; i++) {
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

        seqEngine.updateParams(params, 0);
    }

    void setTimeSignature(float beats, float division) {
        seqEngine.setTimeSignature(beats, division);
    }

protected:

    float i2cValues[8] = {0,0,0,0,0,0,0,0};

    float sampleRatef;
    bool firstParamsReceived = false;
};

#endif  // __BREAKOR_AUDIO_APP_HPP__
