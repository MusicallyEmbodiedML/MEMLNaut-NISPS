#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../XIASRIAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../MachineListeningMixin.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"


class MEMLNautModeXIASRI {
public:
    constexpr static size_t kN_InputParams = InterfaceRL::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;
    // Run NN inference at 100 Hz (vs the 200 Hz default) to thin out the core-0 memory
    // bursts that contend with the audio core's reverb accesses (SRAM bank contention).
    static constexpr uint32_t kMLInferencePeriodUs = 10000;

    // Focus groups: Delays, Network, Pitch, Mix, Reverb.
    static constexpr size_t kXIASRI_NGroups = 5;

    InterfaceRL interface;
    std::shared_ptr<InterfaceRL> interfacePtr;
    MachineListeningMixin mlMixin;

    XIASRIAudioApp<> audioAppXIASRI;
    std::shared_ptr<MIDIInOut> midi_interf;

    FocusManager<XIASRIAudioApp<>::kN_Params, kXIASRI_NGroups> focusManager;

    void setupInterface() {
        interface.setup(kN_InputParams, XIASRIAudioApp<>::kN_Params);
        interface.bindInterface(InterfaceRL::INPUT_MODES::MACHINE_LISTENING);
        interface.setModeInfo("xiasri", "XIASRI");

        // Focus groups. NOTE: machine-listening mode — focus only steers active dims (which
        // NN outputs the training pushes); it must NOT latch the live output (no
        // paramTransformHook), or the continuously-driven sound would freeze.
        focusManager.setGroupName(0, "Dly");    // delay mixes + feedbacks
        focusManager.setGroupName(1, "Net");    // allpass/comb network
        focusManager.setGroupName(2, "Pitch");  // pitch transpose + mix
        focusManager.setGroupName(3, "Mix");    // wet/dry
        focusManager.setGroupName(4, "Verb");   // large reverb

        constexpr uint32_t kDly   = 1u << 0;
        constexpr uint32_t kNet   = 1u << 1;
        constexpr uint32_t kPitch = 1u << 2;
        constexpr uint32_t kMix   = 1u << 3;
        constexpr uint32_t kVerb  = 1u << 4;
        std::array<uint32_t, XIASRIAudioApp<>::kN_Params> masks = {};
        masks[0] = kDly; masks[1] = kDly; masks[2] = kDly;             // dl1-3 mix
        masks[4] = kNet; masks[5] = kNet; masks[6] = kNet; masks[7] = kNet; // allp1/2 + comb1/2
        masks[8] = kDly; masks[9] = kDly; masks[10] = kDly;            // dl1-3 fb
        masks[11] = kMix;                                              // wet/dry
        masks[12] = kPitch; masks[13] = kPitch;                        // pitch transpose + mix
        for (size_t i = 14; i <= 21; ++i) masks[i] = kNet;             // allp3-6 fb + fbmix
        masks[22] = kDly; masks[23] = kDly;                            // dl4 mix + fb
        for (size_t i = 24; i <= 34; ++i) masks[i] = kVerb;            // large reverb
        // (param 3 is unused by the DSP — left ungrouped)
        focusManager.setParamGroups(masks);

        interfacePtr = make_non_owning(interface);
    }

    String getHelpTitle() {
        return "XIASRI Mode";
    }
    
    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppXIASRI.Process(x);
    }

    void setupMIDI(std::shared_ptr<MIDIInOut> new_midi_interf) {
        midi_interf = new_midi_interf;
        midi_interf->Setup(0);
        midi_interf->SetMIDISendChannel(1);
        interface.bindMIDI(midi_interf, true);
    }

    void addViews() {
        // FX Enable screen: highlight = enabled. Bit i ↔ button i (Pitch, FX, Verb).
        auto enableView = std::make_shared<BlockSelectView>(
            "FX Enable", TFT_YELLOW, 3, 95, 70, TFT_BLACK,
            std::vector<String>{ "Pitch", "FX", "Verb" },
            TFT_BLUE, 2);
        enableView->SetOnSelectCallback([this, enableView](size_t id) {
            audioAppXIASRI.enableMask_ ^= (1u << (id - 1));
            enableView->toggleAlt(id - 1);
        });
        for (size_t i = 0; i < 3; ++i)
            enableView->setAltColour(i, (audioAppXIASRI.enableMask_ >> i) & 1u);
        MEMLNaut::Instance()->disp->AddView(enableView);

        // Mark which NN output dims are live based on the focused groups.
        auto updateActiveDims = [this]() {
            const uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = XIASRIAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        // Focus screen — one toggle per group. 5 buttons => 2 rows x 3 cols, maximised.
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, (int)kXIASRI_NGroups, 95, 76, TFT_WHITE,
            std::vector<String>{ "Dly", "Net", "Pitch", "Mix", "Verb" },
            TFT_GREENYELLOW, 2 /* fontNum */);
        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            const size_t groupIdx = id - 1;
            const uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        interface.addInputSourceView(false);  // no MIDI CC Out screen for this mode
    };

    void setupAudio(float sample_rate) {
        audioAppXIASRI.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioAppXIASRI.loop();
    }

    __force_inline void analyse(stereosample_t x) { mlMixin.analyse(x); }

    __force_inline void processAnalysisParams() { mlMixin.processAnalysisParams(); }

    AudioDriver::codec_config_t getCodecConfig() { return audioAppXIASRI.GetDriverConfig(); }

    void loopCore0() {}

};
