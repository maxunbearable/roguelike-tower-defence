#include "sim/systems/EnemySystems.h"

#include <vector>

#include <algorithm>

#include "sim/World.h"

namespace td::sim {

void runWaveSystem(World& w, float dt) {
    // Each group carries its own spawn parameters, so groups belonging to two
    // different waves can be in flight at once (an early overlap call).
    for (auto& rt : w.groups_) {
        if (rt.remaining <= 0) continue;
        rt.timer -= dt;
        while (rt.timer <= 0.0f && rt.remaining > 0) {
            w.spawnEnemy(rt.enemyId, rt.hpMult, rt.armorAdd, rt.bountyMult);
            --rt.remaining;
            rt.timer += rt.interval;
        }
    }

    if (w.allGroupsExhausted() && w.aliveEnemies() == 0) {
        ++w.waveIndex_;
        if (w.waveIndex_ >= w.waveCount()) {
            w.phase_ = Phase::Cleared;
        } else {
            w.phase_ = Phase::Build;
            w.buildTimer_ = w.map_->buildTime;
        }
    }
}

void runMovementSystem(World& w, float dt) {
    const auto& path = w.path();
    auto& r = w.reg();
    // Hard crowd control on a boss becomes a heavy slow, and soft slows are
    // capped. A boss that can be held still by one cheap status is not a fight,
    // and every map builds to one.
    constexpr float kBossHardCcSpeed = 0.25f;
    constexpr float kBossMaxSlow = 0.55f;

    r.view<Position, PrevPosition, PathFollower, const Speed>().each(
        [&](entt::entity e, Position& pos, PrevPosition& prev, PathFollower& pf,
            const Speed& sp) {
            prev.v = pos.v;

            const bool boss = r.all_of<Boss>(e);
            float speed = sp.base;
            if (r.all_of<Petrified>(e) || r.all_of<Frozen>(e)) {
                speed = boss ? sp.base * kBossHardCcSpeed : 0.0f;
            } else if (const auto* slow = r.try_get<Slowed>(e)) {
                // Slows take the maximum percentage rather than compounding, so
                // stacking two sources can never freeze an enemy outright.
                const float pct = boss ? std::min(slow->pct, kBossMaxSlow) : slow->pct;
                speed *= (1.0f - pct);
            }

            pf.distance += speed * dt;
            pos.v = path.positionAt(pf.distance);
        });
}

void runLeakSystem(World& w, float dt) {
    (void)dt;
    const float end = w.path().totalLength();

    // Collected first, destroyed after: destroying entities while iterating an
    // EnTT view invalidates it.
    std::vector<entt::entity> leaked;
    w.reg().view<const PathFollower, const EnemyTag>().each(
        [&](entt::entity e, const PathFollower& pf, const EnemyTag&) {
            if (pf.distance >= end) leaked.push_back(e);
        });

    for (const auto e : leaked) {
        if (const auto* p = w.reg().try_get<Position>(e)) {
            w.emit({VisualEvent::Kind::Leak, p->v, {}, 0.0f, false, {}});
        }
        w.reg().destroy(e);
        w.loseLife(1);
    }
}

}  // namespace td::sim
