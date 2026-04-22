#pragma once

#include "../../src/memllib/hardware/memlnaut/display/View.hpp"
#include "../../src/memllib/hardware/memlnaut/display/SliderView.hpp"
#include <functional>
#include <memory>

class BuntySequencingView : public ViewBase {
public:
    using BPMCallback = std::function<void(int)>;

    BuntySequencingView(String name) : ViewBase(name) {}

    void setBPMCallback(BPMCallback cb) {
        bpmCallback_ = std::move(cb);
    }

    void OnSetup() override {
        bpmSlider_ = std::make_shared<SliderView>(String("bpm_slider"));
        bpmSlider_->setRange(30, 200, 1);
        bpmSlider_->setLabel("BPM");
        bpmSlider_->setValue(107);
        bpmSlider_->setValueChangedCallback([this](int bpm) {
            if (bpmCallback_) bpmCallback_(bpm);
        });

        constexpr int titleH = 30;
        int sliderW = area.w * 4 / 5;
        int sliderH = 70;
        int sliderX = area.x + (area.w - sliderW) / 2;
        int sliderY = area.y + titleH + (area.h - titleH - sliderH) / 2;
        AddSubView(bpmSlider_, rect{ sliderX, sliderY, sliderW, sliderH });
    }

    void OnDraw() override {
        constexpr int titleH = 30;
        scr->fillRect(area.x, area.y, area.w, titleH, TFT_BLACK);
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->setTextDatum(TC_DATUM);
        scr->setTextFont(2);
        scr->drawString("Sequencing", area.x + area.w / 2, area.y + 6);
        scr->setTextDatum(TL_DATUM);
        scr->setTextFont(1);
    }

    bool acceptsFocus() override { return true; }

    bool setFocus() override {
        bool result = ViewBase::setFocus();
        if (bpmSlider_) bpmSlider_->setActive(true);
        redraw();
        return result;
    }

    void removeFocus() override {
        ViewBase::removeFocus();
        if (bpmSlider_) bpmSlider_->setActive(false);
        redraw();
    }

    void HandleRotaryEncChange(int delta) override {
        if (bpmSlider_) {
            bpmSlider_->setValue(bpmSlider_->getValue() + delta * bpmSlider_->getStep());
        }
    }

    void HandleRotaryEncSwitch() override {
        removeFocus();
    }

private:
    std::shared_ptr<SliderView> bpmSlider_;
    BPMCallback bpmCallback_;
};
