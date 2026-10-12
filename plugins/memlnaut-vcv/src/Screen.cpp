#include "Screen.hpp"

#include <cmath>

namespace memlnaut {

namespace {

constexpr float kW = 320.f, kH = 240.f;
constexpr float kTabH = 18.f;
constexpr float kFooterY = 222.f;

// The firmware's TFT colours.
const NVGcolor kBlack = nvgRGB(0, 0, 0);
const NVGcolor kWhite = nvgRGB(255, 255, 255);
const NVGcolor kGreen = nvgRGB(0, 255, 0);
const NVGcolor kBlue = nvgRGB(0, 0, 255);
const NVGcolor kRed = nvgRGB(255, 0, 0);
const NVGcolor kYellow = nvgRGB(255, 255, 0);
const NVGcolor kCyan = nvgRGB(0, 255, 255);
const NVGcolor kOrange = nvgRGB(255, 165, 0);
const NVGcolor kSkyBlue = nvgRGB(135, 206, 235);
const NVGcolor kViolet = nvgRGB(148, 81, 255); // 0x929F, the Load Model accent
const NVGcolor kDarkGrey = nvgRGB(128, 128, 128);
const NVGcolor kLightGrey = nvgRGB(211, 211, 211);
const NVGcolor kEmpty = nvgRGB(66, 65, 66);   // 0x4208
const NVGcolor kGrid = nvgRGB(40, 40, 40);

// RL map
constexpr float kMapX = 6.f, kMapY = 24.f, kMapS = 192.f;

const char* const kPageNames[] = { "RL", "MAP", "NO", "SLOTS", "SETUP" };
const char* const kDislikeNames[] = { "Current (push away)", "Nudge + blame", "Re-roll", "Nudge>Back>Re-roll",
                                      "Borrow from likes" };
const char* const kDislikeDescs[] = { "Push the sound away from your likes",
                                      "Step away, mostly undoing what differs from the nearest like",
                                      "Jump to a fresh sound here",
                                      "Repeated no: nudge, then back to a like, then re-roll",
                                      "Rebuild from qualities of nearby likes" };
const char* const kStoreModes[] = { "Add", "Replace 5%", "Replace 10%", "Replace 15%", "Reward decay 10%",
                                    "Reward decay 20%" };

NVGcolor actionColour(const std::string& a) {
    if (a == "yes" || a == "drop") return kGreen;
    if (a == "no") return kRed;
    if (a == "forget") return kOrange;
    if (a == "save") return kOrange;
    if (a == "load") return kViolet;
    return kSkyBlue; // scramble, drag, jolt
}

NVGcolor lerp(NVGcolor a, NVGcolor b, float t) { return nvgLerpRGBA(a, b, t); }

void text(NVGcontext* vg, float x, float y, const std::string& s, float size, NVGcolor c, int align) {
    nvgFontSize(vg, size);
    nvgFillColor(vg, c);
    nvgTextAlign(vg, align);
    nvgText(vg, x, y, s.c_str(), nullptr);
}

void rect(NVGcontext* vg, float x, float y, float w, float h, NVGcolor c, float r = 0.f) {
    nvgBeginPath(vg);
    if (r > 0.f) nvgRoundedRect(vg, x, y, w, h, r);
    else nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, c);
    nvgFill(vg);
}

void frame(NVGcontext* vg, float x, float y, float w, float h, NVGcolor c, float r = 0.f, float width = 1.f) {
    nvgBeginPath(vg);
    if (r > 0.f) nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, r);
    else nvgRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f);
    nvgStrokeColor(vg, c);
    nvgStrokeWidth(vg, width);
    nvgStroke(vg);
}

// A triangle pointing "up" when rot = 0, turned by rot (0..1 = a full turn).
void triangle(NVGcontext* vg, float cx, float cy, float size, float rot) {
    const float a = rot * 2.f * static_cast<float>(M_PI);
    nvgBeginPath(vg);
    for (int k = 0; k < 3; ++k) {
        const float t = a + static_cast<float>(k) * 2.f * static_cast<float>(M_PI) / 3.f;
        const float px = cx + size * std::sin(t), py = cy - size * std::cos(t);
        if (k == 0) nvgMoveTo(vg, px, py);
        else nvgLineTo(vg, px, py);
    }
    nvgClosePath(vg);
}

std::string formatLoss(float v) {
    if (v < 0.f) return "-";
    if (v == 0.f) return "0";
    int e = static_cast<int>(std::floor(std::log10(v)));
    float m = v / std::pow(10.f, static_cast<float>(e));
    if (m >= 9.95f) {
        m /= 10.f;
        e++;
    }
    return string::f("%.1fe%d", m, e);
}

} // namespace

// --- data ------------------------------------------------------------------------

void Screen::step() {
    OpaqueWidget::step();
    if (!module) {
        fillPreview();
        return;
    }
    {
        std::lock_guard<std::mutex> lock(module->snapMutex);
        d_ = module->snap;
    }
    if (!haveData_) {
        seenSerial_ = d_.eventSerial; // no flash for whatever happened before we looked
        haveData_ = true;
    } else if (d_.eventSerial != seenSerial_) {
        seenSerial_ = d_.eventSerial;
        eventTime_ = system::getTime();
    }
}

// Module browser: a still picture of a trained-looking network.
void Screen::fillPreview() {
    for (int i = 0; i < kNumOutputs; ++i)
        d_.action[i] = 0.5f + 0.4f * std::sin(static_cast<float>(i) * 0.7f) * std::cos(static_cast<float>(i) * 0.19f);
    d_.input = { 0.6f, 0.4f, 0.5f, 0.2f };
    d_.likes = 7;
    d_.dislikes = 2;
    d_.lossLog.fill(NAN);
    for (int i = 0; i < kLossColumns; ++i) d_.lossLog[i] = -2.f - 1.2f * static_cast<float>(i) / kLossColumns;
    d_.lossCursor = kLossColumns - 1;
    d_.lastLoss = 0.0021f;
    d_.message = "In flow";
    d_.action_ = "yes";
    const MemPoint pts[] = { { 0.2f, 0.3f, 0.5f, 0.1f, 0 }, { 0.7f, 0.8f, 0.3f, 0.6f, 0 },
                             { 0.5f, 0.2f, 0.8f, 0.3f, 1 }, { 0.85f, 0.35f, 0.4f, 0.9f, 0 } };
    d_.memCount = 4;
    for (int i = 0; i < 4; ++i) d_.mem[i] = pts[i];
}

// --- drawing -----------------------------------------------------------------------

void Screen::draw(const DrawArgs& args) {
    // The unlit screen: black glass. Everything else is on the light layer.
    rect(args.vg, 0.f, 0.f, box.size.x, box.size.y, nvgRGB(4, 5, 6), mm2px(0.6f));
}

void Screen::drawLayer(const DrawArgs& args, int layer) {
    if (layer != 1) return;
    std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
    if (!font) return;
    NVGcontext* vg = args.vg;
    nvgSave(vg);
    nvgScissor(vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgScale(vg, box.size.x / kW, box.size.y / kH);
    nvgFontFaceId(vg, font->handle);

    rect(vg, 0.f, 0.f, kW, kH, kBlack);
    drawTabs(vg);
    switch (page_) {
        case RL: drawRL(vg); break;
        case MAP: drawMap(vg); break;
        case NO: drawNo(vg); break;
        case SLOTS: drawSlots(vg); break;
        case SETUP: drawSetup(vg); break;
    }
    drawFooter(vg);
    nvgRestore(vg);
    OpaqueWidget::drawLayer(args, layer);
}

void Screen::drawTabs(NVGcontext* vg) {
    const float w = kW / static_cast<float>(NUM_PAGES);
    for (int p = 0; p < NUM_PAGES; ++p) {
        const bool on = p == page_;
        if (on) {
            rect(vg, p * w, 0.f, w, kTabH, nvgRGB(24, 30, 36));
            rect(vg, p * w + 4.f, kTabH - 2.f, w - 8.f, 2.f, kGreen);
        }
        text(vg, p * w + w / 2.f, kTabH / 2.f + 1.f, kPageNames[p], 13.f, on ? kWhite : kDarkGrey,
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    rect(vg, 0.f, kTabH, kW, 1.f, nvgRGB(30, 36, 42));
}

void Screen::drawFooter(NVGcontext* vg) {
    rect(vg, 0.f, kFooterY - 1.f, kW, 1.f, nvgRGB(30, 36, 42));
    const double age = system::getTime() - eventTime_;
    const NVGcolor c = age < 4.0 ? actionColour(d_.action_) : kDarkGrey;
    text(vg, 4.f, kFooterY + 9.f, d_.message.empty() ? "Like the sounds you want more of" : d_.message, 13.f, c,
         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
}

void Screen::drawRL(NVGcontext* vg) {
    // Bars: one per output, coloured along the spectrum the firmware uses (green->blue,
    // or red->yellow while the exploration noise is on).
    const float x0 = 2.f, y0 = 22.f, w = kW - 4.f, h = 174.f;
    for (int g = 1; g < 4; ++g) rect(vg, x0, y0 + h * g / 4.f, w, 1.f, kGrid);
    const float slot = w / kNumOutputs;
    const NVGcolor c0 = d_.noiseActive ? kRed : kGreen, c1 = d_.noiseActive ? kYellow : kBlue;
    for (int i = 0; i < kNumOutputs; ++i) {
        const float v = clamp(d_.action[i], 0.f, 1.f);
        const float bh = std::max(1.f, v * h);
        rect(vg, x0 + i * slot + 1.f, y0 + h - bh, slot - 2.f, bh, lerp(c0, c1, i / float(kNumOutputs - 1)));
    }
    if (d_.dragging) text(vg, kW / 2.f, y0 + 12.f, "HOLDING: move to where you want it", 12.f, kSkyBlue,
                          NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    // Status line: [loss] [trace] [yes/no split + counts] [last action]
    const float by = 198.f;
    const NVGcolor lossColour = d_.trainingIdle ? kDarkGrey : kCyan;
    text(vg, 2.f, by + 10.f, d_.trainingIdle ? "idle" : formatLoss(d_.lastLoss), 12.f, lossColour,
         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    // Loss trace, oldest on the left, on a log scale fitted to what is on screen
    // (snapped to quarter decades, at least half a decade tall).
    float lo = INFINITY, hi = -INFINITY;
    for (float v : d_.lossLog)
        if (std::isfinite(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    const float tx = 42.f, th = 16.f, ty = by + 2.f;
    if (lo <= hi) {
        lo = std::floor(lo / 0.25f) * 0.25f;
        hi = std::ceil(hi / 0.25f) * 0.25f;
        if (hi - lo < 0.5f) {
            const float mid = std::round((lo + hi) * 0.5f / 0.25f) * 0.25f;
            lo = mid - 0.25f;
            hi = mid + 0.25f;
        }
        nvgBeginPath(vg);
        bool pen = false;
        for (int k = 0; k < kLossColumns; ++k) {
            const float v = d_.lossLog[(d_.lossCursor + 1 + k) % kLossColumns];
            if (std::isnan(v)) {
                pen = false;
                continue;
            }
            const float t = std::isinf(v) ? 0.f : clamp((v - lo) / (hi - lo), 0.f, 1.f);
            const float px = tx + k, py = ty + (1.f - t) * (th - 1.f);
            if (pen) nvgLineTo(vg, px, py);
            else nvgMoveTo(vg, px, py);
            pen = true;
        }
        nvgStrokeColor(vg, lossColour);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
    }

    const float cx = 106.f, cw = 50.f;
    const size_t total = d_.likes + d_.dislikes;
    if (total == 0) {
        rect(vg, cx, by + 7.f, cw, 6.f, kEmpty);
    } else {
        const float gw = cw * static_cast<float>(d_.likes) / static_cast<float>(total);
        if (gw > 0.f) rect(vg, cx, by + 7.f, gw, 6.f, kGreen);
        if (gw < cw) rect(vg, cx + gw, by + 7.f, cw - gw, 6.f, kRed);
    }
    text(vg, cx + cw + 4.f, by + 10.f, string::f("%zu/%zu", d_.likes, d_.dislikes), 12.f, kLightGrey,
         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    const float ax = 206.f, aw = kW - ax - 2.f;
    const bool flash = system::getTime() - eventTime_ < 0.7;
    const NVGcolor bg = flash ? actionColour(d_.action_) : kBlack;
    rect(vg, ax, by + 2.f, aw, 16.f, bg, 3.f);
    text(vg, ax + aw / 2.f, by + 10.f, d_.action_, 12.f, flash ? kBlack : kDarkGrey,
         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
}

void Screen::drawMap(NVGcontext* vg) {
    auto px = [](float x) { return kMapX + clamp(x, 0.f, 1.f) * kMapS; };
    auto py = [](float y) { return kMapY + (1.f - clamp(y, 0.f, 1.f)) * kMapS; };

    frame(vg, kMapX, kMapY, kMapS, kMapS, kDarkGrey);
    for (int k = 1; k < 4; ++k) {
        rect(vg, kMapX + kMapS * k / 4.f, kMapY + 1.f, 1.f, kMapS - 2.f, kGrid);
        rect(vg, kMapX + 1.f, kMapY + kMapS * k / 4.f, kMapS - 2.f, 1.f, kGrid);
    }

    // Memories: small triangles in the cursor's code (size Z, rotation W).
    for (int i = 0; i < d_.memCount; ++i) {
        const MemPoint& m = d_.mem[i];
        triangle(vg, px(m.x), py(m.y), 3.f + 3.f * m.z, m.w);
        nvgFillColor(vg, m.kind == 0 ? kGreen : m.kind == 1 ? kRed : kDarkGrey);
        nvgFill(vg);
    }

    // The nearest like (over all four dims), outlined at full size: a target to steer to.
    const float cursorSize = 7.f + 12.f * clamp(d_.input[2], 0.f, 1.f);
    int nearest = -1;
    float best = INFINITY;
    for (int i = 0; i < d_.memCount; ++i) {
        const MemPoint& m = d_.mem[i];
        if (m.kind != 0) continue;
        const float dx = m.x - d_.input[0], dy = m.y - d_.input[1], dz = m.z - d_.input[2], dw = m.w - d_.input[3];
        const float dist = dx * dx + dy * dy + dz * dz + dw * dw;
        if (dist < best) {
            best = dist;
            nearest = i;
        }
    }
    if (nearest >= 0) {
        const MemPoint& m = d_.mem[nearest];
        triangle(vg, px(m.x), py(m.y), 7.f + 12.f * m.z, m.w);
        nvgStrokeColor(vg, nvgRGB(0, 150, 0));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
    }

    triangle(vg, px(d_.input[0]), py(d_.input[1]), cursorSize, d_.input[3]);
    nvgFillColor(vg, kYellow);
    nvgFill(vg);
    nvgStrokeColor(vg, kWhite);
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);

    // Input bars.
    const char* labels[] = { "X", "Y", "Z", "W" };
    const float bx0 = 214.f, by0 = 34.f, bh = 140.f;
    for (int i = 0; i < kNumInputs; ++i) {
        const float x = bx0 + i * 25.f;
        const float v = clamp(d_.input[i], 0.f, 1.f);
        frame(vg, x, by0, 18.f, bh, kEmpty);
        rect(vg, x + 1.f, by0 + bh - v * (bh - 2.f) - 1.f, 16.f, v * (bh - 2.f), kYellow);
        text(vg, x + 9.f, by0 + bh + 10.f, labels[i], 12.f, kLightGrey, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        text(vg, x + 9.f, by0 - 7.f, string::f("%02d", static_cast<int>(std::round(v * 99.f))), 10.f, kDarkGrey,
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    text(vg, 262.f, 205.f, d_.xyFromCV ? "X/Y from CV" : "drag the map", 11.f, kDarkGrey,
         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
}

void Screen::drawNo(NVGcontext* vg) {
    const int sel = module ? module->dislikeMode : 0;
    for (int i = 0; i < 5; ++i) {
        const float y = 22.f + i * 39.f;
        const bool on = i == sel;
        if (on) {
            rect(vg, 4.f, y, kW - 8.f, 36.f, nvgRGB(60, 14, 14), 3.f);
            frame(vg, 4.f, y, kW - 8.f, 36.f, kRed, 3.f);
        }
        text(vg, 12.f, y + 12.f, kDislikeNames[i], 14.f, on ? kWhite : kLightGrey, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        text(vg, 12.f, y + 27.f, kDislikeDescs[i], 10.5f, on ? kLightGrey : kDarkGrey, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
}

void Screen::drawSlots(NVGcontext* vg) {
    const NVGcolor accent = slotSaveMode_ ? kOrange : kViolet;
    const float bw = (kW - 18.f) / 2.f;
    for (int m = 0; m < 2; ++m) {
        const bool on = (m == 0) == slotSaveMode_;
        const NVGcolor c = m == 0 ? kOrange : kViolet;
        const float x = 6.f + m * (bw + 6.f);
        if (on) rect(vg, x, 23.f, bw, 20.f, c, 3.f);
        else frame(vg, x, 23.f, bw, 20.f, c, 3.f);
        text(vg, x + bw / 2.f, 33.f, m == 0 ? "SAVE MODEL" : "LOAD MODEL", 13.f, on ? kBlack : c,
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    const float cw = (kW - 12.f - 5.f * 6.f) / 6.f, ch = 78.f;
    for (int s = 0; s < kNumSlots; ++s) {
        const float x = 6.f + (s % 6) * (cw + 6.f), y = 50.f + (s / 6) * (ch + 7.f);
        const bool used = d_.slotMask & (1u << s);
        const bool current = s == d_.currentSlot;
        if (current) rect(vg, x, y, cw, ch, accent, 4.f);
        else frame(vg, x, y, cw, ch, used ? accent : kEmpty, 4.f);
        const NVGcolor ink = current ? kBlack : used ? kWhite : kEmpty;
        text(vg, x + 5.f, y + 10.f, string::f("%d", s + 1), 13.f, ink, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        text(vg, x + cw / 2.f, y + ch / 2.f + 6.f, used ? "model" : "empty", 11.f, ink,
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
}

void Screen::drawSetup(NVGcontext* vg) {
    const int store = module ? clamp(module->storeMode, 0, 5) : 2;
    const bool tod = module && module->trainOnDemand;
    const int range = module ? module->inputRange : 0;
    const int dis = module ? clamp(module->dislikeMode, 0, 4) : 0;
    const std::string rows[][2] = {
        { "Memory mode", kStoreModes[store] },
        { "Train on demand", tod ? "on" : "off" },
        { "CV input range", range ? "-5..5 V" : "0..10 V" },
        { "Dislike mode", kDislikeNames[dis] },
    };
    for (int i = 0; i < 4; ++i) {
        const float y = 22.f + i * 36.f;
        text(vg, 10.f, y + 17.f, rows[i][0], 14.f, kLightGrey, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        text(vg, kW - 10.f, y + 17.f, rows[i][1], 14.f, kCyan, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        rect(vg, 6.f, y + 34.f, kW - 12.f, 1.f, kGrid);
    }
    text(vg, 10.f, 180.f, "click a row to change it", 11.f, kDarkGrey, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    text(vg, 10.f, 200.f, string::f("net %d in (%d padded) > 16 > 16 > %d, 200 Hz", kNumInputs, kNetInputs, kNumOutputs),
         11.f, kDarkGrey, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
}

// --- mouse -------------------------------------------------------------------------

Vec Screen::toVirtual(Vec p) const { return Vec(p.x / box.size.x * kW, p.y / box.size.y * kH); }

void Screen::setPadFrom(Vec v) {
    if (!module) return;
    module->params[MEMLNautModule::PAD_X_PARAM].setValue(clamp((v.x - kMapX) / kMapS, 0.f, 1.f));
    module->params[MEMLNautModule::PAD_Y_PARAM].setValue(clamp(1.f - (v.y - kMapY) / kMapS, 0.f, 1.f));
}

void Screen::click(Vec v) {
    if (v.y < kTabH) {
        page_ = clamp(static_cast<int>(v.x / (kW / static_cast<float>(NUM_PAGES))), 0, NUM_PAGES - 1);
        return;
    }
    if (!module) return;
    switch (page_) {
        case MAP:
            if (v.x >= kMapX - 6.f && v.x <= kMapX + kMapS + 6.f && v.y >= kMapY - 6.f && v.y <= kMapY + kMapS + 6.f &&
                !d_.xyFromCV) {
                padDragging_ = true;
                setPadFrom(v);
            }
            break;
        case NO: {
            const int i = static_cast<int>((v.y - 22.f) / 39.f);
            if (i >= 0 && i < 5) module->dislikeMode = i;
            break;
        }
        case SLOTS: {
            if (v.y >= 23.f && v.y <= 43.f) {
                slotSaveMode_ = v.x < kW / 2.f;
                break;
            }
            const float cw = (kW - 12.f - 5.f * 6.f) / 6.f, ch = 78.f;
            const int col = static_cast<int>((v.x - 6.f) / (cw + 6.f));
            const int row = static_cast<int>((v.y - 50.f) / (ch + 7.f));
            if (v.x < 6.f || v.y < 50.f || col < 0 || col > 5 || row < 0 || row > 1) break;
            module->slotRequest = (slotSaveMode_ ? 0 : 1) * kNumSlots + row * 6 + col;
            break;
        }
        case SETUP: {
            const int i = static_cast<int>((v.y - 22.f) / 36.f);
            if (i == 0) module->storeMode = (module->storeMode + 1) % 6;
            if (i == 1) module->trainOnDemand = !module->trainOnDemand;
            if (i == 2) module->inputRange = module->inputRange ? 0 : 1;
            if (i == 3) page_ = NO;
            break;
        }
        default: break;
    }
}

void Screen::onButton(const ButtonEvent& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) return; // right-click: the module's menu
    if (e.action == GLFW_PRESS) {
        click(toVirtual(e.pos));
        e.consume(this);
    }
}

void Screen::onDragMove(const DragMoveEvent& e) {
    if (!padDragging_ || d_.xyFromCV) return;
    // Mouse position in this widget, from the rack's: zoom-proof, unlike summing deltas.
    const Vec origin = getRelativeOffset(Vec(), APP->scene->rack);
    setPadFrom(toVirtual(APP->scene->rack->getMousePos().minus(origin)));
}

void Screen::onDragEnd(const DragEndEvent& e) { padDragging_ = false; }

} // namespace memlnaut
