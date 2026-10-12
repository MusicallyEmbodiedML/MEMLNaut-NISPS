// Panel layout (mm, 24 HP = 121.92 x 128.5) and the label overlay. res/MEMLNaut.svg
// draws the shapes to the same numbers; the labels are drawn here because Rack's SVG
// renderer does not render <text>.
#pragma once

#include "plugin.hpp"

namespace memlnaut {
namespace layout {

// screen (4:3, like the hardware's 320x240 TFT)
constexpr float kScreenX = 4.f, kScreenY = 11.f, kScreenW = 66.f, kScreenH = 49.5f;

// left column: gesture buttons over their gate inputs, knobs, CV inputs
constexpr float gestureX(int i) { return 9.5f + 11.f * static_cast<float>(i); }
constexpr float kButtonY = 72.f, kGateY = 82.5f;
constexpr float knobX(int i) { return 10.f + 13.5f * static_cast<float>(i); }
constexpr float kKnobY = 97.f;
constexpr float kCvY = 114.f;

// right column: 32 outputs in 4 columns x 8 rows, then the poly outs and range switch
constexpr float outX(int col) { return 80.f + 10.5f * static_cast<float>(col); }
constexpr float outY(int row) { return 17.f + 11.3f * static_cast<float>(row); }
constexpr float kPolyY = 112.f, kPolyAX = 82.f, kPolyBX = 109.5f, kRangeX = 95.75f;

} // namespace layout

struct PanelLabels : TransparentWidget {
    void text(NVGcontext* vg, float xMm, float yMm, const char* s, float sizeMm, NVGcolor c) {
        nvgFontSize(vg, mm2px(sizeMm));
        nvgFillColor(vg, c);
        nvgText(vg, mm2px(xMm), mm2px(yMm), s, nullptr);
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, font->handle);
        const NVGcolor ink = nvgRGB(0xd5, 0xdb, 0xe2);
        const NVGcolor dim = nvgRGB(0x8a, 0x94, 0xa0);
        const NVGcolor accent = nvgRGB(0x4c, 0xd9, 0x7b);
        using namespace layout;

        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
        text(vg, 12.f, 7.6f, "MEMLNaut", 4.2f, ink);
        text(vg, 43.f, 7.6f, "NISPS", 2.6f, accent);

        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
        const char* gestures[] = { "YES", "NO", "JOLT", "SCRAM", "DRAG", "FORGET" };
        for (int i = 0; i < 6; ++i) text(vg, gestureX(i), kButtonY - 5.6f, gestures[i], 2.3f, ink);
        text(vg, 3.2f, kGateY + 0.8f, ">", 2.4f, dim); // gate inputs row
        const char* knobs[] = { "LEARN", "REWARD", "EXPLORE", "Z", "W" };
        for (int i = 0; i < 5; ++i) text(vg, knobX(i), kKnobY - 6.6f, knobs[i], 2.3f, ink);
        const char* cvs[] = { "X", "Y", "Z", "W", "EXPL", "LEARN" };
        for (int i = 0; i < 6; ++i) text(vg, gestureX(i), kCvY - 5.6f, cvs[i], 2.3f, ink);
        text(vg, 37.f, 124.8f, "CV IN", 2.f, dim);

        for (int i = 0; i < 32; ++i) {
            char n[4];
            snprintf(n, sizeof(n), "%d", i + 1);
            text(vg, outX(i % 4), outY(i / 4) - 4.6f, n, 2.f, dim);
        }
        text(vg, kPolyAX, kPolyY - 5.6f, "1-16", 2.3f, ink);
        text(vg, kPolyBX, kPolyY - 5.6f, "17-32", 2.3f, ink);
        text(vg, kRangeX, kPolyY - 5.6f, "RANGE", 2.f, ink);
        text(vg, kRangeX, kPolyY + 7.6f, "0-10/\xc2\xb1" "5", 1.8f, dim);
        text(vg, (kPolyAX + kPolyBX) / 2.f, 124.8f, "NN OUT", 2.f, dim);
    }
};

} // namespace memlnaut
