#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/CCSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/display/RotarySelectView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../TRxSAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../MachineListeningMixin.hpp"
#include "MEMLNautMode.hpp"
#include "paramsite/ParamsitePresets.hpp"
#include <memory>
#include <array>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <span>

// Paramsite: MIDI CC control of any device. The standard NISPS input sources drive 32
// network outputs, sent as MIDI CCs. The MIDI Mode page chooses one of four User
// setups or a synth preset (paramsite/ParamsitePresets.hpp); the choice is saved and
// the MEMLNaut restarts into it, because each mode builds its own screens at boot.
// Per mode: CC Assign page (up to 32 CCs, saved to flash and inside each model),
// MIDI Learn page (sweeps one CC alone, for MIDI learn in a DAW or plugin), MIDI
// Channel page, Focus groups, and SD model folder. Built with USB MIDI
// (scripts/build.sh -m), everything sent on DIN MIDI also goes to USB.

// RotarySelectView that also reports taking control and an encoder press, so a
// press (confirm) can be told apart from leaving the page. Used by MIDI Mode and
// MIDI Learn.
class ParamsiteSelectView : public RotarySelectView {
public:
    using RotarySelectView::RotarySelectView;
    void setOnConfirm(std::function<void()> cb) { onConfirm_ = cb; }
    void setOnFocus(std::function<void()> cb) { onFocus_ = cb; }
    bool setFocus() override {
        bool focused = RotarySelectView::setFocus();
        if (focused && onFocus_) onFocus_();
        return focused;
    }
    void HandleRotaryEncSwitch() override {
        if (onConfirm_) onConfirm_();
        RotarySelectView::HandleRotaryEncSwitch();
    }
private:
    std::function<void()> onConfirm_;
    std::function<void()> onFocus_;
};

class MEMLNautModeParamsite {
public:
    static constexpr size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    static constexpr size_t kN_Params      = 32;
    static constexpr size_t kMaxGroups     = 12;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<kN_Params> audioApp;
    std::array<String, TRxSAudioApp<kN_Params>::nVoiceSpaces> voiceSpaceList;

    using InterfaceRL_t = InterfaceRL<kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;
    MachineListeningMixin mlMixin;

    FocusManager<kN_Params, kMaxGroups> focusManager;

    void setupInterface() {
        presetIdx_ = loadModeChoice();
        const ParamsitePreset& P = preset();

        interface.setup(kN_InputParams, kN_Params, false);  // no Messages screen
        // Knob Y fades from the home values (0) to the network (full); it never
        // scales the reward here. Must be set before bindInterface(), which reads
        // the override.
        interface.setRewardScale(1.0f);
        interface.setRVY1Override([this](float v) { fadeAmount_ = v; interface.markInputDirty(); });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo(P.root, P.tag);
        interfacePtr = make_non_owning(interface);

        for (size_t i = 0; i < P.groups.size() && i < kMaxGroups; i++)
            focusManager.setGroupName(i, P.groups[i]);

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        // Home + fade applies only on the way out to MIDI, not to training or the
        // outputs screen. Only the first nCCs_ outputs have a CC.
        interface.paramOutputHook = [this](std::span<const float> nn) {
            const float f = fadeAmount_;
            const size_t n = std::min(nn.size(), nCCs_);
            for (size_t i = 0; i < n; i++) {
                float v = homeValues_[i] + f * (nn[i] - homeValues_[i]);
                liveValues_[i] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
            }
            nLive_ = n;
            ccPending_ = true;
            sendCCsIfDue();
        };
    }

    String getHelpTitle() { return String("Paramsite: ") + preset().name; }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioApp.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioApp.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioApp.getVoiceSpaceNames();
        mlMixin.setup(interface);
    }

    __force_inline void loop() { audioApp.loop(); }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        const ParamsitePreset& P = preset();
        midi_interf = new_midi_interf;
        midi_interf->Setup(kN_Params);
        channel_ = loadChannel();
        midi_interf->SetMIDISendChannel(channel_);
        midi_interf->SetMIDINoteChannel(channel_);
        if (P.notesThru) {
            midi_interf->SetNoteCallback([this](bool note_on, uint8_t note, uint8_t velocity) {
                if (note_on) midi_interf->sendNoteOn(note, velocity);
                else         midi_interf->sendNoteOff(note, velocity);
            });
        }
        interface.bindMIDI(midi_interf);
    }

    void addViews() {
        addFocusView();
        interface.addInputSourceView(false);
        addCCView();
        addLearnView();
        addChannelView();
        addModeView();
    }

    __force_inline void analyse(stereosample_t x) { mlMixin.analyse(x); }
    __force_inline void processAnalysisParams() { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioApp.GetDriverConfig(); }

    void loopCore0() {
        if (learnActive_) learnSweep();
        if (learnEnded_) {                  // put every CC back to the network's values
            learnEnded_ = false;
            if (midi_interf) midi_interf->ResendAllCCs();
            ccPending_ = true;
            interface.markInputDirty();
        }
        sendCCsIfDue();
        if (ccView_) ccView_->refreshLiveColumn();
        if (channelSavePending_) {
            channelSavePending_ = false;
            if (channel_ != savedChannel_) saveChannel(channel_);
        }
        if (restartPending_) {
            restartPending_ = false;
            saveModeChoice(pendingPresetIdx_);
            Serial.printf("Paramsite: restarting into %s\n", kParamsitePresets[pendingPresetIdx_].name);
            Serial.flush();
            delay(50);
            rp2040.reboot();
        }
    }

private:
    static constexpr const char* kModeFile = "/paramsite_mode.bin";
    // 32 CCs take about 30 ms at MIDI's 31250 baud, so send at most this often.
    static constexpr uint32_t kCCIntervalMs = 30;
    static constexpr uint8_t kCCFileMagic = 0xA5;

    size_t presetIdx_ = 0;
    volatile size_t pendingPresetIdx_ = 0;
    volatile bool confirmPressed_ = false;
    volatile bool restartPending_ = false;

    std::array<float, kN_Params> homeValues_{};
    std::array<float, kN_Params> liveValues_{};
    size_t nLive_ = 0;
    size_t nCCs_ = 0;
    volatile bool ccPending_ = false;
    uint32_t lastCCSendMs_ = 0;
    volatile float fadeAmount_ = 0.f;
    volatile bool held_ = false;

    volatile uint8_t channel_ = 1;
    uint8_t savedChannel_ = 1;
    volatile bool channelSavePending_ = false;

    // MIDI Learn: while the page has control, only the chosen CC is sent, as a
    // slow sweep, so a DAW or plugin's MIDI learn picks up that CC alone.
    volatile bool learnActive_ = false;
    volatile bool learnEnded_ = false;
    volatile size_t learnSlot_ = 0;
    uint32_t lastLearnMs_ = 0;
    int lastLearnValue_ = -1;
    std::shared_ptr<ParamsiteSelectView> learnView_;

    uint32_t presentGroupsMask_ = 0;
    std::vector<uint8_t> knownCCs_;
    std::shared_ptr<CCSelectView> ccView_;
    std::shared_ptr<BlockSelectView> focusView_;

    const ParamsitePreset& preset() const { return kParamsitePresets[presetIdx_]; }

    // ---- MIDI output ----

    void sendCCsIfDue() {
        if (!ccPending_ || !midi_interf) return;
        if (learnActive_) return;           // keep pending; sent when learn ends
        if (held_) { ccPending_ = false; return; }
        uint32_t now = millis();
        if (now - lastCCSendMs_ < kCCIntervalMs) return;
        ccPending_ = false;
        midi_interf->SendParamsAsMIDICC(std::span<const float>(liveValues_.data(), nLive_));
        lastCCSendMs_ = now;
    }

    // ---- Focus ----

    uint32_t groupMaskFor(size_t slot, uint8_t cc) const {
        const ParamsitePreset& P = preset();
        if (P.ccGroup) return P.ccGroup(cc);
        return (1u << (slot / 8)) & ((1u << P.groups.size()) - 1);   // User: by output slot
    }

    // Outputs without a CC are switched off, so the network only learns the ones in use.
    void updateActiveDims() {
        uint32_t mask = focusManager.getSelectedMask();
        std::vector<bool> active(kN_Params);
        for (size_t i = 0; i < kN_Params; i++)
            active[i] = i < nCCs_ && ((mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0));
        interface.setActiveDims(active);
    }

    void recomputeFocusGroups(const std::vector<uint8_t>& ccs) {
        std::array<uint32_t, kN_Params> masks{};
        for (size_t i = 0; i < ccs.size() && i < masks.size(); i++)
            masks[i] = groupMaskFor(i, ccs[i]);
        focusManager.setParamGroups(masks);

        presentGroupsMask_ = 0;
        for (auto m : masks) presentGroupsMask_ |= m;

        uint32_t currentSel = focusManager.getSelectedMask();
        uint32_t validSel   = currentSel & presentGroupsMask_;
        if (validSel != currentSel) {
            focusManager.setFocus(validSel, interface.getLastAction());
            if (focusView_) {
                for (size_t i = 0; i < kMaxGroups; i++)
                    if (((currentSel >> i) & 1u) && !((validSel >> i) & 1u))
                        focusView_->setAltState(i, false);
            }
        }
        updateActiveDims();
    }

    void addFocusView() {
        const ParamsitePreset& P = preset();
        std::vector<String> labels;
        for (auto g : P.groups) labels.push_back(g);
        const size_t nGroups = labels.size();
        if (P.hold) labels.push_back("HOLD");
        const bool compact = labels.size() > 8;   // TR-8S: 12 groups

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, labels.size(), compact ? 40 : 60, compact ? 50 : 60, TFT_WHITE,
            labels, TFT_GREENYELLOW, 2);
        focusView->SetOnSelectCallback([this, focusView, nGroups](size_t id) {
            size_t idx = id - 1;
            if (idx == nGroups) {                 // HOLD: freeze CC output
                held_ = !held_;
                if (!held_) { ccPending_ = true; interface.markInputDirty(); }
                focusView->toggleAlt(idx);
                return;
            }
            if (!((presentGroupsMask_ >> idx) & 1u)) return;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << idx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(idx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);
        focusView_ = focusView;
    }

    // ---- CC Assign ----

    float defaultHome(uint8_t cc) const {
        return preset().defaultHome ? preset().defaultHome(cc) : 0.f;
    }

    void syncHomeArray(const std::vector<float>& homes) {
        homeValues_.fill(0.f);
        for (size_t i = 0; i < homes.size() && i < homeValues_.size(); i++)
            homeValues_[i] = homes[i];
    }

    // Apply a mapping (CCs + homes in matching order) to the page, MIDI and focus groups.
    void applyMapping(const std::vector<uint8_t>& ccs, const std::vector<float>& homes) {
        if (!ccView_) return;
        // The page sorts its CCs and its order is the output order; keep homes with CCs.
        std::vector<std::pair<uint8_t, float>> pairs;
        for (size_t i = 0; i < ccs.size(); i++)
            pairs.emplace_back(ccs[i], i < homes.size() ? homes[i] : defaultHome(ccs[i]));
        std::sort(pairs.begin(), pairs.end());
        std::vector<uint8_t> sortedCCs;
        std::vector<float>   sortedHomes;
        for (auto& p : pairs) { sortedCCs.push_back(p.first); sortedHomes.push_back(p.second); }

        ccView_->setSelectedCCs(sortedCCs);
        ccView_->setHomeValues(sortedHomes);
        knownCCs_ = ccView_->getSelectedCCs();
        nCCs_ = knownCCs_.size();
        if (midi_interf) midi_interf->SetParamCCNumbers(knownCCs_);
        syncHomeArray(ccView_->getHomeValues());
        recomputeFocusGroups(knownCCs_);
        refreshLearnOptions();
        ccView_->resetDirty();
        ccView_->redraw();
        interface.markInputDirty();
    }

    void addCCView() {
        const ParamsitePreset& P = preset();
        auto ccView = std::make_shared<CCSelectView>(kN_Params, "CC Assign");
        if (P.options) {
            ccView->setOptions(*P.options);
        } else {
            // User slots: any CC except 0 (bank select) and 120-127 (channel mode).
            std::vector<CCOption> opts;
            for (uint8_t cc = 1; cc <= 119; cc++) opts.push_back({cc, "CC " + String(cc)});
            ccView->setOptions(opts);
        }
        ccView->setShowHome(true);
        ccView->setLiveValues(liveValues_.data());
        ccView_ = ccView;

        std::vector<uint8_t> ccs;
        std::vector<float>   homes;
        loadCCAssignments(ccs, homes);
        applyMapping(ccs, homes);

        // New selection: newly added CCs get their default home.
        ccView->setOnChangeCallback([this, ccView](const std::vector<uint8_t>& ccs) {
            std::vector<float> homes = ccView->getHomeValues();
            for (size_t i = 0; i < ccs.size() && i < homes.size(); i++)
                if (std::find(knownCCs_.begin(), knownCCs_.end(), ccs[i]) == knownCCs_.end())
                    homes[i] = defaultHome(ccs[i]);
            ccView->setHomeValues(homes);
            knownCCs_ = ccs;
            nCCs_ = ccs.size();
            midi_interf->SetParamCCNumbers(ccs);
            syncHomeArray(ccView->getHomeValues());
            recomputeFocusGroups(ccs);
            refreshLearnOptions();
            interface.markInputDirty();
        });
        ccView->setOnHomeChangeCallback([this, ccView]() {
            syncHomeArray(ccView->getHomeValues());
            interface.markInputDirty();
        });
        // Write flash only on leaving the page: flash writes block MIDI.
        ccView->setOnSaveCallback([this, ccView]() {
            saveCCAssignments(ccView->getSelectedCCs(), ccView->getHomeValues());
        });

        // Knob G edits the home of the CC under the cursor (no audio in this mode).
        MEMLNaut::Instance()->setRVGain1Callback([ccView](float v) {
            if (ccView->isFocused()) ccView->setHomeForCursor(v);
        }, 0);

        // Each saved model carries its mapping; the flash file is the boot default.
        interface.setExtraSaveCallback([this]() -> std::vector<uint8_t> {
            if (!ccView_) return {};
            return serializeMapping(ccView_->getSelectedCCs(), ccView_->getHomeValues());
        });
        interface.setExtraLoadCallback([this](const uint8_t* data, uint16_t size, uint16_t) {
            std::vector<uint8_t> ccs;
            std::vector<float>   homes;
            if (deserializeMapping(data, size, ccs, homes)) applyMapping(ccs, homes);
        });

        MEMLNaut::Instance()->disp->AddView(ccView);
    }

    // ---- MIDI Learn ----

    String ccLabel(uint8_t cc) const {
        if (preset().options)
            for (const auto& o : *preset().options)
                if (o.num == cc) return "CC " + String(cc) + "  " + o.name;
        return "CC " + String(cc);
    }

    void refreshLearnOptions() {
        if (!learnView_) return;
        std::vector<String> opts, descs;
        for (uint8_t cc : knownCCs_) {
            opts.push_back(ccLabel(cc));
            descs.push_back("Press: sweep this CC only, then MIDI-learn it");
        }
        if (opts.empty()) { opts.push_back("No CCs assigned"); descs.push_back(""); }
        learnView_->setOptions(std::span<String>(opts.data(), opts.size()));
        learnView_->setDescriptions(std::span<String>(descs.data(), descs.size()));
        if (learnSlot_ >= opts.size()) learnSlot_ = 0;
        learnView_->setSelection(learnSlot_);
    }

    void addLearnView() {
        auto view = std::make_shared<ParamsiteSelectView>("MIDI Learn");
        learnView_ = view;
        refreshLearnOptions();
        view->setNewSelectionCallback([this](size_t idx) { learnSlot_ = idx; lastLearnValue_ = -1; });
        view->setOnFocus([this]() {
            if (learnSlot_ < knownCCs_.size()) { lastLearnValue_ = -1; learnActive_ = true; }
        });
        view->setFocusLostCallback([this]() {
            if (learnActive_) { learnActive_ = false; learnEnded_ = true; }
        });
        MEMLNaut::Instance()->disp->AddView(view);
    }

    // A 2-second triangle, 0-127, on the chosen CC only.
    void learnSweep() {
        if (!midi_interf || learnSlot_ >= knownCCs_.size()) return;
        uint32_t now = millis();
        if (now - lastLearnMs_ < 20) return;
        lastLearnMs_ = now;
        float phase = (now % 2000) / 2000.f;
        float tri = phase < 0.5f ? phase * 2.f : 2.f - phase * 2.f;
        int value = (int)(tri * 127.f + 0.5f);
        if (value == lastLearnValue_) return;
        lastLearnValue_ = value;
        midi_interf->queueCC(knownCCs_[learnSlot_], (uint8_t)value);
        midi_interf->flushQueue();
    }

    // ---- MIDI Channel ----

    void addChannelView() {
        std::vector<String> opts, descs;
        for (int ch = 1; ch <= 16; ch++) {
            opts.push_back("Channel " + String(ch));
            descs.push_back(ch == preset().channel ? "Default for this mode" : "");
        }
        auto chView = std::make_shared<RotarySelectView>("MIDI Channel");
        chView->setOptions(std::span<String>(opts.data(), opts.size()));
        chView->setDescriptions(std::span<String>(descs.data(), descs.size()));
        chView->setSelection(channel_ - 1);
        // Encoder handler context: store only; flash is written in loopCore0.
        chView->setNewSelectionCallback([this](size_t idx) {
            channel_ = static_cast<uint8_t>(idx + 1);
            if (midi_interf) {
                midi_interf->SetMIDISendChannel(channel_);
                midi_interf->SetMIDINoteChannel(channel_);
            }
        });
        chView->setFocusLostCallback([this]() { channelSavePending_ = true; });
        MEMLNaut::Instance()->disp->AddView(chView);
    }

    // ---- MIDI Mode ----

    void addModeView() {
        // A Cancel entry at each end of the list, so either way off the end of
        // the presets is a way out without choosing a new mode.
        const size_t nPresets = kParamsitePresets.size();
        std::vector<String> opts, descs;
        opts.push_back("Cancel");
        descs.push_back("Keep the current mode");
        for (const auto& p : kParamsitePresets) {
            opts.push_back(p.name);
            descs.push_back(p.desc);
        }
        opts.push_back("Cancel");
        descs.push_back("Keep the current mode");
        auto modeView = std::make_shared<ParamsiteSelectView>("MIDI Mode");
        modeView->setOptions(std::span<String>(opts.data(), opts.size()));
        modeView->setDescriptions(std::span<String>(descs.data(), descs.size()));
        modeView->setSelection(presetIdx_ + 1);
        pendingPresetIdx_ = presetIdx_;
        modeView->setNewSelectionCallback([this, nPresets](size_t idx) {
            pendingPresetIdx_ = (idx == 0 || idx > nPresets) ? presetIdx_ : idx - 1;
        });
        // Press to confirm: restart into the chosen mode. Leaving without a press,
        // or pressing on Cancel, puts the list back on the current mode.
        modeView->setOnConfirm([this]() { confirmPressed_ = true; });
        modeView->setFocusLostCallback([this, modeView]() {
            if (confirmPressed_ && pendingPresetIdx_ != presetIdx_) {
                restartPending_ = true;
            } else {
                pendingPresetIdx_ = presetIdx_;
                modeView->setSelection(presetIdx_ + 1);
            }
            confirmPressed_ = false;
        });
        MEMLNaut::Instance()->disp->AddView(modeView);
    }

    // ---- Files ----

    size_t loadModeChoice() {
        FILE* f = fopen(kModeFile, "rb");
        if (f) {
            char root[32] = {};
            size_t n = fread(root, 1, sizeof(root) - 1, f);
            fclose(f);
            root[n] = 0;
            for (size_t i = 0; i < kParamsitePresets.size(); i++)
                if (strcmp(root, kParamsitePresets[i].root) == 0) return i;
        }
        return 0;   // User 1
    }

    // Stored by root name, so reordering the preset table keeps the choice.
    void saveModeChoice(size_t idx) {
        FILE* f = fopen(kModeFile, "wb");
        if (!f) { Serial.println("Paramsite: failed to save mode"); return; }
        const char* root = kParamsitePresets[idx].root;
        fwrite(root, 1, strlen(root), f);
        fclose(f);
    }

    String channelFile() const { return String("/") + preset().root + "_ch.bin"; }

    uint8_t loadChannel() {
        uint8_t ch = preset().channel;
        FILE* f = fopen(channelFile().c_str(), "rb");
        if (f) {
            uint8_t b;
            if (fread(&b, 1, 1, f) == 1 && b >= 1 && b <= 16) ch = b;
            fclose(f);
        }
        savedChannel_ = ch;
        return ch;
    }

    void saveChannel(uint8_t ch) {
        FILE* f = fopen(channelFile().c_str(), "wb");
        if (!f) { Serial.println("Paramsite: failed to save MIDI channel"); return; }
        fwrite(&ch, 1, 1, f);
        fclose(f);
        savedChannel_ = ch;
    }

    // Mapping blob: [0xA5][N][N CCs][N homes 0..127], as in the TR-8S mode.
    static std::vector<uint8_t> serializeMapping(const std::vector<uint8_t>& ccs,
                                                 const std::vector<float>& homes) {
        uint8_t n = (uint8_t)std::min<size_t>(ccs.size(), 255);
        std::vector<uint8_t> out;
        out.reserve(2 + 2 * n);
        out.push_back(kCCFileMagic);
        out.push_back(n);
        for (size_t i = 0; i < n; i++) out.push_back(ccs[i]);
        for (size_t i = 0; i < n; i++) {
            float h = (i < homes.size()) ? homes[i] : 0.f;
            out.push_back((uint8_t)(h * 127.f + 0.5f));
        }
        return out;
    }

    static bool deserializeMapping(const uint8_t* data, size_t size,
                                   std::vector<uint8_t>& ccs, std::vector<float>& homes) {
        ccs.clear();
        homes.clear();
        if (size >= 2 && data[0] == kCCFileMagic) {
            size_t n = data[1];
            if (size >= 2 + 2 * n) {
                for (size_t i = 0; i < n; i++) ccs.push_back(data[2 + i]);
                for (size_t i = 0; i < n; i++) homes.push_back(data[2 + n + i] / 127.f);
                return true;
            }
        }
        return false;
    }

    // Reads both formats: with homes (TR-8S, Blofeld, and everything Paramsite saves)
    // and plain CC bytes (the other single-synth modes), so their existing setups load.
    void loadCCAssignments(std::vector<uint8_t>& ccs, std::vector<float>& homes) {
        ccs.clear();
        homes.clear();
        FILE* f = fopen(preset().ccFile, "rb");
        if (f) {
            std::vector<uint8_t> raw;
            uint8_t b;
            while (fread(&b, 1, 1, f) == 1) raw.push_back(b);
            fclose(f);
            if (deserializeMapping(raw.data(), raw.size(), ccs, homes)) return;
            if (!raw.empty()) {
                for (uint8_t cc : raw) {
                    if (cc > 127) continue;
                    ccs.push_back(cc);
                    homes.push_back(defaultHome(cc));
                }
                if (!ccs.empty()) return;
            }
        }
        ccs.assign(preset().defaults->begin(), preset().defaults->end());
        for (uint8_t cc : ccs) homes.push_back(defaultHome(cc));
    }

    // Always with homes. The old plain-byte single-synth firmwares (TR-6S, Erica
    // Filterbank) cannot read this format.
    void saveCCAssignments(const std::vector<uint8_t>& ccs, const std::vector<float>& homes) {
        FILE* f = fopen(preset().ccFile, "wb");
        if (!f) { Serial.println("Paramsite: failed to save CC assignments"); return; }
        std::vector<uint8_t> blob = serializeMapping(ccs, homes);
        fwrite(blob.data(), 1, blob.size(), f);
        fclose(f);
    }
};
