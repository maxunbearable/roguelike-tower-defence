#include "render/PixelCanvas.h"

#include <algorithm>

#include "render/Palette.h"

namespace td::render {
namespace {

struct Blit {
    int scale;
    float offX;
    float offY;
};

Blit computeBlit() {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const int s = std::max(1, std::min(sw / kVirtualW, sh / kVirtualH));
    return {s, static_cast<float>((sw - kVirtualW * s) / 2),
            static_cast<float>((sh - kVirtualH * s) / 2)};
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

int PixelCanvas::scale() const { return computeBlit().scale; }

void PixelCanvas::blitToWindow() const {
    const Blit b = computeBlit();
    // Negative source height: raylib render textures are stored y-flipped, and
    // forgetting this draws the entire game upside down.
    const Rectangle src{0.0f, 0.0f, static_cast<float>(kVirtualW),
                        -static_cast<float>(kVirtualH)};
    const Rectangle dst{b.offX, b.offY, static_cast<float>(kVirtualW * b.scale),
                        static_cast<float>(kVirtualH * b.scale)};
    DrawTexturePro(rt_.texture, src, dst, Vector2{0, 0}, 0.0f, WHITE);
}

core::Vec2 PixelCanvas::windowToVirtual(core::Vec2 windowPos) const {
    const Blit b = computeBlit();
    return core::Vec2{(windowPos.x - b.offX) / static_cast<float>(b.scale),
                      (windowPos.y - b.offY) / static_cast<float>(b.scale)};
}

}  // namespace td::render
