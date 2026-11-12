#pragma once

#include "../src/memllib/hardware/memlnaut/display/RLView.hpp"
#include "../src/memllib/hardware/memlnaut/display/ButtonView.hpp"
#include <functional>
#include <array>
#include <cmath>

class DJRLView : public RLView {
public:
    using BPMCallback = std::function<void(float)>;

    DJRLView(String name, size_t nOutputs, int barwidth = 4,
             int colour = TFT_GREEN, float rangeLow = 0.f, float rangeHigh = 1.f)
        : RLView(name, nOutputs, barwidth, colour, rangeLow, rangeHigh)
    {}

    void setBPMCallback(BPMCallback cb) { bpmCallback_ = cb; }
    float getBPM() const { return bpm_; }

    // Tap-tempo button: a ButtonView tile (same style as the Focus/Enable buttons) in
    // the right quarter of the screen, labelled with the current BPM. The tile draws its
    // own pressed state; each release is a tap.
    void OnSetup() override {
        RLView::OnSetup();
        const int bx = area.x + (area.w * 3) / 4;
        const rect bounds = { bx + kTapGap, area.y + kTapGap,
                              area.w / 4 - 2 * kTapGap, area.h - kStatusBarHeight - 2 * kTapGap };
        tapButton_ = std::make_shared<ButtonView>(bpmLabel(), 1, TFT_GREENYELLOW);
        tapButton_->SetReleaseCallback([this](size_t) {
            recordTap();
            tapButton_->setLabel(bpmLabel());
        });
        AddSubView(tapButton_, bounds);
    }

protected:
    rect getBarGraphArea() const override {
        return {area.x, area.y, (area.w * 3) / 4, area.h - kStatusBarHeight};
    }

private:
    static constexpr size_t   kMaxTaps      = 4;
    static constexpr uint32_t kTapTimeoutMs = 2000;
    static constexpr int      kTapGap       = 6;

    String bpmLabel() const { return String((int)roundf(bpm_)) + " bpm"; }

    void recordTap() {
        uint32_t now = millis();
        if (lastTapMs_ > 0 && (now - lastTapMs_) < kTapTimeoutMs) {
            intervals_[tapCount_ % kMaxTaps] = now - lastTapMs_;
            tapCount_++;
            size_t n = std::min(tapCount_, kMaxTaps);
            uint32_t sum = 0;
            for (size_t i = 0; i < n; i++) sum += intervals_[i];
            bpm_ = 60000.f / (sum / static_cast<float>(n));
            bpm_ = fmaxf(20.f, fminf(300.f, bpm_));
        } else {
            tapCount_ = 0;
        }
        lastTapMs_ = now;
        if (bpmCallback_) bpmCallback_(bpm_);
    }

    BPMCallback bpmCallback_;
    std::shared_ptr<ButtonView> tapButton_;
    float    bpm_       {100.f};
    uint32_t lastTapMs_ {0};
    size_t   tapCount_  {0};
    std::array<uint32_t, kMaxTaps> intervals_{};
};
