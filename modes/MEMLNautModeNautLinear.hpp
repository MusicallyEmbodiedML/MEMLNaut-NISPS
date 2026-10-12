#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/NautLinearAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"



class MEMLNautModeNautLinear {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    using InterfaceRL_t = InterfaceRL<NautLinearAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    inline static NautLinearAudioApp<> audioAppNautLinear;
    std::shared_ptr<MIDIInOut> midi_interf;


    void setupInterface() {
        interface.setup(kN_InputParams, NautLinearAudioApp<>::kN_Params, false /* no message screen */);
        interface.setRVX1Override([this](float value) {
            audioAppNautLinear.setWetDryQueued(value);
        });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("nautlinear", "NautLinear");

        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "NautLinear Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppNautLinear.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
    }

    void addViews() {
        interface.addInputSourceView(false);  // no MIDI CC Out screen for this mode
    };

    void setupAudio(float sample_rate) {
        audioAppNautLinear.Setup(sample_rate, interfacePtr);
    }

    __force_inline void loop() {
        audioAppNautLinear.loop();
    }

    __force_inline void analyse(stereosample_t x) {}

    __force_inline void processAnalysisParams() {}

    AudioDriver::codec_config_t getCodecConfig() { return audioAppNautLinear.GetDriverConfig(); }

    void loopCore0() {}

};
