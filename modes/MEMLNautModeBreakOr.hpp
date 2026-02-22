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

    void setupInterface() {
        interface.setup(kN_InputParams, BreakOrAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interfacePtr = make_non_owning(interface);    
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


/*
    // BODY
    Value result = Value::nil();

    auto ratios           = args[0].as_sequential();
    const auto pulseWidth = args[1].as_float();
    const auto phase      = args[2].as_float();

    double trig     = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            // check pulse width
            double beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    result = Value(trig);
*/

#endif // MEMLNAUT_MODE_RHYTHM_BOX_HPP