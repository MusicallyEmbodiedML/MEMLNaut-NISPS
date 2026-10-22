#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/InfinibitsAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../MachineListeningMixin.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"

// Infinibits: ChunkyBits' grain network as a drone, fed by two detuned oscillators
// at 80 Hz. Knob Y scales all feedback (0-2x), as in ChunkyBits.
class MEMLNautModeInfinibits {
public:
    constexpr static size_t kN_InputParams    = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    using InterfaceRL_t = InterfaceRL<InfinibitsAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;
    MachineListeningMixin mlMixin;

    inline static InfinibitsAudioApp<> audioAppInfinibits;
    std::shared_ptr<MIDIInOut> midi_interf;

    void setupInterface() {
        interface.setup(kN_InputParams, InfinibitsAudioApp<>::kN_Params);
        interface.setRVY1Override([this](float value) {
            audioAppInfinibits.setFeedbackFactorQueued(value * 2.f);
        });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("infinibits", "Infinibits");
        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() { return "Infinibits Mode"; }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppInfinibits.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
    }

    void addViews() {
        auto waveformView = std::make_shared<RotarySelectView>("Waveform");
        std::vector<String> waveNames;
        for (size_t i = 0; i < InfinibitsAudioApp<>::kNumWaveforms; ++i)
            waveNames.push_back(InfinibitsAudioApp<>::kWaveformNames[i]);
        waveformView->setOptions(std::span<String>(waveNames.data(), waveNames.size()));
        waveformView->setSelection(0);
        waveformView->setNewSelectionCallback([](size_t idx) {
            if (idx < InfinibitsAudioApp<>::kNumWaveforms)
                audioAppInfinibits.setWaveformQueued(
                    static_cast<InfinibitsAudioApp<>::Waveform>(idx));
        });
        MEMLNaut::Instance()->disp->AddView(waveformView);
        interface.addInputSourceView();
    }

    void setupAudio(float sample_rate) {
        audioAppInfinibits.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioAppInfinibits.loop();
    }

    __force_inline void analyse(stereosample_t x)    { mlMixin.analyse(x); }
    __force_inline void processAnalysisParams()       { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() {
        return audioAppInfinibits.GetDriverConfig();
    }

    void loopCore0() {}
};
