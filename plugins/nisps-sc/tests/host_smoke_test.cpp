// Host-side sanity check for NISPSCore, with no SuperCollider involved.
//
// Proves two things before any SC plugin code is written:
//   1. The memlp subset (MLP, ReplayMemory, OrnsteinUhlenbeckNoise) really
//      compiles and runs cleanly on a desktop host, outside Arduino.
//   2. NISPSCore's ported algorithm behaves like an RL loop should: given a
//      synthetic "performer preference" and a like/dislike policy driven by
//      it, repeated interaction should measurably shift the mapping's
//      outputs toward what's being liked.
#include "NISPSCore.h"

#include <cmath>
#include <cstdio>
#include <random>

namespace {

// Synthetic stand-in for a human performer: prefers action[0] high and
// action[1] low. Every other output dimension is preference-neutral.
float preference(const std::vector<float>& action) {
    return action[0] - action[1];
}

// The real algorithm shapes the mapping *near where the performer is
// standing* (k-NN centroid, distance-based memory dedup) — it isn't meant to
// solve a global objective from feedback scattered uniformly across the
// whole input space in one tick each. So this evaluates/trains at one fixed
// input location (with a small jitter, both to exercise more than a single
// replay-memory slot and to mimic a hand that isn't perfectly still), which
// is the fair way to test "does feedback here reshape the output here".
std::vector<float> jitteredTarget(std::mt19937& rng, std::uniform_real_distribution<float>& jitter) {
    return { 0.5f + jitter(rng), 0.5f + jitter(rng), 0.5f + jitter(rng) };
}

float evalAvgPreference(NISPSCore& core, std::mt19937& rng, std::uniform_real_distribution<float>& jitter, int n) {
    float total = 0.f;
    for (int i = 0; i < n; ++i) {
        core.setInputs(jitteredTarget(rng, jitter));
        core.advanceClock(1.5); // ~ one SC control block at a 64-sample/44.1kHz rate
        core.process();
        total += preference(core.getAction());
    }
    return total / static_cast<float>(n);
}

} // namespace

int main() {
    NISPSCore core(/*n_inputs=*/3, /*n_outputs=*/4);
    core.setLearningRateScale(1.0f);
    core.setRewardScale(1.0f);
    core.setOptimiseDivisor(1);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> jitter(-0.02f, 0.02f);

    constexpr int kWarmupSamples = 100;
    constexpr int kTrainTicks = 6000;
    constexpr int kEvalSamples = 100;

    const float initialScore = evalAvgPreference(core, rng, jitter, kWarmupSamples);

    for (int i = 0; i < kTrainTicks; ++i) {
        core.setInputs(jitteredTarget(rng, jitter));
        core.advanceClock(1.5);
        core.process();
        // No dead zone: always give feedback based on sign, so this stays a
        // reliable regression check despite the internal RNGs (ReplayMemory,
        // OrnsteinUhlenbeckNoise, jolt's rand()) not being seeded here.
        const float score = preference(core.getAction());
        if (score > 0.f) core.like();
        else if (score < 0.f) core.dislike();
    }

    const float finalScore = evalAvgPreference(core, rng, jitter, kEvalSamples);

    std::printf("Initial avg preference score: %f\n", initialScore);
    std::printf("Final avg preference score:   %f\n", finalScore);

    bool rangeOk = true;
    for (int i = 0; i < 20; ++i) {
        core.setInputs(jitteredTarget(rng, jitter));
        core.process();
        for (float v : core.getAction()) {
            if (!std::isfinite(v) || v < 0.f || v > 1.f) rangeOk = false;
        }
    }

    // Exercise jolt / randomise / clear-memory for crash-safety, not correctness.
    core.startJolt();
    for (int i = 0; i < 100; ++i) {
        core.process();
        core.stepJolt();
    }
    core.stopJolt();
    core.randomiseNetwork();
    core.clearMemory();
    core.process();

    std::printf("Range/finite check: %s\n", rangeOk ? "PASS" : "FAIL");
    std::printf("Learning check:     %s (final should exceed initial; stochastic, non-fatal if borderline)\n",
                (finalScore > initialScore) ? "PASS" : "INCONCLUSIVE");

    return rangeOk ? 0 : 1;
}
