#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/display/SectionView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../PAFSynthAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>

class MEMLNautModePAFSynth {
public:
    constexpr static size_t kN_InputParams = MEMLNAUT_ANALOG_INPUTS;
    static constexpr size_t kDesiredSampleRate = 48000;

    inline static PAFSynthAudioApp<> audioAppPAFSynth;
    std::array<String, PAFSynthAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    void setupInterface() {
        interface.setup(kN_InputParams, PAFSynthAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("pafsynth", "PAFSynth");
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
        auto synthSection = std::make_shared<SectionView>("Synth");

        auto voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
        voiceSpaceSelectView->setOptions(voiceSpaceList);
        voiceSpaceSelectView->setNewVoiceCallback([this](size_t idx) {
            audioAppPAFSynth.setVoiceSpace(idx);
        });
        synthSection->addChild(voiceSpaceSelectView);

        auto noteTrigView = std::make_shared<XYPadView>("Play", TFT_SILVER);
        static bool is_playing_note = false;
        static uint8_t last_note_number = 0;
        noteTrigView->SetOnTouchCallback([this](float x, float y) {
            if (is_playing_note) {
                midi_interf->sendNoteOff(last_note_number, 0);
                is_playing_note = false;
            }
            uint8_t noteVel = static_cast<uint8_t>(powf(y, 0.5f) * 127.f);
            uint8_t midimsg[2] = {static_cast<uint8_t>(x * 127.f), noteVel};
            queue_try_add(&audioAppPAFSynth.qMIDINoteOn, &midimsg);
            midi_interf->sendNoteOn(midimsg[0], midimsg[1]);
            last_note_number = midimsg[0];
            is_playing_note = true;
        });
        noteTrigView->SetOnTouchReleaseCallback([this](float x, float y) {
            uint8_t midimsg[2] = {last_note_number, 0};
            queue_try_add(&audioAppPAFSynth.qMIDINoteOff, &midimsg);
            midi_interf->sendNoteOff(last_note_number, 0);
            is_playing_note = false;
        });
        synthSection->addChild(noteTrigView);

        MEMLNaut::Instance()->disp->AddView(synthSection);
    };

    // size_t getNMIDICtrlOutputs() {
    //     return 16;
    // }

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    void loopCore0() {}

};
