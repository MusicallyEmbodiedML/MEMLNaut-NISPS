#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../src/memllib/hardware/memlnaut/display/MixerView.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "AudioApps/MEMLAciiidAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include "../MachineListeningMixin.hpp"
#include <memory>
#include <array>

class MEMLNautModeMEMLAciiid {
public:
    using App = MEMLAciiidAudioApp<>;

    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    inline static App audioApp;

    // Output width fixed at compile time -> static-memory mapping network.
    using InterfaceRL_t = InterfaceRL<App::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    bool sequencerPlaying = true;  // the audio app starts the sequencer playing

    FocusManager<App::kN_Params, 9> focusManager;
    MachineListeningMixin mlMixin;

    void setupInterface() {
        interface.setup(kN_InputParams, App::kN_Params, false);  // no Messages screen
        // RL screen: label the outputs by section (see kParamGroupMask for the layout).
        interface.nnOutputsGraphView->setGroups({0, 8, 9, 24, 30, 45, 52, 61, 70}, {"Seq", "Drum", "BSeq", "Bass", "LSeq", "Lead", "CHat", "OHat", "FX"});

        interface.setRVY1Override([this](float value) {
            float bpm = 30.f + value * 170.f;
            queue_try_add(&audioApp.bpmControlQueue, &bpm);
        });

        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("memlaciiid", "MEMLAciiid");
        interface.setTrainOnDemand(true);  // settle when the likes are learned (see InterfaceRL)
        interfacePtr = make_non_owning(interface);

        focusManager.setGroupName(0, "Seq");
        focusManager.setGroupName(1, "Drum");
        focusManager.setGroupName(2, "BSeq");
        focusManager.setGroupName(3, "Bass");
        focusManager.setGroupName(4, "LSeq");
        focusManager.setGroupName(5, "Lead");
        focusManager.setGroupName(6, "CHat");
        focusManager.setGroupName(7, "OHat");
        focusManager.setGroupName(8, "FX");
        focusManager.setParamGroups(App::kParamGroupMask);
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        MEMLNaut::Instance()->setTogB2Callback([this](bool state) {
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                const int playing = sequencerPlaying ? 1 : 0;  // the queue holds an int
                queue_try_add(&audioApp.sequencerControlQueue, &playing);
            }
        });
    }

    String getHelpTitle() {
        return "MEMLAciiid Mode";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioApp.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioApp.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioApp.loop();
    }

    std::shared_ptr<MIDIInOut> midi_interf;

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(16);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf);
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            std::vector<bool> active(App::kN_Params);
            for (size_t i = 0; i < App::kN_Params; i++)
                active[i] = (mask == 0) || ((App::kParamGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus screen — select which parameter groups are live
        std::shared_ptr<BlockSelectView> focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 9, 80, 70, TFT_WHITE,
            std::vector<String>{"Seq", "Drum", "BSeq", "Bass", "LSeq", "Lead", "CHat", "OHat", "FX"}, TFT_GREENYELLOW, 2);
        focusView->setAccent(TFT_CYAN);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            size_t groupIdx = id - 1;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        // Enable screen — toggle each sound and each effect. Highlighted = enabled; tile i
        // is bit i of enableMask_ (all on by default).
        const std::vector<String> enableNames{"Drum", "Bass", "Lead", "CHat", "OHat", "Shaper", "Delay"};
        std::shared_ptr<BlockSelectView> enableView = std::make_shared<BlockSelectView>(
            "Enable", TFT_DARKGREEN, enableNames.size(), 80, 70, TFT_WHITE, enableNames, TFT_GREEN, 2);
        for (size_t i = 0; i < enableNames.size(); i++) enableView->setAltColour(i, true);
        enableView->SetOnSelectCallback([enableView](size_t id) {
            size_t v = id - 1;
            audioApp.enableMask_ ^= (1u << v);
            enableView->toggleAlt(v);
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, enableView);

        // Mixer screen — touch faders trimming each sound, the delay return and the master
        // drive. Saved to flash on release, restored at boot.
        mixerView = std::make_shared<MixerView>("Mixer",
            std::vector<String>{"Drum", "Bass", "Lead", "CHat", "OHat", "Delay", "Mast"}, TFT_CYAN);
        loadMix();
        mixerView->setOnChange([](size_t ch, float v) {
            if (ch < App::kNumMix) audioApp.mixGain_[ch] = v;
        });
        mixerView->setOnCommit([this]() { saveMix(); });
        MEMLNaut::Instance()->disp->InsertViewAfter(enableView, mixerView);

        interface.addInputSourceView(false);  // no MIDI CC Out screen
    };

    __force_inline void analyse(stereosample_t x)    { mlMixin.analyse(x); }
    __force_inline void processAnalysisParams()       { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioApp.GetDriverConfig(); }

    std::shared_ptr<MixerView> mixerView;
    static constexpr const char* kMixFile = "/memlaciiid_mix.bin";

    void saveMix() {
        float g[App::kNumMix];
        for (size_t i = 0; i < App::kNumMix; i++) g[i] = audioApp.mixGain_[i];
        FILE* f = fopen(kMixFile, "wb");
        if (f) { fwrite(g, sizeof(g), 1, f); fclose(f); }
    }

    void loadMix() {
        float g[App::kNumMix];
        FILE* f = fopen(kMixFile, "rb");
        if (!f) return;
        const bool ok = fread(g, sizeof(g), 1, f) == 1;
        fclose(f);
        if (!ok) return;
        for (size_t i = 0; i < App::kNumMix; i++) {
            const float v = (g[i] >= MixerView::kMin && g[i] <= MixerView::kMax) ? g[i] : 1.f;
            audioApp.mixGain_[i] = v;
            mixerView->setValue(i, v);
        }
    }

    void loopCore0() {}
};
