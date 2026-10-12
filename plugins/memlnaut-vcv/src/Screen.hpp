// The MEMLNaut's screen, redrawn in NanoVG. Drawn on a 320x240 virtual canvas (the
// hardware's TFT, landscape) scaled to the widget, on Rack's light layer so it glows
// with the room lights down. The tabs replace the rotary encoder's view switching;
// the mouse replaces the touchscreen and the joystick (the MAP page is an XY pad).
//
// Pages (after the firmware's views):
//   RL     RLView: the outputs as bars over a status line (loss trace, yes/no
//          memory split, last gesture flashed)
//   MAP    InputsView's 4D joystick map: memories as triangles, the cursor sized by Z
//          and turned by W, the nearest like outlined as a target; drag to play X/Y
//   NO     the "Dislike Mode" selector
//   SLOTS  the Save Model / Load Model slots (kept in the patch)
//   SETUP  memory mode, train on demand, CV input range
#pragma once

#include "MEMLNaut.hpp"

namespace memlnaut {

struct Screen : OpaqueWidget {
    MEMLNautModule* module = nullptr;

    void step() override;
    void draw(const DrawArgs& args) override;
    void drawLayer(const DrawArgs& args, int layer) override;
    void onButton(const ButtonEvent& e) override;
    void onDragMove(const DragMoveEvent& e) override;
    void onDragEnd(const DragEndEvent& e) override;

private:
    enum Page { RL, MAP, NO, SLOTS, SETUP, NUM_PAGES };
    int page_ = RL;
    bool slotSaveMode_ = true;

    ScreenData d_;
    bool haveData_ = false;
    unsigned long seenSerial_ = 0;
    double eventTime_ = -1e9;

    bool padDragging_ = false;

    Vec toVirtual(Vec px) const;
    void click(Vec v);
    void setPadFrom(Vec v);
    void fillPreview();

    void drawTabs(NVGcontext* vg);
    void drawFooter(NVGcontext* vg);
    void drawRL(NVGcontext* vg);
    void drawMap(NVGcontext* vg);
    void drawNo(NVGcontext* vg);
    void drawSlots(NVGcontext* vg);
    void drawSetup(NVGcontext* vg);
};

} // namespace memlnaut
