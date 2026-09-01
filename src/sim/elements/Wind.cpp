// The three Wind specialisations. Wind trades raw damage for reach: it spreads
// to enemies the tower never targeted, or buys the towers behind it another pass.

#include <algorithm>
#include <vector>

#include "sim/AreaEffects.h"
#include "sim/ElementBehavior.h"
#include "sim/World.h"
#include "sim/elements/Factories.h"

namespace td::sim {
namespace {

// --- Shock ----------------------------------------------------------------
// Jumps onward from whatever was hit, carrying a decaying fraction. Fires per
// HIT, so a piercing volley chains from each enemy it passes through.
class ShockBehavior : public ElementBehavior {
public:
    ShockBehavior(const core::StatBlock& s, std::string dmgType)
        : jumps_(std::max(0, static_cast<int>(s.get("wind.shock.jumps")))),
          radius_(s.get("wind.shock.radius")),
          damagePct_(s.get("wind.shock.damagePct") * s.get("wind.potency", 1.0f)),
          falloff_(s.get("wind.shock.falloff", 0.6f)),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float dealt) override {
        if (dealt <= 0.0f || jumps_ <= 0) return;
        auto& r = w.reg();
        const auto* start = r.try_get<Position>(target);
        if (!start) return;

        std::vector<entt::entity> hit{target};
        core::Vec2 from = start->v;
        float carry = dealt * damagePct_;
        for (int j = 0; j < jumps_; ++j) {
            const auto near = enemiesWithin(w, from, radius_, hit);
            if (near.empty()) break;
            const auto next = near.front();  // nearest first, so jumps are deterministic
            dealTyped(w, next, carry, type_);
            if (const auto* p = r.try_get<Position>(next)) {
                w.emit({VisualEvent::Kind::Hit, p->v, {}, carry, false, "shock"});
                from = p->v;
            }
            hit.push_back(next);
            carry *= falloff_;
        }
    }

private:
    int jumps_;
    float radius_, damagePct_, falloff_;
    std::string type_;
};

// --- Gust -----------------------------------------------------------------
// Shoves the target back along the path it walked, which buys every tower
// covering that stretch another pass. Bosses are shoved far less: a boss that
// could be held at the entrance by knockback would never arrive.
class GustBehavior : public ElementBehavior {
public:
    explicit GustBehavior(const core::StatBlock& s)
        : pushTiles_(s.get("wind.gust.pushTiles") * s.get("wind.potency", 1.0f)),
          chance_(s.get("wind.gust.chance")),
          bossMult_(s.get("wind.gust.bossPushMult", 0.25f)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        auto* pf = r.try_get<PathFollower>(target);
        if (!pf) return;
        if (!w.rng().chance(chance_)) return;
        const float push = pushTiles_ * (r.all_of<Boss>(target) ? bossMult_ : 1.0f);
        // Never past the start of the path, or an enemy could be pushed to a
        // negative distance and reappear at the spawn.
        pf->distance = std::max(0.0f, pf->distance - push);
    }

private:
    float pushTiles_, chance_, bossMult_;
};

// --- Cyclone --------------------------------------------------------------
// Every Nth shot lands as a vortex. Counted per tower, so a fast tower spins one
// up constantly and a slow one rarely but from further away.
class CycloneBehavior : public ElementBehavior {
public:
    CycloneBehavior(const core::StatBlock& s, std::string dmgType)
        : radius_(s.get("wind.cyclone.radius") + s.get("wind.potency", 1.0f) - 1.0f),
          damage_(s.get("wind.cyclone.damage") * s.get("wind.potency", 1.0f)),
          slowPct_(std::min(0.9f, s.get("wind.cyclone.slowPct"))),
          slowDuration_(s.get("wind.cyclone.slowDuration") * s.get("wind.duration", 1.0f)),
          everyN_(std::max(1, static_cast<int>(s.get("wind.cyclone.everyNShots", 4.0f)))),
          type_(std::move(dmgType)) {}

    void onShoot(World& w, entt::entity tower, entt::entity projectile) override {
        auto& r = w.reg();
        auto* counter = r.try_get<ShotCounter>(tower);
        if (!counter) return;
        ++counter->n;
        if (counter->n % everyN_ == 0) r.emplace_or_replace<CyclonePayload>(projectile);
    }

    void onHit(World& w, entt::entity projectile, entt::entity target, float) override {
        auto& r = w.reg();
        if (projectile == entt::null || !r.valid(projectile) ||
            !r.all_of<CyclonePayload>(projectile)) {
            return;
        }
        const auto* origin = r.try_get<Position>(target);
        if (!origin) return;
        const core::Vec2 centre = origin->v;

        w.emit({VisualEvent::Kind::Quake, centre, {}, radius_, false, "cyclone"});

        for (const auto e : enemiesWithin(w, centre, radius_)) {
            dealTyped(w, e, damage_, type_);
            if (auto* slow = r.try_get<Slowed>(e)) {
                slow->pct = std::max(slow->pct, slowPct_);  // maximum, never compounding
                slow->remaining = std::max(slow->remaining, slowDuration_);
            } else {
                r.emplace<Slowed>(e, slowPct_, slowDuration_);
            }
        }
        r.remove<CyclonePayload>(projectile);  // one vortex per marked shot
    }

private:
    float radius_, damage_, slowPct_, slowDuration_;
    int everyN_;
    std::string type_;
};

}  // namespace

std::unique_ptr<ElementBehavior> makeWind(const std::string& spec, const core::StatBlock& stats,
                                          const std::string& damageType) {
    if (spec == "shock") return std::make_unique<ShockBehavior>(stats, damageType);
    if (spec == "gust") return std::make_unique<GustBehavior>(stats);
    if (spec == "cyclone") return std::make_unique<CycloneBehavior>(stats, damageType);
    return std::make_unique<ElementBehavior>();  // imbued but not specialised
}

}  // namespace td::sim
