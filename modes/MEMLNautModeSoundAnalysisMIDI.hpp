#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../ThruAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../XiasriAnalysis.hpp"
#include "../src/memllib/utils/sharedMem.hpp"
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"



class MEMLNautModeSoundAnalysisMIDI {
public:
    constexpr static size_t kN_InputParams = XiasriAnalysis::kN_Params + 3;  //ML + joystick
    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;
    XiasriAnalysis mlAnalysis{kSampleRate};
    SharedBuffer<float, XiasriAnalysis::kN_Params> machine_list_buffer;

    ThruAudioApp<> audioAppSoundAnalysisMIDI;
    std::array<String, ThruAudioApp<>::nVoiceSpaces> voiceSpaceList;
    std::shared_ptr<MIDIInOut> midi_interf;

    void setupInterface() {
        interface.setup(kN_InputParams, ThruAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK_AND_MACHINE_LISTENING);
        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "Sound Analysis MIDI Mode";
    }
    
    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppSoundAnalysisMIDI.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(8);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf);
    }

    void addViews() {
    };

    void setupAudio(float sample_rate) {
        audioAppSoundAnalysisMIDI.Setup(sample_rate, interfacePtr);
        // Reinitialize XiasriAnalysis filters after maxiSettings is properly configured
        mlAnalysis.ReinitFilters();
    }

    __force_inline void loop() {
        audioAppSoundAnalysisMIDI.loop();
    }

    __force_inline void analyse(stereosample_t x) {
        union {
            XiasriAnalysis::parameters_t p;
            float v[XiasriAnalysis::kN_Params];
        } param_u;
        param_u.p = mlAnalysis.Process(x.L + x.R);
        // Write params into shared_buffer
        machine_list_buffer.writeNonBlocking(param_u.v, XiasriAnalysis::kN_Params);
    }

    // size_t getNMIDICtrlOutputs() {
    //     return 8;
    // }

    __force_inline void processAnalysisParams() {
        // Read SharedBuffer
        std::vector<float> mlist_params(XiasriAnalysis::kN_Params, 0);
        machine_list_buffer.readNonBlocking(mlist_params);
        // Send parameters to RL interface
        interface.readAnalysisParameters(mlist_params);
        // PERIODIC_RUN(
        //     Serial.printf("%f %f %f\n", mlist_params[0], mlist_params[1], mlist_params[2]);
        //     , 100);

    }

    void loopCore0() {}

};
