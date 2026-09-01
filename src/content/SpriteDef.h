#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace td::content {

// A decoded sprite: straight RGBA8, row-major, top-left origin. Kept free of
// raylib so sprite decoding stays unit-testable without a window.
struct SpriteDef {
    std::string id;
    int w = 0;
    int h = 0;
    std::vector<uint32_t> pixels;  // 0xRRGGBBAA

    uint32_t at(int x, int y) const {
        return pixels[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    }
};

}  // namespace td::content
