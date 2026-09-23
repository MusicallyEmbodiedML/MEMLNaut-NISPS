// RhythmCore - the MEMLNaut's two rhythm generators as plain, host-testable
// C++ with no Max, Arduino or Pico dependency. The Max externals in
// ../meml.euclid~ and ../meml.ratioseq~ are glue around this header, and
// ../tests/rhythm_test.cpp checks it against the firmware's behaviour.
//
// Sources, both phase-driven so they work off any phasor:
//   euclideanGate()  <- the euclidean() defined in modes/AudioApps/
//                       ElysiamorfAudioApp.hpp and MEMLCeliumAudioApp.hpp
//                       (identical copies; also in BreakOr/PAFSynth).
//   ratioSeqGate()   <- ratioSeq() in modes/AudioApps/RatioSeq.hpp, as driven
//                       by RatioSeqEngine::tick() (MODE_MEMLCELIUM).
//
// Where this deliberately differs from the firmware, it is marked PORT NOTE.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace memlrhythm {

inline float clamp01(float x) {
    return x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
}

// Wrap any phase (including negative, or several cycles out) into [0,1).
inline float wrapPhase(float phase) {
    if (!std::isfinite(phase)) return 0.f;
    phase -= std::floor(phase);
    return (phase < 0.f || phase >= 1.f) ? 0.f : phase;
}

// ---------------------------------------------------------------------------
// Euclidean
// ---------------------------------------------------------------------------
// n steps per cycle, k pulses spread evenly over them, rotated by offset
// steps, each pulse lasting pulseWidth of one step. The index test
// ((i + n - offset) * k) % n < k is the firmware's, kept verbatim: it is a
// Bresenham-style distribution, so E(3,8) comes out x..x..x. as expected.
//
// PORT NOTE: the firmware guards none of its inputs (n == 0 divides by zero,
// k > n makes every step a pulse, offset >= n wraps through unsigned
// arithmetic). Those are clamped here, which cannot change the result for any
// input the firmware itself produces.
inline bool euclideanGate(float phase, long n, long k, long offset, float pulseWidth) {
    if (n < 1 || k < 1) return false;
    if (k > n) k = n;
    offset = ((offset % n) + n) % n;

    const float fi = wrapPhase(phase) * static_cast<float>(n);
    long i = static_cast<long>(fi);
    const float rem = fi - static_cast<float>(i);
    if (i >= n) i = n - 1; // firmware: if (i == n) i--;

    const long idx = ((i + n - offset) * k) % n;
    return (idx < k) && (rem < pulseWidth);
}

// Which step index the given phase falls in (for the step outlet).
inline long euclideanStep(float phase, long n) {
    if (n < 1) return 0;
    long i = static_cast<long>(wrapPhase(phase) * static_cast<float>(n));
    if (i >= n) i = n - 1;
    return i;
}

// The firmware's normalised mapping for NN-driven control, from
// EuclideanAudioApp::VoiceOperator_ (src/memllib/examples/EuclideanAudioApp.hpp).
// Kept because it is the only 0..1 -> (n,k,offset) mapping in the codebase,
// and it is what a NISPS/IML output vector is shaped for.
inline long discreteMap(float x, long lo, long hi) {
    if (lo > hi) return lo;
    if (lo == hi) return lo;
    const float mapped = static_cast<float>(lo) + clamp01(x) * static_cast<float>(hi - lo);
    return static_cast<long>(std::round(mapped));
}

// Nearest power of 2 or 3 within [lo,hi] - the firmware constrains n to these
// so the cycle length stays musically meaningful.
inline long findValidN(long target, long lo, long hi) {
    long nearest = lo;
    long minDiff = std::abs(target - nearest);
    for (long p = 1; p <= hi; p *= 2) {
        if (p < lo) continue;
        const long d = std::abs(target - p);
        if (d < minDiff) { minDiff = d; nearest = p; }
    }
    for (long p = 1; p <= hi; p *= 3) {
        if (p < lo) continue;
        const long d = std::abs(target - p);
        if (d < minDiff) { minDiff = d; nearest = p; }
    }
    return nearest;
}

struct EuclidParams {
    long n = 8;
    long k = 3;
    long offset = 0;
};

// nNorm/kNorm/offsetNorm in 0..1, mapped exactly as VoiceOperator_ does.
inline EuclidParams euclidFromNorm(float nNorm, float kNorm, float offsetNorm,
                                   long nMin, long nMax, long kMin, long kMax) {
    EuclidParams p;
    p.n = findValidN(discreteMap(nNorm, nMin, nMax), nMin, nMax);
    const long localKMax = std::min(kMax, p.n);
    p.k = discreteMap(kNorm, kMin, localKMax);
    p.offset = (p.n > 0) ? discreteMap(offsetNorm, 0, p.n - 1) : 0;
    if (p.k > p.n) p.k = p.n;
    return p;
}

// ---------------------------------------------------------------------------
// Ratio sequencer
// ---------------------------------------------------------------------------
// One cycle is divided into count slices whose lengths are proportional to
// ratios[]; a slice fires while its own internal phase is below pulseWidth.
// So {1,2,1} over a bar gives a short-long-short pattern, and the ratios are
// what the NN modulates.
struct RatioPattern {
    static constexpr size_t kMaxRatios = 32;
    float ratios[kMaxRatios] = { 1.f };
    size_t count = 1;
    float sum = 1.f;

    void set(const float* values, size_t n) {
        count = std::min(n, kMaxRatios);
        sum = 0.f;
        for (size_t i = 0; i < count; ++i) {
            ratios[i] = values[i];
            sum += values[i];
        }
        if (count == 0) { count = 1; ratios[0] = 1.f; sum = 1.f; }
    }
};

// PORT NOTE: the firmware's loop is `for (size_t v : ratios)` over a float
// array, so each ratio is truncated to an integer inside the loop while
// ratioSum is computed from the untruncated floats. Every ratio the firmware
// generates is already an integer 1..4 (see ratioFromNorm), so this is
// invisible there; here the floats are used as written, which makes
// fractional ratios behave sensibly instead of inconsistently.
inline bool ratioSeqGate(float phasor, float phaseOffset, const RatioPattern& p, float pulseWidth) {
    if (p.count == 0 || !(p.sum > 0.f)) return false;

    float offsetPhase = phaseOffset + phasor;
    if (offsetPhase >= 1.f) offsetPhase -= 1.f;

    const float phaseAdj = p.sum * offsetPhase;
    float accumulated = 0.f;
    float lastAccumulated = 0.f;
    for (size_t i = 0; i < p.count; ++i) {
        accumulated += p.ratios[i];
        if (phaseAdj <= accumulated) {
            const float span = accumulated - lastAccumulated;
            const float beatPhase = (span > 0.f) ? (phaseAdj - lastAccumulated) / span : 0.f;
            return beatPhase <= pulseWidth;
        }
        lastAccumulated = accumulated;
    }
    return false;
}

// One sequence's step, mirroring RatioSeqEngine::tick()'s per-sequence body.
//
// PORT NOTE: phaseOffset is applied twice in the firmware - once here when
// building seqPhasor, and again inside ratioSeq() - so an offset shifts the
// pattern by twice its value. That is kept, because the offsets the firmware
// produces are bar-grid fractions and the doubling is part of how the
// published patterns sound.
inline void ratioVoiceStep(float barPhase, float phasorMul, float phaseOffset, float pulseWidth,
                           const RatioPattern& ratios, const RatioPattern& ampRatios,
                           bool& trigOut, bool& ampOut) {
    float seqPhasor = wrapPhase(barPhase) * phasorMul;
    seqPhasor = std::fmod(seqPhasor + phaseOffset, 1.f);
    if (seqPhasor < 0.f) seqPhasor += 1.f;
    trigOut = ratioSeqGate(seqPhasor, phaseOffset, ratios, pulseWidth);
    ampOut = ratioSeqGate(seqPhasor, phaseOffset, ampRatios, 0.5f);
}

// The firmware's normalised mappings, from RatioSeqEngine::updateParams().
inline float ratioFromNorm(float x) {
    return static_cast<float>(static_cast<int>(clamp01(x) * 3.f)) + 1.f; // 1..4
}

inline float mulFromNorm(float x) {
    static const float muls[4] = { 1.f, 2.f, 4.f, 8.f };
    return muls[static_cast<int>(clamp01(x) * 3.999999f)];
}

inline float offsetFromNorm(float x, float beats) {
    if (!(beats > 0.f)) return 0.f;
    return static_cast<float>(static_cast<int>(clamp01(x) * beats)) / beats;
}

// Velocity the firmware sends on a note-on: RatioSeqEngine calls
// onNoteOn(i, highAmp ? 127 : 64).
inline int velocityFor(bool highAmp) { return highAmp ? 127 : 64; }

} // namespace memlrhythm
