#ifndef MEMLNAUT_MODE_BRAYKORE_HPP
#define MEMLNAUT_MODE_BRAYKORE_HPP

//Braykore

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "./AudioApps/BraykoreAudioApp.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/audio/FocusManager.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"


class MEMLNautModeBraykore {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    FocusManager<BraykoreAudioApp<>::kN_Params, 5> focusManager;
    inline static BraykoreAudioApp<> audioAppBraykore;

    using InterfaceRL_t = InterfaceRL<BraykoreAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    bool sequencerPlaying = false;

    void setupInterface() {
        interface.setup(kN_InputParams, BraykoreAudioApp<>::kN_Params);

        interface.setRVY1Override([this](float value) {
            float bpm = 10.f + value * 100.f;
            queue_try_add(&audioAppBraykore.bpmQueue, &bpm);
        });

        focusManager.setGroupName(0, "Seq");
        focusManager.setGroupName(1, "FM1");
        focusManager.setGroupName(2, "FM2");
        focusManager.setGroupName(3, "FX");
        focusManager.setGroupName(4, "FM3");
        focusManager.setParamGroups(BraykoreAudioApp<>::kParamGroupMask);
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };

        interface.bindInterface(InterfaceRLBase::INPUT_MODES::JOYSTICK, true);
        interface.setModeInfo("braykore", "Braykore");
        interfacePtr = make_non_owning(interface);

        MEMLNaut::Instance()->setTogB2Callback([this](bool state) {
            if (state) {
                sequencerPlaying = !sequencerPlaying;
                int ctrl = sequencerPlaying ? 1 : 0;
                queue_try_add(&audioAppBraykore.sequencerControlQueue, &ctrl);
            }
        });
    }

    String getHelpTitle() {
        return "Braykore";
    }

    __force_inline stereosample_t process(stereosample_t x) {
        return audioAppBraykore.Process(x);
    }

    void setupAudio(float sample_rate) {
        audioAppBraykore.Setup(sample_rate, interfacePtr);
        audioAppBraykore.loadBreakbeat("buttercup", sample_rate);
    }

    __force_inline void loop() {
        audioAppBraykore.loop();
    }

    void addViews() {
        auto updateActiveDims = [this]() {
            uint32_t mask = focusManager.getSelectedMask();
            constexpr size_t N = BraykoreAudioApp<>::kN_Params;
            std::vector<bool> active(N);
            for (size_t i = 0; i < N; i++)
                active[i] = (mask == 0) || ((BraykoreAudioApp<>::kParamGroupMask[i] & mask) != 0);
            interface.setActiveDims(active);
        };
        updateActiveDims();

        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, 5, 140, 70, TFT_WHITE,
            std::vector<String>{"Seq", "FM1", "FM2", "FX", "FM3"},
            TFT_GREENYELLOW, 2);

        focusView->SetOnSelectCallback([this, focusView, updateActiveDims](size_t id) {
            size_t groupIdx = id - 1;
            uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);
        interface.addInputSourceView();
    }

    inline void processAnalysisParams() {}

    void setupMIDI(std::shared_ptr<MIDIInOut>) {}

    void analyse(stereosample_t) {}

    AudioDriver::codec_config_t getCodecConfig() { return audioAppBraykore.GetDriverConfig(); }

    void loopCore0() {}
};



#endif // MEMLNAUT_MODE_BRAYKORE_HPP
