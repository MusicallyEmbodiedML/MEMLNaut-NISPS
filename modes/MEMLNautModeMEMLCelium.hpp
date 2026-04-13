#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "AudioApps/MEMLCeliumAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>

class MEMLNautModeMEMLCelium {
public:
    constexpr static size_t kN_InputParams = MEMLNAUT_ANALOG_INPUTS;  

    inline static MEMLCeliumAudioApp<> audioAppMEMLCelium;
    std::array<String, MEMLCeliumAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    bool sequencerPlaying = false;


    void setupInterface() {
        interface.setup(kN_InputParams, MEMLCeliumAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("memlcelium", "MEMLCelium");
        interfacePtr = make_non_owning(interface);

        MEMLNaut::Instance()->setTogA2Callback([this](bool state) { // scr_ref no longer captured directly
            Serial.println(state ? "TogA2 ON" : "TogA2 OFF");
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                queue_try_add(&audioAppMEMLCelium.sequencerControlQueue, &sequencerPlaying);
            }
 
        });        
    }

    String getHelpTitle() {
        return "MEMLCelium Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppMEMLCelium.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppMEMLCelium.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppMEMLCelium.getVoiceSpaceNames();
    }

    __force_inline void loop() {
      audioAppMEMLCelium.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
      midi_interf = new_midi_interf;
      midi_interf->Setup(16);
      midi_interf->SetMIDISendChannel(1);
      interface.bindMIDI(midi_interf);

      midi_interf->SetNoteCallback([this](bool noteon, uint8_t note_number, uint8_t vel_value) {
        if (noteon) {
          uint8_t midimsg[2] = { note_number, vel_value };
          queue_try_add(&audioAppMEMLCelium.qMIDINoteOn, &midimsg);
        }else{
          uint8_t midimsg[2] = { note_number, vel_value };
          queue_try_add(&audioAppMEMLCelium.qMIDINoteOff, &midimsg);
        }
      });
    }

    void addViews() {
        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");

        MEMLNaut::Instance()->disp->InsertViewAfter(interface.rlStatsView, voiceSpaceSelectView);
        voiceSpaceSelectView->setOptions(voiceSpaceList);
        voiceSpaceSelectView->setNewVoiceCallback(
            [this](size_t idx) {
                audioAppMEMLCelium.setVoiceSpace(idx);
            });

      std::shared_ptr<XYPadView> noteTrigView = std::make_shared<XYPadView>("Play", TFT_SILVER);

      static bool is_playing_note = false;
      static uint8_t last_note_number = 0;

      noteTrigView->SetOnTouchCallback([this](float x, float y) {
            if (is_playing_note) {
                midi_interf->sendNoteOff(last_note_number, 0);
                is_playing_note = false;
            }
            uint8_t noteVel = static_cast<uint8_t>(powf(y, 0.5f) * 127.f);
            uint8_t midimsg[2] = {static_cast<uint8_t>(x * 127.f), noteVel};
            queue_try_add(&audioAppMEMLCelium.qMIDINoteOn, &midimsg);
            midi_interf->sendNoteOn(midimsg[0], midimsg[1]);
            last_note_number = midimsg[0];
            is_playing_note = true;
      });
      noteTrigView->SetOnTouchReleaseCallback([this](float x, float y) {
            uint8_t midimsg[2] = {last_note_number,0};
            queue_try_add(&audioAppMEMLCelium.qMIDINoteOff, &midimsg);
            midi_interf->sendNoteOff(last_note_number, 0);
            is_playing_note = false;
      });
      MEMLNaut::Instance()->disp->AddView(noteTrigView);
    };

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    void loopCore0() {}

};
