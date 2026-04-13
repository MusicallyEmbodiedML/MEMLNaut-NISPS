#ifndef __RATIO_SEQ_ENGINE_HPP__
#define __RATIO_SEQ_ENGINE_HPP__

#include "RatioSeq.hpp"
#include <array>
#include <functional>
#include <cstddef>
#include <cmath>

template<size_t NSEQUENCES>
class RatioSeqEngine {
public:
    std::array<ratioSeqState, NSEQUENCES> states;

    std::function<void(size_t seqIdx, int velocity)> onNoteOn;
    std::function<void(size_t seqIdx)> onNoteOff;
    std::function<void()> onBarReset;

    void setup(float sample_rate) {
        sampleRatef = sample_rate;
        updateBPM(bpm);
    }

    __force_inline void updateBPM(float newBPM) {
        bpm = newBPM;
        float beatLengthInSeconds = 60.f / bpm;
        float barLengthInSeconds = beatLengthInSeconds * timeSigBeats;
        float barLengthInSamples = barLengthInSeconds * (sampleRatef / sequencingSampleDiv);
        barPhasorInc = 1.f / barLengthInSamples;
        float midiClockLengthInSeconds = beatLengthInSeconds / 24.f;
        float midiClockLengthInSamples = midiClockLengthInSeconds * sampleRatef;
        midiClockPhasorInc = 1.f / midiClockLengthInSamples;
    }

    void setTimeSignature(float beats, float division) {
        timeSigBeats = beats;
        timeSigBeatsInv = 1.f / beats;
        timeSigDivision = division;
        updateBPM(bpm);
    }

    void setPlaying(bool play) {
        playing = play;
        if (!playing) {
            if (onNoteOff) {
                for (size_t i = 0; i < NSEQUENCES; i++) {
                    onNoteOff(i);
                }
            }
            barPhasor = 0.f;
            midiClockPhasor = 0.f;
            sequencingSampleCounter = 0;
            for (auto &s : states) {
                s.phasor = 0.f;
                s.lastTrig = false;
            }
        }
    }

    __force_inline void resetBar() {
        barPhasor = 0.f;
        if (onBarReset) onBarReset();
    }

    // Returns true on a sequencing tick. Call once per audio sample from Process().
    __force_inline bool tick() {
        if (!playing) return false;

        bool ticked = false;
        if (sequencingSampleCounter == 0) {
            barPhasor += barPhasorInc;
            if (barPhasor >= 1.f) {
                barPhasor -= 1.f;
            }

            for (size_t i = 0; i < NSEQUENCES; i++) {
                auto &seq = states[i];
                float seqPhasor = barPhasor * seq.phasorMul;
                seqPhasor = fmodf(seqPhasor + seq.phaseOffset, 1.f);

                bool trig = ratioSeq<3>(seqPhasor, seq.phaseOffset, seq.ratioSum, seq.ratios, seq.pulseWidth);
                bool highAmp = ratioSeq<2>(seqPhasor, seq.phaseOffset, seq.ampRatioSum, seq.ampRatios, 0.5f);

                if (trig && !seq.lastTrig) {
                    if (onNoteOn) onNoteOn(i, highAmp ? 127 : 64);
                } else if (!trig && seq.lastTrig) {
                    if (onNoteOff) onNoteOff(i);
                }
                seq.lastTrig = trig;
            }
            ticked = true;
        }

        sequencingSampleCounter++;
        if (sequencingSampleCounter >= sequencingSampleDiv) {
            sequencingSampleCounter = 0;
        }

        return ticked;
    }

    // Call from loop() (not audio path). startIdx is the first param index to consume.
    template<size_t NPARAMS>
    void updateParams(const std::array<float, NPARAMS>& params, size_t startIdx) {
        size_t paramIdx = startIdx;
        for (auto &v : states) {
            float sum = 0.f;
            for (size_t i = 0; i < v.ratios.size(); i++) {
                v.ratios[i] = (float)(int)(params[paramIdx++] * 3.f) + 1.f;
                sum += v.ratios[i];
            }
            v.ratioSum = sum;

            static float muls[4] = {1.f, 2.f, 4.f, 8.f};
            v.phasorMul = muls[(int)(params[paramIdx++] * 3.999999f)];
            v.phaseOffset = ((int)(params[paramIdx++] * timeSigBeats)) * timeSigBeatsInv;

            sum = 0.f;
            for (size_t i = 0; i < v.ampRatios.size(); i++) {
                v.ampRatios[i] = (float)(int)(params[paramIdx++] * 3.f) + 1.f;
                sum += v.ampRatios[i];
            }
            v.ampRatioSum = sum;
        }
    }

    float midiClockPhasorInc = 0.f;
    float midiClockPhasor = 0.f;
    bool playing = false;

private:
    float sampleRatef = 48000.f;
    float bpm = 120.f;
    float timeSigBeats = 4.f;
    float timeSigBeatsInv = 0.25f;
    float timeSigDivision = 4.f;
    float barPhasor = 0.f;
    float barPhasorInc = 0.f;
    size_t sequencingSampleDiv = 400;
    size_t sequencingSampleCounter = 0;
};

#endif  // __RATIO_SEQ_ENGINE_HPP__
