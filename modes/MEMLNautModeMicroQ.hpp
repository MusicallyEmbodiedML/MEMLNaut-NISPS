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

// Waldorf microQ MIDI CC options, from the microQ User's Manual, Appendix
// "Controller Numbers". Left out: bank select (0/32), sustain and sostenuto
// (64/66) and 120+. Set Global > Controller Recv to on (W..Z off these CCs).
static const std::vector<CCOption> kMicroQCCOptions = {
    // Performance
    {  1, "Mod Wheel"      }, {  2, "Breath"         }, {  4, "Foot Ctrl"      },
    {  5, "Glide Rate"     }, {  7, "Volume"         }, { 10, "Pan"            },
    { 50, "Pitch Mod"      }, { 51, "Glide Mode"     }, { 65, "Glide Active"   },
    // Arpeggiator
    { 12, "Arp Range"      }, { 13, "Arp Length"     }, { 14, "Arp Active"     },
    // LFOs
    { 15, "LFO1 Shape"     }, { 16, "LFO1 Speed"     }, { 17, "LFO1 Sync"      }, { 18, "LFO1 Delay"     },
    { 19, "LFO2 Shape"     }, { 20, "LFO2 Speed"     }, { 21, "LFO2 Sync"      }, { 22, "LFO2 Delay"     },
    { 23, "LFO3 Shape"     }, { 24, "LFO3 Speed"     }, { 25, "LFO3 Sync"      }, { 26, "LFO3 Delay"     },
    // Oscillators
    { 27, "OSC1 Octave"    }, { 28, "OSC1 Semi"      }, { 29, "OSC1 Detune"    }, { 30, "OSC1 FM"        },
    { 31, "OSC1 Shape"     }, { 33, "OSC1 PW"        }, { 34, "OSC1 PWM"       },
    { 35, "OSC2 Octave"    }, { 36, "OSC2 Semi"      }, { 37, "OSC2 Detune"    }, { 38, "OSC2 FM"        },
    { 39, "OSC2 Shape"     }, { 40, "OSC2 PW"        }, { 41, "OSC2 PWM"       },
    { 42, "OSC3 Octave"    }, { 43, "OSC3 Semi"      }, { 44, "OSC3 Detune"    }, { 45, "OSC3 FM"        },
    { 46, "OSC3 Shape"     }, { 47, "OSC3 PW"        }, { 48, "OSC3 PWM"       }, { 49, "OSC Sync"       },
    // Mixer
    { 52, "OSC1 Level"     }, { 53, "OSC1 Balance"   }, { 54, "RingMod Level"  }, { 55, "RingMod Bal"    },
    { 56, "OSC2 Level"     }, { 57, "OSC2 Balance"   }, { 58, "OSC3 Level"     }, { 59, "OSC3 Balance"   },
    { 60, "N/E Level"      }, { 61, "N/E Balance"    },
    // Filters
    { 67, "Filter Routing" },
    { 68, "F1 Type"        }, { 69, "F1 Cutoff"      }, { 70, "F1 Resonance"   }, { 71, "F1 Drive"       },
    { 72, "F1 Keytrack"    }, { 73, "F1 Env Amt"     }, { 74, "F1 Env Velo"    }, { 75, "F1 CutoffMod"   },
    { 76, "F1 FM"          }, { 77, "F1 Pan"         }, { 78, "F1 PanMod"      },
    { 79, "F2 Type"        }, { 80, "F2 Cutoff"      }, { 81, "F2 Resonance"   }, { 82, "F2 Drive"       },
    { 83, "F2 Keytrack"    }, { 84, "F2 Env Amt"     }, { 85, "F2 Env Velo"    }, { 86, "F2 CutoffMod"   },
    { 87, "F2 FM"          }, { 88, "F2 Pan"         }, { 89, "F2 PanMod"      },
    // Amp and effects
    { 90, "Amp Volume"     }, { 91, "Amp Velocity"   }, { 92, "Amp Mod"        },
    { 93, "FX1 Mix"        }, { 94, "FX2 Mix"        },
    // Filter envelope
    { 95, "FE Attack"      }, { 96, "FE Decay"       }, { 97, "FE Sustain"     },
    { 98, "FE Decay2"      }, { 99, "FE Sustain2"    }, {100, "FE Release"     },
    // Amp envelope
    {101, "AE Attack"      }, {102, "AE Decay"       }, {103, "AE Sustain"     },
    {104, "AE Decay2"      }, {105, "AE Sustain2"    }, {106, "AE Release"     },
    // Envelopes 3 and 4
    {107, "E3 Attack"      }, {108, "E3 Decay"       }, {109, "E3 Sustain"     },
    {110, "E3 Decay2"      }, {111, "E3 Sustain2"    }, {112, "E3 Release"     },
    {113, "E4 Attack"      }, {114, "E4 Decay"       }, {115, "E4 Sustain"     },
    {116, "E4 Decay2"      }, {117, "E4 Sustain2"    }, {118, "E4 Release"     },
};

// Default 32: as the Blofeld (same engine), with filter 2 drive in place of the
// Blofeld-only noise colour.
static const std::vector<uint8_t> kMicroQDefaultCCs = {
    16, 20,                          // LFO1/2 Speed
    29, 30, 33, 34, 37, 38, 40, 44,  // OSC detunes, FM, PW, PWM
    52, 54, 56, 58, 60,              // OSC1/2/3, ring mod and N/E levels
    69, 70, 71, 73,                  // Filter 1 cutoff, resonance, drive, env amount
    80, 81, 82, 84,                  // Filter 2 cutoff, resonance, drive, env amount
    93, 94,                          // FX1/FX2 mix
    95, 96, 97, 100,                 // Filter envelope ADSR
    101, 102, 106                    // Amp envelope attack, decay, release
};

// Focus group bitmasks
static constexpr uint32_t kMQFocusOSC    = (1u << 0);
static constexpr uint32_t kMQFocusMIX    = (1u << 1);
static constexpr uint32_t kMQFocusFILTER = (1u << 2);
static constexpr uint32_t kMQFocusENV    = (1u << 3);
static constexpr uint32_t kMQFocusLFO    = (1u << 4);
static constexpr uint32_t kMQFocusFX     = (1u << 5);

class MEMLNautModeMicroQ {
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
        interface.setModeInfo("microq", "microQ");
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "OSC");
        focusManager.setGroupName(1, "MIX");
        focusManager.setGroupName(2, "FILT");
        focusManager.setGroupName(3, "ENV");
        focusManager.setGroupName(4, "LFO");
        focusManager.setGroupName(5, "FX");

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
    }

    String getHelpTitle() { return "Waldorf microQ Mode"; }

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
        midi_interf->SetMIDISendChannel(1);  // microQ default channel
        midi_interf->SetMIDINoteChannel(1);
        midi_interf->SetParamCCNumbers(kMicroQDefaultCCs);
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
            std::vector<String>{"OSC", "MIX", "FILT", "ENV", "LFO", "FX", "HOLD"},
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
        ccView->setOptions(kMicroQCCOptions);

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
    static constexpr const char* kCCFile = "/microq_v2_cc.bin";
    uint32_t lastCCSendMs_ = 0;
    bool held_ = false;

    static uint32_t ccToGroupMask(uint8_t cc) {
        if (cc >= 15 && cc <= 26)   return kMQFocusLFO;
        if (cc >= 27 && cc <= 49)   return kMQFocusOSC;
        if (cc >= 52 && cc <= 61)   return kMQFocusMIX;
        if (cc >= 67 && cc <= 89)   return kMQFocusFILTER;
        if (cc >= 90 && cc <= 94)   return kMQFocusFX;
        if (cc >= 95 && cc <= 118)  return kMQFocusENV;
        return 0;  // performance and arp CCs: always active
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
            Serial.printf("microQ: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
            if (!ccs.empty()) return ccs;
        } else {
            Serial.println("microQ: no saved CC assignments, using defaults");
        }
        return std::vector<uint8_t>(kMicroQDefaultCCs.begin(), kMicroQDefaultCCs.end());
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs) {
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(ccs.data(), 1, ccs.size(), f);
            fclose(f);
            Serial.printf("microQ: saved %u CC assignments to flash\n", (unsigned)ccs.size());
        } else {
            Serial.println("microQ: failed to open flash for writing");
        }
    }
};
