#pragma once

#include <string>

#include <entt/entt.hpp>

#include "core/Vec2.h"

namespace td::sim {

// Things worth reacting to visually. The simulation emits these and stays free
// of raylib; the renderer drains them and decides what they look like. Keeping
// the queue here rather than calling into the renderer is what lets the whole
// game still run headlessly in tests.
struct VisualEvent {
    enum class Kind { Shot, Hit, Death, Quake, Leak, Build };

    Kind kind = Kind::Hit;
    core::Vec2 pos;
    core::Vec2 dir;
    float value = 0.0f;  // damage for Hit, radius for Quake
    bool crit = false;
    std::string tag;  // enemy id, element spec, whatever the effect needs
};

}  // namespace td::sim
