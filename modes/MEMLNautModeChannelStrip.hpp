#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../ChannelStripAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "MEMLNautMode.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../src/memllib/hardware/memlnaut/display/VUMeterView.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../VUMeter.hpp"
#include <memory>
#include <array>

class MEMLNautModeChannelStrip {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    // Focus groups mirror the processing chain / bypass blocks: pre+post gain,
    // input filters, EQ (peaks + shelves), and the compressor.
    static constexpr size_t kChStrip_NGroups = 4;

    ChannelStripAudioApp<> audioAppChannelStrip;
    std::array<String, ChannelStripAudioApp<>::nVoiceSpaces> voiceSpaceList;
    using InterfaceRL_t = InterfaceRL<ChannelStripAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;
    std::shared_ptr<BlockSelectView> bypassView;

    FocusManager<ChannelStripAudioApp<>::kN_Params, kChStrip_NGroups> focusManager;

    void setupInterface() {
        interface.setup(kN_InputParams, ChannelStripAudioApp<>::kN_Params);

        // Repurpose the RVX1 knob (default: reward scaling) as a dry/wet blend.
        // Must be set before bindInterface(), which captures the override.
        interface.setRVX1Override([this](float value) {
            audioAppChannelStrip.setWetDryQueued(value);
        });

        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true); //set 4D joystick
        interface.setModeInfo("chstrip", "ChannelStrip");

        // Focus groups. Param indices are consistent across all voice spaces
        // (see voicespaces/ChannelStrip/basic.hpp): 0 = preGain, 23 = postGain,
        // 7/8 = input lo/hi-pass cutoffs, 1/4/5/6 = EQ peaks (freqs + shared Q/gain),
        // 14-16 = low shelf, 17-19 = high shelf, 10-13 = compressor.
        focusManager.setGroupName(0, "Gain");   // pre + post gain
        focusManager.setGroupName(1, "Filt");   // input hi/lo-pass filters
        focusManager.setGroupName(2, "EQ");     // peaks + shelves
        focusManager.setGroupName(3, "Comp");   // compressor

        constexpr uint32_t kGain = 1u << 0;
        constexpr uint32_t kFilt = 1u << 1;
        constexpr uint32_t kEQ   = 1u << 2;
        constexpr uint32_t kComp = 1u << 3;
        std::array<uint32_t, ChannelStripAudioApp<>::kN_Params> masks = {};
        masks[0]  = kGain;  masks[23] = kGain;                  // pre / post gain
        masks[7]  = kFilt;  masks[8]  = kFilt;                  // lo / hi-pass cutoffs
        masks[1]  = kEQ;    masks[4]  = kEQ;                    // low / high peak freqs
        masks[5]  = kEQ;    masks[6]  = kEQ;                    // shared peak Q / gain
        masks[14] = kEQ;    masks[15] = kEQ;    masks[16] = kEQ; // low shelf
        masks[17] = kEQ;    masks[18] = kEQ;    masks[19] = kEQ; // high shelf
        masks[10] = kComp;  masks[11] = kComp;                  // threshold / ratio
        masks[12] = kComp;  masks[13] = kComp;                  // attack / release
        focusManager.setParamGroups(masks);

        // Joystick mode: latch params from non-focused groups (matches VerbFX/MEMLCelium).
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "Channel Strip Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppChannelStrip.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> midi_interf) {
    }

    void addViews() {
        bypassView = std::make_shared<BlockSelectView>("Bypasses", TFT_YELLOW, 5, 80, 70, TFT_BLACK,
        std::vector<String>{ "All", "EQ", "Comp", "PPG", "InFilt" });
        bypassView->SetOnSelectCallback([this] (size_t id) {
            Serial.println("Bypass toggled: " + String(id));
            bypassView->toggleAlt(id-1);
            queue_t& audioAppQ = audioAppChannelStrip.controlMessageQueue;
            switch(id) {
                case 1:
                    {
                        auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_ALL;
                        queue_try_add(&audioAppQ, &msg);
                    }                    
                    break;
                case 2:
                    {
                        auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_EQ;
                        queue_try_add(&audioAppQ, &msg);
                    }
                    break;
                case 3:
                    {
                        auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_COMP;
                        queue_try_add(&audioAppQ, &msg);
                    }
                    break;
                case 4:
                    {
                        auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_PREPOSTGAIN;
                        queue_try_add(&audioAppQ, &msg);
                    }
                    break;
                case 5:
                    {
                        auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_INFILTERS;
                        queue_try_add(&audioAppQ, &msg);
                    }
                    break;
            }
        });
        MEMLNaut::Instance()->disp->AddView(bypassView);

        // VU meters (In L/R, Out L/R) — sits right after the bypass view. The view arms the
        // audio-core measurement only while it is on screen.
        auto vuView = std::make_shared<VUMeterView>(
            "VU", std::vector<String>{ "In L", "In R", "Out L", "Out R" },
            VUMeter::levels,
            [](bool a) { VUMeter::active = a; });
        MEMLNaut::Instance()->disp->InsertViewAfter(bypassView, vuView);

        // Mark which NN output dims are live based on the focused groups.
        auto updateActiveDims = [this]() {
            const uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = ChannelStripAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus screen — one toggle per group. 4 buttons => single row of 4
        // sized to span the 320px screen: 10 + 4*68 + 3*10 gap = 312px.
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, (int)kChStrip_NGroups, 68, 76, TFT_WHITE,
            std::vector<String>{ "Gain", "Filt", "EQ", "Comp" },
            TFT_GREENYELLOW, 2 /* fontNum */);
        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            const size_t groupIdx = id - 1;
            const uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");

        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, voiceSpaceSelectView);
        voiceSpaceSelectView->setOptions(voiceSpaceList);  //set by core 1 on startup
        voiceSpaceSelectView->setNewVoiceCallback(
            [this](size_t idx) {
                audioAppChannelStrip.setVoiceSpace(idx);
            });
        interface.addInputSourceView();
    };

    void setupAudio(float sample_rate) {
        audioAppChannelStrip.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppChannelStrip.getVoiceSpaceNames();
    }

    __force_inline void loop() {
      audioAppChannelStrip.loop();
    }

    size_t getNMIDICtrlOutputs() {
        return 0;
    }

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    AudioDriver::codec_config_t getCodecConfig() { return audioAppChannelStrip.GetDriverConfig(); }

    void loopCore0() {}

};
