#pragma once

#include <string>
#include <vector>

#include "raylib.h"

#include "core/Vec2.h"
#include "sim/VisualEvent.h"

namespace td::render {

// Particles, floating damage numbers and screen shake. Purely presentational:
// nothing here feeds back into the simulation, so effects can be tuned or
// removed without touching game behaviour.
class Effects {
public:
    void consume(const std::vector<sim::VisualEvent>& events);
    void update(float dt);
    void drawWorldLayer() const;  // behind the HUD, in tile-pixel space
    core::Vec2 shakeOffset() const;
    // Camera Comfort: scales all screen shake. Shake is feedback, so the option
    // offers "less" as well as "none" -- turning it off entirely costs the
    // player information about what just happened.
    void setShakeScale(float s) { shakeScale_ = s; }

private:
    struct Particle {
        core::Vec2 pos, vel;
        float life = 0.0f, maxLife = 1.0f;
        float size = 1.0f, drag = 1.0f, gravity = 0.0f;
        Color color{255, 255, 255, 255};
    };
    struct Number {
        core::Vec2 pos;
        float life = 0.0f, maxLife = 1.0f;
        int value = 0;
        bool crit = false;
        // Tinted by damage type, so "my earth tower does nothing here" is
        // learnable in the moment rather than three waves later.
        Color color{245, 245, 235, 255};
        int mark = 0;  // -1 resisted, +1 vulnerable, 0 neutral
    };
    struct Ring {
        core::Vec2 pos;
        float life = 0.0f, maxLife = 1.0f, radius = 1.0f;
        Color color{255, 255, 255, 255};
    };

    void burst(core::Vec2 at, core::Vec2 dir, int count, Color c, float speed, float life);
    void addShake(float amount);

    std::vector<Particle> particles_;
    std::vector<Number> numbers_;
    std::vector<Ring> rings_;
    float shakeScale_ = 1.0f;
    float shake_ = 0.0f;
    float time_ = 0.0f;
};

}  // namespace td::render
