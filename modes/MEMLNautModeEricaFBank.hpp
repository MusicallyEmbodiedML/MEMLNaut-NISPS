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

// Erica Synths Resonant Filterbank MIDI CC options.
// CCs 70-83 are "active-mode dependent" on the device — the same CC number
// controls different parameters depending on which mode (FB, FILTER, CLK MOD,
// DYN EQ) the Erica unit is currently in. All four sets are listed here with
// mode prefixes so users can pick the labels matching their device's active mode.
static const std::vector<CCOption> kEricaFBankCCOptions = {
    // Filterbank (FB) mode — individual band gains
    { 70, "FB B1 29Hz"    },
    { 71, "FB B2 61Hz"    },
    { 72, "FB B3 115Hz"   },
    { 73, "FB B4 218Hz"   },
    { 74, "FB B5 411Hz"   },
    { 75, "FB B6 777Hz"   },
    { 76, "FB B7 1.5kHz"  },
    { 77, "FB B8 2.8kHz"  },
    { 78, "FB B9 5.2kHz"  },
    { 79, "FB B10 11kHz"  },
    // Filterbank mode — grouped gains
    { 80, "FB Low Gain"   },
    { 81, "FB Mid Gain"   },
    { 82, "FB Hi Gain"    },
    { 83, "FB All Gain"   },
    // Universal — always active regardless of device mode
    { 84, "Resonance"     },
    { 85, "FB Chn 1"      },
    { 86, "FB Chn 2"      },
    { 87, "FB Chn 3"      },
    { 88, "FB Chn 4"      },
    { 89, "FB Chn 5"      },
    { 90, "FB Chn 6"      },
    { 91, "FB Chn 7"      },
    { 92, "FB Chn 8"      },
    { 93, "FB Chn 9"      },
    { 94, "FB Chn 10"     },
    { 95, "FB Chn All"    },
    // Global (mono channel)
    { 96, "Mix"           },
    { 97, "Spread"        },
    // Filter mode — same CC range 70-75, different parameter meanings
    { 70, "Flt: Cutoff"   },
    { 71, "Flt: Slope"    },
    { 72, "Flt: Width"    },
    { 73, "Flt: Type"     },
    { 74, "Flt: Gain"     },
    { 75, "Flt: Offset"   },
    // Clocked Modulation mode — CC range 70-76
    { 70, "CLK: Mod Gain" },
    { 71, "CLK: Waveshape"},
    { 72, "CLK: Freq"     },
    { 73, "CLK: Step Dir" },
    { 74, "CLK: BPM"      },
    { 75, "CLK: BandOff"  },
    { 76, "CLK: R Invert" },
    // Dynamic Equalizer mode — CC range 70-76
    { 70, "DYN: Threshold"},
    { 71, "DYN: Ctrl Dcy" },
    { 72, "DYN: Ctrl Atk" },
    { 73, "DYN: EnvDelay" },
    { 74, "DYN: Flt Freq" },
    { 75, "DYN: AudioSrc" },
    { 76, "DYN: BandOff"  },
};

// Default 16: all 10 individual band gains, 3 grouped gains, resonance, mix, spread
static const std::vector<uint8_t> kEricaFBankDefaultCCs = {
    70, 71, 72,      // LOW bands: 29Hz, 61Hz, 115Hz
    73, 74, 75, 76,  // MID bands: 218Hz, 411Hz, 777Hz, 1.5kHz
    77, 78, 79,      // HIGH bands: 2.8kHz, 5.2kHz, 11kHz
    80, 81, 82,      // Band Low / Mid / High grouped
    84,              // Resonance
    96, 97           // Mix, Spread
};

// Focus group bitmasks — frequency-range based
static constexpr uint32_t kEricaFBankFocusLOW  = (1u << 0);  // 29–115 Hz
static constexpr uint32_t kEricaFBankFocusMID   = (1u << 1);  // 218 Hz – 1.5 kHz
static constexpr uint32_t kEricaFBankFocusHIGH  = (1u << 2);  // 2.8–11 kHz
static constexpr uint32_t kEricaFBankFocusGRP   = (1u << 3);  // Grouped band gains
static constexpr uint32_t kEricaFBankFocusRESO  = (1u << 4);  // Resonance + feedback
static constexpr uint32_t kEricaFBankFocusGLOB  = (1u << 5);  // Mix + spread

class MEMLNautModeEricaFBank {
public:
    constexpr static size_t kN_InputParams     = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<16> audioApp;
    std::array<String, TRxSAudioApp<16>::nVoiceSpaces> voiceSpaceList;

    using InterfaceRL_t = InterfaceRL<TRxSAudioApp<16>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    FocusManager<TRxSAudioApp<16>::kN_Params, 6> focusManager;
    uint32_t presentGroupsMask_ = 0;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<16>::kN_Params);
        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("erica_fbank", "Resonant FBank");
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "LOW");
        focusManager.setGroupName(1, "MID");
        focusManager.setGroupName(2, "HIGH");
        focusManager.setGroupName(3, "GRP");
        focusManager.setGroupName(4, "RESO");
        focusManager.setGroupName(5, "GLOB");

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
    }

    String getHelpTitle() { return "Erica Resonant FBank Mode"; }

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
        midi_interf->Setup(TRxSAudioApp<16>::kN_Params);
        midi_interf->SetMIDISendChannel(1);  // Erica default; change on device via CONFIG > MIDI
        midi_interf->SetParamCCNumbers(kEricaFBankDefaultCCs);
        interface.bindMIDI(midi_interf);
        interface.paramOutputHook = [this](std::span<const float> params) {
            if (held_) return;
            uint32_t now = millis();
            if (now - lastCCSendMs_ >= 30) {
                midi_interf->SendParamsAsMIDICC(params);
                lastCCSendMs_ = now;
            }
        };
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = TRxSAudioApp<16>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 7, 60, 60, TFT_WHITE,
            std::vector<String>{"LOW", "MID", "HIGH", "GRP", "RESO", "GLOB", "HOLD"},
            TFT_GREENYELLOW, 2);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            if (id == 7) {
                held_ = !held_;
                focusView->toggleAlt(6);
                return;
            }
            size_t groupIdx = id - 1;
            if (!((presentGroupsMask_ >> groupIdx) & 1u)) return;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        auto ccView = std::make_shared<CCSelectView>(TRxSAudioApp<16>::kN_Params, "CC Assign");
        ccView->setOptions(kEricaFBankCCOptions);

        auto saved = loadCCAssignments();
        // The view sorts the CCs, and its order is the NN output order, so use it.
        ccView->setSelectedCCs(saved);
        const std::vector<uint8_t>& ordered = ccView->getSelectedCCs();
        midi_interf->SetParamCCNumbers(ordered);
        recomputeFocusGroups(ordered, focusView, updateActiveDims);

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
    static constexpr const char* kCCFile = "/erica_fbank_cc.bin";
    uint32_t lastCCSendMs_ = 0;
    bool held_ = false;

    static uint32_t ccToGroupMask(uint8_t cc) {
        if (cc >= 70 && cc <= 72) return kEricaFBankFocusLOW;
        if (cc >= 73 && cc <= 76) return kEricaFBankFocusMID;
        if (cc >= 77 && cc <= 79) return kEricaFBankFocusHIGH;
        if (cc >= 80 && cc <= 83) return kEricaFBankFocusGRP;
        if (cc >= 84 && cc <= 95) return kEricaFBankFocusRESO;
        if (cc >= 96 && cc <= 97) return kEricaFBankFocusGLOB;
        return 0;
    }

    void recomputeFocusGroups(const std::vector<uint8_t>& ccs,
                              std::shared_ptr<BlockSelectView> focusView,
                              std::function<void()> updateActiveDims) {
        std::array<uint32_t, TRxSAudioApp<16>::kN_Params> masks{};
        for (size_t i = 0; i < ccs.size() && i < masks.size(); i++)
            masks[i] = ccToGroupMask(ccs[i]);
        focusManager.setParamGroups(masks);

        presentGroupsMask_ = 0;
        for (auto m : masks) presentGroupsMask_ |= m;

        uint32_t currentSel = focusManager.getSelectedMask();
        uint32_t validSel   = currentSel & presentGroupsMask_;
        if (validSel != currentSel) {
            focusManager.setFocus(validSel, interface.getLastAction());
            for (size_t i = 0; i < 6; i++) {
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
            Serial.printf("EricaFBank: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
            if (!ccs.empty()) return ccs;
        } else {
            Serial.println("EricaFBank: no saved CC assignments, using defaults");
        }
        return std::vector<uint8_t>(kEricaFBankDefaultCCs.begin(), kEricaFBankDefaultCCs.end());
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs) {
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(ccs.data(), 1, ccs.size(), f);
            fclose(f);
            Serial.printf("EricaFBank: saved %u CC assignments to flash\n", (unsigned)ccs.size());
        } else {
            Serial.println("EricaFBank: failed to open flash for writing");
        }
    }
};
