#include "sim/AreaEffects.h"

#include <algorithm>

#include "sim/World.h"

namespace td::sim {

float resistOf(World& w, entt::entity target, const std::string& damageType) {
    const auto* tag = w.reg().try_get<EnemyTag>(target);
    if (!tag) return 1.0f;
    return w.defs().enemy(tag->defId).resistTo(damageType);
}

void dealTyped(World& w, entt::entity target, float amount, const std::string& damageType) {
    auto* hp = w.reg().try_get<Health>(target);
    if (!hp) return;
    hp->hp -= amount * resistOf(w, target, damageType);
}

void dealFlat(World& w, entt::entity target, float amount) {
    auto* hp = w.reg().try_get<Health>(target);
    if (!hp) return;
    hp->hp -= amount;
}

std::vector<entt::entity> enemiesWithin(World& w, core::Vec2 centre, float radius,
                                        const std::vector<entt::entity>& exclude) {
    std::vector<std::pair<float, entt::entity>> found;
    w.reg().view<const Position, const Health, const EnemyTag>().each(
        [&](entt::entity e, const Position& p, const Health& hp, const EnemyTag&) {
            if (hp.hp <= 0.0f) return;
            if (std::find(exclude.begin(), exclude.end(), e) != exclude.end()) return;
            const float d = core::distance(centre, p.v);
            if (d > radius) return;
            found.emplace_back(d, e);
        });
    // Nearest first, and entity id breaks ties, so chain order is deterministic
    // and does not depend on EnTT's internal storage order.
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        return static_cast<uint32_t>(a.second) < static_cast<uint32_t>(b.second);
    });
    std::vector<entt::entity> out;
    out.reserve(found.size());
    for (const auto& [d, e] : found) out.push_back(e);
    return out;
}

void areaDamage(World& w, core::Vec2 centre, float radius, float amount,
                const std::string& damageType, float falloff,
                const std::vector<entt::entity>& exclude) {
    if (radius <= 0.0f || amount <= 0.0f) return;
    for (const auto e : enemiesWithin(w, centre, radius, exclude)) {
        const auto* p = w.reg().try_get<Position>(e);
        if (!p) continue;
        const float t = std::clamp(core::distance(centre, p->v) / radius, 0.0f, 1.0f);
        const float scale = 1.0f + (falloff - 1.0f) * t;  // 1.0 at centre -> falloff at rim
        dealTyped(w, e, amount * scale, damageType);
    }
}

}  // namespace td::sim
