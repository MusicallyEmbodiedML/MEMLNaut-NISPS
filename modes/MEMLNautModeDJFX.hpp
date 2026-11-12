#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/DJFXAudioApp.hpp"
#include "./DJRLView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/display/VUMeterView.hpp"
#include "../VUMeter.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../MachineListeningMixin.hpp"


// One focus group per effect — 10 groups, matching the param layout in DJFXAudioApp.
static constexpr size_t kDJFX_NGroups = 10;

class MEMLNautModeDJFX {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;
    using InterfaceRL_t = InterfaceRL<DJFXAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;
    MachineListeningMixin mlMixin;

    inline static DJFXAudioApp<> audioAppDJFX;
    std::shared_ptr<MIDIInOut> midi_interf;

    FocusManager<DJFXAudioApp<>::kN_Params, kDJFX_NGroups> focusManager;


    void setupInterface() {
        auto djView = std::make_shared<DJRLView>("Neural Net", DJFXAudioApp<>::kN_Params);
        djView->setBPMCallback([this](float bpm) {
            audioAppDJFX.setBPMQueued(bpm);
        });
        interface.nnOutputsGraphView = djView;

        interface.setup(kN_InputParams, DJFXAudioApp<>::kN_Params);
        interface.setRVY1Override([this](float value) {
            if (audioAppDJFX.djMode_)
                audioAppDJFX.setRVY1Queued(value);    // DJ Mode: RVY1 = DJ filter sweep
            else
                audioAppDJFX.setWetDryQueued(value);  // Manual: RVY1 = wet/dry mix
        });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("djfx", "LiveFX");  // "djfx" kept = SD preset dir (back-compat)

        interface.setExtraSaveCallback([this]() -> std::vector<uint8_t> {
            std::vector<uint8_t> data(4);
            const uint32_t mask = audioAppDJFX.enableMask_;
            memcpy(data.data(), &mask, 4);
            return data;
        });
        interface.setExtraLoadCallback([this](const uint8_t* data, uint16_t size, uint16_t) {
            if (size >= 4) {
                uint32_t mask;
                memcpy(&mask, data, 4);
                audioAppDJFX.enableMask_ = mask;
                syncEnableView();
            }
        });

        interfacePtr = make_non_owning(interface);

        // Name each focus group.
        focusManager.setGroupName(0, "Gr1");
        focusManager.setGroupName(1, "Gr2");
        focusManager.setGroupName(2, "Dly");
        focusManager.setGroupName(3, "Flgr");
        focusManager.setGroupName(4, "Chr");
        focusManager.setGroupName(5, "AP");
        focusManager.setGroupName(6, "Dsp");
        focusManager.setGroupName(7, "Stt");
        focusManager.setGroupName(8, "Rng");
        focusManager.setGroupName(9, "Rev");

        // Assign each of the 57 params to its group (matching ProcessParams layout).
        std::array<uint32_t, DJFXAudioApp<>::kN_Params> masks = {
            // 0-7: Grain Delay 1
            1u<<0, 1u<<0, 1u<<0, 1u<<0, 1u<<0, 1u<<0, 1u<<0, 1u<<0,
            // 8-15: Grain Delay 2
            1u<<1, 1u<<1, 1u<<1, 1u<<1, 1u<<1, 1u<<1, 1u<<1, 1u<<1,
            // 16-17: Grain crossfade and mix (Gr1 group)
            1u<<0, 1u<<0,
            // 18-20: Dynamic Delay
            1u<<2, 1u<<2, 1u<<2,
            // 21-25: Flanger
            1u<<3, 1u<<3, 1u<<3, 1u<<3, 1u<<3,
            // 26-30: Chorus
            1u<<4, 1u<<4, 1u<<4, 1u<<4, 1u<<4,
            // 31-35: Allpass + LFO
            1u<<5, 1u<<5, 1u<<5, 1u<<5, 1u<<5,
            // 36-37: Downsampler
            1u<<6, 1u<<6,
            // 38-40: Stutter Gate
            1u<<7, 1u<<7, 1u<<7,
            // 41-42: Ring Mod
            1u<<8, 1u<<8,
            // 43-45: Delay feedback bandpass (freq, Q, mix)
            1u<<2, 1u<<2, 1u<<2,
            // 46-56: Reverb
            1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9, 1u<<9
        };
        focusManager.setParamGroups(masks);

        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

    }

    std::shared_ptr<BlockSelectView> enableView_;

    // Reflect the effect enable mask on the FX Enable tiles.
    void syncEnableView() {
        if (!enableView_) return;
        const uint32_t mask = audioAppDJFX.enableMask_;
        for (size_t i = 0; i < 10; i++) enableView_->setAltColour(i, (mask >> i) & 1u);
    }

    String getHelpTitle() {
        return "LiveFX Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppDJFX.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
    }

    void addViews() {
        // Lambda to mark active NN dimensions based on which groups are focused.
        auto updateActiveDims = [this]() {
            const uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = DJFXAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus view — 10 toggle buttons, one per effect group.
        // 10 buttons => 2 rows x 5 cols. Maximised for 320x240: width 5*53+50=315<=319,
        // height 2*76+20=172 leaves room for the message line.
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, (int)kDJFX_NGroups, 53, 76, TFT_WHITE,
            std::vector<String>{"Gr1","Gr2","Dly","Flgr","Chr","AP","Dsp","Stt","Rng","Rev"},
            TFT_GREENYELLOW, 2 /* fontNum */);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            const size_t groupIdx = id - 1;
            const uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        interface.addInputSourceView(false);  // no MIDI CC Out screen for this mode

        // FX Enable — one toggle per effect, MEMLCelium style (highlighted = on).
        enableView_ = std::make_shared<BlockSelectView>(
            "FX Enable", TFT_DARKGREEN, 10, 80, 70, TFT_WHITE,
            std::vector<String>{ "Allpass", "Flanger", "Chorus", "Grain 1", "Grain 2", "Delay", "Downsamp", "Stutter", "Ring Mod", "Reverb" }, TFT_GREEN, 2);
        syncEnableView();
        enableView_->SetOnSelectCallback([this](size_t id) {
            audioAppDJFX.enableMask_ ^= (1u << (id - 1));
            syncEnableView();
        });
        auto enableView = enableView_;
        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, enableView);

        // VU meters (In L/R, Out L/R) — sits right after FX Enable. The view arms the
        // audio-core measurement only while it is on screen.
        auto vuView = std::make_shared<VUMeterView>(
            "VU", std::vector<String>{ "In L", "In R", "Out L", "Out R" },
            VUMeter::levels,
            [](bool a) { VUMeter::active = a; });
        MEMLNaut::Instance()->disp->InsertViewAfter(enableView, vuView);

        // Options screen — single toggle for DJ Mode. On (highlighted): joystick drives
        // wet/dry, RVY1 = DJ filter. Off: RVY1 drives wet/dry, no filtering.
        auto optionsView = std::make_shared<BlockSelectView>(
            "Options", TFT_YELLOW, 1, 95, 70, TFT_BLACK,
            std::vector<String>{ "DJ Mode" },
            TFT_BLUE, 2);
        optionsView->SetOnSelectCallback([this, optionsView](size_t id) {
            audioAppDJFX.djMode_ = !audioAppDJFX.djMode_;
            optionsView->toggleAlt(id - 1);
        });
        optionsView->setAltColour(0, audioAppDJFX.djMode_);  // reflect initial state
        MEMLNaut::Instance()->disp->AddView(optionsView);
    };

    void setupAudio(float sample_rate) {
        audioAppDJFX.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioAppDJFX.loop();
        const auto src = interface.getInputSource();
        // Joystick drives wet/dry only in DJ Mode. When off, RVY1 controls wet/dry
        // manually (see setRVY1Override) and the stick is exploration-only.
        if (audioAppDJFX.djMode_ &&
            (src == InterfaceRLBase::INPUT_SOURCE::JOYSTICK_3D ||
             src == InterfaceRLBase::INPUT_SOURCE::JOYSTICK_4D)) {
            // Wet/dry envelope follows how far the stick is pushed from centre:
            // dry at rest, fading to 100% wet near full deflection. Only the two
            // spring-centred axes (X, Y) drive it — JOY_Z rests off-centre on this
            // hardware and would otherwise pin the mix fully wet. All axes still
            // feed the NN exploration; only the wet amount is decoupled from Z.
            static float kWetDeadzone = 0.05f;  // SRAM: keep solidly dry at rest
            static float kWetGain     = 2.3f;   // hit 100% wet just before the rail
            const auto& inputs = interface.getControlInput();
            const size_t n = inputs.size() < 2 ? inputs.size() : 2;
            float maxDev = 0.f;
            for (size_t i = 0; i < n; ++i) {
                const float d = fabsf(inputs[i] - 0.5f);
                if (d > maxDev) maxDev = d;
            }
            const float dev = fmaxf(0.f, maxDev - kWetDeadzone);
            audioAppDJFX.setWetDryQueued(fminf(dev * kWetGain, 1.f));
        }
    }

    __force_inline void analyse(stereosample_t x) { mlMixin.analyse(x); }

    __force_inline void processAnalysisParams() { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioAppDJFX.GetDriverConfig(); }

    void loopCore0() {}

};
