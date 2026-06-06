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

// TR-6S MIDI CC targets — all 33 supported CCs, grouped by instrument then effects
static const std::vector<CCOption> kTR6SCCOptions = {
    { 96, "BD Ctrl"        },
    { 97, "SD Ctrl"        },
    { 25, "SD Tune"        },
    { 28, "SD Decay"       },
    { 29, "SD Level"       },
    {102, "LT Ctrl"        },
    { 46, "LT Tune"        },
    { 47, "LT Decay"       },
    { 48, "LT Level"       },
    {106, "HC Ctrl"        },
    { 58, "HC Tune"        },
    { 59, "HC Decay"       },
    { 60, "HC Level"       },
    {107, "CH Ctrl"        },
    { 61, "CH Tune"        },
    { 62, "CH Decay"       },
    { 63, "CH Level"       },
    {108, "OH Ctrl"        },
    { 80, "OH Tune"        },
    { 81, "OH Decay"       },
    { 82, "OH Level"       },
    { 91, "Reverb Level"   },
    { 16, "Delay Level"    },
    { 17, "Delay Time"     },
    { 18, "Delay Feedback" },
    { 19, "Master FX Ctrl" },
    { 15, "Master FX"      },
    { 71, "Accent"         },
    {  9, "Shuffle"        },
    { 70, "Fill IN Trig"   },
};

// Default 16 assignments: 6 Ctrl knobs + Reverb + Delay (Level/Time/FB) + SD Tune/Decay + LT/HC/CH/OH Tune
static const std::vector<uint8_t> kTR6SDefaultCCs = {
    96, 97, 102, 106, 107, 108,   // BD/SD/LT/HC/CH/OH Ctrl
    91, 16, 17, 18,               // Reverb Level, Delay Level/Time/Feedback
    25, 28,                       // SD Tune, SD Decay
    46, 58, 61, 80                // LT/HC/CH/OH Tune
};

// Focus group bitmasks — one per TR-6S instrument/category
static constexpr uint32_t kTR6SFocusBD = (1u << 0);
static constexpr uint32_t kTR6SFocusSD = (1u << 1);
static constexpr uint32_t kTR6SFocusLT = (1u << 2);
static constexpr uint32_t kTR6SFocusHC = (1u << 3);
static constexpr uint32_t kTR6SFocusCH = (1u << 4);
static constexpr uint32_t kTR6SFocusOH = (1u << 5);
static constexpr uint32_t kTR6SFocusFX = (1u << 6);

class MEMLNautModeTR6S {
public:
    constexpr static size_t kN_InputParams    = InterfaceRL::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<> audioApp;
    std::array<String, TRxSAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    FocusManager<TRxSAudioApp<>::kN_Params, 7> focusManager;
    uint32_t presentGroupsMask_ = 0;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("tr6s", "TR-6S");
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "BD");
        focusManager.setGroupName(1, "SD");
        focusManager.setGroupName(2, "LT");
        focusManager.setGroupName(3, "HC");
        focusManager.setGroupName(4, "CH");
        focusManager.setGroupName(5, "OH");
        focusManager.setGroupName(6, "FX");

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
    }

    String getHelpTitle() { return "TR-6S Mode"; }

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
        midi_interf->SetMIDISendChannel(10);  // TR-6S default channel
        midi_interf->SetParamCCNumbers(kTR6SDefaultCCs);
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

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 7, 60, 60, TFT_WHITE,
            std::vector<String>{"BD", "SD", "LT", "HC", "CH", "OH", "FX"},
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
        ccView->setOptions(kTR6SCCOptions);

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
    static constexpr const char* kCCFile = "/tr6s_cc.bin";

    static uint32_t ccToGroupMask(uint8_t cc) {
        switch (cc) {
            case 96:                              return kTR6SFocusBD;
            case 97: case 25: case 28: case 29:  return kTR6SFocusSD;
            case 102: case 46: case 47: case 48: return kTR6SFocusLT;
            case 106: case 58: case 59: case 60: return kTR6SFocusHC;
            case 107: case 61: case 62: case 63: return kTR6SFocusCH;
            case 108: case 80: case 81: case 82: return kTR6SFocusOH;
            case 91: case 16: case 17: case 18:
            case 19: case 15: case 71: case 9: case 70: return kTR6SFocusFX;
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
            for (size_t i = 0; i < 7; i++) {
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
            Serial.printf("TR6S: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
            if (!ccs.empty()) return ccs;
        } else {
            Serial.println("TR6S: no saved CC assignments, using defaults");
        }
        return std::vector<uint8_t>(kTR6SDefaultCCs.begin(), kTR6SDefaultCCs.end());
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs) {
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(ccs.data(), 1, ccs.size(), f);
            fclose(f);
            Serial.printf("TR6S: saved %u CC assignments to flash\n", (unsigned)ccs.size());
        } else {
            Serial.println("TR6S: failed to open flash for writing");
        }
    }
};
