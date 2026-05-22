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
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"


class MEMLNautModeBreakOr {
public:
    constexpr static size_t kN_InputParams = InterfaceRL::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    USeqI2C i2cOut;
    FocusManager<BreakOrAudioApp<>::kN_Params, 8> focusManager;
    inline static BreakOrAudioApp<> audioAppBreakOr;
    std::array<String, BreakOrAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    BreakOrAudioApp<>::SequencerClockModes clockMode = BreakOrAudioApp<>::INTERNAL;

    bool sequencerPlaying = false;

    void setupInterface() {
        interface.setup(kN_InputParams, BreakOrAudioApp<>::kN_Params);

        interface.setRVX1Override([this](float value) {
            if (clockMode == BreakOrAudioApp<>::INTERNAL) {
                float bpm = 30.f + value * 170.f;
                queue_try_add(&audioAppBreakOr.bpmQueue, &bpm);
            }
        });

        for (size_t i = 0; i < 8; i++) {
            focusManager.setGroupName(i, "S" + String(i + 1));
        }
        focusManager.setParamGroups(BreakOrAudioApp<>::kParamGroupMask);
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

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

    }

    __force_inline void loop() {
      audioAppBreakOr.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
      midi_interf = new_midi_interf;
      midi_interf->Setup(0);
      midi_interf->SetMIDISendChannel(10);
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
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = BreakOrAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((BreakOrAudioApp<>::kParamGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 8, 60, 50, TFT_WHITE,
            std::vector<String>{"S1","S2","S3","S4","S5","S6","S7","S8"},
            TFT_GREENYELLOW, 2);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            size_t groupIdx = id - 1;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);
        interface.addInputSourceView();
    };

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    AudioDriver::codec_config_t getCodecConfig() { return audioAppBreakOr.GetDriverConfig(); }

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