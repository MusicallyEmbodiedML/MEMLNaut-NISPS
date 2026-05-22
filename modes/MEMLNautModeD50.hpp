#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../D50AudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include "D50SysExOutput.hpp"
#include <memory>
#include <array>

class MEMLNautModeD50 {
public:
    constexpr static size_t kN_InputParams = InterfaceRL::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static D50AudioApp<> audioAppD50;
    std::array<String, D50AudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    void setupInterface() {
        interface.setup(kN_InputParams, D50AudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("d50", "D50");
        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "D50 Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppD50.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppD50.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppD50.getVoiceSpaceNames();
    }

    __force_inline void loop() {
        audioAppD50.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;
    std::shared_ptr<D50SysExOutput> d50Output;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);  // no CC outputs — SysEx path only
        midi_interf->SetMIDISendChannel(1);

        // Device ID = MIDI basic channel - 1 (D50 MIDI impl §4.1: nnnn+1 = channel)
        d50Output = std::make_shared<D50SysExOutput>(midi_interf, 0x00);
        d50Output->setParamMappings({
            // Upper Partial 1 temp area: base [00 00 00], offsets from D50 MIDI impl §4-4
            //                  addrHi  addrMid  addrLo  min  max
            { 0x00,   0x00,    0x0C,   0,   100 },  // TVF Cutoff Frequency
        });

        interface.bindMIDI(midi_interf);
        interface.paramOutputHook = [this](std::span<const float> params) {
            d50Output->sendParams(params);
        };
    }

    void addViews() {
        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, voiceSpaceSelectView);
        voiceSpaceSelectView->setOptions(voiceSpaceList);
        voiceSpaceSelectView->setNewVoiceCallback([this](size_t idx) {
            audioAppD50.setVoiceSpace(idx);
        });

        std::shared_ptr<XYPadView> noteTrigView = std::make_shared<XYPadView>("Play", TFT_SILVER);

        static bool is_playing_note = false;
        static uint8_t last_note_number = 0;

        noteTrigView->SetOnTouchCallback([this](float x, float y) {
            if (is_playing_note) {
                midi_interf->sendNoteOff(last_note_number, 0);
                is_playing_note = false;
            }
            uint8_t note = static_cast<uint8_t>(x * 127.f);
            uint8_t vel  = static_cast<uint8_t>(powf(y, 0.5f) * 127.f);
            midi_interf->sendNoteOn(note, vel);
            last_note_number = note;
            is_playing_note = true;
        });
        noteTrigView->SetOnTouchReleaseCallback([this](float x, float y) {
            midi_interf->sendNoteOff(last_note_number, 0);
            is_playing_note = false;
        });
        MEMLNaut::Instance()->disp->AddView(noteTrigView);
        interface.addInputSourceView();
    }

    inline void processAnalysisParams() {}
    void analyse(stereosample_t) {}
    AudioDriver::codec_config_t getCodecConfig() { return audioAppD50.GetDriverConfig(); }
    void loopCore0() {}
};
