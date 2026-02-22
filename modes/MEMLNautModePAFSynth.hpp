#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../PAFSynthAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>

class MEMLNautModePAFSynth {
public:
    constexpr static size_t kN_InputParams = 4; // joystick x, y, rotate

    inline static PAFSynthAudioApp<> audioAppPAFSynth;
    std::array<String, PAFSynthAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    void setupInterface() {
        interface.setup(kN_InputParams, PAFSynthAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interfacePtr = make_non_owning(interface);    
    }

    String getHelpTitle() {
        return "PAF Synth Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppPAFSynth.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppPAFSynth.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppPAFSynth.getVoiceSpaceNames();
    }

    __force_inline void loop() {
      audioAppPAFSynth.loop();
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
          queue_try_add(&audioAppPAFSynth.qMIDINoteOn, &midimsg);
        }else{
          uint8_t midimsg[2] = { note_number, vel_value };
          queue_try_add(&audioAppPAFSynth.qMIDINoteOff, &midimsg);
        }
        // Serial.printf("MIDI Note %d: %d %d\n", note_number, vel_value, noteon);
      });
      // Serial.println("MIDI note callback set.");
    }

    void addViews() {
        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");

        MEMLNaut::Instance()->disp->InsertViewAfter(interface.rlStatsView, voiceSpaceSelectView);
        voiceSpaceSelectView->setOptions(voiceSpaceList);  //set by core 1 on startup
        voiceSpaceSelectView->setNewVoiceCallback(
            [this](size_t idx) {
                audioAppPAFSynth.setVoiceSpace(idx);
            });


      std::shared_ptr<XYPadView> noteTrigView = std::make_shared<XYPadView>("Play", TFT_SILVER);

      // Cache MIDI notes being echoed
      static bool is_playing_note = false;
      static uint8_t last_note_number = 0;

      noteTrigView->SetOnTouchCallback([this](float x, float y) {
          // Serial.printf("Note trigger at: %.2f, %.2f\n", x, y);
            // If a note is already playing, stop it
            if (is_playing_note) {
                midi_interf->sendNoteOff(last_note_number, 0);
                is_playing_note = false;
            }
            uint8_t noteVel = static_cast<uint8_t>(powf(y, 0.5f) * 127.f);
            uint8_t midimsg[2] = {static_cast<uint8_t>(x * 127.f), noteVel};
            queue_try_add(&audioAppPAFSynth.qMIDINoteOn, &midimsg);
            midi_interf->sendNoteOn(midimsg[0], midimsg[1]);
            last_note_number = midimsg[0];
            is_playing_note = true; // Set flag to indicate a note is playing
            // Serial.printf("sending %d %d\n",midimsg[0], midimsg[1]);
      });
      noteTrigView->SetOnTouchReleaseCallback([this](float x, float y) {
          // Serial.printf("Note release at: %.2f, %.2f\n", x, y);
            uint8_t midimsg[2] = {last_note_number,0};
            queue_try_add(&audioAppPAFSynth.qMIDINoteOff, &midimsg);
            midi_interf->sendNoteOff(last_note_number, 0);
            is_playing_note = false; // Reset flag when note is released
      });
      MEMLNaut::Instance()->disp->AddView(noteTrigView);
    };

    // size_t getNMIDICtrlOutputs() {
    //     return 16;
    // }

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

};
