#include "ui/RadialMenu.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"

#include "render/Palette.h"
#include "render/PixelCanvas.h"
#include "ui/Paint.h"

namespace td::ui {
namespace {

// Both were raised: at radius 20 a fitted building icon came out around 34px
// and read as a dark smudge, and a 58px ring put the buttons on top of the
// tower being upgraded.
constexpr float kRing = 74.0f;    // distance from centre to each button
constexpr float kButton = 27.0f;  // button half-extent
constexpr float kPi = 3.14159265f;

}  // namespace

void RadialMenu::open(int tileX, int tileY, core::Vec2 centre, std::vector<RadialItem> items) {
    open_ = true;
    tileX_ = tileX;
    tileY_ = tileY;
    centre_ = centre;
    items_ = std::move(items);
}

void RadialMenu::close() {
    open_ = false;
    items_.clear();
    tileX_ = tileY_ = -1;
}

core::Vec2 RadialMenu::slotOf(size_t i) const {
    const size_t n = items_.empty() ? 1 : items_.size();
    // A single option sits directly above rather than awkwardly to one side.
    if (n == 1) return {centre_.x, centre_.y - kRing};
    const float a = -kPi * 0.5f + (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(n);
    return {centre_.x + std::cos(a) * kRing, centre_.y + std::sin(a) * kRing};
}

int RadialMenu::hitTest(core::Vec2 mouse) const {
    if (!open_) return -1;
    for (size_t i = 0; i < items_.size(); ++i) {
        const core::Vec2 p = slotOf(i);
        if (core::distance(p, mouse) <= kButton + 2.0f) return static_cast<int>(i);
    }
    return -1;
}

bool RadialMenu::contains(core::Vec2 mouse) const {
    if (!open_) return false;
    return core::distance(centre_, mouse) <= kRing + kButton + 6.0f;
}

void RadialMenu::draw(const render::SpriteAtlas& atlas, core::Vec2 mouse, int gold) const {
    if (!open_) return;
    using namespace td::render;
    using namespace td::ui::paint;

    const int hovered = hitTest(mouse);
    const int cx = static_cast<int>(centre_.x);
    const int cy = static_cast<int>(centre_.y);

    // A ring rather than a filled disc. The old translucent disc covered the
    // very tower the player is deciding about.
    DrawCircleLines(cx, cy, kRing, Color{255, 255, 255, 40});
    DrawCircleLines(cx, cy, kRing + 1, Color{16, 14, 24, 60});

    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& it = items_[i];
        const core::Vec2 p = slotOf(i);
        const bool hot = static_cast<int>(i) == hovered;
        const bool canAfford = it.enabled && gold >= it.cost;
        const int bx = static_cast<int>(p.x - kButton);
        const int by = static_cast<int>(p.y - kButton);
        const int bw = static_cast<int>(kButton * 2.0f);

        button(atlas, bx, by, bw, bw, hot, /*on=*/false, /*off=*/!canAfford);

        // Fitted, because an icon may be a 12px glyph or a 90px building.
        atlas.drawFitted(it.icon, p.x, p.y - 1, kButton * 1.5f,
                         canAfford ? WHITE : Color{150, 140, 128, 220});

        if (it.cost > 0) {
            const char* c = TextFormat("%d", it.cost);
            const int cw = MeasureText(c, 10);
            const int lx = static_cast<int>(p.x) - cw / 2;
            const int ly = static_cast<int>(p.y + kButton) + 1;
            DrawRectangle(lx - 4, ly - 2, cw + 8, 13, Color{20, 17, 28, 215});
            DrawText(c, lx, ly, 10, canAfford ? palette::kHpFill : Color{236, 128, 112, 255});
        }
    }

    // The selected tower's stats, beside the ring. Kingdom Rush's rule: the
    // description sits next to the menu so the player never has to buy something
    // to find out what it does. Nothing in this game showed a tower's damage.
    if (!info_.empty()) {
        constexpr int kRowH = 15;
        int pw = 210;
        for (const auto& row : info_) {
            const int need = 30 + MeasureText(row.label.c_str(), 10) +
                             MeasureText(row.value.c_str(), 10) +
                             (row.next.empty() ? 0 : MeasureText(row.next.c_str(), 10) + 18);
            pw = std::max(pw, need);
        }
        const int ph = 34 + static_cast<int>(info_.size()) * kRowH + 8;

        // Prefer the right of the ring; flip left when it would leave the board.
        int px = cx + static_cast<int>(kRing) + 26;
        if (px + pw > kVirtualW - 6) px = cx - static_cast<int>(kRing) - 26 - pw;
        px = std::clamp(px, 6, kVirtualW - pw - 6);
        int py = std::clamp(cy - ph / 2, 6, kPlayH - ph - 6);

        panel(atlas, px, py, pw, ph);
        DrawText(infoTitle_.c_str(), px + 14, py + 10, 20, kInk);
        int ry = py + 34;
        for (const auto& row : info_) {
            DrawText(row.label.c_str(), px + 14, ry, 10, kInkDim);
            if (row.next.empty()) {
                DrawText(row.value.c_str(),
                         px + pw - 14 - MeasureText(row.value.c_str(), 10), ry, 10, kInk);
            } else {
                // current -> next, with the improvement in green.
                const int nw = MeasureText(row.next.c_str(), 10);
                DrawText(row.next.c_str(), px + pw - 14 - nw, ry, 10, kInkGood);
                const char* arrow = ">";
                DrawText(arrow, px + pw - 20 - nw - MeasureText(arrow, 10), ry, 10, kInkDim);
                DrawText(row.value.c_str(),
                         px + pw - 26 - nw - MeasureText(arrow, 10) -
                             MeasureText(row.value.c_str(), 10),
                         ry, 10, kInkDim);
            }
            ry += kRowH;
        }
    }

    // The hovered item's name and what it does, on a painted panel. Kingdom
    // Rush's rule: the description sits beside the ring so the player never has
    // to commit to find out what a button means.
    if (hovered >= 0) {
        const auto& it = items_[static_cast<size_t>(hovered)];
        const std::string title =
            it.cost > 0 ? it.label + "   " + std::to_string(it.cost) + "g" : it.label;
        const int tw = std::max({MeasureText(title.c_str(), 20),
                                 MeasureText(it.detail.c_str(), 10), 150}) + 34;
        const int th = it.detail.empty() ? 52 : 68;

        int tx = cx - tw / 2;
        int ty = cy + static_cast<int>(kRing) + 30;
        if (ty + th > kPlayH) ty = cy - static_cast<int>(kRing) - 30 - th;
        tx = std::clamp(tx, 4, kVirtualW - tw - 4);
        ty = std::clamp(ty, 4, kPlayH - th - 4);

        panel(atlas, tx, ty, tw, th);
        DrawText(title.c_str(), tx + 17, ty + 12, 20,
                 gold >= it.cost ? kInk : kInkWarn);
        if (!it.detail.empty()) {
            DrawText(it.detail.c_str(), tx + 17, ty + 40, 10, kInkDim);
        }
    }
}

}  // namespace td::ui
