#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
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

// Waldorf Blofeld MIDI CC targets, from the Blofeld User's Manual (rev 1.1,
// Appendix "Controller Numbers"). Left out: bank select (0/32), sustain and
// sostenuto (64/66), and the channel mode messages (120+), which would change
// program or hang notes rather than shape the sound.
// On the Blofeld set Global > Ctrl Receive = On, and keep Control W..Z off
// these numbers: a CC used by Control W..Z no longer reaches its Sound parameter.
static const std::vector<CCOption> kBlofeldCCOptions = {
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
    { 60, "Noise Level"    }, { 61, "Noise Balance"  }, { 62, "Noise Colour"   },
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

// Default 32 (sorted, so the order matches the CC Assign page and the NN outputs).
static const std::vector<uint8_t> kBlofeldDefaultCCs = {
    16, 20,                          // LFO1/2 Speed
    29, 30, 33, 34, 37, 38, 40, 44,  // OSC detunes, FM, PW, PWM
    52, 54, 56, 58, 60, 62,          // OSC1/2/3, ring mod and noise levels, noise colour
    69, 70, 71, 73,                  // Filter 1 cutoff, resonance, drive, env amount
    80, 81, 84,                      // Filter 2 cutoff, resonance, env amount
    93, 94,                          // FX1/FX2 mix
    95, 96, 97, 100,                 // Filter envelope ADSR
    101, 102, 106                    // Amp envelope attack, decay, release
};

// Home value for a CC before the user edits it: centre for bipolar parameters, open
// enough filters and loud enough oscillators to hear something with knob Y at zero.
static float blofeldDefaultHome(uint8_t cc) {
    switch (cc) {
        case 7: case 52: case 56: case 90:            return 100.f / 127.f;  // levels
        case 58:                                      return 50.f / 127.f;   // OSC3 level
        case 69: case 80:                             return 80.f / 127.f;   // cutoffs
        case 70: case 81:                             return 20.f / 127.f;   // resonances
        case 73: case 84:                             return 80.f / 127.f;   // env amounts, a little positive
        case 95: case 101:                            return 0.f;            // attacks
        case 96: case 102:                            return 50.f / 127.f;   // decays
        case 97: case 103:                            return 64.f / 127.f;   // sustains
        case 100: case 106:                           return 40.f / 127.f;   // releases
        case 16: case 20: case 24:                    return 50.f / 127.f;   // LFO speeds
        case 93: case 94:                             return 20.f / 127.f;   // FX mixes
        case 10: case 29: case 34: case 37: case 41:
        case 44: case 48: case 50: case 53: case 55:
        case 57: case 59: case 61: case 62: case 72:
        case 74: case 75: case 77: case 78: case 83:
        case 85: case 86: case 88: case 89: case 91:
        case 92: case 33: case 40: case 47:           return 64.f / 127.f;   // bipolar / mid
        case 28: case 36: case 43:                    return 64.f / 127.f;   // semitone 0
        case 27: case 35: case 42:                    return 64.f / 127.f;   // octave 8'
        default:                                      return 0.f;
    }
}

// Focus groups
static constexpr size_t kBlofeldNumGroups = 7;
static constexpr uint32_t kBFFocusOSC  = (1u << 0);
static constexpr uint32_t kBFFocusMIX  = (1u << 1);
static constexpr uint32_t kBFFocusFILT = (1u << 2);
static constexpr uint32_t kBFFocusFENV = (1u << 3);
static constexpr uint32_t kBFFocusAENV = (1u << 4);
static constexpr uint32_t kBFFocusMOD  = (1u << 5);   // LFOs, envelopes 3 and 4
static constexpr uint32_t kBFFocusFX   = (1u << 6);   // amp and effects

class MEMLNautModeBlofeld {
public:
    constexpr static size_t kN_InputParams     = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static TRxSAudioApp<32> audioApp;
    std::array<String, TRxSAudioApp<32>::nVoiceSpaces> voiceSpaceList;

    using InterfaceRL_t = InterfaceRL<TRxSAudioApp<32>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    FocusManager<TRxSAudioApp<32>::kN_Params, kBlofeldNumGroups> focusManager;
    uint32_t presentGroupsMask_ = 0;

    // Per-CC home values [0..1] and the values actually sent, indexed by NN output slot
    // (== sorted CC order). Written in the output hook and read in loopCore0, both core 0.
    std::array<float, TRxSAudioApp<32>::kN_Params> homeValues_{};
    std::array<float, TRxSAudioApp<32>::kN_Params> liveValues_{};
    std::shared_ptr<CCSelectView> ccView_;
    std::shared_ptr<BlockSelectView> focusView_;
    std::function<void()> updateActiveDims_;
    // Knob Y (RV_Y1): fades NN modulation in/out around home.
    volatile float fadeAmount_ = 0.f;

    void setupInterface() {
        interface.setup(kN_InputParams, TRxSAudioApp<32>::kN_Params);
        // Knob Y: reward scale -> modulation fade. Must be set before bindInterface().
        interface.setRVY1Override([this](float v) { fadeAmount_ = v; interface.markInputDirty(); });
        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("blofeld", "Blofeld");
        interfacePtr = make_non_owning(interface);

        const char* names[kBlofeldNumGroups] = {"OSC", "MIX", "FILT", "FENV", "AENV", "MOD", "FX"};
        for (size_t i = 0; i < kBlofeldNumGroups; i++)
            focusManager.setGroupName(i, names[i]);

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        // out = home + fade * (nn - home), applied only on the way out to MIDI.
        interface.paramOutputHook = [this](std::span<const float> nn) {
            const float fade = fadeAmount_;
            const size_t n = std::min(nn.size(), liveValues_.size());
            for (size_t i = 0; i < n; i++) {
                float v = homeValues_[i] + fade * (nn[i] - homeValues_[i]);
                liveValues_[i] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
            }
            nLive_ = n;
            ccPending_ = true;
            sendCCsIfDue();
        };
    }

    void syncHomeArray(const std::vector<float>& homes) {
        homeValues_.fill(0.f);
        for (size_t i = 0; i < homes.size() && i < homeValues_.size(); i++)
            homeValues_[i] = homes[i];
    }

    String getHelpTitle() { return "Waldorf Blofeld Mode"; }

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
        midi_interf->SetMIDISendChannel(1);  // set the Blofeld's Global MIDI Channel to 1 (or omni)
        midi_interf->SetMIDINoteChannel(1);
        midi_interf->SetParamCCNumbers(kBlofeldDefaultCCs);
        // The desktop Blofeld has no MIDI out, so a keyboard plugged into the
        // MEMLNaut's MIDI in plays the Blofeld through here.
        midi_interf->SetNoteCallback([this](bool note_on, uint8_t note, uint8_t velocity) {
            if (note_on)
                midi_interf->sendNoteOn(note, velocity);
            else
                midi_interf->sendNoteOff(note, velocity);
        });
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

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, kBlofeldNumGroups, 60, 60, TFT_WHITE,
            std::vector<String>{"OSC", "MIX", "FILT", "FENV", "AENV", "MOD", "FX"},
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
        ccView->setOptions(kBlofeldCCOptions);
        ccView->setShowHome(true);
        ccView->setLiveValues(liveValues_.data());
        ccView_ = ccView;

        std::vector<uint8_t> savedCCs;
        std::vector<float>   savedHomes;
        loadCCAssignments(savedCCs, savedHomes);
        applyMapping(savedCCs, savedHomes);

        // New CC selection: give newly added CCs their default home, then remap.
        ccView->setOnChangeCallback([this, ccView, focusView, updateActiveDims](const std::vector<uint8_t>& ccs) {
            std::vector<float> homes = ccView->getHomeValues();
            for (size_t i = 0; i < ccs.size() && i < homes.size(); i++)
                if (std::find(knownCCs_.begin(), knownCCs_.end(), ccs[i]) == knownCCs_.end())
                    homes[i] = blofeldDefaultHome(ccs[i]);
            ccView->setHomeValues(homes);
            knownCCs_ = ccs;
            midi_interf->SetParamCCNumbers(ccs);
            syncHomeArray(ccView->getHomeValues());
            recomputeFocusGroups(ccs, focusView, updateActiveDims);
            interface.markInputDirty();
        });

        ccView->setOnHomeChangeCallback([this, ccView]() {
            syncHomeArray(ccView->getHomeValues());
            interface.markInputDirty();
        });

        // Write flash only on leaving the page: flash writes block MIDI and audio.
        ccView->setOnSaveCallback([this, ccView]() {
            saveCCAssignments(ccView->getSelectedCCs(), ccView->getHomeValues());
        });

        // Knob G sets the home of the CC under the cursor while the CC page has control.
        // This mode makes no sound of its own, so the volume knob is free.
        MEMLNaut::Instance()->setRVGain1Callback([ccView](float v) {
            if (ccView->isFocused()) ccView->setHomeForCursor(v);
        }, 0);

        // Each saved model carries its own mapping; the flash file is the boot default.
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

    // Apply a mapping (assignments + homes, in matching order) to the view and live state.
    void applyMapping(const std::vector<uint8_t>& ccs, const std::vector<float>& homes) {
        if (!ccView_) return;
        // The view sorts its CCs; pair each home with its CC first so they stay together.
        std::vector<std::pair<uint8_t, float>> pairs;
        for (size_t i = 0; i < ccs.size(); i++)
            pairs.emplace_back(ccs[i], i < homes.size() ? homes[i] : blofeldDefaultHome(ccs[i]));
        std::sort(pairs.begin(), pairs.end());
        std::vector<uint8_t> sortedCCs;
        std::vector<float>   sortedHomes;
        for (auto& p : pairs) { sortedCCs.push_back(p.first); sortedHomes.push_back(p.second); }

        ccView_->setSelectedCCs(sortedCCs);
        ccView_->setHomeValues(sortedHomes);
        knownCCs_ = ccView_->getSelectedCCs();
        if (midi_interf) midi_interf->SetParamCCNumbers(knownCCs_);
        syncHomeArray(ccView_->getHomeValues());
        if (focusView_ && updateActiveDims_) recomputeFocusGroups(knownCCs_, focusView_, updateActiveDims_);
        ccView_->resetDirty();
        ccView_->redraw();
        interface.markInputDirty();
    }

    inline void processAnalysisParams() {}
    void analyse(stereosample_t) {}
    AudioDriver::codec_config_t getCodecConfig() { return audioApp.GetDriverConfig(); }
    void loopCore0() {
        sendCCsIfDue();   // flush the last values held back by the rate limit
        if (ccView_) ccView_->refreshLiveColumn();
    }

private:
    static constexpr const char* kCCFile = "/blofeld_cc.bin";
    // 32 CCs take about 30 ms at MIDI's 31250 baud, so send at most this often.
    static constexpr uint32_t kCCIntervalMs = 30;
    uint32_t lastCCSendMs_ = 0;
    bool ccPending_ = false;
    size_t nLive_ = 0;
    std::vector<uint8_t> knownCCs_;

    void sendCCsIfDue() {
        if (!ccPending_ || !midi_interf) return;
        uint32_t now = millis();
        if (now - lastCCSendMs_ < kCCIntervalMs) return;
        midi_interf->SendParamsAsMIDICC(std::span<const float>(liveValues_.data(), nLive_));
        lastCCSendMs_ = now;
        ccPending_ = false;
    }

    static uint32_t ccToGroupMask(uint8_t cc) {
        if (cc >= 15 && cc <= 26)   return kBFFocusMOD;
        if (cc >= 27 && cc <= 49)   return kBFFocusOSC;
        if (cc >= 52 && cc <= 62)   return kBFFocusMIX;
        if (cc >= 67 && cc <= 89)   return kBFFocusFILT;
        if (cc >= 90 && cc <= 94)   return kBFFocusFX;
        if (cc >= 95 && cc <= 100)  return kBFFocusFENV;
        if (cc >= 101 && cc <= 106) return kBFFocusAENV;
        if (cc >= 107 && cc <= 118) return kBFFocusMOD;
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
            for (size_t i = 0; i < kBlofeldNumGroups; i++) {
                if (((currentSel >> i) & 1u) && !((validSel >> i) & 1u))
                    focusView->setAltState(i, false);
            }
        }
        updateActiveDims();
    }

    // Mapping blob: [0xA5 magic][uint8 N][N CC bytes][N home bytes 0..127], as in TR-8S mode.
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
                Serial.printf("Blofeld: loaded %u CC assignments from flash\n", (unsigned)ccs.size());
                return;
            }
        } else {
            Serial.println("Blofeld: no saved CC assignments, using defaults");
        }
        ccs.assign(kBlofeldDefaultCCs.begin(), kBlofeldDefaultCCs.end());
        for (uint8_t cc : ccs) homes.push_back(blofeldDefaultHome(cc));
    }

    void saveCCAssignments(const std::vector<uint8_t>& ccs, const std::vector<float>& homes) {
        std::vector<uint8_t> blob = serializeMapping(ccs, homes);
        FILE* f = fopen(kCCFile, "wb");
        if (f) {
            fwrite(blob.data(), 1, blob.size(), f);
            fclose(f);
            Serial.printf("Blofeld: saved %u CC assignments to flash\n", (unsigned)(blob.size() >= 2 ? blob[1] : 0));
        } else {
            Serial.println("Blofeld: failed to open flash for writing");
        }
    }
};
