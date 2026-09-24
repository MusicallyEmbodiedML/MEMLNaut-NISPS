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
#include "NISPSStateFlat.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>

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


// Save/restore round trip. The check that matters is that a restored engine
// maps identically to the one that was captured: if any piece of the network
// were missed -- memlp's own MLP::Serialise() drops the biases, which is why
// NISPSCore does not use it -- the outputs would drift apart here.
bool testStateRoundTrip() {
    NISPSCore core(3, 4);
    core.setLearningRateScale(0.7f);
    core.setRewardScale(1.3f);
    core.setNoiseLevel(0.f); // deterministic: no exploration noise in the comparison
    core.setOptimiseDivisor(2);
    // ADD keeps every experience, so the replay memory actually fills up and
    // the memory half of the round trip is worth something.
    core.setMemoryStoreMode(NISPSCore::MemoryStoreMode::ADD);

    std::mt19937 rng(7);
    std::uniform_real_distribution<float> spread(0.f, 1.f);

    // Train it into some non-trivial state, with both likes and dislikes, at
    // inputs spread across the space so the memory holds distinct items.
    for (int i = 0; i < 500; ++i) {
        core.setInputs({ spread(rng), spread(rng), spread(rng) });
        core.advanceClock(5.0);
        core.process();
        if (preference(core.getAction()) > 0.f) core.like();
        else core.dislike();
    }

    const size_t memBefore = core.memorySize();
    NISPSState saved;
    core.captureState(saved);

    // The snapshot must carry biases, not just weights: this is exactly what
    // memlp's MLP::Serialise() leaves behind.
    bool sawNonZeroBias = false;
    size_t biasCount = 0;
    for (const auto& layer : saved.layers) {
        biasCount += layer.biases.size();
        for (float b : layer.biases)
            if (b != 0.f) sawNonZeroBias = true;
    }
    std::printf("Biases captured:    %s (%zu biases, %s)\n",
                (biasCount > 0 && sawNonZeroBias) ? "PASS" : "FAIL", biasCount,
                sawNonZeroBias ? "not all zero" : "ALL ZERO");

    // Record what the trained mapping does at a set of fixed probe points.
    //
    // process() maps AND trains, and optimise() samples the replay memory at
    // random, so probing has to suppress training or the comparison measures
    // the RNG rather than the restored weights. A divisor this large means
    // optimise() never comes round during a probe pass, leaving pure
    // inference. Exploration noise is off for the same reason - the OU
    // generators' state is deliberately not part of a snapshot, being a
    // property of the moment rather than of what was learned.
    const std::vector<std::vector<float>> probes = {
        { 0.10f, 0.20f, 0.30f }, { 0.50f, 0.50f, 0.50f }, { 0.90f, 0.10f, 0.70f },
        { 0.00f, 0.00f, 0.00f }, { 1.00f, 1.00f, 1.00f },
    };
    auto probeMap = [&probes](NISPSCore& c) {
        c.setOptimiseDivisor(1000000);
        c.setNoiseLevel(0.f);
        std::vector<std::vector<float>> outs;
        for (const auto& p : probes) {
            c.setInputs(p);
            c.process();
            outs.push_back(c.getAction());
        }
        return outs;
    };
    const std::vector<std::vector<float>> expected = probeMap(core);

    // Scramble, then restore.
    core.randomiseNetwork();
    core.clearMemory();
    const std::vector<std::vector<float>> scrambled = probeMap(core);
    bool diverged = false;
    for (size_t i = 0; i < probes.size(); ++i)
        for (size_t j = 0; j < scrambled[i].size(); ++j)
            if (std::fabs(scrambled[i][j] - expected[i][j]) > 1e-6f) diverged = true;
    if (!diverged) {
        std::printf("Round trip:         INCONCLUSIVE (randomise did not change the mapping)\n");
    }

    if (!core.restoreState(saved)) {
        std::printf("Round trip:         FAIL (restoreState rejected its own snapshot)\n");
        return false;
    }

    const std::vector<std::vector<float>> restored = probeMap(core);
    bool identical = true;
    float worst = 0.f;
    for (size_t i = 0; i < probes.size(); ++i) {
        for (size_t j = 0; j < restored[i].size(); ++j) {
            worst = std::max(worst, std::fabs(restored[i][j] - expected[i][j]));
            if (std::fabs(restored[i][j] - expected[i][j]) > 1e-6f) identical = false;
        }
    }
    const bool memOk = (core.memorySize() == memBefore) && (memBefore > 8);

    // Restoring into an engine whose biases have been deliberately corrupted
    // must still reproduce the mapping - the proof that biases round trip.
    NISPSCore biasProbe(3, 4);
    biasProbe.randomiseNetwork();
    const bool biasRestored = biasProbe.restoreState(saved);
    bool biasIdentical = biasRestored;
    if (biasRestored) {
        const std::vector<std::vector<float>> got = probeMap(biasProbe);
        for (size_t i = 0; i < probes.size(); ++i)
            for (size_t j = 0; j < got[i].size(); ++j)
                if (std::fabs(got[i][j] - expected[i][j]) > 1e-6f) biasIdentical = false;
    }
    std::printf("Restore into other: %s\n", biasIdentical ? "PASS" : "FAIL");

    // A snapshot from a differently shaped engine must be refused outright.
    NISPSCore other(2, 4);
    const bool refusedShape = !other.restoreState(saved);

    // And a captured-then-restored state must itself survive a second trip.
    NISPSState again;
    core.captureState(again);
    const bool stable = (again.layers.size() == saved.layers.size()) &&
                        (again.memory.size() == saved.memory.size());

    std::printf("Round trip:         %s (max output diff %g)\n", identical ? "PASS" : "FAIL", worst);
    std::printf("Memory restored:    %s (%zu items)\n", memOk ? "PASS" : "FAIL", core.memorySize());
    std::printf("Shape mismatch:     %s (refused)\n", refusedShape ? "PASS" : "FAIL");
    std::printf("Recapture stable:   %s\n", stable ? "PASS" : "FAIL");
    return identical && memOk && refusedShape && stable && biasIdentical &&
           biasCount > 0 && sawNonZeroBias;
}


// The flat form is what a file actually stores, so the file-format half of
// persistence gets the same treatment: a full round trip, plus the corrupt
// files a hand-edited or truncated JSON would produce.
bool testFlatRoundTrip() {
    NISPSCore core(3, 4);
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> spread(0.f, 1.f);
    core.setMemoryStoreMode(NISPSCore::MemoryStoreMode::ADD);
    for (int i = 0; i < 120; ++i) {
        core.setInputs({ spread(rng), spread(rng), spread(rng) });
        core.advanceClock(5.0);
        core.process();
        if (i % 3) core.like();
        else core.dislike();
    }

    NISPSState st;
    core.captureState(st);

    nisps_flat::Flat flat;
    nisps_flat::flatten(st, flat);

    NISPSState back;
    std::string error;
    bool ok = nisps_flat::unflatten(flat, back, error);
    if (!ok) std::printf("  unflatten rejected a good state: %s\n", error.c_str());

    // Same numbers in, same numbers out.
    bool same = ok && back.n_inputs == st.n_inputs && back.n_outputs == st.n_outputs &&
                back.layers.size() == st.layers.size() && back.memory.size() == st.memory.size();
    for (size_t l = 0; ok && same && l < st.layers.size(); ++l) {
        same = same && back.layers[l].weights == st.layers[l].weights &&
               back.layers[l].biases == st.layers[l].biases;
    }
    for (size_t i = 0; ok && same && i < st.memory.size(); ++i) {
        same = same && back.memory[i].input == st.memory[i].input &&
               back.memory[i].action == st.memory[i].action &&
               back.memory[i].reward == st.memory[i].reward;
    }
    same = same && back.noiseLevel == st.noiseLevel && back.rewardScale == st.rewardScale &&
           back.optimiseDivisor == st.optimiseDivisor && back.memoryStoreMode == st.memoryStoreMode;

    // And a restored-from-flat state must still load into an engine.
    NISPSCore fresh(3, 4);
    const bool loads = same && fresh.restoreState(back);

    // Corruptions of the kind a truncated or hand-edited file produces.
    struct Corruption {
        const char* what;
        std::function<void(nisps_flat::Flat&)> apply;
    };
    const std::vector<Corruption> corruptions = {
        { "truncated weight array", [](nisps_flat::Flat& f) { f.weights.pop_back(); } },
        { "truncated bias array", [](nisps_flat::Flat& f) { f.biases.pop_back(); } },
        { "memory count too high", [](nisps_flat::Flat& f) { f.memoryCount += 1; } },
        { "memory count too low", [](nisps_flat::Flat& f) { f.memoryCount -= 1; } },
        { "odd layer shape", [](nisps_flat::Flat& f) { f.layerShape.pop_back(); } },
        { "zero in layer shape", [](nisps_flat::Flat& f) { f.layerShape[1] = 0; } },
        { "wrong version", [](nisps_flat::Flat& f) { f.version = 99; } },
        { "no inputs", [](nisps_flat::Flat& f) { f.inputs = 0; } },
        { "empty shape", [](nisps_flat::Flat& f) { f.layerShape.clear(); } },
    };
    bool allRejected = true;
    for (const auto& c : corruptions) {
        nisps_flat::Flat bad;
        nisps_flat::flatten(st, bad);
        c.apply(bad);
        NISPSState dummy;
        std::string why;
        if (nisps_flat::unflatten(bad, dummy, why)) {
            std::printf("  accepted a corrupt state: %s\n", c.what);
            allRejected = false;
        } else if (why.empty()) {
            std::printf("  rejected %s without saying why\n", c.what);
            allRejected = false;
        }
    }

    // A file with no live position is fine - it just starts where it is.
    nisps_flat::Flat noPos;
    nisps_flat::flatten(st, noPos);
    noPos.input.clear();
    noPos.action.clear();
    NISPSState posState;
    std::string posErr;
    const bool optionalOk = nisps_flat::unflatten(noPos, posState, posErr);

    std::printf("Flat round trip:    %s\n", same ? "PASS" : "FAIL");
    std::printf("Flat reloads:       %s\n", loads ? "PASS" : "FAIL");
    std::printf("Corrupt rejected:   %s (%zu cases)\n", allRejected ? "PASS" : "FAIL", corruptions.size());
    std::printf("Position optional:  %s\n", optionalOk ? "PASS" : "FAIL");
    return same && loads && allRejected && optionalOk;
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

    const bool roundTripOk = testStateRoundTrip();
    const bool flatOk = testFlatRoundTrip();

    std::printf("Range/finite check: %s\n", rangeOk ? "PASS" : "FAIL");
    std::printf("Learning check:     %s (final should exceed initial; stochastic, non-fatal if borderline)\n",
                (finalScore > initialScore) ? "PASS" : "INCONCLUSIVE");

    return (rangeOk && roundTripOk && flatOk) ? 0 : 1;
}
