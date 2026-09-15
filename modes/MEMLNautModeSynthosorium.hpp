#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../src/memllib/synth/maximilian.h" // maxiBiquad/maxiSettings used by ThruAudioApp
#include "../ThruAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include <cstdio>

// Fixed 16-in / 16-out MIDI CC mapping: no input-source picker, no MIDI CC
// output-assignment screen. CC1-16 in -> NN inputs 1-16 (any channel), NN
// outputs 1-16 -> CC1-16 out.
static const std::vector<uint8_t> kSynthosoriumOutputCCs = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

class MEMLNautModeSynthosorium {
public:
    static constexpr size_t kN_InputParams  = 16;
    static constexpr size_t kN_OutputParams = 16;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static ThruAudioApp<kN_OutputParams> audioApp;
    std::array<String, ThruAudioApp<kN_OutputParams>::nVoiceSpaces> voiceSpaceList;

    using InterfaceRL_t = InterfaceRL<kN_OutputParams, kN_InputParams>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    void setupInterface() {
        interface.setup(kN_InputParams, kN_OutputParams);
        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, false);
        interface.setModeInfo("synthosorium", "Synthosorium");
        interfacePtr = make_non_owning(interface);

        // bindInterface() just sized the bar graph for the (unused) default
        // input source's 3 joystick axes — we always drive all 16 CC inputs.
        if (interface.nnInputsGraphView) interface.nnInputsGraphView->setNumDisplayBars(kN_InputParams);

        // Override whatever assembleInputs() computed from the (unused) default
        // input source with the live MIDI CC1-16 values every cycle.
        interface.inputInjectionHook = [this](std::vector<float>& in) {
            for (size_t i = 0; i < kN_InputParams; i++) in[i] = ccIn_[i];
        };
    }

    String getHelpTitle() { return "Synthosorium Mode"; }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioApp.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(kN_OutputParams);
        midi_interf->SetMIDISendChannel(1);  // change on device via CONFIG > MIDI
        midi_interf->SetParamCCNumbers(kSynthosoriumOutputCCs);

        // Own CC1-16 -> NN input handling (bypasses the INPUT_SOURCE/bindMIDI
        // machinery, which tops out at 8 raw MIDI CC inputs).
        midi_interf->SetCCCallback([this](uint8_t cc_number, uint8_t cc_value) {
            if (cc_number < 1 || cc_number > kN_InputParams) return;
            ccIn_[cc_number - 1] = static_cast<float>(cc_value) / 127.f;
            interface.markInputDirty();
        });

        interface.paramOutputHook = [this](std::span<const float> params) {
            uint32_t now = millis();
            if (now - lastCCSendMs_ >= 30) {
                midi_interf->SendParamsAsMIDICC(params);
                lastCCSendMs_ = now;
            }
        };
    }

    void addViews() {
        // Deliberately no interface.addInputSourceView() call: this mode has no
        // input-source picker and no MIDI CC output-assignment screen — both
        // sides of the mapping are fixed above.
    }

    inline void processAnalysisParams() {}
    void analyse(stereosample_t) {}

    void setupAudio(float sample_rate) {
        audioApp.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioApp.getVoiceSpaceNames();
    }

    __force_inline void loop() { audioApp.loop(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioApp.GetDriverConfig(); }
    void loopCore0() {}

private:
    std::shared_ptr<MIDIInOut> midi_interf;
    std::array<float, kN_InputParams> ccIn_{};  // last-received CC1-16, [0,1], default 0
    uint32_t lastCCSendMs_ = 0;
};
