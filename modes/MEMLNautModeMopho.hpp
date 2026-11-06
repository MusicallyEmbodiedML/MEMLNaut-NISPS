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

// DSI Mopho MIDI CC options, from the Dave Smith Instruments Mopho manual
// ("MIDI Implementation", CC table). The Mopho family shares this map. Only these
// parameters have CCs; the rest (LFOs, mod matrix, ...) are NRPN only. On the Mopho
// set Global > MIDI Param Receive to CC or All.
static const std::vector<CCOption> kMophoCCOptions = {
    // Performance
    {  1, "Mod Wheel"      }, {  7, "Volume"         },
    { 14, "BPM"            }, { 15, "Clock Divide"   },
    // Oscillators
    { 20, "OSC1 Freq"      }, { 21, "OSC1 Fine"      }, { 22, "OSC1 Shape"     }, { 23, "Glide 1"        },
    { 24, "OSC2 Freq"      }, { 25, "OSC2 Fine"      }, { 26, "OSC2 Shape"     }, { 27, "Glide 2"        },
    // Mixer
    { 28, "Osc Mix"        }, { 29, "Noise Level"    }, { 30, "Sub Osc 1"      }, { 31, "Sub Osc 2"      },
    // Filter
    {102, "Cutoff"         }, {103, "Resonance"      }, {104, "Filt Key Amt"   }, {105, "Filt Audio Mod" },
    {106, "Filt Env Amt"   }, {107, "Filt Env Vel"   },
    // Filter envelope
    {108, "FE Delay"       }, {109, "FE Attack"      }, {110, "FE Decay"       }, {111, "FE Sustain"     },
    {112, "FE Release"     },
    // Amp
    {113, "VCA Level"      }, {115, "Amp Env Amt"    }, {116, "Amp Velocity"   },
    // Amp envelope
    {117, "AE Delay"       }, {118, "AE Attack"      }, {119, "AE Decay"       }, { 75, "AE Sustain"     },
    { 76, "AE Release"     },
    // Envelope 3
    { 85, "E3 Dest"        }, { 86, "E3 Amount"      }, { 87, "E3 Velocity"    }, { 88, "E3 Delay"       },
    { 89, "E3 Attack"      }, { 90, "E3 Decay"       }, { 77, "E3 Sustain"     }, { 78, "E3 Release"     },
};

// Default 32: every sound CC except volume, VCA level, tempo, the glides, the
// velocity amounts and the envelope 3 destination.
static const std::vector<uint8_t> kMophoDefaultCCs = {
    20, 21, 22, 24, 25, 26,            // OSC freq, fine, shape
    28, 29, 30, 31,                    // Mixer
    102, 103, 104, 105, 106,           // Filter
    108, 109, 110, 111, 112,           // Filter envelope
    115,                               // Amp env amount
    117, 118, 119, 75, 76,             // Amp envelope
    86, 88, 89, 90, 77, 78             // Envelope 3
};

// Focus group bitmasks
static constexpr uint32_t kMophoFocusOSC  = (1u << 0);
static constexpr uint32_t kMophoFocusMIX  = (1u << 1);
static constexpr uint32_t kMophoFocusFILT = (1u << 2);
static constexpr uint32_t kMophoFocusFENV = (1u << 3);
static constexpr uint32_t kMophoFocusAENV = (1u << 4);   // amp envelope and VCA
static constexpr uint32_t kMophoFocusENV3 = (1u << 5);

class MEMLNautModeMopho {
public:
    constexpr static size_t kN_InputParams     = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<32> audioApp;
    std::array<String, TRxSAudioApp<32>::nVoiceSpaces> voiceSpaceList;

    using InterfaceRL_t = InterfaceRL<TRxSAudioApp<32>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    FocusManager<TRxSAudioApp<32>::kN_Params, 6> focusManager;
    uint32_t presentGroupsMask_ = 0;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<32>::kN_Params);
        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("mopho", "Mopho");
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "OSC");
        focusManager.setGroupName(1, "MIX");
        focusManager.setGroupName(2, "FILT");
        focusManager.setGroupName(3, "FENV");
        focusManager.setGroupName(4, "AENV");
        focusManager.setGroupName(5, "ENV3");

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
    }

    String getHelpTitle() { return "DSI Mopho Mode"; }

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
        midi_interf->SetMIDISendChannel(1);
        midi_interf->SetMIDINoteChannel(1);
        midi_interf->SetParamCCNumbers(kMophoDefaultCCs);
        midi_interf->SetNoteCallback([this](bool note_on, uint8_t note, uint8_t velocity) {
            if (note_on)
                midi_interf->sendNoteOn(note, velocity);
            else
                midi_interf->sendNoteOff(note, velocity);
        });
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
            constexpr size_t N = TRxSAudioApp<32>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 7, 60, 60, TFT_WHITE,
            std::vector<String>{"OSC", "MIX", "FILT", "FENV", "AENV", "ENV3", "HOLD"},
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

        auto ccView = std::make_shared<CCSelectView>(TRxSAudioApp<32>::kN_Params, "CC Assign");
        ccView->setOptions(kMophoCCOptions);

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
    static constexpr const char* kCCFile = "/mopho_v2_cc.bin";
    uint32_t lastCCSendMs_ = 0;
    bool held_ = false;

    static uint32_t ccToGroupMask(uint8_t cc) {
        if (cc >= 20 && cc <= 27)                 return kMophoFocusOSC;
        if (cc >= 28 && cc <= 31)                 return kMophoFocusMIX;
        if (cc >= 102 && cc <= 107)               return kMophoFocusFILT;
        if (cc >= 108 && cc <= 112)               return kMophoFocusFENV;
        if ((cc >= 113 && cc <= 119) || cc == 75 || cc == 76) return kMophoFocusAENV;
        if ((cc >= 85 && cc <= 90) || cc == 77 || cc == 78)   return kMophoFocusENV3;
        return 0;  // mod wheel, volume, BPM, clock divide: always active
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
            Serial.printf("Mopho: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
            if (!ccs.empty()) return ccs;
        } else {
            Serial.println("Mopho: no saved CC assignments, using defaults");
        }
        return std::vector<uint8_t>(kMophoDefaultCCs.begin(), kMophoDefaultCCs.end());
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs) {
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(ccs.data(), 1, ccs.size(), f);
            fclose(f);
            Serial.printf("Mopho: saved %u CC assignments to flash\n", (unsigned)ccs.size());
        } else {
            Serial.println("Mopho: failed to open flash for writing");
        }
    }
};
