#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/display/CCSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../TRxSAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include <cstdio>
#include <algorithm>
#include <span>

// TR-8S MIDI CC targets — full set from the Roland TR-8S MIDI Implementation
// (Version 1.02, Feb 2018). 11 instruments (BD/SD/LT/MT/HT/RS/HC/CH/OH/CC/RC),
// each with Ctrl/Tune/Decay/Level, plus global effect controls.
static const std::vector<CCOption> kTR8SCCOptions = {
    // BD
    { 96, "BD Ctrl"  }, { 20, "BD Tune"  }, { 23, "BD Decay" }, { 24, "BD Level" },
    // SD
    { 97, "SD Ctrl"  }, { 25, "SD Tune"  }, { 28, "SD Decay" }, { 29, "SD Level" },
    // LT
    {102, "LT Ctrl"  }, { 46, "LT Tune"  }, { 47, "LT Decay" }, { 48, "LT Level" },
    // MT
    {103, "MT Ctrl"  }, { 49, "MT Tune"  }, { 50, "MT Decay" }, { 51, "MT Level" },
    // HT
    {104, "HT Ctrl"  }, { 52, "HT Tune"  }, { 53, "HT Decay" }, { 54, "HT Level" },
    // RS
    {105, "RS Ctrl"  }, { 55, "RS Tune"  }, { 56, "RS Decay" }, { 57, "RS Level" },
    // HC
    {106, "HC Ctrl"  }, { 58, "HC Tune"  }, { 59, "HC Decay" }, { 60, "HC Level" },
    // CH
    {107, "CH Ctrl"  }, { 61, "CH Tune"  }, { 62, "CH Decay" }, { 63, "CH Level" },
    // OH
    {108, "OH Ctrl"  }, { 80, "OH Tune"  }, { 81, "OH Decay" }, { 82, "OH Level" },
    // CC (Crash)
    {109, "CC Ctrl"  }, { 83, "CC Tune"  }, { 84, "CC Decay" }, { 85, "CC Level" },
    // RC (Ride)
    {110, "RC Ctrl"  }, { 86, "RC Tune"  }, { 87, "RC Decay" }, { 88, "RC Level" },
    // Global / FX
    { 91, "Reverb Level"   }, { 16, "Delay Level"    }, { 17, "Delay Time"     },
    { 18, "Delay Feedback" }, { 19, "Master FX Ctrl" }, { 15, "Master FX On"   },
    { 71, "Accent"         }, {  9, "Shuffle"        }, { 12, "Ext In Level"   },
    { 14, "Fill In On"     }, { 70, "Fill In Trig"   },
};

// Default 32 assignments: Ctrl + Tune for every instrument (22), Decay for the
// six core voices (6), and four global FX (4).
static const std::vector<uint8_t> kTR8SDefaultCCs = {
    // Ctrl, all 11 instruments
    96, 97, 102, 103, 104, 105, 106, 107, 108, 109, 110,
    // Tune, all 11 instruments
    20, 25, 46, 49, 52, 55, 58, 61, 80, 83, 86,
    // Decay for BD/SD/LT/HC/CH/OH
    23, 28, 47, 59, 62, 81,
    // Reverb + Delay Level/Time/Feedback
    91, 16, 17, 18
};

// Focus groups — one per TR-8S instrument plus a combined FX group.
static constexpr size_t kTR8SNumGroups = 12;
static constexpr uint32_t kTR8SFocusBD = (1u << 0);
static constexpr uint32_t kTR8SFocusSD = (1u << 1);
static constexpr uint32_t kTR8SFocusLT = (1u << 2);
static constexpr uint32_t kTR8SFocusMT = (1u << 3);
static constexpr uint32_t kTR8SFocusHT = (1u << 4);
static constexpr uint32_t kTR8SFocusRS = (1u << 5);
static constexpr uint32_t kTR8SFocusHC = (1u << 6);
static constexpr uint32_t kTR8SFocusCH = (1u << 7);
static constexpr uint32_t kTR8SFocusOH = (1u << 8);
static constexpr uint32_t kTR8SFocusCC = (1u << 9);
static constexpr uint32_t kTR8SFocusRC = (1u << 10);
static constexpr uint32_t kTR8SFocusFX = (1u << 11);

class MEMLNautModeTR8S {
public:
    constexpr static size_t kN_InputParams    = InterfaceRL::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<32> audioApp;
    std::array<String, TRxSAudioApp<32>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    FocusManager<TRxSAudioApp<32>::kN_Params, kTR8SNumGroups> focusManager;
    uint32_t presentGroupsMask_ = 0;

    // Per-CC "home" base values [0..1], indexed by NN output slot (== sorted CC order).
    std::array<float, TRxSAudioApp<32>::kN_Params> homeValues_{};
    // Live output values [0..1] actually sent as MIDI (home + fade modulation). Read by
    // the CC Assign view's "out" column. Written in the output hook (core 0), read in
    // loopCore0 (core 0) — same core, no lock needed.
    std::array<float, TRxSAudioApp<32>::kN_Params> liveValues_{};
    std::shared_ptr<CCSelectView> ccView_;
    std::shared_ptr<BlockSelectView> focusView_;
    std::function<void()> updateActiveDims_;
    // rvx (RV_X1) knob: fades NN modulation in/out around home. Written from the knob
    // callback, read in the param transform hook (same cross-core pattern as focusManager).
    volatile float fadeAmount_ = 0.f;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<32>::kN_Params);
        // Repurpose rvx (RV_X1) from reward scale to the modulation fade amount.
        // Must be set before bindInterface(), which reads the override.
        interface.setRVX1Override([this](float v) { fadeAmount_ = v; interface.markInputDirty(); });
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("tr8s", "TR-8S");
        interfacePtr = make_non_owning(interface);

        const char* names[kTR8SNumGroups] =
            {"BD", "SD", "LT", "MT", "HT", "RS", "HC", "CH", "OH", "CC", "RC", "FX"};
        for (size_t i = 0; i < kTR8SNumGroups; i++)
            focusManager.setGroupName(i, names[i]);

        // Focus latching applies to the NN outputs themselves (as before), so the NN
        // outputs screen and training see the raw, focus-filtered network values.
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        // Home + fade is applied ONLY on the way out to MIDI — it does not touch the NN
        // outputs graph or the training action. Linear interpolation home -> NN output:
        // out = home + fade*(nn - home).  fade (rvx) = 0 -> home, fade = 1 -> NN output.
        interface.paramOutputHook = [this](std::span<const float> nn) {
            const float fade = fadeAmount_;
            const size_t n = nn.size();
            for (size_t i = 0; i < n && i < liveValues_.size(); i++) {
                float v = homeValues_[i] + fade * (nn[i] - homeValues_[i]);
                liveValues_[i] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
            }
            if (midi_interf) midi_interf->SendParamsAsMIDICC(std::span<const float>(liveValues_.data(), n));
        };
    }

    // Copy the CC-page home values into the slot-indexed array read by the hook.
    void syncHomeArray(const std::vector<float>& homes) {
        homeValues_.fill(0.f);
        for (size_t i = 0; i < homes.size() && i < homeValues_.size(); i++)
            homeValues_[i] = homes[i];
    }

    String getHelpTitle() { return "TR-8S Mode"; }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioApp.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioApp.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioApp.getVoiceSpaceNames();
    }

    __force_inline void loop() { audioApp.loop(); }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(TRxSAudioApp<32>::kN_Params);
        midi_interf->SetMIDISendChannel(10);  // TR-8S default channel
        midi_interf->SetParamCCNumbers(kTR8SDefaultCCs);
        interface.bindMIDI(midi_interf);
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = TRxSAudioApp<32>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims_ = updateActiveDims;
        updateActiveDims();

        // 12 focus blocks laid out 6x2; narrow buttons to fit the 320px display.
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, kTR8SNumGroups, 40, 50, TFT_WHITE,
            std::vector<String>{"BD", "SD", "LT", "MT", "HT", "RS",
                                "HC", "CH", "OH", "CC", "RC", "FX"},
            TFT_GREENYELLOW, 2);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            size_t groupIdx = id - 1;
            if (!((presentGroupsMask_ >> groupIdx) & 1u)) return;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);
        focusView_ = focusView;

        auto ccView = std::make_shared<CCSelectView>(TRxSAudioApp<32>::kN_Params, "CC Assign");
        ccView->setOptions(kTR8SCCOptions);
        ccView->setShowHome(true);
        ccView->setLiveValues(liveValues_.data());  // "out" column reads live MIDI values
        ccView_ = ccView;                            // for live refresh from loopCore0

        // Boot default: load the global mapping file. Per-model mappings override this
        // later via the extra-load callback whenever a model is loaded.
        std::vector<uint8_t> savedCCs;
        std::vector<float>   savedHomes;
        loadCCAssignments(savedCCs, savedHomes);
        applyMapping(savedCCs, savedHomes);

        // CC selection changed: remap MIDI, resync homes + focus groups (live, no flash).
        ccView->setOnChangeCallback([this, ccView, focusView, updateActiveDims](const std::vector<uint8_t>& ccs) {
            midi_interf->SetParamCCNumbers(ccs);
            syncHomeArray(ccView->getHomeValues());
            recomputeFocusGroups(ccs, focusView, updateActiveDims);
        });

        // Home value edited (via the gain knob): resync + push to output (live, no flash).
        ccView->setOnHomeChangeCallback([this, ccView]() {
            syncHomeArray(ccView->getHomeValues());
            interface.markInputDirty();
        });

        // Persist to flash only when leaving the page / exiting edit — flash writes block
        // and would disrupt audio/MIDI processing if done on every knob tick.
        ccView->setOnSaveCallback([this, ccView]() {
            saveCCAssignments(ccView->getSelectedCCs(), ccView->getHomeValues());
        });

        // Gain knob sets the home of the cursored CC, but only while the CC page is
        // focused for editing. (This mode produces no audio, so the gain knob is free.)
        MEMLNaut::Instance()->setRVGain1Callback([ccView](float v) {
            if (ccView->isFocused()) ccView->setHomeForCursor(v);
        }, 0);

        // Bundle the mapping (CC assignments + home values) into each saved model, so
        // loading a model restores its own mapping. The global file stays the boot default;
        // edits on this page update that default via save-on-exit.
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
        interface.addInputSourceView(false);
    }

    // Apply a CC mapping (assignments + homes) to the view and live state. Used by the boot
    // load and the per-model extra-load. Leaves the page non-dirty (no auto re-save to the
    // global file — only real user edits update that).
    void applyMapping(const std::vector<uint8_t>& ccs, const std::vector<float>& homes) {
        if (!ccView_) return;
        ccView_->setSelectedCCs(ccs);
        ccView_->setHomeValues(homes);
        if (midi_interf) midi_interf->SetParamCCNumbers(ccs);
        syncHomeArray(ccView_->getHomeValues());
        if (focusView_ && updateActiveDims_) recomputeFocusGroups(ccs, focusView_, updateActiveDims_);
        ccView_->resetDirty();
        ccView_->redraw();
    }

    inline void processAnalysisParams() {}
    void analyse(stereosample_t) {}
    AudioDriver::codec_config_t getCodecConfig() { return audioApp.GetDriverConfig(); }
    void loopCore0() {
        if (ccView_) ccView_->refreshLiveColumn();  // live "out" column update (throttled)
    }

private:
    static constexpr const char* kCCFile = "/tr8s_cc.bin";

    static uint32_t ccToGroupMask(uint8_t cc) {
        switch (cc) {
            case 96: case 20: case 23: case 24:  return kTR8SFocusBD;
            case 97: case 25: case 28: case 29:  return kTR8SFocusSD;
            case 102: case 46: case 47: case 48: return kTR8SFocusLT;
            case 103: case 49: case 50: case 51: return kTR8SFocusMT;
            case 104: case 52: case 53: case 54: return kTR8SFocusHT;
            case 105: case 55: case 56: case 57: return kTR8SFocusRS;
            case 106: case 58: case 59: case 60: return kTR8SFocusHC;
            case 107: case 61: case 62: case 63: return kTR8SFocusCH;
            case 108: case 80: case 81: case 82: return kTR8SFocusOH;
            case 109: case 83: case 84: case 85: return kTR8SFocusCC;
            case 110: case 86: case 87: case 88: return kTR8SFocusRC;
            case 91: case 16: case 17: case 18: case 19:
            case 15: case 71: case 9: case 12: case 14: case 70: return kTR8SFocusFX;
            default: return 0;
        }
    }

    void recomputeFocusGroups(const std::vector<uint8_t>& ccs,
                              std::shared_ptr<BlockSelectView> focusView,
                              std::function<void()> updateActiveDims) {
        std::array<uint32_t, TRxSAudioApp<32>::kN_Params> masks{};
        for (size_t i = 0; i < ccs.size() && i < masks.size(); i++)
            masks[i] = ccToGroupMask(ccs[i]);
        focusManager.setParamGroups(masks);

        presentGroupsMask_ = 0;
        for (auto m : masks) presentGroupsMask_ |= m;

        uint32_t currentSel = focusManager.getSelectedMask();
        uint32_t validSel   = currentSel & presentGroupsMask_;
        if (validSel != currentSel) {
            focusManager.setFocus(validSel, interface.getLastAction());
            for (size_t i = 0; i < kTR8SNumGroups; i++) {
                if (((currentSel >> i) & 1u) && !((validSel >> i) & 1u))
                    focusView->setAltState(i, false);
            }
        }
        updateActiveDims();
    }

    // Mapping blob format: [0xA5 magic][uint8 N][N CC bytes][N home bytes (0..127)].
    // 0xA5 (>127) can't be a legacy first-CC byte, so old raw-CC files are detected
    // and loaded with default (0) homes. Used for both the global flash file and the
    // per-model extra-save blob.
    static constexpr uint8_t kCCFileMagic = 0xA5;

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

    void loadCCAssignments(std::vector<uint8_t>& ccs, std::vector<float>& homes) {
        ccs.clear();
        homes.clear();
        FILE* f = fopen(kCCFile, "rb");
        if (f) {
            std::vector<uint8_t> raw;
            uint8_t b;
            while (fread(&b, 1, 1, f) == 1) raw.push_back(b);
            fclose(f);

            if (deserializeMapping(raw.data(), raw.size(), ccs, homes)) {
                Serial.printf("TR8S: loaded %u CC assignments (with homes) from flash\n", (unsigned)ccs.size());
                return;
            } else if (!raw.empty()) {
                // Legacy: raw CC bytes, default homes.
                ccs = raw;
                homes.assign(ccs.size(), 0.f);
                Serial.printf("TR8S: loaded %u legacy CC assignments from flash\n", (unsigned)ccs.size());
                return;
            }
        } else {
            Serial.println("TR8S: no saved CC assignments, using defaults");
        }
        ccs.assign(kTR8SDefaultCCs.begin(), kTR8SDefaultCCs.end());
        homes.assign(ccs.size(), 0.f);
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs, const std::vector<float>& homes) {
        std::vector<uint8_t> blob = serializeMapping(ccs, homes);
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(blob.data(), 1, blob.size(), f);
            fclose(f);
            Serial.printf("TR8S: saved %u CC assignments (with homes) to flash\n",
                          (unsigned)(blob.size() >= 2 ? blob[1] : 0));
        } else {
            Serial.println("TR8S: failed to open flash for writing");
        }
    }
};
