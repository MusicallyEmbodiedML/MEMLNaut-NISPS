#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "AudioApps/MEMLCeliumAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>

class MEMLNautModeMEMLCelium {
public:
    constexpr static size_t kN_InputParams = InterfaceRL::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static MEMLCeliumAudioApp<> audioAppMEMLCelium;
    std::array<String, MEMLCeliumAudioApp<>::nVoiceSpaces> voiceSpaceList;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;

    bool sequencerPlaying = false;

    FocusManager<MEMLCeliumAudioApp<>::kN_Params, 6> focusManager;


    void setupInterface() {
        interface.setup(kN_InputParams, MEMLCeliumAudioApp<>::kN_Params);

        interface.setRVX1Override([this](float value) {
            float bpm = 30.f + value * 170.f;
            queue_try_add(&audioAppMEMLCelium.bpmControlQueue, &bpm);
        });

        interface.bindInterface(InterfaceRL::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("memlcelium", "MEMLCelium");
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "Seq");
        focusManager.setGroupName(1, "Synth");
        focusManager.setGroupName(2, "Env");
        focusManager.setGroupName(3, "V1");
        focusManager.setGroupName(4, "V2");
        focusManager.setGroupName(5, "V3");
        focusManager.setParamGroups(MEMLCeliumAudioApp<>::kParamGroupMask);
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        MEMLNaut::Instance()->setTogA2Callback([this](bool state) {
            Serial.println(state ? "TogA2 ON" : "TogA2 OFF");
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                queue_try_add(&audioAppMEMLCelium.sequencerControlQueue, &sequencerPlaying);
            }
        });
    }

    String getHelpTitle() {
        return "MEMLCelium Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppMEMLCelium.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppMEMLCelium.Setup(sample_rate, interfacePtr);
        voiceSpaceList = audioAppMEMLCelium.getVoiceSpaceNames();
    }

    __force_inline void loop() {
      audioAppMEMLCelium.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
      midi_interf = new_midi_interf;
      midi_interf->Setup(16);
      midi_interf->SetMIDISendChannel(1);
      interface.bindMIDI(midi_interf);

      midi_interf->SetNoteCallback([this](bool noteon, uint8_t note_number, uint8_t vel_value) {
        if (noteon) {
          uint8_t midimsg[2] = { note_number, vel_value };
          queue_try_add(&audioAppMEMLCelium.qMIDINoteOn, &midimsg);
        }else{
          uint8_t midimsg[2] = { note_number, vel_value };
          queue_try_add(&audioAppMEMLCelium.qMIDINoteOff, &midimsg);
        }
      });
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = MEMLCeliumAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((MEMLCeliumAudioApp<>::kParamGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus screen — select which parameter groups are live
        std::shared_ptr<BlockSelectView> focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 6, 80, 70, TFT_WHITE,
            std::vector<String>{"Seq", "Synth", "Env", "Voice 1", "Voice 2", "Voice 3"}, TFT_GREENYELLOW, 2);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            size_t groupIdx = id - 1;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
        voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");

        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, voiceSpaceSelectView);
        size_t nVS = audioAppMEMLCelium.getPopulatedVoiceSpaceCount();
        voiceSpaceSelectView->setOptions(std::span<String>(voiceSpaceList.data(), nVS));
        voiceSpaceSelectView->setNewVoiceCallback(
            [this](size_t idx) {
                audioAppMEMLCelium.setVoiceSpace(idx);
            });

    //   std::shared_ptr<XYPadView> noteTrigView = std::make_shared<XYPadView>("Play", TFT_SILVER);

    //   static bool is_playing_note = false;
    //   static uint8_t last_note_number = 0;

    //   noteTrigView->SetOnTouchCallback([this](float x, float y) {
    //         if (is_playing_note) {
    //             midi_interf->sendNoteOff(last_note_number, 0);
    //             is_playing_note = false;
    //         }
    //         uint8_t noteVel = static_cast<uint8_t>(powf(y, 0.5f) * 127.f);
    //         uint8_t midimsg[2] = {static_cast<uint8_t>(x * 127.f), noteVel};
    //         queue_try_add(&audioAppMEMLCelium.qMIDINoteOn, &midimsg);
    //         midi_interf->sendNoteOn(midimsg[0], midimsg[1]);
    //         last_note_number = midimsg[0];
    //         is_playing_note = true;
    //   });
    //   noteTrigView->SetOnTouchReleaseCallback([this](float x, float y) {
    //         uint8_t midimsg[2] = {last_note_number,0};
    //         queue_try_add(&audioAppMEMLCelium.qMIDINoteOff, &midimsg);
    //         midi_interf->sendNoteOff(last_note_number, 0);
    //         is_playing_note = false;
    //   });
    //   MEMLNaut::Instance()->disp->AddView(noteTrigView);
        interface.addInputSourceView();
    };

    inline void processAnalysisParams() {}

    void analyse(stereosample_t) {}

    AudioDriver::codec_config_t getCodecConfig() { return audioAppMEMLCelium.GetDriverConfig(); }

    void loopCore0() {}

};
