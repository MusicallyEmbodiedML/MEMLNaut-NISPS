#ifndef MEMLNAUT_MODE_RHYTHM_BOX_HPP
#define MEMLNAUT_MODE_RHYTHM_BOX_HPP

//Break||

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "./AudioApps/BreakOrAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/interface/USeqI2C.hpp"


class MEMLNautModeBreakOr {
public:
    constexpr static size_t kN_InputParams = MEMLNAUT_ANALOG_INPUTS;
    static constexpr size_t kDesiredSampleRate = 48000;

    USeqI2C i2cOut;
    inline static BreakOrAudioApp<> audioAppBreakOr;
    std::array<String, BreakOrAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    BreakOrAudioApp<>::SequencerClockModes clockMode = BreakOrAudioApp<>::MIDI_CLOCK;

    bool sequencerPlaying = false;

    void setupInterface() {
        interface.setup(kN_InputParams, BreakOrAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("breakor", "BreakOr");
        interfacePtr = make_non_owning(interface);

        MEMLNaut::Instance()->setTogA2Callback([this](bool state) { // scr_ref no longer captured directly
            Serial.println(state ? "TogA2 ON" : "TogA2 OFF");
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                queue_try_add(&audioAppBreakOr.sequencerControlQueue, &sequencerPlaying);
            }
 
        });

        i2cOut.begin();
    }

    String getHelpTitle() {
        return "Break||";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppBreakOr.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppBreakOr.Setup(sample_rate, interfacePtr, clockMode);
        voiceSpaceList = audioAppBreakOr.getVoiceSpaceNames();
        audioAppBreakOr.setupMIDI(midi_interf);
        MEMLNaut::Instance()->setRVGain1Callback([this](float value) {
            if (clockMode == BreakOrAudioApp<>::INTERNAL) {
                queue_try_add(&audioAppBreakOr.bpmQueue, &value);
            }
        }, 0); // Set threshold to 0 to trigger on any change

    }

    __force_inline void loop() {
      audioAppBreakOr.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
      midi_interf = new_midi_interf;
      midi_interf->Setup(0);
      midi_interf->SetMIDISendChannel(1);
      midi_interf->SetMIDINoteChannel(10);
      midi_interf->SetNoteCallback([this](bool noteon, uint8_t note_number, uint8_t vel_value) {
        Serial.printf("MIDI Note %d: %d %d\n", note_number, vel_value, noteon);
        if (note_number==0) {
            int resetTrigger=1;
            queue_try_add(&audioAppBreakOr.barPhaseResetQueue, &resetTrigger); // value doesn't matter, just a trigger
            Serial.println("Bar phase reset triggered by MIDI note 0");

        }
      });
      midi_interf->SetBPMCallback([this](float bpm) {
        // Serial.printf("Estimated BPM: %.2f\n", bpm);
        if (clockMode == BreakOrAudioApp<>::MIDI_CLOCK) {
            queue_try_add(&audioAppBreakOr.bpmQueue, &bpm);
        }

      });
      interface.bindMIDI(midi_interf);
    }

    void addViews() {
        // std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        // voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");

        // MEMLNaut::Instance()->disp->InsertViewAfter(interface.rlStatsView, voiceSpaceSelectView);
        // voiceSpaceSelectView->setOptions(voiceSpaceList);  //set by core 1 on startup
        // voiceSpaceSelectView->setNewVoiceCallback(
        //     [this](size_t idx) {
        //         audioAppPAFSynth.setVoiceSpace(idx);
        //     });


    };

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    float i2cValues[8];
    void loopCore0() {
        PERIODIC_RUN_US(
            if (queue_try_remove(&audioAppBreakOr.i2cOutQueue, &i2cValues)) {
                // Serial.printf("i2c received: %f %f %f %f %f %f %f %f\n", i2cValues[0], i2cValues[1], i2cValues[2], i2cValues[3], i2cValues[4], i2cValues[5], i2cValues[6], i2cValues[7]);
                bool i2cSuccess = i2cOut.sendValues(i2cValues, 8);
            }
            , 5000)

    }

};



#endif // MEMLNAUT_MODE_RHYTHM_BOX_HPP