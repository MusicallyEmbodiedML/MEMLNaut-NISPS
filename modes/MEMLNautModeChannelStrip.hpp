#pragma once

#include "../src/memllib/hardware/memlnaut/display/SectionView.hpp"
#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../ChannelStripAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "MEMLNautMode.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include <memory>
#include <array>

class MEMLNautModeChannelStrip {
public:
    constexpr static size_t kN_InputParams = MEMLNAUT_ANALOG_INPUTS;
    static constexpr size_t kDesiredSampleRate = 48000;
    ChannelStripAudioApp<> audioAppChannelStrip;
    std::array<String, ChannelStripAudioApp<>::nVoiceSpaces> voiceSpaceList;
    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;
    std::shared_ptr<BlockSelectView> bypassView;

    void setupInterface() {
        interface.setup(kN_InputParams, ChannelStripAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true); //set 4D joystick
        interface.setModeInfo("chstrip", "ChannelStrip");
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
        auto synthSection = std::make_shared<SectionView>("Synth");

        bypassView = std::make_shared<BlockSelectView>("Bypasses", TFT_YELLOW, 5, 80, 70, TFT_BLACK,
        std::vector<String>{ "All", "EQ", "Comp", "PPG", "InFilt" });
        bypassView->SetOnSelectCallback([this] (size_t id) {
            Serial.println("Bypass toggled: " + String(id));
            bypassView->toggleAlt(id-1);
            queue_t& audioAppQ = audioAppChannelStrip.controlMessageQueue;
            switch(id) {
                case 1: { auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_ALL;         queue_try_add(&audioAppQ, &msg); } break;
                case 2: { auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_EQ;          queue_try_add(&audioAppQ, &msg); } break;
                case 3: { auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_COMP;        queue_try_add(&audioAppQ, &msg); } break;
                case 4: { auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_PREPOSTGAIN; queue_try_add(&audioAppQ, &msg); } break;
                case 5: { auto msg = ChannelStripAudioApp<>::controlMessages::MSG_BYPASS_INFILTERS;   queue_try_add(&audioAppQ, &msg); } break;
            }
        });
        synthSection->addChild(bypassView);

        auto voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
        voiceSpaceSelectView->setOptions(voiceSpaceList);
        voiceSpaceSelectView->setNewVoiceCallback([this](size_t idx) {
            audioAppChannelStrip.setVoiceSpace(idx);
        });
        synthSection->addChild(voiceSpaceSelectView);

        MEMLNaut::Instance()->disp->AddView(synthSection);
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

    void loopCore0() {}

};
