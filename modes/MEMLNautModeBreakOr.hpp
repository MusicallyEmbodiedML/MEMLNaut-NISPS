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

class MEMLNautModeBreakOr {
public:
    constexpr static size_t kN_InputParams = 4; // joystick x, y, rotate

    inline static BreakOrAudioApp<> audioAppBreakOr;
    std::array<String, BreakOrAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    bool sequencerPlaying = false;

    void setupInterface() {
        interface.setup(kN_InputParams, BreakOrAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interfacePtr = make_non_owning(interface);    

        MEMLNaut::Instance()->setTogA2Callback([this](bool state) { // scr_ref no longer captured directly
            Serial.println(state ? "TogA2 ON" : "TogA2 OFF");
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                queue_try_add(&audioAppBreakOr.sequencerControlQueue, &sequencerPlaying);
            }
 
            // //tiggle up
            // if (state) {
            //     //store output
            //     savedAction = action;
            //     actionBeingDragged=true;
            //     msgView->post("Where do you want it?");
            // }else{
            //     //button up
            //     if (actionBeingDragged) {
            //         this->storeExperience(1.f, controlInput, savedAction);
            //         actionBeingDragged=false;
            //         msgView->post("Here!");
            //     }
            // }
        });

    }

    String getHelpTitle() {
        return "Break||";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppBreakOr.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppBreakOr.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppBreakOr.getVoiceSpaceNames();
        audioAppBreakOr.setupMIDI(midi_interf);
        MEMLNaut::Instance()->setRVGain1Callback([this](float value) {
            // Serial.printf("Volume control: %f\n", value);
            queue_try_add(&audioAppBreakOr.bpmQueue, &value);
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

};



#endif // MEMLNAUT_MODE_RHYTHM_BOX_HPP