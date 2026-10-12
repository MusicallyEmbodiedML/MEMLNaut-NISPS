#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/ChunkyBitsAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../MachineListeningMixin.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"

class MEMLNautModeChunkyBits {
public:
    constexpr static size_t kN_InputParams    = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    using InterfaceRL_t = InterfaceRL<ChunkyBitsAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;
    MachineListeningMixin mlMixin;

    inline static ChunkyBitsAudioApp<> audioAppChunkyBits;
    std::shared_ptr<MIDIInOut> midi_interf;

    float envelopeValue_ = 0.f;

    void setupInterface() {
        interface.setup(kN_InputParams, ChunkyBitsAudioApp<>::kN_Params);
        interface.setRVX1Override([this](float value) {
            audioAppChunkyBits.setFeedbackFactorQueued(value * 2.f);
        });
        // Envelope→NN-input feedback disabled: the AR envelope no longer feeds back into
        // the network inputs. With no hook, that slot falls back to the unused-input pad.
        // NOTE: the env path is left wired but inert for easy re-enable — the audio core
        // still computes envVal_ and pushes it via envQueue, and loopCore0() still drains
        // it into envelopeValue_ (now unused). To restore feedback, re-add the hook here.
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("chunkybits", "ChunkyBits");
        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() { return "ChunkyBits Mode"; }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppChunkyBits.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
        midi_interf->SetNoteCallback([this](bool noteon, uint8_t note, uint8_t vel) {
            if (!noteon) return;
            uint8_t msg[2] = {note, vel};
            queue_try_add(&audioAppChunkyBits.noteOnQueue, msg);
        });
    }

    void addViews() {
        auto waveformView = std::make_shared<RotarySelectView>("Waveform");
        std::vector<String> waveNames;
        for (size_t i = 0; i < ChunkyBitsAudioApp<>::kNumWaveforms; ++i)
            waveNames.push_back(ChunkyBitsAudioApp<>::kWaveformNames[i]);
        waveformView->setOptions(std::span<String>(waveNames.data(), waveNames.size()));
        waveformView->setSelection(0);
        waveformView->setNewSelectionCallback([](size_t idx) {
            if (idx < ChunkyBitsAudioApp<>::kNumWaveforms)
                audioAppChunkyBits.setWaveformQueued(
                    static_cast<ChunkyBitsAudioApp<>::Waveform>(idx));
        });
        MEMLNaut::Instance()->disp->AddView(waveformView);
        interface.addInputSourceView();
    }

    void setupAudio(float sample_rate) {
        audioAppChunkyBits.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioAppChunkyBits.loop();
    }

    __force_inline void analyse(stereosample_t x)    { mlMixin.analyse(x); }
    __force_inline void processAnalysisParams()       { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() {
        return audioAppChunkyBits.GetDriverConfig();
    }

    void loopCore0() {
        float ev;
        if (queue_try_remove(&audioAppChunkyBits.envQueue, &ev))
            envelopeValue_ = ev;
    }
};
