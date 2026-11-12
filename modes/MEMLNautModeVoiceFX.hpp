#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/VoiceFXAudioApp.hpp"
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


// VoiceFX — a clone of LiveFX (MEMLNautModeDJFX) without the downsampler, stutter
// and ring mod, and without DJ Mode: knob Y sets wet/dry, and the joystick only
// explores (its distance from centre no longer drives wet/dry).

// One focus group per effect — 9 groups, matching the param layout in VoiceFXAudioApp.
static constexpr size_t kVoiceFX_NGroups = 9;

class MEMLNautModeVoiceFX {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;
    using InterfaceRL_t = InterfaceRL<VoiceFXAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;
    MachineListeningMixin mlMixin;

    inline static VoiceFXAudioApp<> audioAppVoiceFX;
    std::shared_ptr<MIDIInOut> midi_interf;

    FocusManager<VoiceFXAudioApp<>::kN_Params, kVoiceFX_NGroups> focusManager;


    void setupInterface() {
        auto djView = std::make_shared<DJRLView>("Neural Net", VoiceFXAudioApp<>::kN_Params);
        djView->setBPMCallback([this](float bpm) {
            audioAppVoiceFX.setBPMQueued(bpm);
        });
        interface.nnOutputsGraphView = djView;

        interface.setup(kN_InputParams, VoiceFXAudioApp<>::kN_Params, false /* no message screen */);
        interface.setRVY1Override([this](float value) {
            audioAppVoiceFX.setWetDryQueued(value);  // knob Y = wet/dry mix
        });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("voicefx", "VoiceFX");

        interface.setExtraSaveCallback([this]() -> std::vector<uint8_t> {
            std::vector<uint8_t> data(4);
            const uint32_t mask = audioAppVoiceFX.enableMask_;
            memcpy(data.data(), &mask, 4);
            return data;
        });
        interface.setExtraLoadCallback([this](const uint8_t* data, uint16_t size, uint16_t) {
            if (size >= 4) {
                uint32_t mask;
                memcpy(&mask, data, 4);
                audioAppVoiceFX.enableMask_ = mask & VoiceFXAudioApp<>::kFX_All;
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
        focusManager.setGroupName(6, "Rev");
        focusManager.setGroupName(7, "Comp");
        focusManager.setGroupName(8, "EQ");

        // Assign each of the 59 params to its group (matching ProcessParams layout).
        std::array<uint32_t, VoiceFXAudioApp<>::kN_Params> masks = {
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
            // 36-38: Delay feedback bandpass (freq, Q, mix)
            1u<<2, 1u<<2, 1u<<2,
            // 39-49: Reverb
            1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6, 1u<<6,
            // 50-54: Compressor
            1u<<7, 1u<<7, 1u<<7, 1u<<7, 1u<<7,
            // 55-58: EQ
            1u<<8, 1u<<8, 1u<<8, 1u<<8
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
        const uint32_t mask = audioAppVoiceFX.enableMask_;
        for (size_t i = 0; i < VoiceFXAudioApp<>::kNFX; i++) enableView_->setAltColour(i, (mask >> i) & 1u);
    }

    String getHelpTitle() {
        return "VoiceFX Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppVoiceFX.Process(x);
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
            constexpr size_t N = VoiceFXAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus view — 9 toggle buttons, one per effect group (2 rows).
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, (int)kVoiceFX_NGroups, 56, 76, TFT_WHITE,
            std::vector<String>{"Gr1","Gr2","Dly","Flgr","Chr","AP","Rev","Comp","EQ"},
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
            "FX Enable", TFT_DARKGREEN, VoiceFXAudioApp<>::kNFX, 56, 70, TFT_WHITE,
            std::vector<String>{ "Allpass", "Flanger", "Chorus", "Grain 1", "Grain 2", "Delay", "Reverb", "Comp", "EQ" }, TFT_GREEN, 2);
        syncEnableView();
        enableView_->SetOnSelectCallback([this](size_t id) {
            audioAppVoiceFX.enableMask_ ^= (1u << (id - 1));
            syncEnableView();
        });
        auto enableView = enableView_;
        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, enableView);

    };

    void setupAudio(float sample_rate) {
        audioAppVoiceFX.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioAppVoiceFX.loop();
    }

    __force_inline void analyse(stereosample_t x) { mlMixin.analyse(x); }

    __force_inline void processAnalysisParams() { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioAppVoiceFX.GetDriverConfig(); }

    void loopCore0() {}

};
