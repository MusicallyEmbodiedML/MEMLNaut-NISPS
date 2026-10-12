#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/display/MixerView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "AudioApps/MixMasterMEMLAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include "../MachineListeningMixin.hpp"
#include <memory>
#include <array>

class MEMLNautModeMixMasterMEML {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static MixMasterMEMLAudioApp<> audioAppMixMasterMEML;

    // Output width fixed at compile time -> static-memory mapping network.
    using InterfaceRL_t = InterfaceRL<MixMasterMEMLAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    bool sequencerPlaying = false;

    FocusManager<MixMasterMEMLAudioApp<>::kN_Params, 9> focusManager;
    MachineListeningMixin mlMixin;


    void setupInterface() {
        interface.setup(kN_InputParams, MixMasterMEMLAudioApp<>::kN_Params, false);  // no Messages screen
        // RL screen: label the outputs by section (see kParamGroupMask for the layout).
        interface.nnOutputsGraphView->setGroups({0, 24, 47, 66, 86, 92, 105}, {"Seq", "V1", "V2", "V3", "FX", "Arp", "Bass"});

        interface.setRVX1Override([this](float value) {
            float bpm = 30.f + value * 170.f;
            queue_try_add(&audioAppMixMasterMEML.bpmControlQueue, &bpm);
        });

        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("mixmastermeml", "MixMasterMEML");
        interface.setTrainOnDemand(true);  // settle when the likes are learned (see InterfaceRL)
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "Seq");
        focusManager.setGroupName(1, "Synth");
        focusManager.setGroupName(2, "Env");
        focusManager.setGroupName(3, "V1");
        focusManager.setGroupName(4, "V2");
        focusManager.setGroupName(5, "V3");
        focusManager.setGroupName(6, "FX");
        focusManager.setGroupName(7, "Arp");
        focusManager.setGroupName(8, "Bass");
        focusManager.setParamGroups(MixMasterMEMLAudioApp<>::kParamGroupMask);
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        MEMLNaut::Instance()->setTogA2Callback([this](bool state) {
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                queue_try_add(&audioAppMixMasterMEML.sequencerControlQueue, &sequencerPlaying);
            }
        });
    }

    String getHelpTitle() {
        return "MixMasterMEML Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppMixMasterMEML.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppMixMasterMEML.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
      audioAppMixMasterMEML.loop();
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
          queue_try_add(&audioAppMixMasterMEML.qMIDINoteOn, &midimsg);
        }else{
          uint8_t midimsg[2] = { note_number, vel_value };
          queue_try_add(&audioAppMixMasterMEML.qMIDINoteOff, &midimsg);
        }
      });
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = MixMasterMEMLAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((MixMasterMEMLAudioApp<>::kParamGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus screen — select which parameter groups are live
        std::shared_ptr<BlockSelectView> focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 9, 80, 70, TFT_WHITE,
            std::vector<String>{"Seq", "Synth", "Env", "Voice 1", "Voice 2", "Voice 3", "FX", "Arp", "Bass"}, TFT_GREENYELLOW, 2);
        focusView->setAccent(TFT_CYAN);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            size_t groupIdx = id - 1;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        // Enable screen — toggle each sound source and effect, to isolate parts. Highlighted
        // = enabled; tile i is bit i of voiceEnableMask_ (all on by default).
        const std::vector<String> enableNames{"Voice 1", "Voice 2", "Voice 3", "Arp", "Bass", "Shaper", "Delay"};
        std::shared_ptr<BlockSelectView> voiceEnableView = std::make_shared<BlockSelectView>(
            "Enable", TFT_DARKGREEN, enableNames.size(), 80, 70, TFT_WHITE, enableNames, TFT_GREEN, 2);
        for (size_t i = 0; i < enableNames.size(); i++) voiceEnableView->setAltColour(i, true);
        voiceEnableView->SetOnSelectCallback([this, voiceEnableView](size_t id) {
            size_t v = id - 1;
            audioAppMixMasterMEML.voiceEnableMask_ ^= (1u << v);
            voiceEnableView->toggleAlt(v);
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, voiceEnableView);

        // Mixer screen — touch faders trimming each source, the delay return and the
        // master drive. Saved to flash on release, restored at boot.
        using App = MixMasterMEMLAudioApp<>;
        mixerView = std::make_shared<MixerView>("Mixer",
            std::vector<String>{"V1", "V2", "V3", "Arp", "Bass", "Delay", "Mast"}, TFT_CYAN);
        loadMix();
        mixerView->setOnChange([](size_t ch, float v) {
            if (ch < App::kNumMix) audioAppMixMasterMEML.mixGain_[ch] = v;
        });
        mixerView->setOnCommit([this]() { saveMix(); });
        MEMLNaut::Instance()->disp->InsertViewAfter(voiceEnableView, mixerView);

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
    //         queue_try_add(&audioAppMixMasterMEML.qMIDINoteOn, &midimsg);
    //         midi_interf->sendNoteOn(midimsg[0], midimsg[1]);
    //         last_note_number = midimsg[0];
    //         is_playing_note = true;
    //   });
    //   noteTrigView->SetOnTouchReleaseCallback([this](float x, float y) {
    //         uint8_t midimsg[2] = {last_note_number,0};
    //         queue_try_add(&audioAppMixMasterMEML.qMIDINoteOff, &midimsg);
    //         midi_interf->sendNoteOff(last_note_number, 0);
    //         is_playing_note = false;
    //   });
    //   MEMLNaut::Instance()->disp->AddView(noteTrigView);
        interface.addInputSourceView(false);  // no MIDI CC Out screen
    };

    __force_inline void analyse(stereosample_t x)    { mlMixin.analyse(x); }
    __force_inline void processAnalysisParams()       { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioAppMixMasterMEML.GetDriverConfig(); }

    std::shared_ptr<MixerView> mixerView;
    static constexpr const char* kMixFile = "/mixmastermeml_mix.bin";

    void saveMix() {
        float g[MixMasterMEMLAudioApp<>::kNumMix];
        for (size_t i = 0; i < MixMasterMEMLAudioApp<>::kNumMix; i++) g[i] = audioAppMixMasterMEML.mixGain_[i];
        FILE* f = fopen(kMixFile, "wb");
        if (f) { fwrite(g, sizeof(g), 1, f); fclose(f); }
    }

    void loadMix() {
        float g[MixMasterMEMLAudioApp<>::kNumMix];
        FILE* f = fopen(kMixFile, "rb");
        if (!f) return;
        const bool ok = fread(g, sizeof(g), 1, f) == 1;
        fclose(f);
        if (!ok) return;
        for (size_t i = 0; i < MixMasterMEMLAudioApp<>::kNumMix; i++) {
            const float v = (g[i] >= MixerView::kMin && g[i] <= MixerView::kMax) ? g[i] : 1.f;
            audioAppMixMasterMEML.mixGain_[i] = v;
            mixerView->setValue(i, v);
        }
    }

    void loopCore0() {}

};
