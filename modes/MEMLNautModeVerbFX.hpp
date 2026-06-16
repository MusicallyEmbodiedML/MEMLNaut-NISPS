#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/VerbFXAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../src/memllib/audio/FocusManager.hpp"



class MEMLNautModeVerbFX {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    // Focus groups: Verb (comb+allpass tail), Filt (filterbank), Delay, Mix (routing).
    static constexpr size_t kVerbFX_NGroups = 4;

    using InterfaceRL_t = InterfaceRL<VerbFXAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    FocusManager<VerbFXAudioApp<>::kN_Params, kVerbFX_NGroups> focusManager;

    inline static VerbFXAudioApp<> audioAppVerbFX;
    std::array<String, VerbFXAudioApp<>::nVoiceSpaces> voiceSpaceList;
    std::shared_ptr<MIDIInOut> midi_interf;
    std::shared_ptr<BlockSelectView> enableView;


    void setupInterface() {
        interface.setup(kN_InputParams, VerbFXAudioApp<>::kN_Params, false /* no message screen */);
        interface.setRVX1Override([this](float value) {
            audioAppVerbFX.setWetDryQueued(value);
        });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("verbfx", "VerbFX");

        // Focus groups. Param indices are consistent across all voice spaces (see
        // voicespaces/VerbFX/*.hpp): 0 = filterbank<->delay xfade, 1-16 = comb fb/cutoff,
        // 17-20 = allpass fb, 21-28 = filterbank freqs, 29-36 = filterbank res,
        // 37-42 = delay times/feedback, 43 = verb-vs-delay, 44 = delay->verb, 45-46 = morph/blend.
        focusManager.setGroupName(0, "Verb");   // comb + allpass reverb tail
        focusManager.setGroupName(1, "Filt");   // filterbank freqs + res
        focusManager.setGroupName(2, "Delay");  // delay times + feedback
        focusManager.setGroupName(3, "Mix");    // routing / wet balance

        constexpr uint32_t kVerb  = 1u << 0;
        constexpr uint32_t kFilt  = 1u << 1;
        constexpr uint32_t kDelay = 1u << 2;
        constexpr uint32_t kMix   = 1u << 3;
        std::array<uint32_t, VerbFXAudioApp<>::kN_Params> masks = {};
        masks[0] = kMix;                                        // filterbank<->delay xfade
        for (size_t i = 1; i <= 20; ++i)  masks[i] = kVerb;     // comb fb/cutoff + allpass fb
        for (size_t i = 21; i <= 36; ++i) masks[i] = kFilt;     // filterbank freqs + res
        for (size_t i = 37; i <= 42; ++i) masks[i] = kDelay;    // delay times + feedback
        masks[43] = kMix; masks[44] = kMix;                     // verb-vs-delay, delay->verb
        masks[45] = kMix; masks[46] = kMix;                     // delay morph + blend
        focusManager.setParamGroups(masks);

        // Joystick mode: latch params from non-focused groups (matches MEMLCelium).
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "VerbFX Mode";
    }
    
    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppVerbFX.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
    }

    void addViews() {
        enableView = std::make_shared<BlockSelectView>("FX Enable", TFT_YELLOW, 6, 80, 70, TFT_BLACK,
            std::vector<String>{ "FilterBnk", "Reverb", "ShortDly", "MedDly", "LongDly", "Dly->Verb" },
            TFT_BLUE, 2);
        enableView->SetOnSelectCallback([this](size_t id) {
            enableView->toggleAlt(id - 1);
            queue_t& q = audioAppVerbFX.controlMessageQueue;
            switch(id) {
                case 1: { auto msg = VerbFXAudioApp<>::controlMessages::MSG_ENABLE_FILTERBANK;    queue_try_add(&q, &msg); } break;
                case 2: { auto msg = VerbFXAudioApp<>::controlMessages::MSG_ENABLE_REVERB;        queue_try_add(&q, &msg); } break;
                case 3: { auto msg = VerbFXAudioApp<>::controlMessages::MSG_ENABLE_SHORT_DELAY;   queue_try_add(&q, &msg); } break;
                case 4: { auto msg = VerbFXAudioApp<>::controlMessages::MSG_ENABLE_MEDIUM_DELAY;  queue_try_add(&q, &msg); } break;
                case 5: { auto msg = VerbFXAudioApp<>::controlMessages::MSG_ENABLE_LONG_DELAY;    queue_try_add(&q, &msg); } break;
                case 6: { auto msg = VerbFXAudioApp<>::controlMessages::MSG_ENABLE_DELAY_TO_REVERB; queue_try_add(&q, &msg); } break;
            }
        });
        // (enableView is inserted after the Focus screen below.)

        // Mark which NN output dims are live based on the focused groups.
        auto updateActiveDims = [this]() {
            const uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = VerbFXAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus screen — one toggle per group. 4 buttons => single row of 4
        // (BlockSelectView uses 2 rows only when nButtons > 4), so size them to
        // span the 320px screen: 10 + 4*68 + 3*10 gap = 312px.
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, (int)kVerbFX_NGroups, 68, 76, TFT_WHITE,
            std::vector<String>{ "Verb", "Filt", "Delay", "Mix" },
            TFT_GREENYELLOW, 2 /* fontNum */);
        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            const size_t groupIdx = id - 1;
            const uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        // FX Enable comes right after Focus in the view order.
        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, enableView);

        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
        MEMLNaut::Instance()->disp->InsertViewAfter(enableView, voiceSpaceSelectView);
        voiceSpaceSelectView->setOptions(voiceSpaceList);
        voiceSpaceSelectView->setNewVoiceCallback(
            [this](size_t idx) {
                audioAppVerbFX.setVoiceSpace(idx);
            });
        interface.addInputSourceView(false);  // no MIDI CC Out screen for this mode
    };

    void setupAudio(float sample_rate) {
        audioAppVerbFX.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppVerbFX.getVoiceSpaceNames();
    }

    __force_inline void loop() {
        audioAppVerbFX.loop();
    }

    __force_inline void analyse(stereosample_t x) {}

    __force_inline void processAnalysisParams() {}
    
    AudioDriver::codec_config_t getCodecConfig() { return audioAppVerbFX.GetDriverConfig(); }

    void loopCore0() {}

};
