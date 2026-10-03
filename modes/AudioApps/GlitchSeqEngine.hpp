#ifndef __GLITCH_SEQ_ENGINE_HPP__
#define __GLITCH_SEQ_ENGINE_HPP__

#include <array>
#include <functional>
#include <cstddef>
#include <cmath>

// RatioSeqEngine variant for MEMLGlitchAmb. Each sequence splits its cycle into NNOTES
// segments by integer ratios (one note per segment, as in RatioSeqEngine), but instead
// of an accent pattern every note has its own level, chosen by one parameter: the bottom
// third of the range is a rest, the middle third low (kLowLevel), the top third full.
// Rests never trigger, so patterns can thin out to nothing.
template<size_t NSEQUENCES, size_t NNOTES = 3>
class GlitchSeqEngine {
public:
    static constexpr float kLowLevel = 0.1f;
    // Parameters consumed per sequence: NNOTES ratios, speed, offset, NNOTES levels.
    static constexpr size_t kParamsPerSeq = 2 * NNOTES + 2;

    struct SeqState {
        std::array<float, NNOTES> ratios{};
        std::array<float, NNOTES> levels{};  // 0 = rest
        float ratioSum = static_cast<float>(NNOTES);
        float phasorMul = 1.f;
        float phaseOffset = 0.f;
        float pulseWidth = 0.5f;
        bool lastTrig = false;
    };
    std::array<SeqState, NSEQUENCES> states;

    // level: 0..1 amplitude of the note (never called for a rest).
    std::function<void(size_t seqIdx, float level)> onNoteOn;
    std::function<void(size_t seqIdx)> onNoteOff;

    GlitchSeqEngine() {
        for (auto& s : states) { s.ratios.fill(1.f); s.levels.fill(1.f); }
    }

    void setup(float sample_rate) {
        sampleRatef = sample_rate;
        updateBPM(bpm);
    }

    __force_inline void updateBPM(float newBPM) {
        bpm = newBPM;
        const float barLengthInSeconds = 60.f / bpm * timeSigBeats;
        const float barLengthInSamples = barLengthInSeconds * (sampleRatef / sequencingSampleDiv);
        barPhasorInc = 1.f / barLengthInSamples;
    }

    void setPlaying(bool play) {
        playing = play;
        if (!playing) {
            if (onNoteOff) for (size_t i = 0; i < NSEQUENCES; i++) onNoteOff(i);
            barPhasor = 0.f;
            barCount = 0;
            sequencingSampleCounter = 0;
            for (auto& s : states) s.lastTrig = false;
        }
    }

    // Call once per audio sample from Process(). Returns true on a sequencing tick.
    __force_inline bool tick() {
        if (!playing) return false;
        bool ticked = false;
        if (sequencingSampleCounter == 0) {
            barPhasor += barPhasorInc;
            if (barPhasor >= 1.f) {
                barPhasor -= 1.f;
                barCount++;
            }

            for (size_t i = 0; i < NSEQUENCES; i++) {
                auto& seq = states[i];
                const float seqPhasor = fmodf(barPhasor * seq.phasorMul + seq.phaseOffset, 1.f);
                float level = 0.f;
                const int note = activeNote(seq, seqPhasor);
                if (note >= 0) level = seq.levels[note];
                const bool trig = level > 0.f;

                if (trig && !seq.lastTrig) {
                    if (onNoteOn) onNoteOn(i, level);
                } else if (!trig && seq.lastTrig) {
                    if (onNoteOff) onNoteOff(i);
                }
                seq.lastTrig = trig;
            }
            ticked = true;
        }
        if (++sequencingSampleCounter >= sequencingSampleDiv) sequencingSampleCounter = 0;
        return ticked;
    }

    // Call from loop() (not the audio path). Consumes kParamsPerSeq params per sequence
    // from startIdx: NNOTES ratios (1..3), speed (x1/2/4/8 per bar), offset (whole
    // beats), NNOTES levels (rest / low / full).
    template<size_t NPARAMS>
    void updateParams(const std::array<float, NPARAMS>& params, size_t startIdx) {
        size_t p = startIdx;
        for (auto& v : states) {
            float sum = 0.f;
            for (size_t i = 0; i < NNOTES; i++) {
                v.ratios[i] = static_cast<float>(static_cast<int>(params[p++] * 3.f)) + 1.f;
                sum += v.ratios[i];
            }
            v.ratioSum = sum;

            static constexpr float muls[4] = {1.f, 2.f, 4.f, 8.f};
            v.phasorMul = muls[static_cast<int>(params[p++] * 3.999999f)];
            v.phaseOffset = static_cast<int>(params[p++] * timeSigBeats) / timeSigBeats;

            for (size_t i = 0; i < NNOTES; i++) {
                const int band = static_cast<int>(params[p++] * 2.999999f);  // 0, 1, 2
                v.levels[i] = band == 0 ? 0.f : (band == 1 ? kLowLevel : 1.f);
            }
        }
    }

    bool playing = false;
    float getBarPhasor() const { return barPhasor; }
    // Whole bars since playback started (0 on start). With getBarPhasor() this is the
    // shared musical clock, for anything that must stay in sync with the sequences.
    uint32_t getBarCount() const { return barCount; }

private:
    // Index of the note whose segment contains phase, if within its pulse width; else -1.
    static __force_inline int activeNote(const SeqState& seq, float phase) {
        const float pos = phase * seq.ratioSum;
        float start = 0.f;
        for (size_t n = 0; n < NNOTES; n++) {
            const float end = start + seq.ratios[n];
            if (pos <= end) {
                const float beatPhase = (pos - start) / seq.ratios[n];
                return beatPhase <= seq.pulseWidth ? static_cast<int>(n) : -1;
            }
            start = end;
        }
        return -1;
    }

    float sampleRatef = 48000.f;
    float bpm = 120.f;
    float timeSigBeats = 4.f;
    float barPhasor = 0.f;
    uint32_t barCount = 0;
    float barPhasorInc = 0.f;
    size_t sequencingSampleDiv = 48;  // 1ms at 48kHz: fine enough for the arp's 1/64T steps
    size_t sequencingSampleCounter = 0;
};

#endif  // __GLITCH_SEQ_ENGINE_HPP__
