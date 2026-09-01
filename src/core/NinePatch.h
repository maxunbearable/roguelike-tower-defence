#pragma once

#include <array>
#include <algorithm>
#include <utility>
#include <vector>

namespace td::core {

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

struct NinePatchQuad {
    Rect src;
    Rect dst;
};

// Splits a square-bordered source texture into nine pieces and lays them over an
// arbitrary destination rectangle: the four corners keep their source size, the
// four edges stretch along one axis, and the centre stretches along both.
//
// This is the only way to draw a painted panel at a UI-driven size without
// scaling its border, which on pixel art smears the outline into mush.
//
// Returned row-major: indices 0,2,6,8 are the corners, 4 is the centre.
inline std::array<NinePatchQuad, 9> ninePatch(float srcW, float srcH, float inset, float dstX,
                                              float dstY, float dstW, float dstH) {
    // A destination narrower than two insets cannot hold both corners at full
    // size. Shrinking the inset is the graceful failure; keeping it would give
    // the centre a negative extent, which raylib draws as garbage.
    const float ix = std::min(inset, dstW * 0.5f);
    const float iy = std::min(inset, dstH * 0.5f);

    // Source insets stay put: the art's border is a fixed width regardless of
    // how small the destination got.
    const float sx = std::min(inset, srcW * 0.5f);
    const float sy = std::min(inset, srcH * 0.5f);

    const float srcXs[3] = {0.0f, sx, srcW - sx};
    const float srcWs[3] = {sx, srcW - 2.0f * sx, sx};
    const float srcYs[3] = {0.0f, sy, srcH - sy};
    const float srcHs[3] = {sy, srcH - 2.0f * sy, sy};

    const float dstXs[3] = {dstX, dstX + ix, dstX + dstW - ix};
    const float dstWs[3] = {ix, dstW - 2.0f * ix, ix};
    const float dstYs[3] = {dstY, dstY + iy, dstY + dstH - iy};
    const float dstHs[3] = {iy, dstH - 2.0f * iy, iy};

    std::array<NinePatchQuad, 9> out{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            auto& q = out[static_cast<size_t>(row * 3 + col)];
            q.src = {srcXs[col], srcYs[row], srcWs[col], srcHs[row]};
            q.dst = {dstXs[col], dstYs[row], std::max(0.0f, dstWs[col]),
                     std::max(0.0f, dstHs[row])};
        }
    }
    return out;
}

// Splits a destination length into whole repeats of a source length, clipping
// the last one. Returned as (offset from the region start, length) pairs.
//
// A nine-slice must TILE its stretchable regions, not scale them: stretching a
// 32px patterned cell across 1300px turns the parchment's hatching into giant
// blocks. Corners still draw 1:1, so only the edges and centre come through
// here.
inline std::vector<std::pair<float, float>> tileRuns(float srcLen, float dstLen) {
    std::vector<std::pair<float, float>> runs;
    if (srcLen <= 0.0f || dstLen <= 0.0f) return runs;
    for (float at = 0.0f; at < dstLen; at += srcLen) {
        runs.emplace_back(at, std::min(srcLen, dstLen - at));
    }
    return runs;
}

}  // namespace td::core
