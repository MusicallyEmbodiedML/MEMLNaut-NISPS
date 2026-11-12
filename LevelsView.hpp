#ifndef __LEVELS_VIEW_HPP__
#define __LEVELS_VIEW_HPP__

// Levels screen, added to every mode by MEMLNaut-NISPS.ino: the four VU meters
// (In L, In R, Out L, Out R) with the input gain fader to their left, so the gain can be
// set while watching the input meters.
//
// The input gain is the codec's analog line-in gain (applied before the ADC), 0 to
// +22.5 dB in 1.5 dB steps. One setting for the whole device: saved to flash when the
// fader is released, loaded at boot before the codec is configured (InputGain::load()
// in setup(), InputGain::level passed in the codec config in setup1()).

#include "src/memllib/hardware/memlnaut/display/View.hpp"
#include "src/memllib/hardware/memlnaut/display/VUMeterView.hpp"
#include "src/memllib/hardware/memlnaut/display/MixerView.hpp"
#include "src/memllib/audio/AudioDriver.hpp"
#include "VUMeter.hpp"
#include <cstdio>
#include <memory>

namespace InputGain {
    constexpr float   kStepDb  = 1.5f;
    constexpr uint8_t kMax     = 15;            // codec line-in levels 0..15
    constexpr uint8_t kDefault = 3;             // +4.5 dB, the modes' previous fixed setting
    constexpr const char* kFile = "/input_gain.bin";

    inline uint8_t level = kDefault;

    inline void load() {
        FILE* f = fopen(kFile, "rb");
        if (!f) return;
        uint8_t v = kDefault;
        const bool ok = fread(&v, 1, 1, f) == 1;
        fclose(f);
        if (ok && v <= kMax) level = v;
    }

    inline void save() {
        FILE* f = fopen(kFile, "wb");
        if (f) { fwrite(&level, 1, 1, f); fclose(f); }
    }

    // Apply now (codec I2C write). Only after AudioDriver::Setup().
    inline void set(uint8_t v) {
        if (v > kMax) v = kMax;
        level = v;
        AudioDriver::SetLineInLevel(v);
    }
}

class LevelsView : public ViewBase {
public:
    explicit LevelsView(String name) : ViewBase(name) {}

    void OnSetup() override {
        const int gainW = kGainW;
        vu_ = std::make_shared<VUMeterView>(
            "VU", std::vector<String>{ "In L", "In R", "Out L", "Out R" },
            VUMeter::levels,
            [](bool a) { VUMeter::active = a; });
        AddSubView(vu_, { area.x + gainW, area.y, area.w - gainW, area.h });

        gain_ = std::make_shared<MixerView>("In Gain", std::vector<String>{ "In Gain" }, TFT_CYAN);
        gain_->setRange(0.f, InputGain::kStepDb * InputGain::kMax,
                        InputGain::kStepDb * InputGain::kDefault, InputGain::kStepDb);
        gain_->setFormatter([](float db) { return String("+") + String(db, 1) + "dB"; });
        gain_->setValue(0, InputGain::kStepDb * InputGain::level);
        gain_->setOnChange([](size_t, float db) {
            const uint8_t v = static_cast<uint8_t>(roundf(db / InputGain::kStepDb));
            if (v != InputGain::level) InputGain::set(v);
        });
        gain_->setOnCommit([]() { InputGain::save(); });
        AddSubView(gain_, { area.x, area.y, gainW, area.h });
    }

    void OnDraw() override {}   // the meters and the fader draw themselves

    // OnDisplay is forwarded to the subviews by ViewBase (arms the meters); OnHide is not.
    void OnHide() override { if (vu_) vu_->OnHide(); }

    // Presses and releases inside the fader are routed to it by ViewBase; drags are not,
    // and a release that drifts off the fader would otherwise miss its commit (save).
    void OnTouchDown(size_t x, size_t) override { gainTouch_ = inGain(x); }
    void OnTouchDrag(size_t x, size_t y) override { if (gainTouch_) gain_->OnTouchDrag(x, y); }
    void OnTouchUp(size_t x, size_t y) override {
        if (gainTouch_ && !inGain(x)) gain_->OnTouchUp(x, y);
        gainTouch_ = false;
    }

private:
    static constexpr int kGainW = 90;
    bool inGain(size_t x) const { return static_cast<int>(x) < area.x + kGainW; }

    std::shared_ptr<VUMeterView> vu_;
    std::shared_ptr<MixerView> gain_;
    bool gainTouch_ = false;
};

#endif // __LEVELS_VIEW_HPP__
