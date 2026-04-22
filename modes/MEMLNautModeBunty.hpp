#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/BuntyAudioApp.hpp"
#include "./AudioApps/BuntySequencingView.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../XiasriAnalysis.hpp"
#include "../src/memllib/utils/sharedMem.hpp"
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"



class MEMLNautModeBunty {
public:
    constexpr static size_t kN_InputParams = MEMLNAUT_ANALOG_INPUTS;
    constexpr static size_t kN_SerialInputs = MEMLNAUT_ANALOG_INPUTS;
    static constexpr size_t kDesiredSampleRate = 32000;
    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    BuntyAudioApp<> audioAppBunty;
    std::array<String, BuntyAudioApp<>::nVoiceSpaces> voiceSpaceList;
    std::shared_ptr<MIDIInOut> midi_interf;
    std::shared_ptr<BlockSelectView> enableView;
    std::shared_ptr<BuntySequencingView> sequencingView_;
    std::shared_ptr<UARTInput> uartInput;


    void setupInterface() {
        interface.setup(kN_InputParams, BuntyAudioApp<>::kN_Params);
        interface.setRVX1Override([this](float value) {
            audioAppBunty.setWetDryQueued(value);
        });
        // std::vector<size_t> serialChannels;
        // for (size_t i = 0; i < kN_SerialInputs; i++) serialChannels.push_back(i);
        // uartInput = std::make_shared<UARTInput>(serialChannels);
        // interface.bindUARTInput(uartInput, serialChannels);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, JOYSTICK_IS_4D);
        interface.setModeInfo("bunty", "Bunty");
        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "Bunty Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppBunty.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
    }

    void addViews() {
        sequencingView_ = std::make_shared<BuntySequencingView>("Sequencing");
        sequencingView_->setBPMCallback([this](int bpm) {
            audioAppBunty.setBPMQueued((float)bpm);
        });
        MEMLNaut::Instance()->disp->AddView(sequencingView_);

        enableView = std::make_shared<BlockSelectView>("FX Enable", TFT_YELLOW, 8, 60, 60, TFT_BLACK,
            std::vector<String>{ "FiltBnk", "Reverb", "ShortDly", "MedDly", "LongDly", "Dly>Verb", "PitchSft", "RingMod" },
            TFT_BLUE, 2);
        enableView->SetOnSelectCallback([this](size_t id) {
            enableView->toggleAlt(id - 1);
            queue_t& q = audioAppBunty.controlMessageQueue;
            switch(id) {
                case 1: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_FILTERBANK;    queue_try_add(&q, &msg); } break;
                case 2: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_REVERB;        queue_try_add(&q, &msg); } break;
                case 3: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_SHORT_DELAY;   queue_try_add(&q, &msg); } break;
                case 4: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_MEDIUM_DELAY;  queue_try_add(&q, &msg); } break;
                case 5: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_LONG_DELAY;    queue_try_add(&q, &msg); } break;
                case 6: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_DELAY_TO_REVERB; queue_try_add(&q, &msg); } break;
                case 7: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_PITCHSHIFT;    queue_try_add(&q, &msg); } break;
                case 8: { auto msg = BuntyAudioApp<>::controlMessages::MSG_ENABLE_RINGMOD;       queue_try_add(&q, &msg); } break;
            }
        });
        MEMLNaut::Instance()->disp->AddView(enableView);

        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.rlStatsView, voiceSpaceSelectView);
        voiceSpaceSelectView->setOptions(voiceSpaceList);
        voiceSpaceSelectView->setNewVoiceCallback(
            [this](size_t idx) {
                audioAppBunty.setVoiceSpace(idx);
            });
    };

    void setupAudio(float sample_rate) {
        audioAppBunty.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppBunty.getVoiceSpaceNames();
    }

    __force_inline void loop() {
        audioAppBunty.loop();
    }

    __force_inline void analyse(stereosample_t x) {
    }

    __force_inline void processAnalysisParams() {
    }

    void loopCore0() {
        // if (uartInput) uartInput->Poll();
    }

};
