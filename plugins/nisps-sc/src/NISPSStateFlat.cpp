#include "NISPSStateFlat.h"

#include <algorithm>

namespace nisps_flat {

void flatten(const NISPSState& st, Flat& out) {
    out = Flat();
    out.version = st.version;
    out.inputs = static_cast<long>(st.n_inputs);
    out.outputs = static_cast<long>(st.n_outputs);

    for (const auto& layer : st.layers) {
        out.layerShape.push_back(static_cast<long>(layer.inputs));
        out.layerShape.push_back(static_cast<long>(layer.nodes));
        out.weights.insert(out.weights.end(), layer.weights.begin(), layer.weights.end());
        out.biases.insert(out.biases.end(), layer.biases.begin(), layer.biases.end());
    }

    out.memoryCount = static_cast<long>(st.memory.size());
    for (const auto& item : st.memory) {
        out.memInputs.insert(out.memInputs.end(), item.input.begin(), item.input.end());
        out.memActions.insert(out.memActions.end(), item.action.begin(), item.action.end());
        out.memRewards.push_back(item.reward);
        out.memAges.push_back(static_cast<float>(item.ageMs));
    }

    out.input = st.input;
    out.action = st.action;
    out.lrscale = st.learningRateScale;
    out.rewardscale = st.rewardScale;
    out.noise = st.noiseLevel;
    out.traindivisor = static_cast<long>(st.optimiseDivisor);
    out.storemode = st.memoryStoreMode;
}

bool unflatten(const Flat& flat, NISPSState& out, std::string& error) {
    error.clear();

    if (flat.version != NISPSState::kVersion) {
        error = "state version " + std::to_string(flat.version) + ", expected " +
                std::to_string(NISPSState::kVersion);
        return false;
    }
    if (flat.inputs < 1 || flat.outputs < 1) {
        error = "state declares " + std::to_string(flat.inputs) + " inputs and " +
                std::to_string(flat.outputs) + " outputs";
        return false;
    }
    if (flat.layerShape.empty() || (flat.layerShape.size() % 2) != 0) {
        error = "layer shape is missing or has an odd number of entries";
        return false;
    }

    size_t weightsNeeded = 0;
    size_t biasesNeeded = 0;
    for (size_t i = 0; i < flat.layerShape.size(); i += 2) {
        const long inputs = flat.layerShape[i];
        const long nodes = flat.layerShape[i + 1];
        if (inputs < 1 || nodes < 1) {
            error = "layer " + std::to_string(i / 2) + " has shape " + std::to_string(inputs) + "x" +
                    std::to_string(nodes);
            return false;
        }
        weightsNeeded += static_cast<size_t>(inputs) * static_cast<size_t>(nodes);
        biasesNeeded += static_cast<size_t>(nodes);
    }
    if (flat.weights.size() != weightsNeeded || flat.biases.size() != biasesNeeded) {
        error = "have " + std::to_string(flat.weights.size()) + " weights and " +
                std::to_string(flat.biases.size()) + " biases, layer shape needs " +
                std::to_string(weightsNeeded) + " and " + std::to_string(biasesNeeded);
        return false;
    }

    const size_t nIn = static_cast<size_t>(flat.inputs);
    const size_t nOut = static_cast<size_t>(flat.outputs);
    if (flat.memoryCount < 0 ||
        static_cast<size_t>(flat.memoryCount) * nIn != flat.memInputs.size() ||
        static_cast<size_t>(flat.memoryCount) * nOut != flat.memActions.size() ||
        static_cast<size_t>(flat.memoryCount) != flat.memRewards.size() ||
        static_cast<size_t>(flat.memoryCount) != flat.memAges.size()) {
        error = "replay memory arrays do not match a memory count of " + std::to_string(flat.memoryCount);
        return false;
    }

    // Everything checks out; only now is anything written.
    out = NISPSState();
    out.version = flat.version;
    out.n_inputs = nIn;
    out.n_outputs = nOut;

    size_t w = 0, b = 0;
    for (size_t i = 0; i < flat.layerShape.size(); i += 2) {
        NISPSState::Layer layer;
        layer.inputs = static_cast<size_t>(flat.layerShape[i]);
        layer.nodes = static_cast<size_t>(flat.layerShape[i + 1]);
        const size_t n = layer.inputs * layer.nodes;
        layer.weights.assign(flat.weights.begin() + w, flat.weights.begin() + w + n);
        layer.biases.assign(flat.biases.begin() + b, flat.biases.begin() + b + layer.nodes);
        w += n;
        b += layer.nodes;
        out.layers.push_back(std::move(layer));
    }

    out.memory.reserve(static_cast<size_t>(flat.memoryCount));
    for (size_t i = 0; i < static_cast<size_t>(flat.memoryCount); ++i) {
        NISPSState::MemoryItem item;
        item.input.assign(flat.memInputs.begin() + i * nIn, flat.memInputs.begin() + (i + 1) * nIn);
        item.action.assign(flat.memActions.begin() + i * nOut, flat.memActions.begin() + (i + 1) * nOut);
        item.reward = flat.memRewards[i];
        item.ageMs = flat.memAges[i];
        out.memory.push_back(std::move(item));
    }

    // The live position is optional: a file without it simply starts wherever
    // the engine already is.
    if (flat.input.size() == nIn) out.input = flat.input;
    if (flat.action.size() == nOut) out.action = flat.action;

    out.learningRateScale = flat.lrscale;
    out.rewardScale = flat.rewardscale;
    out.noiseLevel = flat.noise;
    out.optimiseDivisor = static_cast<size_t>(std::max<long>(1, flat.traindivisor));
    out.memoryStoreMode = static_cast<int>(std::clamp<long>(flat.storemode, 0, 5));
    return true;
}

} // namespace nisps_flat
