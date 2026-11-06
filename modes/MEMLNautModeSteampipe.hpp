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

// Erica Synths x 112dB Steampipe MIDI CC options.
//
// The rows are the Steampipe's own MIDI CC MAP list (Settings > MIDI CC MAP), in
// order, as stored in its firmware v1.1 (Envelope, Steam, Pipe, filters, Glide,
// Reverb, Volume; the five LFO rows are left out). CC = 70 + row. The manual
// confirms rows 0-4 (CC 70-74) only; the factory numbers of the later rows are not
// published, so check them on the Steampipe's MIDI CC MAP page and set any that
// differ. The map is editable on the device.
static const std::vector<CCOption> kSteampipeCCOptions = {
    // Envelope
    { 70, "Env Attack"     }, { 71, "Env Decay"      }, { 72, "Env Sustain"    }, { 73, "Env Release"    },
    { 74, "Env Scaling"    }, { 75, "Env Velocity"   }, { 76, "Legato"         },
    // Steam / generator
    { 77, "Stm DC"         }, { 78, "Stm Cutoff"     }, { 79, "Stm Resonance"  }, { 80, "Stm KTrack"     },
    { 81, "Stm Envelope"   }, { 82, "Stm Gain"       }, { 83, "Stm 1-Pole"     },
    // Pipe / resonator
    { 84, "Pipe Push"      }, { 85, "Pipe Decay"     }, { 86, "Pipe Release"   }, { 87, "Pipe SoftHard"  },
    { 88, "Pipe Symmetry"  }, { 89, "Pipe Tune"      }, { 90, "Pipe Fine"      }, { 91, "Pipe Harmonics" },
    { 92, "Pipe SplitPnt"  }, { 93, "Pipe Split"     }, { 94, "Pipe Polarity"  },
    { 95, "HighPass"       }, { 96, "LowPass"        },
    // Glide
    { 97, "Detune"         }, { 98, "Glide"          }, { 99, "Glide Time"     },
    // Reverb
    {100, "Rvb Bass"       }, {101, "Rvb Treble"     }, {102, "Rvb Size"       }, {103, "Rvb Spin"       },
    {104, "Rvb Mix"        }, {105, "Rvb Spread"     },
    // Output
    {106, "Volume"         },
};

// Default 16 (this mode has 16 outputs): one spread across the sections.
static const std::vector<uint8_t> kSteampipeDefaultCCs = {
    70, 71, 73,                  // Env attack, decay, release
    78, 79, 81, 82,              // Steam cutoff, resonance, envelope, gain
    84, 85, 86, 91,              // Pipe push, decay, release, harmonics
    102, 103, 104, 105,          // Reverb size, spin, mix, spread
    96                           // LowPass
};

// Focus group bitmasks — one per section
static constexpr uint32_t kSteampipeFocusENV   = (1u << 0);  // Envelope
static constexpr uint32_t kSteampipeFocusSTEAM = (1u << 1);  // Steam / generator
static constexpr uint32_t kSteampipeFocusPIPE  = (1u << 2);  // Pipe, filters, glide
static constexpr uint32_t kSteampipeFocusREVERB= (1u << 3);  // Reverb

class MEMLNautModeSteampipe {
public:
    constexpr static size_t kN_InputParams     = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<16> audioApp;
    std::array<String, TRxSAudioApp<16>::nVoiceSpaces> voiceSpaceList;

    using InterfaceRL_t = InterfaceRL<TRxSAudioApp<16>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    FocusManager<TRxSAudioApp<16>::kN_Params, 4> focusManager;
    uint32_t presentGroupsMask_ = 0;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<16>::kN_Params);
        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("steampipe", "Steampipe");
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "ENV");
        focusManager.setGroupName(1, "STEAM");
        focusManager.setGroupName(2, "PIPE");
        focusManager.setGroupName(3, "REVERB");

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
    }

    String getHelpTitle() { return "Steampipe Mode"; }

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
        midi_interf->SetMIDISendChannel(1);   // Steampipe default channel
        midi_interf->SetMIDINoteChannel(1);
        midi_interf->SetParamCCNumbers(kSteampipeDefaultCCs);
        // Pass MIDI notes through to the Steampipe (8-voice polyphonic synth)
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
            constexpr size_t N = TRxSAudioApp<16>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 5, 60, 60, TFT_WHITE,
            std::vector<String>{"ENV", "STEAM", "PIPE", "REVERB", "HOLD"},
            TFT_GREENYELLOW, 2);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            if (id == 5) {
                held_ = !held_;
                focusView->toggleAlt(4);
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
        ccView->setOptions(kSteampipeCCOptions);

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
    static constexpr const char* kCCFile = "/steampipe_v2_cc.bin";
    uint32_t lastCCSendMs_ = 0;
    bool held_ = false;

    static uint32_t ccToGroupMask(uint8_t cc) {
        if (cc >= 70 && cc <= 76)  return kSteampipeFocusENV;
        if (cc >= 77 && cc <= 83)  return kSteampipeFocusSTEAM;
        if (cc >= 84 && cc <= 99)  return kSteampipeFocusPIPE;
        if (cc >= 100 && cc <= 105) return kSteampipeFocusREVERB;
        return 0;  // Volume: always active
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
            for (size_t i = 0; i < 4; i++) {
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
            Serial.printf("Steampipe: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
            if (!ccs.empty()) return ccs;
        } else {
            Serial.println("Steampipe: no saved CC assignments, using defaults");
        }
        return std::vector<uint8_t>(kSteampipeDefaultCCs.begin(), kSteampipeDefaultCCs.end());
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs) {
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(ccs.data(), 1, ccs.size(), f);
            fclose(f);
            Serial.printf("Steampipe: saved %u CC assignments to flash\n", (unsigned)ccs.size());
        } else {
            Serial.println("Steampipe: failed to open flash for writing");
        }
    }
};
