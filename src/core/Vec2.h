#pragma once

namespace td::core {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }

    friend constexpr bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
};

float length(Vec2 v);
float distance(Vec2 a, Vec2 b);
Vec2 normalized(Vec2 v);  // returns {0,0} for the zero vector
Vec2 lerp(Vec2 a, Vec2 b, float t);

}  // namespace td::core
