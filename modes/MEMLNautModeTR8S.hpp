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

// Default 16 assignments: one Ctrl knob per instrument (11) + 5 global FX.
static const std::vector<uint8_t> kTR8SDefaultCCs = {
    96, 97, 102, 103, 104, 105, 106, 107, 108, 109, 110,  // BD..RC Ctrl
    91, 16, 17, 18, 19                                     // Reverb, Delay L/T/FB, Master FX Ctrl
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

    inline static TRxSAudioApp<> audioApp;
    std::array<String, TRxSAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    FocusManager<TRxSAudioApp<>::kN_Params, kTR8SNumGroups> focusManager;
    uint32_t presentGroupsMask_ = 0;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("tr8s", "TR-8S");
        interfacePtr = make_non_owning(interface);

        const char* names[kTR8SNumGroups] =
            {"BD", "SD", "LT", "MT", "HT", "RS", "HC", "CH", "OH", "CC", "RC", "FX"};
        for (size_t i = 0; i < kTR8SNumGroups; i++)
            focusManager.setGroupName(i, names[i]);

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
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
        midi_interf->Setup(TRxSAudioApp<>::kN_Params);
        midi_interf->SetMIDISendChannel(10);  // TR-8S default channel
        midi_interf->SetParamCCNumbers(kTR8SDefaultCCs);
        interface.bindMIDI(midi_interf);
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = TRxSAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
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

        auto ccView = std::make_shared<CCSelectView>(TRxSAudioApp<>::kN_Params, "CC Assign");
        ccView->setOptions(kTR8SCCOptions);

        auto saved = loadCCAssignments();
        ccView->setSelectedCCs(saved);
        midi_interf->SetParamCCNumbers(saved);
        recomputeFocusGroups(saved, focusView, updateActiveDims);

        ccView->setOnChangeCallback([this, focusView, updateActiveDims](const std::vector<uint8_t>& ccs) {
            midi_interf->SetParamCCNumbers(ccs);
            saveCCAssignments(ccs);
            recomputeFocusGroups(ccs, focusView, updateActiveDims);
        });
        MEMLNaut::Instance()->disp->AddView(ccView);
        interface.addInputSourceView(false);
    }

    inline void processAnalysisParams() {}
    void analyse(stereosample_t) {}
    AudioDriver::codec_config_t getCodecConfig() { return audioApp.GetDriverConfig(); }
    void loopCore0() {}

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
        std::array<uint32_t, TRxSAudioApp<>::kN_Params> masks{};
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

    std::vector<uint8_t> loadCCAssignments() {
        FILE* f = fopen(kCCFile, "rb");
        if (f) {
            std::vector<uint8_t> ccs;
            uint8_t b;
            while (fread(&b, 1, 1, f) == 1) ccs.push_back(b);
            fclose(f);
            Serial.printf("TR8S: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
            if (!ccs.empty()) return ccs;
        } else {
            Serial.println("TR8S: no saved CC assignments, using defaults");
        }
        return std::vector<uint8_t>(kTR8SDefaultCCs.begin(), kTR8SDefaultCCs.end());
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs) {
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(ccs.data(), 1, ccs.size(), f);
            fclose(f);
            Serial.printf("TR8S: saved %u CC assignments to flash\n", (unsigned)ccs.size());
        } else {
            Serial.println("TR8S: failed to open flash for writing");
        }
    }
};
