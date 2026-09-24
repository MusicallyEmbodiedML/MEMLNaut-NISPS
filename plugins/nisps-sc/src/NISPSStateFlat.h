// Flat form of a NISPSState: the same data as nested arrays of plain numbers,
// which is what a file format can actually store.
//
// This sits between NISPSCore and whatever writes the file so that the fiddly
// half of persistence - splitting one flat weight array back into layers,
// and rejecting a file whose array lengths disagree with its own declared
// shape - is plain C++ that the host test can exercise, instead of living
// inside a Max dictionary callback where it cannot be run. The Max object
// (nisps-max) only moves these fields to and from dictionary keys.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "NISPSCore.h"

namespace nisps_flat {

struct Flat {
    int version = NISPSState::kVersion;
    long inputs = 0;
    long outputs = 0;

    // Two entries per layer: inputs per node, then node count.
    std::vector<long> layerShape;
    std::vector<float> weights; // every layer's weights, in layer order
    std::vector<float> biases;  // every layer's biases, in layer order

    long memoryCount = 0;
    std::vector<float> memInputs;  // memoryCount * inputs
    std::vector<float> memActions; // memoryCount * outputs
    std::vector<float> memRewards; // memoryCount
    std::vector<float> memAges;    // memoryCount

    std::vector<float> input;
    std::vector<float> action;

    float lrscale = 1.f;
    float rewardscale = 1.f;
    float noise = 0.f;
    long traindivisor = 1;
    long storemode = 2;
};

void flatten(const NISPSState& st, Flat& out);

// Rebuilds a state, or leaves `out` untouched and explains why not. Every
// length is checked against the shape the file declares for itself, so a
// truncated or hand-edited file is refused rather than half-loaded.
bool unflatten(const Flat& flat, NISPSState& out, std::string& error);

} // namespace nisps_flat
