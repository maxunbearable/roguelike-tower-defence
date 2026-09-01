// The three Fire specialisations.
//
// Each is gated on a DIFFERENT property of a hit, which is what makes the three
// tower firing profiles separate them with no rule naming any tower:
//   burn  scales with hit MAGNITUDE -> rewards heavy, slow shots
//   blast scales with hit COUNT     -> rewards multishot
//   melt  scales with hit RATE      -> rewards fast shots

#include <algorithm>

#include "sim/AreaEffects.h"
#include "sim/ElementBehavior.h"
#include "sim/World.h"
#include "sim/elements/Factories.h"

namespace td::sim {
namespace {

// --- Burn -----------------------------------------------------------------
// Takes a fraction of the damage that actually landed and burns it over time.
// A 400-damage sniper shot therefore ignites far harder than a 20-damage volley
// arrow, which is the whole reason Sniper+Burn differs from Hunter+Burn.
class BurnBehavior : public ElementBehavior {
public:
    BurnBehavior(const core::StatBlock& s, std::string dmgType)
        : pctPerHit_(s.get("fire.burn.pctPerHit") * s.get("fire.potency", 1.0f)),
          maxStacks_(std::max(1.0f, s.get("fire.burn.maxStacks"))),
          duration_(s.get("fire.burn.duration") * s.get("fire.duration", 1.0f)),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float dealt) override {
        if (dealt <= 0.0f || duration_ <= 0.0f) return;
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;

        const float add = dealt * pctPerHit_ / duration_;
        if (auto* b = r.try_get<Burning>(target)) {
            // A bigger hit raises the ceiling as well as the rate, so a heavy
            // shot is never wasted on an already-burning target.
            b->maxDps = std::max(b->maxDps, add * maxStacks_);
            b->dps = std::min(b->maxDps, b->dps + add);
            b->remaining = std::max(b->remaining, duration_);
            b->damageType = type_;
        } else {
            r.emplace<Burning>(target, add, add * maxStacks_, duration_, type_);
        }
    }

private:
    float pctPerHit_, maxStacks_, duration_;
    std::string type_;
};

// --- Blast ----------------------------------------------------------------
// Every hit explodes. Because it fires per HIT rather than per shot, a multishot
// spread detonates once per arrow.
class BlastBehavior : public ElementBehavior {
public:
    BlastBehavior(const core::StatBlock& s, std::string dmgType)
        : radius_(s.get("fire.blast.radius") + s.get("fire.potency", 1.0f) - 1.0f),
          damagePct_(s.get("fire.blast.damagePct") * s.get("fire.potency", 1.0f)),
          falloff_(s.get("fire.blast.falloff", 0.5f)),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float dealt) override {
        if (dealt <= 0.0f) return;
        const auto* pos = w.reg().try_get<Position>(target);
        if (!pos) return;
        w.emit({VisualEvent::Kind::Quake, pos->v, {}, radius_, false, "blast"});
        // The struck enemy is excluded: it already took the hit itself.
        areaDamage(w, pos->v, radius_, dealt * damagePct_, type_, falloff_, {target});
    }

private:
    float radius_, damagePct_, falloff_;
    std::string type_;
};

// --- Melt -----------------------------------------------------------------
// Peels resistance toward neutral. The cap is what stops a high-rate tower from
// erasing resistance entirely, so a resistant map still demands an answer --
// melt only softens the wall.
class MeltBehavior : public ElementBehavior {
public:
    MeltBehavior(const core::StatBlock& s, std::string dmgType)
        : perHit_(s.get("fire.melt.resistPerHit") * s.get("fire.potency", 1.0f)),
          maxAmount_(s.get("fire.melt.maxResist")),
          duration_(s.get("fire.melt.duration") * s.get("fire.duration", 1.0f)),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;
        if (auto* m = r.try_get<Melted>(target)) {
            m->amount = std::min(maxAmount_, m->amount + perHit_);
            m->remaining = std::max(m->remaining, duration_);
        } else {
            r.emplace<Melted>(target, std::min(maxAmount_, perHit_), duration_);
        }
    }

private:
    float perHit_, maxAmount_, duration_;
    std::string type_;
};

}  // namespace

std::unique_ptr<ElementBehavior> makeFire(const std::string& spec, const core::StatBlock& stats,
                                          const std::string& damageType) {
    if (spec == "burn") return std::make_unique<BurnBehavior>(stats, damageType);
    if (spec == "blast") return std::make_unique<BlastBehavior>(stats, damageType);
    if (spec == "melt") return std::make_unique<MeltBehavior>(stats, damageType);
    return std::make_unique<ElementBehavior>();  // imbued but not specialised
}

}  // namespace td::sim
