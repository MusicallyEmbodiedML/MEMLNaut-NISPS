#pragma once

#include "../src/memllib/interface/MIDIInOut.hpp"
#include "./AudioApps/NautLinearAudioApp.hpp"
#include "MEMLNautMode.hpp"
#include <memory>
#include <array>
#include "../src/memllib/audio/AudioDriver.hpp"
#include "../src/memllib/examples/InterfaceRL.hpp"
#include "../src/memllib/PicoDefs.hpp"
#include "../src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "../src/memllib/hardware/memlnaut/display/VUMeterView.hpp"
#include "../src/memllib/hardware/memlnaut/display/BlockSelectView.hpp"
#include "../MachineListeningMixin.hpp"
#include "../src/memllib/audio/FocusManager.hpp"
#include "../VUMeter.hpp"



class MEMLNautModeNautLinear {
public:
    constexpr static size_t kN_InputParams = InterfaceRLBase::kMaxNNInputs;
    constexpr static size_t kDesiredSampleRate = 48000;

    using InterfaceRL_t = InterfaceRL<NautLinearAudioApp<>::kN_Params>;
    InterfaceRL_t interface;
    std::shared_ptr<InterfaceRL_t> interfacePtr;

    inline static NautLinearAudioApp<> audioAppNautLinear;
    std::shared_ptr<MIDIInOut> midi_interf;
    MachineListeningMixin mlMixin;

    // Focus groups, same items and order as the Enable screen.
    static constexpr size_t kNFocusGroups = 10;
    FocusManager<NautLinearAudioApp<>::kN_Params, kNFocusGroups> focusManager;


    void setupInterface() {
        interface.setup(kN_InputParams, NautLinearAudioApp<>::kN_Params, false /* no message screen */);
        // RL screen: label the outputs by section (see NautLinearParams).
        interface.nnOutputsGraphView->setGroups(
            { NautLinearParams::AUTOLEVEL, NautLinearParams::HARD_TRIM,
              NautLinearParams::MIX, NautLinearParams::POST_CUTOFF,
              NautLinearParams::FB_MASTER, NautLinearParams::GRAIN_LEN },
            { "Pre", "Shape", "Mix", "Post", "FB", "Grain" });
        interface.setRVY1Override([this](float value) {
            audioAppNautLinear.setWetDryQueued(value);
        });
        interface.bindInterface(MEMLNAUT_INPUT_MODE, JOYSTICK_IS_4D);
        interface.setModeInfo("nautlinear", "NautLinear");

        // Per-model extra data: byte 0 = feedback tap (1 = pre-makeup, 0 = post),
        // bytes 1-2 = enable mask (little-endian; bits as NautLinearAudioApp::enableMask_).
        interface.setExtraSaveCallback([]() -> std::vector<uint8_t> {
            const uint32_t mask = audioAppNautLinear.enableMask_;
            return { static_cast<uint8_t>(audioAppNautLinear.fbPreMakeup_ ? 1 : 0),
                     static_cast<uint8_t>(mask & 0xff),
                     static_cast<uint8_t>((mask >> 8) & 0xff) };
        });
        interface.setExtraLoadCallback([this](const uint8_t* data, uint16_t size, uint16_t) {
            if (size >= 1) setFeedbackPreMakeup(data[0] != 0);
            // older files have no mask: everything enabled
            setEnableMask(size >= 3 ? (data[1] | (static_cast<uint32_t>(data[2]) << 8))
                                    : NautLinearAudioApp<>::kEnableAll);
        });

        setupFocus();

        interfacePtr = make_non_owning(interface);
    }

    // Focus groups: one per shaper (its own params + its mix level, feedback gain and
    // grain send), FB and Delay. Shared params belong to every group they affect.
    // Auto-level, pre-EQ and the post filter belong to no group, so they hold still
    // whenever any focus is active.
    void setupFocus() {
        using namespace NautLinearParams;
        const char* names[kNFocusGroups] = { "Hard", "Tanh", "Knee", "Diode", "Oct",
                                             "Asym", "Sine", "Cheby", "FB", "Delay" };
        for (size_t g = 0; g < kNFocusGroups; ++g) focusManager.setGroupName(g, names[g]);

        constexpr uint32_t kHard = 1u << 0, kTanh = 1u << 1, kKnee = 1u << 2, kDiode = 1u << 3,
                           kOct  = 1u << 4, kAsym = 1u << 5, kSine = 1u << 6, kCheby = 1u << 7,
                           kFB   = 1u << 8, kDelay = 1u << 9;
        std::array<uint32_t, NautLinearAudioApp<>::kN_Params> m = {};

        m[DRIVE] = kHard | kTanh | kKnee | kDiode | kOct;  // master drive
        m[BIAS]  = kHard | kTanh | kKnee;
        m[HARD_TRIM] = kHard;
        m[TANH_TRIM] = kTanh;
        m[KNEE_TRIM] = m[KNEE_HARDNESS] = kKnee;
        m[DIODE_TRIM] = m[DIODE_ETA_POS] = m[DIODE_ETA_NEG] = kDiode;
        m[OCT_TRIM] = m[OCT_RECTIFY] = kOct;
        m[ASYM_EXP_POS] = m[ASYM_EXP_NEG] = kAsym;
        m[SINE_IN] = m[SINE_GAIN] = m[SINE_ASYM] = kSine;
        m[CHEBY_IN] = kCheby;
        for (size_t h = 0; h < kChebyHarmonics; ++h) m[CHEBY_T2 + h] = kCheby;

        // per-shaper mix / feedback / grain send
        for (size_t i = 0; i < 8; ++i) {
            m[MIX + i]        = 1u << i;
            m[FB_SHAPER + i]  = (1u << i) | kFB;
            m[GRAIN_SEND + i] = (1u << i) | kDelay;
        }
        m[FB_MASTER] = kFB;
        m[GRAIN_LEN] = m[GRAIN_START] = m[GRAIN_FB] = m[GRAIN_PITCH] = m[GRAIN_SPREAD] = kDelay;
        m[GRAIN_FB_SEND] = kFB | kDelay;

        focusManager.setParamGroups(m);
        interface.paramTransformHook = [this](std::vector<float>& p) {
            focusManager.applyInPlace(p);
        };
    }

    // Mark which NN output dims are live for the focused groups.
    void updateActiveDims() {
        const uint32_t mask = focusManager.getSelectedMask();
        constexpr size_t N = NautLinearAudioApp<>::kN_Params;
        std::vector<bool> active(N);
        for (size_t i = 0; i < N; i++)
            active[i] = (mask == 0) || ((focusManager.paramGroupMask[i] & mask) != 0);
        interface.setActiveDims(active);
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

        // VU meters (In L/R, Out L/R) — right after the RL outputs screen. The view
        // arms the audio-core measurement only while it is on screen.
        auto vuView = std::make_shared<VUMeterView>(
            "VU", std::vector<String>{ "In L", "In R", "Out L", "Out R" },
            VUMeter::levels,
            [](bool a) { VUMeter::active = a; });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, vuView);

        // Focus screen — same items as Enable. Selecting groups latches every param
        // outside them (none selected = all live). Highlight = focused.
        updateActiveDims();
        auto focusView = std::make_shared<BlockSelectView>(
            "Focus", TFT_DARKGREY, kNFocusGroups, 58, 70, TFT_WHITE,
            std::vector<String>{ "Hard", "Tanh", "Knee", "Diode", "Oct",
                                 "Asym", "Sine", "Cheby", "FB", "Delay" },
            TFT_GREENYELLOW, 2);
        focusView->SetOnSelectCallback([this, focusView](size_t id) {
            const size_t groupIdx = id - 1;
            const uint32_t newMask = focusManager.getSelectedMask() ^ (1u << groupIdx);
            focusManager.setFocus(newMask, interface.getLastAction());
            focusView->toggleAlt(groupIdx);
            updateActiveDims();
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(interface.nnOutputsGraphView, focusView);

        // Enable screen — one toggle per shaper, plus feedback and grain delay.
        // Highlight = enabled. 10 buttons -> 2 rows of 5.
        enableView = std::make_shared<BlockSelectView>(
            "Enable", TFT_DARKGREEN, 10, 58, 70, TFT_WHITE,
            std::vector<String>{ "Hard", "Tanh", "Knee", "Diode", "Oct",
                                 "Asym", "Sine", "Cheby", "FB", "Delay" },
            TFT_GREEN, 2);
        enableView->SetOnSelectCallback([this](size_t id) {
            setEnableMask(audioAppNautLinear.enableMask_ ^ (1u << (id - 1)));
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(focusView, enableView);
        setEnableMask(audioAppNautLinear.enableMask_);  // reflect initial state

        // Options — feedback tap point, as a pair of radio buttons (highlight = selected).
        // 10 + 2*140 + 10 gap = 300px wide.
        optionsView = std::make_shared<BlockSelectView>(
            "Options", TFT_YELLOW, 2, 140, 70, TFT_BLACK,
            std::vector<String>{ "FB Pre-Make", "FB Post-Make" },
            TFT_BLUE, 2);
        optionsView->SetOnSelectCallback([this](size_t id) {
            setFeedbackPreMakeup(id == 1);
        });
        MEMLNaut::Instance()->disp->InsertViewAfter(vuView, optionsView);
        setFeedbackPreMakeup(audioAppNautLinear.fbPreMakeup_);  // reflect initial state
    };

    void setupAudio(float sample_rate) {
        audioAppNautLinear.Setup(sample_rate, interfacePtr);
        mlMixin.setup(interface);
    }

    __force_inline void loop() {
        audioAppNautLinear.loop();
    }

    __force_inline void analyse(stereosample_t x)    { mlMixin.analyse(x); }
    __force_inline void processAnalysisParams()       { mlMixin.processAnalysisParams(); }

    std::shared_ptr<BlockSelectView> optionsView;
    std::shared_ptr<BlockSelectView> enableView;

    void setEnableMask(uint32_t mask) {
        mask &= NautLinearAudioApp<>::kEnableAll;
        audioAppNautLinear.enableMask_ = mask;
        if (enableView) {
            for (size_t i = 0; i < 10; ++i) enableView->setAltColour(i, (mask >> i) & 1u);
        }
    }

    void setFeedbackPreMakeup(bool pre) {
        audioAppNautLinear.fbPreMakeup_ = pre;
        if (optionsView) {
            optionsView->setAltColour(0, pre);
            optionsView->setAltColour(1, !pre);
        }
    }

    AudioDriver::codec_config_t getCodecConfig() { return audioAppNautLinear.GetDriverConfig(); }

    void loopCore0() {}

};
