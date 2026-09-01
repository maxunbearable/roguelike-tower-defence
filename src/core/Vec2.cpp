#include "core/Vec2.h"

#include <cmath>

namespace td::core {

float length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

float distance(Vec2 a, Vec2 b) { return length(b - a); }

Vec2 normalized(Vec2 v) {
    const float len = length(v);
    if (len <= 0.0f) return {0.0f, 0.0f};
    return {v.x / len, v.y / len};
}

Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

}  // namespace td::core
