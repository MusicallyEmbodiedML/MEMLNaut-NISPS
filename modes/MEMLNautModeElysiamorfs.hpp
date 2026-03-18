#ifndef MEMLNAUT_MODE_ELYSIAMORFS_HPP
#define MEMLNAUT_MODE_ELYSIAMORFS_HPP

//Elysiamorfs

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "./AudioApps/ElysiamorfAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/interface/USeqI2C.hpp"


class MEMLNautModeElysiamorfs {
public:
    constexpr static size_t kN_InputParams = 4; // joystick x, y, rotate

    USeqI2C i2cOut;
    inline static ElysiamorfAudioApp<> audioAppElysiamorfs;
    std::array<String, ElysiamorfAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    ElysiamorfAudioApp<>::SequencerClockModes clockMode = ElysiamorfAudioApp<>::INTERNAL;

    bool sequencerPlaying = false;

    void setupInterface() {
        interface.setup(kN_InputParams, ElysiamorfAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interfacePtr = make_non_owning(interface);

        MEMLNaut::Instance()->setTogA2Callback([this](bool state) { // scr_ref no longer captured directly
            Serial.println(state ? "TogA2 ON" : "TogA2 OFF");
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                queue_try_add(&audioAppElysiamorfs.sequencerControlQueue, &sequencerPlaying);
            }

        });

        i2cOut.begin();
    }

    String getHelpTitle() {
        return "Elysiamorfs";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppElysiamorfs.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppElysiamorfs.Setup(sample_rate, interfacePtr, clockMode);
        voiceSpaceList = audioAppElysiamorfs.getVoiceSpaceNames();
        audioAppElysiamorfs.setupMIDI(midi_interf);
        MEMLNaut::Instance()->setRVGain1Callback([this](float value) {
            if (clockMode == ElysiamorfAudioApp<>::INTERNAL) {
                float bpm = 30.f + (value * 200.f); // Scale BPM from [0,1] to [30,230]
                queue_try_add(&audioAppElysiamorfs.bpmQueue, &bpm);
            }
        }, 0); // Set threshold to 0 to trigger on any change

    }

    __force_inline void loop() {
      audioAppElysiamorfs.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
      midi_interf = new_midi_interf;
      midi_interf->Setup(0);
    //   midi_interf->SetMIDISendChannel(1);
    //   midi_interf->SetMIDINoteChannel(10);
      midi_interf->SetNoteCallback([this](bool noteon, uint8_t note_number, uint8_t vel_value) {
        // Serial.printf("MIDI Note %d: %d %d\n", note_number, vel_value, noteon);
        if (note_number==0) {
            int resetTrigger=1;
            queue_try_add(&audioAppElysiamorfs.barPhaseResetQueue, &resetTrigger); // value doesn't matter, just a trigger
            Serial.println("Bar phase reset triggered by MIDI note 0");

        }
      });
      midi_interf->SetBPMCallback([this](float bpm) {
        if (clockMode == ElysiamorfAudioApp<>::MIDI_CLOCK) {
            queue_try_add(&audioAppElysiamorfs.bpmQueue, &bpm);
        }

      });
      interface.bindMIDI(midi_interf);
    }

    void addViews() {
    };

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    float i2cValues[8];
    void loopCore0() {
        // PERIODIC_RUN_US(
        //     if (queue_try_remove(&audioAppElysiamorfs.i2cOutQueue, &i2cValues)) {
        //         // Serial.printf("i2c received: %f %f %f %f %f %f %f %f\n", i2cValues[0], i2cValues[1], i2cValues[2], i2cValues[3], i2cValues[4], i2cValues[5], i2cValues[6], i2cValues[7]);
        //         bool i2cSuccess = i2cOut.sendValues(i2cValues, 8);
        //     }
        //     , 5000)

    }

};



#endif // MEMLNAUT_MODE_ELYSIAMORFS_HPP
