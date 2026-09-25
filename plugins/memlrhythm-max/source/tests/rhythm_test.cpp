// Host test for RhythmCore, with no Max involved.
//
// Two jobs:
//   1. Check the ported generators against the firmware's own expressions,
//      which are reimplemented verbatim below as reference() functions --
//      so a change to RhythmCore that drifts from MODE_ELYSIAMORFS /
//      MODE_MEMLCELIUM behaviour shows up here.
//   2. Check the musical properties the externals rely on: k pulses per
//      cycle, offset rotating the pattern, ratio slices summing to one cycle.
#include "../common/RhythmCore.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

using namespace memlrhythm;

namespace {

int failures = 0;
int checks = 0;

void check(bool cond, const std::string& what) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("[FAIL] %s\n", what.c_str());
    }
}

// ---- verbatim firmware expressions, for comparison -----------------------

// modes/AudioApps/ElysiamorfAudioApp.hpp / MEMLCeliumAudioApp.hpp
bool referenceEuclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth) {
    const float fi = phase * n;
    int i = static_cast<int>(fi);
    const float rem = fi - i;
    if (i == static_cast<int>(n)) i--;
    const int idx = ((i + n - offset) * k) % n;
    return (idx < static_cast<int>(k) && rem < pulseWidth) ? 1 : 0;
}

// modes/AudioApps/RatioSeq.hpp
template <size_t seqLength>
bool referenceRatioSeq(float phasor, float phaseOffset, float ratioSum,
                       const std::array<float, seqLength>& ratios, float pulseWidth) {
    bool trig = 0;
    float offsetPhase = phaseOffset + phasor;
    if (offsetPhase >= 1.f) offsetPhase -= 1.f;
    float phaseAdj = ratioSum * offsetPhase;
    float accumulatedSum = 0;
    float lastAccumulatedSum = 0;
    for (size_t v : ratios) { // note: size_t, as in the firmware
        accumulatedSum += v;
        if (phaseAdj <= accumulatedSum) {
            float beatPhase = (phaseAdj - lastAccumulatedSum) / (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    return trig;
}

std::string patternString(long n, long k, long offset, float pw) {
    std::string s;
    for (long i = 0; i < n; ++i) {
        const float phase = (static_cast<float>(i) + 0.01f) / static_cast<float>(n);
        s += euclideanGate(phase, n, k, offset, pw) ? 'x' : '.';
    }
    return s;
}

long pulseCount(long n, long k, long offset) {
    long count = 0;
    for (long i = 0; i < n; ++i) {
        const float phase = (static_cast<float>(i) + 0.01f) / static_cast<float>(n);
        if (euclideanGate(phase, n, k, offset, 0.5f)) ++count;
    }
    return count;
}

void testEuclideanMatchesFirmware() {
    // Every combination the firmware can reach, at many phases.
    for (long n = 1; n <= 16; ++n) {
        for (long k = 1; k <= n; ++k) {
            for (long offset = 0; offset < n; ++offset) {
                for (int p = 0; p < 200; ++p) {
                    const float phase = static_cast<float>(p) / 200.f;
                    const bool got = euclideanGate(phase, n, k, offset, 0.5f);
                    const bool want = referenceEuclidean(phase, static_cast<size_t>(n), static_cast<size_t>(k),
                                                         static_cast<size_t>(offset), 0.5f);
                    if (got != want) {
                        check(false, "euclideanGate differs from firmware at n=" + std::to_string(n) +
                                         " k=" + std::to_string(k) + " offset=" + std::to_string(offset) +
                                         " phase=" + std::to_string(phase));
                        return;
                    }
                }
            }
        }
    }
    check(true, "euclideanGate matches firmware over all n,k,offset in 1..16");
}

void testEuclideanMusical() {
    check(patternString(8, 3, 0, 0.5f) == "x..x..x.", "E(3,8) is x..x..x. (got " + patternString(8, 3, 0, 0.5f) + ")");
    // The firmware's index test is Bresenham, not Bjorklund: it gives the same
    // multiset of gaps as the canonical Euclidean rhythm, but for some (n,k)
    // in a different rotation. E(5,16) is the textbook x..x..x..x..x... with
    // gaps 3,3,3,3,4; this yields the same rhythm starting on the long gap.
    check(patternString(16, 5, 0, 0.5f) == "x...x..x..x..x..",
          "E(5,16) is the canonical rhythm rotated (got " + patternString(16, 5, 0, 0.5f) + ")");
    check(patternString(8, 3, 1, 0.5f) == ".x..x..x", "offset 1 rotates E(3,8) (got " + patternString(8, 3, 1, 0.5f) + ")");
    check(patternString(4, 4, 0, 0.5f) == "xxxx", "k == n fires every step");

    bool allCounted = true;
    for (long n = 1; n <= 32; ++n)
        for (long k = 1; k <= n; ++k)
            for (long offset = 0; offset < n; ++offset)
                if (pulseCount(n, k, offset) != k) allCounted = false;
    check(allCounted, "every (n,k,offset) up to n=32 produces exactly k pulses per cycle");

    // Pulse width is a fraction of one step.
    check(euclideanGate(0.0f, 8, 8, 0, 0.5f) && !euclideanGate(0.5f / 8.f + 0.01f, 8, 8, 0, 0.5f),
          "pulseWidth 0.5 gates off halfway through a step");
    check(!euclideanGate(0.3f, 8, 3, 0, 0.f), "pulseWidth 0 never fires");
}

void testEuclideanGuards() {
    check(!euclideanGate(0.5f, 0, 3, 0, 0.5f), "n = 0 is silent, not a division by zero");
    check(!euclideanGate(0.5f, 8, 0, 0, 0.5f), "k = 0 is silent");
    check(euclideanGate(0.01f, 4, 9, 0, 0.5f), "k > n is clamped to n (every step fires)");
    check(patternString(8, 3, 9, 0.5f) == patternString(8, 3, 1, 0.5f), "offset wraps modulo n");
    check(euclideanGate(-0.99f, 8, 3, 0, 0.5f) == euclideanGate(0.01f, 8, 3, 0, 0.5f), "negative phase wraps");
    check(euclideanStep(0.99f, 8) == 7 && euclideanStep(0.0f, 8) == 0, "euclideanStep spans 0..n-1");
}

void testRatioMatchesFirmware() {
    const std::array<float, 3> refRatios = { 1.f, 2.f, 1.f };
    RatioPattern p;
    p.set(refRatios.data(), refRatios.size());
    check(p.sum == 4.f, "RatioPattern sums its ratios");

    bool same = true;
    for (int o = 0; o <= 8; ++o) {
        const float offset = static_cast<float>(o) / 8.f;
        for (int w = 0; w <= 4; ++w) {
            const float pw = static_cast<float>(w) / 4.f;
            for (int i = 0; i < 500; ++i) {
                const float phase = static_cast<float>(i) / 500.f;
                if (ratioSeqGate(phase, offset, p, pw) != referenceRatioSeq<3>(phase, offset, 4.f, refRatios, pw))
                    same = false;
            }
        }
    }
    check(same, "ratioSeqGate matches firmware for integer ratios over phase/offset/pulseWidth");

    // The firmware's own normalised mapping only ever yields integers 1..4,
    // which is why the truncation in its loop is invisible.
    bool allIntegers = true;
    for (int i = 0; i <= 100; ++i) {
        const float r = ratioFromNorm(static_cast<float>(i) / 100.f);
        if (r != std::floor(r) || r < 1.f || r > 4.f) allIntegers = false;
    }
    check(allIntegers, "ratioFromNorm yields integers 1..4");
}

void testRatioMusical() {
    const float ratios[3] = { 1.f, 2.f, 1.f };
    RatioPattern p;
    p.set(ratios, 3);

    // With pulseWidth 1 every phase is inside some slice, so it is always on.
    bool alwaysOn = true;
    for (int i = 0; i < 400; ++i)
        if (!ratioSeqGate(static_cast<float>(i) / 400.f, 0.f, p, 1.f)) alwaysOn = false;
    check(alwaysOn, "pulseWidth 1 gates the whole cycle");

    // Slice onsets land at the cumulative ratio boundaries: 0, 1/4, 3/4.
    std::vector<float> onsets;
    bool prev = false;
    for (int i = 0; i < 4000; ++i) {
        const float phase = static_cast<float>(i) / 4000.f;
        const bool on = ratioSeqGate(phase, 0.f, p, 0.25f);
        if (on && !prev) onsets.push_back(phase);
        prev = on;
    }
    check(onsets.size() == 3, "three ratios give three onsets per cycle (got " + std::to_string(onsets.size()) + ")");
    if (onsets.size() == 3) {
        check(std::fabs(onsets[0] - 0.0f) < 0.01f && std::fabs(onsets[1] - 0.25f) < 0.01f &&
                  std::fabs(onsets[2] - 0.75f) < 0.01f,
              "onsets land on the cumulative ratio boundaries 0, 1/4, 3/4");
    }

    // A longer slice holds its gate open longer at the same pulse width.
    auto sliceOnTime = [&](float from, float to) {
        int on = 0, total = 0;
        for (int i = 0; i < 4000; ++i) {
            const float phase = static_cast<float>(i) / 4000.f;
            if (phase < from || phase >= to) continue;
            ++total;
            if (ratioSeqGate(phase, 0.f, p, 0.5f)) ++on;
        }
        return total ? static_cast<float>(on) / static_cast<float>(total) : 0.f;
    };
    check(std::fabs(sliceOnTime(0.0f, 0.25f) - 0.5f) < 0.02f, "short slice is on for half its length");
    check(std::fabs(sliceOnTime(0.25f, 0.75f) - 0.5f) < 0.02f, "long slice is on for half its length");

    check(!ratioSeqGate(0.3f, 0.f, p, 0.f), "pulseWidth 0 fires only exactly at a boundary");
}

void testRatioVoiceStep() {
    const float ratios[3] = { 1.f, 2.f, 1.f };
    const float ampRatios[2] = { 1.f, 3.f };
    RatioPattern r, a;
    r.set(ratios, 3);
    a.set(ampRatios, 2);

    // phasorMul multiplies the number of cycles per bar.
    auto onsetCount = [&](float mul) {
        int onsets = 0;
        bool prev = false, trig = false, amp = false;
        for (int i = 0; i < 20000; ++i) {
            ratioVoiceStep(static_cast<float>(i) / 20000.f, mul, 0.f, 0.25f, r, a, trig, amp);
            if (trig && !prev) ++onsets;
            prev = trig;
        }
        return onsets;
    };
    check(onsetCount(1.f) == 3, "mul 1: three onsets per bar (got " + std::to_string(onsetCount(1.f)) + ")");
    check(onsetCount(2.f) == 6, "mul 2: six onsets per bar (got " + std::to_string(onsetCount(2.f)) + ")");
    check(onsetCount(4.f) == 12, "mul 4: twelve onsets per bar (got " + std::to_string(onsetCount(4.f)) + ")");

    // The amp gate must vary across the bar, since it picks velocity 127/64.
    bool sawHigh = false, sawLow = false, trig = false, amp = false;
    for (int i = 0; i < 2000; ++i) {
        ratioVoiceStep(static_cast<float>(i) / 2000.f, 1.f, 0.f, 0.5f, r, a, trig, amp);
        if (amp) sawHigh = true;
        else sawLow = true;
    }
    check(sawHigh && sawLow, "amp gate alternates, so both velocities are reachable");
    check(velocityFor(true) == 127 && velocityFor(false) == 64, "velocities are the firmware's 127 / 64");
}

void testNormalisedMappings() {
    check(mulFromNorm(0.f) == 1.f && mulFromNorm(0.3f) == 2.f && mulFromNorm(0.6f) == 4.f && mulFromNorm(1.f) == 8.f,
          "mulFromNorm walks 1, 2, 4, 8");
    check(offsetFromNorm(0.f, 4.f) == 0.f && offsetFromNorm(0.3f, 4.f) == 0.25f && offsetFromNorm(0.6f, 4.f) == 0.5f,
          "offsetFromNorm quantises to the beat grid");

    const EuclidParams a = euclidFromNorm(0.f, 0.f, 0.f, 1, 16, 1, 16);
    check(a.n == 1 && a.k == 1 && a.offset == 0, "euclidFromNorm at 0 gives the minimum");
    const EuclidParams b = euclidFromNorm(1.f, 1.f, 1.f, 1, 16, 1, 16);
    check(b.n == 16 && b.k == 16 && b.offset == 15, "euclidFromNorm at 1 gives the maximum");

    bool allValid = true;
    for (int i = 0; i <= 100; ++i) {
        const EuclidParams p = euclidFromNorm(static_cast<float>(i) / 100.f, 0.5f, 0.5f, 1, 16, 1, 16);
        bool isPow = false;
        for (long q = 1; q <= 16; q *= 2) if (p.n == q) isPow = true;
        for (long q = 1; q <= 16; q *= 3) if (p.n == q) isPow = true;
        if (!isPow || p.k > p.n || p.offset >= p.n) allValid = false;
    }
    check(allValid, "euclidFromNorm always yields n a power of 2 or 3, k <= n, offset < n");
}


void testMuting() {
    // Unmuted, the audible gate is just the gate.
    {
        GateState g;
        bool ok = true;
        const bool pattern[] = { 0, 0, 1, 1, 1, 0, 0, 1, 1, 0 };
        for (bool gate : pattern) {
            gateStep(g, gate, false);
            if (g.audible != gate) ok = false;
        }
        check(ok, "unmuted: the audible gate follows the gate exactly");
    }

    // Muting during a note releases it, once.
    {
        GateState g;
        gateStep(g, true, false);
        check(g.audible, "a gate that opens unmuted is audible");
        const bool changed = gateStep(g, true, true);
        check(changed && !g.audible, "muting during a note releases it (one change)");
        const bool again = gateStep(g, true, true);
        check(!again && !g.audible, "staying muted reports no further change");
    }

    // Muted throughout: no onset ever sounds, however many go by.
    {
        GateState g;
        int changes = 0;
        for (int i = 0; i < 40; ++i) {
            const bool gate = (i % 4) < 2; // ten onsets
            if (gateStep(g, gate, true)) ++changes;
            if (g.audible) changes += 100; // must never happen
        }
        check(changes == 0, "muted: onsets pass silently and nothing is emitted");
    }

    // Unmuting part way through a slice must not invent a note-on.
    {
        GateState g;
        gateStep(g, true, true);  // gate opens while muted
        gateStep(g, true, false); // unmuted, still inside that slice
        check(!g.audible, "unmuting mid-slice does not start a note");
        gateStep(g, true, false);
        check(!g.audible, "and stays silent for the rest of that slice");
        gateStep(g, false, false); // slice ends
        const bool changed = gateStep(g, true, false); // next onset
        check(changed && g.audible, "the next onset after unmuting sounds");
    }

    // Unmuting between slices: the very next onset sounds.
    {
        GateState g;
        gateStep(g, false, true);
        gateStep(g, false, false);
        const bool changed = gateStep(g, true, false);
        check(changed && g.audible, "unmuting between slices arms the next onset");
    }

    // A mute/unmute cycle leaves no note hanging: every note-on is matched.
    {
        GateState g;
        int balance = 0;
        bool muted = false;
        for (int i = 0; i < 400; ++i) {
            const bool gate = (i % 7) < 3;
            if (i % 23 == 0) muted = !muted; // flip at an unrelated period
            if (gateStep(g, gate, muted)) balance += g.audible ? 1 : -1;
        }
        if (g.audible) balance -= 1; // a note still sounding at the end is fine
        check(balance == 0, "every note-on is matched by a note-off across mute flips");
    }
}

} // namespace

int main() {
    testEuclideanMatchesFirmware();
    testEuclideanMusical();
    testEuclideanGuards();
    testRatioMatchesFirmware();
    testRatioMusical();
    testRatioVoiceStep();
    testNormalisedMappings();
    testMuting();

    std::printf("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
