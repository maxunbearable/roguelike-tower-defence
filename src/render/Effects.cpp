#include "render/Effects.h"

#include <algorithm>
#include <cmath>

#include <string>

#include "render/Palette.h"
#include "render/PixelCanvas.h"

namespace td::render {
namespace {

// Deterministic pseudo-random, seeded from a counter. Effects must never touch
// the simulation's RNG or they would desync a seeded run.
uint32_t g_effectSeed = 12345u;
float frand() {
    g_effectSeed = g_effectSeed * 1664525u + 1013904223u;
    return static_cast<float>((g_effectSeed >> 8) & 0xFFFFFF) / 16777216.0f;
}
float frand(float lo, float hi) { return lo + frand() * (hi - lo); }

Color fade(Color c, float a) {
    c.a = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
    return c;
}

}  // namespace

void Effects::addShake(float amount) { shake_ = std::min(5.0f, shake_ + amount); }

void Effects::burst(core::Vec2 at, core::Vec2 dir, int count, Color c, float speed, float life) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = at;
        // Biased along the impact direction, so sparks spray the way the shot
        // was travelling rather than puffing symmetrically.
        const float a = std::atan2(dir.y, dir.x) + frand(-1.1f, 1.1f);
        const float sp = speed * frand(0.4f, 1.0f);
        p.vel = {std::cos(a) * sp, std::sin(a) * sp};
        p.maxLife = p.life = life * frand(0.6f, 1.0f);
        p.size = frand() > 0.7f ? 2.0f : 1.0f;
        p.drag = 0.90f;
        p.gravity = 26.0f;
        p.color = c;
        particles_.push_back(p);
    }
}

void Effects::consume(const std::vector<sim::VisualEvent>& events) {
    for (const auto& e : events) {
        switch (e.kind) {
            case sim::VisualEvent::Kind::Shot: {
                // Muzzle spark, offset along the barrel.
                Particle p;
                p.pos = {e.pos.x + e.dir.x * 0.3f, e.pos.y + e.dir.y * 0.3f - 0.35f};
                p.vel = {e.dir.x * 0.6f, e.dir.y * 0.6f};
                p.maxLife = p.life = 0.06f;
                p.size = 2.0f;
                p.drag = 0.8f;
                p.color = palette::kProjectile;
                particles_.push_back(p);
                break;
            }
            case sim::VisualEvent::Kind::Hit: {
                burst(e.pos, e.dir, e.crit ? 9 : 4,
                      e.crit ? Color{255, 236, 170, 255} : Color{240, 227, 160, 255},
                      e.crit ? 5.0f : 3.2f, e.crit ? 0.30f : 0.20f);
                if (e.value >= 1.0f) {
                    // The tag is "<damageType>" with an optional trailing '-'
                    // for resisted or '+' for vulnerable.
                    std::string type = e.tag;
                    int mark = 0;
                    if (!type.empty() && (type.back() == '-' || type.back() == '+')) {
                        mark = type.back() == '+' ? 1 : -1;
                        type.pop_back();
                    }
                    Color c = type.empty() ? Color{245, 245, 235, 255}
                                           : palette::damageTypeColor(type);
                    // Resisted hits read dim and vulnerable ones read hot, on top
                    // of the type hue, so direction survives at a glance.
                    if (mark < 0) c = Color{static_cast<unsigned char>(c.r * 0.55f),
                                            static_cast<unsigned char>(c.g * 0.55f),
                                            static_cast<unsigned char>(c.b * 0.62f), 255};
                    numbers_.push_back({{e.pos.x, e.pos.y - 0.35f}, 0.62f, 0.62f,
                                        static_cast<int>(e.value + 0.5f), e.crit, c, mark});
                }
                if (e.crit) addShake(0.9f);
                break;
            }
            case sim::VisualEvent::Kind::Death: {
                Color c{124, 200, 108, 255};
                if (e.tag == "wolf") c = Color{150, 152, 168, 255};
                else if (e.tag == "goblin") c = Color{96, 142, 72, 255};
                else if (e.tag == "wraith") c = Color{232, 200, 88, 255};
                burst(e.pos, {0.0f, -1.0f}, 12, c, 4.0f, 0.42f);
                rings_.push_back({e.pos, 0.22f, 0.22f, 0.5f, c});
                addShake(0.5f);
                break;
            }
            case sim::VisualEvent::Kind::Quake: {
                rings_.push_back({e.pos, 0.38f, 0.38f, e.value, Color{224, 160, 96, 255}});
                burst(e.pos, {0.0f, -1.0f}, 10, Color{169, 131, 90, 255}, 3.0f, 0.35f);
                addShake(1.6f);
                break;
            }
            case sim::VisualEvent::Kind::Leak: {
                rings_.push_back({e.pos, 0.4f, 0.4f, 0.9f, palette::kSpawn});
                addShake(2.2f);
                break;
            }
            case sim::VisualEvent::Kind::Build: {
                rings_.push_back({e.pos, 0.3f, 0.3f, 0.8f, Color{255, 255, 255, 255}});
                break;
            }
        }
    }
}

void Effects::update(float dt) {
    time_ += dt;
    shake_ = std::max(0.0f, shake_ - dt * 12.0f);

    for (auto& p : particles_) {
        p.life -= dt;
        p.vel.y += p.gravity * dt * (1.0f / 32.0f);
        p.vel = p.vel * std::pow(p.drag, dt * 60.0f);
        p.pos = p.pos + p.vel * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     particles_.end());

    for (auto& n : numbers_) {
        n.life -= dt;
        n.pos.y -= dt * 0.9f;
    }
    numbers_.erase(std::remove_if(numbers_.begin(), numbers_.end(),
                                  [](const Number& n) { return n.life <= 0.0f; }),
                   numbers_.end());

    for (auto& r : rings_) r.life -= dt;
    rings_.erase(std::remove_if(rings_.begin(), rings_.end(),
                                [](const Ring& r) { return r.life <= 0.0f; }),
                 rings_.end());
}

core::Vec2 Effects::shakeOffset() const {
    if (shake_ <= 0.01f) return {0.0f, 0.0f};
    // Whole-pixel offsets only: a sub-pixel shake would blur the art.
    const float a = time_ * 47.0f;
    return {std::round(std::sin(a * 1.7f) * shake_), std::round(std::cos(a * 2.3f) * shake_)};
}

void Effects::drawWorldLayer() const {
    for (const auto& r : rings_) {
        const float t = 1.0f - (r.life / r.maxLife);
        const float rad = r.radius * kTile * (0.35f + t * 0.9f);
        DrawCircleLines(static_cast<int>(r.pos.x * kTile), static_cast<int>(r.pos.y * kTile), rad,
                        fade(r.color, 1.0f - t));
    }
    for (const auto& p : particles_) {
        const float t = p.life / p.maxLife;
        const int s = static_cast<int>(p.size);
        DrawRectangle(static_cast<int>(p.pos.x * kTile), static_cast<int>(p.pos.y * kTile), s, s,
                      fade(p.color, t));
    }
    for (const auto& n : numbers_) {
        const float t = n.life / n.maxLife;
        const char* txt = TextFormat("%d", n.value);
        const int size = n.crit ? 20 : 10;
        const int x = static_cast<int>(n.pos.x * kTile) - MeasureText(txt, size) / 2;
        const int y = static_cast<int>(n.pos.y * kTile);
        DrawText(txt, x + 1, y + 1, size, fade(Color{0, 0, 0, 255}, t * 0.6f));
        DrawText(txt, x, y, size, fade(n.crit ? Color{255, 232, 150, 255} : n.color, t));
        // A vulnerable hit gets a small up-chevron; resisted gets a down one.
        if (n.mark != 0) {
            const char* m = n.mark > 0 ? "^" : "v";
            DrawText(m, x + MeasureText(txt, size) + 2, y, 10,
                     fade(n.mark > 0 ? palette::kVulnerable : palette::kResistant, t));
        }
    }
}

}  // namespace td::render
