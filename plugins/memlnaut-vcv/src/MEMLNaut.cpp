#include "MEMLNaut.hpp"

#include <cmath>

#include "NISPSCore.h"
#include "NISPSStateFlat.h"
#include "Panel.hpp"
#include "Screen.hpp"

namespace memlnaut {

namespace {

constexpr double kTickMs = 5.0;        // the firmware's 200 Hz control loop
constexpr int kMemRefreshTicks = 20;   // memory dots for the map: every 100 ms
constexpr float kFlashDecayPerMs = 1.f / 250.f;

const char* const kGestureNames[] = { "Like", "Dislike", "Jolt", "Scramble", "Drag (hold)", "Forget" };

// --- NISPSState <-> JSON, through the validated flat form -----------------------

json_t* floatArray(const std::vector<float>& v) {
    json_t* a = json_array();
    for (float f : v) json_array_append_new(a, json_real(f));
    return a;
}

json_t* longArray(const std::vector<long>& v) {
    json_t* a = json_array();
    for (long l : v) json_array_append_new(a, json_integer(l));
    return a;
}

void readFloats(json_t* obj, const char* key, std::vector<float>& out) {
    out.clear();
    json_t* a = json_object_get(obj, key);
    if (!json_is_array(a)) return;
    size_t i;
    json_t* v;
    json_array_foreach(a, i, v) out.push_back(static_cast<float>(json_number_value(v)));
}

void readLongs(json_t* obj, const char* key, std::vector<long>& out) {
    out.clear();
    json_t* a = json_object_get(obj, key);
    if (!json_is_array(a)) return;
    size_t i;
    json_t* v;
    json_array_foreach(a, i, v) out.push_back(static_cast<long>(json_integer_value(v)));
}

json_t* stateToJson(const NISPSState& st) {
    nisps_flat::Flat f;
    nisps_flat::flatten(st, f);
    json_t* o = json_object();
    json_object_set_new(o, "version", json_integer(f.version));
    json_object_set_new(o, "inputs", json_integer(f.inputs));
    json_object_set_new(o, "outputs", json_integer(f.outputs));
    json_object_set_new(o, "layerShape", longArray(f.layerShape));
    json_object_set_new(o, "weights", floatArray(f.weights));
    json_object_set_new(o, "biases", floatArray(f.biases));
    json_object_set_new(o, "memoryCount", json_integer(f.memoryCount));
    json_object_set_new(o, "memInputs", floatArray(f.memInputs));
    json_object_set_new(o, "memActions", floatArray(f.memActions));
    json_object_set_new(o, "memRewards", floatArray(f.memRewards));
    json_object_set_new(o, "memAges", floatArray(f.memAges));
    json_object_set_new(o, "input", floatArray(f.input));
    json_object_set_new(o, "action", floatArray(f.action));
    json_object_set_new(o, "storemode", json_integer(f.storemode));
    return o;
}

// Returns null (and logs why) for anything malformed or the wrong shape.
std::unique_ptr<NISPSState> stateFromJson(json_t* o) {
    if (!json_is_object(o)) return nullptr;
    nisps_flat::Flat f;
    f.version = static_cast<int>(json_integer_value(json_object_get(o, "version")));
    f.inputs = static_cast<long>(json_integer_value(json_object_get(o, "inputs")));
    f.outputs = static_cast<long>(json_integer_value(json_object_get(o, "outputs")));
    readLongs(o, "layerShape", f.layerShape);
    readFloats(o, "weights", f.weights);
    readFloats(o, "biases", f.biases);
    f.memoryCount = static_cast<long>(json_integer_value(json_object_get(o, "memoryCount")));
    readFloats(o, "memInputs", f.memInputs);
    readFloats(o, "memActions", f.memActions);
    readFloats(o, "memRewards", f.memRewards);
    readFloats(o, "memAges", f.memAges);
    readFloats(o, "input", f.input);
    readFloats(o, "action", f.action);
    f.storemode = static_cast<long>(json_integer_value(json_object_get(o, "storemode")));
    auto st = std::make_unique<NISPSState>();
    std::string why;
    if (!nisps_flat::unflatten(f, *st, why)) {
        WARN("MEMLNaut: ignoring a saved model: %s", why.c_str());
        return nullptr;
    }
    if (st->n_inputs != static_cast<size_t>(kNetInputs) || st->n_outputs != static_cast<size_t>(kNumOutputs)) {
        WARN("MEMLNaut: ignoring a saved model of the wrong size (%zu in, %zu out)", st->n_inputs, st->n_outputs);
        return nullptr;
    }
    return st;
}

} // namespace

MEMLNautModule::MEMLNautModule() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for (int g = 0; g < kNumGestures; ++g) {
        configButton(LIKE_PARAM + g, kGestureNames[g]);
        configInput(LIKE_INPUT + g, std::string(kGestureNames[g]) + (g == 4 ? " gate" : " trigger"));
        configLight(LIKE_LIGHT + g, kGestureNames[g]);
    }
    configParam(LEARN_PARAM, 0.f, 1.f, 1.f, "Learning rate", "%", 0.f, 100.f);
    configParam(REWARD_PARAM, 0.f, 1.f, 1.f, "Reward scale", "%", 0.f, 100.f);
    configParam(EXPLORE_PARAM, 0.f, 1.f, 0.f, "Explore (noise)", "%", 0.f, 100.f);
    configParam(Z_PARAM, 0.f, 1.f, 0.5f, "Z (used when Z is unpatched)");
    configParam(W_PARAM, 0.f, 1.f, 0.5f, "W (used when W is unpatched)");
    configSwitch(RANGE_PARAM, 0.f, 1.f, 0.f, "Output range", { "0..10 V", "-5..5 V" });
    configParam(PAD_X_PARAM, 0.f, 1.f, 0.5f, "Pad X");
    configParam(PAD_Y_PARAM, 0.f, 1.f, 0.5f, "Pad Y");
    paramQuantities[Z_PARAM]->randomizeEnabled = false;
    paramQuantities[W_PARAM]->randomizeEnabled = false;
    paramQuantities[PAD_X_PARAM]->randomizeEnabled = false;
    paramQuantities[PAD_Y_PARAM]->randomizeEnabled = false;
    paramQuantities[RANGE_PARAM]->randomizeEnabled = false;

    configInput(X_INPUT, "X");
    configInput(Y_INPUT, "Y");
    configInput(Z_INPUT, "Z");
    configInput(W_INPUT, "W");
    configInput(EXPLORE_INPUT, "Explore CV (adds to the knob, 10 V = full)");
    configInput(LEARN_INPUT, "Learning rate CV (adds to the knob, 10 V = full)");

    for (int i = 0; i < kNumOutputs; ++i) configOutput(NN_OUTPUT + i, string::f("Output %d", i + 1));
    configOutput(POLY_A_OUTPUT, "Outputs 1-16 (poly)");
    configOutput(POLY_B_OUTPUT, "Outputs 17-32 (poly)");

    lossLog_.fill(NAN);
    newEngine();
}

MEMLNautModule::~MEMLNautModule() = default;

void MEMLNautModule::newEngine() {
    core_ = std::make_unique<NISPSCore>(kNumInputs, kNumOutputs, kNetInputs);
    core_->setOptimiseDivisor(1);
    appliedDislikeMode_ = appliedStoreMode_ = appliedTrainOnDemand_ = -1;
    seenCoreSerial_ = core_->lastEvent().serial;
    seenLossPasses_ = 0;
    const std::vector<float>& a = core_->getAction();
    for (int i = 0; i < kNumOutputs; ++i) {
        out_[i] = outTarget_[i] = a[i];
        outStep_[i] = 0.f;
    }
}

float MEMLNautModule::readCV(int input, float fallback) {
    if (!inputs[input].isConnected()) return fallback;
    const float v = inputs[input].getVoltage();
    return clamp(inputRange == 1 ? (v + 5.f) / 10.f : v / 10.f, 0.f, 1.f);
}

void MEMLNautModule::postMessage(const char* action, const std::string& text) {
    msgAction_ = action;
    msgText_ = text;
    msgSerial_++;
}

void MEMLNautModule::process(const ProcessArgs& args) {
    // Gestures: catch every edge at audio rate (a 1 ms trigger must not be missed),
    // act on them at the next tick.
    for (int g = 0; g < kNumGestures; ++g) {
        if (g == DRAG_PARAM - LIKE_PARAM) continue; // drag is a level, read at the tick
        if (buttonTriggers_[g].process(params[LIKE_PARAM + g].getValue() > 0.f)) pending_[g] = true;
        if (gateTriggers_[g].process(inputs[LIKE_INPUT + g].getNormalVoltage(0.f), 0.1f, 1.f)) pending_[g] = true;
    }

    if (++sampleCounter_ >= tickSamples_) {
        sampleCounter_ = 0;
        tickSamples_ = std::max(1, static_cast<int>(std::lround(args.sampleRate * kTickMs / 1000.0)));
        tick(1000.0 * tickSamples_ / args.sampleRate);
    }

    // Outputs ramp from the previous tick's action to this one's over a tick, so a
    // 200 Hz control signal doesn't step.
    const bool bipolar = params[RANGE_PARAM].getValue() > 0.5f;
    for (int i = 0; i < kNumOutputs; ++i) {
        if (sampleCounter_ < tickSamples_) out_[i] += outStep_[i];
        const float u = clamp(out_[i], 0.f, 1.f);
        const float v = bipolar ? u * 10.f - 5.f : u * 10.f;
        outputs[NN_OUTPUT + i].setVoltage(v);
        outputs[i < 16 ? POLY_A_OUTPUT : POLY_B_OUTPUT].setVoltage(v, i & 15);
    }
    outputs[POLY_A_OUTPUT].setChannels(16);
    outputs[POLY_B_OUTPUT].setChannels(16);
}

void MEMLNautModule::applySettings() {
    if (dislikeMode != appliedDislikeMode_) {
        appliedDislikeMode_ = clamp(dislikeMode, 0, static_cast<int>(NISPSCore::DislikeMode::COUNT) - 1);
        dislikeMode = appliedDislikeMode_;
        core_->setDislikeMode(static_cast<NISPSCore::DislikeMode>(appliedDislikeMode_));
    }
    if (storeMode != appliedStoreMode_) {
        appliedStoreMode_ = clamp(storeMode, 0, 5);
        storeMode = appliedStoreMode_;
        core_->setMemoryStoreMode(static_cast<NISPSCore::MemoryStoreMode>(appliedStoreMode_));
    }
    if (static_cast<int>(trainOnDemand) != appliedTrainOnDemand_) {
        appliedTrainOnDemand_ = trainOnDemand ? 1 : 0;
        core_->setTrainOnDemand(trainOnDemand);
    }
}

void MEMLNautModule::handleSlotRequest() {
    const int req = slotRequest.exchange(-1);
    if (req < 0) return;
    const int op = req / kNumSlots, slot = req % kNumSlots;
    if (op == 0) {
        auto st = std::make_unique<NISPSState>();
        core_->captureState(*st);
        slots_[slot] = std::move(st);
        currentSlot_ = slot;
        postMessage("save", string::f("Saved to slot %d", slot + 1));
    } else if (slots_[slot]) {
        if (core_->restoreState(*slots_[slot])) {
            currentSlot_ = slot;
            postMessage("load", string::f("Loaded slot %d", slot + 1));
        } else {
            postMessage("load", "Model incompatible");
        }
    } else {
        postMessage("load", string::f("Slot %d is empty", slot + 1));
    }
}

void MEMLNautModule::tick(double elapsedMs) {
    applySettings();
    handleSlotRequest();

    // Inputs: X/Y from cables or the on-screen pad, Z/W from cables or their knobs.
    const std::vector<float> in = {
        readCV(X_INPUT, params[PAD_X_PARAM].getValue()),
        readCV(Y_INPUT, params[PAD_Y_PARAM].getValue()),
        readCV(Z_INPUT, params[Z_PARAM].getValue()),
        readCV(W_INPUT, params[W_PARAM].getValue()),
    };
    const std::vector<float>& prevIn = core_->getInput();
    bool moved = false;
    for (int i = 0; i < kNumInputs; ++i) moved |= prevIn[i] != in[i];
    if (moved) core_->setInputs(in);

    // Knobs, plus CV where there is one.
    core_->setLearningRateScale(clamp(params[LEARN_PARAM].getValue() + inputs[LEARN_INPUT].getNormalVoltage(0.f) / 10.f, 0.f, 1.f));
    core_->setRewardScale(params[REWARD_PARAM].getValue());
    core_->setNoiseLevel(clamp(params[EXPLORE_PARAM].getValue() + inputs[EXPLORE_INPUT].getNormalVoltage(0.f) / 10.f, 0.f, 1.f));

    // Gestures, in the firmware's loop order: jolt, like, dislike; then the rest.
    enum { LIKE, DISLIKE, JOLT, SCRAMBLE, DRAG, FORGET };
    if (pending_[JOLT]) core_->jolt();
    if (pending_[LIKE]) core_->like();
    if (pending_[DISLIKE]) core_->dislike();
    if (pending_[SCRAMBLE]) core_->randomiseNetwork();
    if (pending_[FORGET]) core_->clearMemory();
    const bool drag = params[DRAG_PARAM].getValue() > 0.f || inputs[DRAG_INPUT].getNormalVoltage(0.f) >= 1.f;
    if (drag && !dragHeld_) core_->startDrag();
    if (!drag && dragHeld_) core_->endDrag();
    dragHeld_ = drag;

    core_->advanceClock(elapsedMs);
    core_->process();

    // Lights: lit while held, and a short flash for each gesture.
    for (int g = 0; g < kNumGestures; ++g) {
        if (pending_[g]) flash_[g] = 1.f;
        flash_[g] = std::max(0.f, flash_[g] - static_cast<float>(elapsedMs) * kFlashDecayPerMs);
        pending_[g] = false;
        const bool held = params[LIKE_PARAM + g].getValue() > 0.f || inputs[LIKE_INPUT + g].getNormalVoltage(0.f) >= 1.f;
        lights[LIKE_LIGHT + g].setBrightness(std::max(held ? 1.f : 0.f, flash_[g]));
    }

    // Next ramp segment, from exactly where the last one was headed (no drift).
    const std::vector<float>& a = core_->getAction();
    for (int i = 0; i < kNumOutputs; ++i) {
        out_[i] = outTarget_[i];
        outTarget_[i] = a[i];
        outStep_[i] = (outTarget_[i] - out_[i]) / static_cast<float>(tickSamples_);
    }

    // Loss trace: one column per kLossColumnMs, the mean of the passes since.
    if (core_->likeLossPasses() != seenLossPasses_) {
        seenLossPasses_ = core_->likeLossPasses();
        lossSum_ += core_->lastLikeLoss();
        lossCount_++;
    }
    lossColumnMs_ += elapsedMs;
    if (lossColumnMs_ >= kLossColumnMs) {
        lossColumnMs_ = 0.0;
        if (lossCount_ > 0) {
            const float mean = static_cast<float>(lossSum_ / lossCount_);
            lossCursor_ = (lossCursor_ + 1) % kLossColumns;
            lossLog_[lossCursor_] = mean > 0.f ? std::log10(mean) : -INFINITY;
            lastLossMean_ = mean;
            lossSum_ = 0.0;
            lossCount_ = 0;
        }
    }

    if (core_->lastEvent().serial != seenCoreSerial_) {
        seenCoreSerial_ = core_->lastEvent().serial;
        postMessage(core_->lastEvent().action.c_str(), core_->lastEvent().message);
    }

    publish();
}

void MEMLNautModule::publish() {
    ScreenData& d = local_;
    const std::vector<float>& a = core_->getAction();
    const std::vector<float>& in = core_->getInput();
    for (int i = 0; i < kNumOutputs; ++i) d.action[i] = a[i];
    for (int i = 0; i < kNumInputs; ++i) d.input[i] = in[i];
    d.xyFromCV = inputs[X_INPUT].isConnected() || inputs[Y_INPUT].isConnected();
    d.likes = core_->likeCount();
    d.dislikes = core_->dislikeCount();
    d.lossLog = lossLog_;
    d.lossCursor = lossCursor_;
    d.lastLoss = lastLossMean_;
    d.trainingIdle = core_->isTrainingSettled();
    d.noiseActive = core_->noiseActive();
    d.dragging = core_->isDragging();
    d.action_ = msgAction_;
    d.message = msgText_;
    d.eventSerial = msgSerial_;
    d.slotMask = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (slots_[s]) d.slotMask |= 1u << s;
    d.currentSlot = currentSlot_;

    if (++memRefreshCounter_ >= kMemRefreshTicks) {
        memRefreshCounter_ = 0;
        std::vector<NISPSCore::MemoryPoint> pts;
        core_->memoryPoints(pts);
        d.memCount = 0;
        for (const auto& p : pts) {
            if (d.memCount >= kMaxMemPoints) break;
            d.mem[d.memCount++] = { p.input[0], p.input[1], p.input[2], p.input[3], static_cast<uint8_t>(p.kind) };
        }
    }

    if (snapMutex.try_lock()) {
        snap = d;
        snapMutex.unlock();
    }
}

void MEMLNautModule::onReset(const ResetEvent& e) {
    Module::onReset(e);
    dislikeMode = 0;
    storeMode = 2;
    trainOnDemand = false;
    inputRange = 0;
    for (auto& s : slots_) s.reset();
    currentSlot_ = -1;
    lossLog_.fill(NAN);
    lastLossMean_ = -1.f;
    newEngine();
    postMessage("forget", "Reset: a fresh network");
}

void MEMLNautModule::onRandomize(const RandomizeEvent& e) {
    Module::onRandomize(e);
    core_->randomiseNetwork();
}

json_t* MEMLNautModule::dataToJson() {
    json_t* root = json_object();
    NISPSState st;
    core_->captureState(st);
    json_object_set_new(root, "model", stateToJson(st));
    json_t* slots = json_array();
    for (const auto& s : slots_) json_array_append_new(slots, s ? stateToJson(*s) : json_null());
    json_object_set_new(root, "slots", slots);
    json_object_set_new(root, "currentSlot", json_integer(currentSlot_));
    json_object_set_new(root, "dislikeMode", json_integer(dislikeMode));
    json_object_set_new(root, "storeMode", json_integer(storeMode));
    json_object_set_new(root, "trainOnDemand", json_boolean(trainOnDemand));
    json_object_set_new(root, "inputRange", json_integer(inputRange));
    return root;
}

void MEMLNautModule::dataFromJson(json_t* root) {
    if (json_t* j = json_object_get(root, "dislikeMode")) dislikeMode = static_cast<int>(json_integer_value(j));
    if (json_t* j = json_object_get(root, "storeMode")) storeMode = static_cast<int>(json_integer_value(j));
    if (json_t* j = json_object_get(root, "trainOnDemand")) trainOnDemand = json_boolean_value(j);
    if (json_t* j = json_object_get(root, "inputRange")) inputRange = static_cast<int>(json_integer_value(j));
    if (json_t* j = json_object_get(root, "currentSlot")) currentSlot_ = static_cast<int>(json_integer_value(j));

    json_t* slots = json_object_get(root, "slots");
    for (int s = 0; s < kNumSlots; ++s) {
        json_t* j = json_is_array(slots) ? json_array_get(slots, static_cast<size_t>(s)) : nullptr;
        slots_[s] = j && !json_is_null(j) ? stateFromJson(j) : nullptr;
    }

    newEngine();
    if (auto st = stateFromJson(json_object_get(root, "model"))) {
        if (core_->restoreState(*st)) {
            const std::vector<float>& a = st->action;
            for (int i = 0; i < kNumOutputs && i < static_cast<int>(a.size()); ++i) out_[i] = outTarget_[i] = a[i];
        }
    }
}

// --- panel -----------------------------------------------------------------------

struct MEMLNautWidget : ModuleWidget {
    MEMLNautWidget(MEMLNautModule* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MEMLNaut.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto* labels = new PanelLabels;
        labels->box.size = box.size;
        addChild(labels);

        auto* screen = new Screen;
        screen->module = module;
        screen->box.pos = mm2px(Vec(layout::kScreenX, layout::kScreenY));
        screen->box.size = mm2px(Vec(layout::kScreenW, layout::kScreenH));
        addChild(screen);

        using M = MEMLNautModule;
        const int ids[] = { M::LIKE_PARAM, M::DISLIKE_PARAM, M::JOLT_PARAM, M::SCRAMBLE_PARAM, M::DRAG_PARAM, M::FORGET_PARAM };
        for (int g = 0; g < M::kNumGestures; ++g) {
            const Vec bp = mm2px(Vec(layout::gestureX(g), layout::kButtonY));
            switch (g) {
                case 0: addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(bp, module, ids[g], M::LIKE_LIGHT + g)); break;
                case 1: addParam(createLightParamCentered<VCVLightBezel<RedLight>>(bp, module, ids[g], M::LIKE_LIGHT + g)); break;
                case 5: addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(bp, module, ids[g], M::LIKE_LIGHT + g)); break;
                default: addParam(createLightParamCentered<VCVLightBezel<BlueLight>>(bp, module, ids[g], M::LIKE_LIGHT + g)); break;
            }
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(layout::gestureX(g), layout::kGateY)), module, M::LIKE_INPUT + g));
        }

        const int knobs[] = { M::LEARN_PARAM, M::REWARD_PARAM, M::EXPLORE_PARAM, M::Z_PARAM, M::W_PARAM };
        for (int k = 0; k < 5; ++k)
            addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(layout::knobX(k), layout::kKnobY)), module, knobs[k]));

        const int cvs[] = { M::X_INPUT, M::Y_INPUT, M::Z_INPUT, M::W_INPUT, M::EXPLORE_INPUT, M::LEARN_INPUT };
        for (int c = 0; c < 6; ++c)
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(layout::gestureX(c), layout::kCvY)), module, cvs[c]));

        for (int i = 0; i < kNumOutputs; ++i)
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(layout::outX(i % 4), layout::outY(i / 4))), module,
                                                       M::NN_OUTPUT + i));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(layout::kPolyAX, layout::kPolyY)), module, M::POLY_A_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(layout::kPolyBX, layout::kPolyY)), module, M::POLY_B_OUTPUT));
        addParam(createParamCentered<CKSS>(mm2px(Vec(layout::kRangeX, layout::kPolyY)), module, M::RANGE_PARAM));
    }

    void appendContextMenu(Menu* menu) override {
        auto* m = getModule<MEMLNautModule>();
        if (!m) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexPtrSubmenuItem("Dislike mode",
            { "Push away", "Nudge + blame", "Re-roll", "Nudge > back > re-roll", "Borrow from likes" }, &m->dislikeMode));
        menu->addChild(createIndexPtrSubmenuItem("Memory mode",
            { "Add", "Replace 5%", "Replace 10%", "Replace 15%", "Reward decay 10%", "Reward decay 20%" }, &m->storeMode));
        menu->addChild(createBoolPtrMenuItem("Train on demand", "", &m->trainOnDemand));
        menu->addChild(createIndexPtrSubmenuItem("CV input range", { "0..10 V", "-5..5 V" }, &m->inputRange));
    }
};

} // namespace memlnaut

Model* modelMEMLNaut = createModel<memlnaut::MEMLNautModule, memlnaut::MEMLNautWidget>("MEMLNaut");
