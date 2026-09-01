#include "render/PixelCanvas.h"

#include "core/Blit.h"

#include <algorithm>

#include "render/Palette.h"

namespace td::render {
namespace {

// Set by the profile's display option; see core/Blit.h for why this matters.
bool g_integerOnly = true;

core::BlitRect computeBlit() {
    return core::fitCanvas(GetScreenWidth(), GetScreenHeight(), kVirtualW, kVirtualH,
                           g_integerOnly);
}

}  // namespace

PixelCanvas::PixelCanvas() {
    rt_ = LoadRenderTexture(kVirtualW, kVirtualH);
    SetTextureFilter(rt_.texture, TEXTURE_FILTER_POINT);
}

PixelCanvas::~PixelCanvas() { UnloadRenderTexture(rt_); }

void PixelCanvas::begin() {
    BeginTextureMode(rt_);
    ClearBackground(palette::kBackdrop);
}

void PixelCanvas::end() { EndTextureMode(); }

float PixelCanvas::scale() const { return computeBlit().scale; }

void PixelCanvas::setIntegerScaling(bool on) { g_integerOnly = on; }

void PixelCanvas::blitToWindow() const {
    const auto b = computeBlit();
    // Negative source height: raylib render textures are stored y-flipped, and
    // forgetting this draws the entire game upside down.
    const Rectangle src{0.0f, 0.0f, static_cast<float>(kVirtualW),
                        -static_cast<float>(kVirtualH)};
    const Rectangle dst{b.offX, b.offY, static_cast<float>(kVirtualW * b.scale),
                        static_cast<float>(kVirtualH * b.scale)};
    DrawTexturePro(rt_.texture, src, dst, Vector2{0, 0}, 0.0f, WHITE);
}

core::Vec2 PixelCanvas::windowToVirtual(core::Vec2 windowPos) const {
    const auto b = computeBlit();
    // Mouse mapping MUST use the same blit as the draw, or clicks land where the
    // player is not looking. Both go through computeBlit for exactly that reason.
    return core::Vec2{(windowPos.x - b.offX) / b.scale, (windowPos.y - b.offY) / b.scale};
}

}  // namespace td::render
