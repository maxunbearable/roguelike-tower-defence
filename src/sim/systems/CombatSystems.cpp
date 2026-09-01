#include "sim/systems/CombatSystems.h"

#include <algorithm>
#include <utility>
#include <cmath>
#include <vector>

#include "core/Damage.h"
#include "sim/ElementBehavior.h"
#include "sim/AreaEffects.h"
#include "sim/World.h"

namespace td::sim {
namespace {

// Enemies are drawn at roughly 0.3 tiles radius; this is the collision size a
// projectile must reach to count as a hit.
constexpr float kHitRadius = 0.35f;
constexpr float kProjectileLifetime = 3.0f;

bool better(TargetPriority p, float candScore, float bestScore) {
    switch (p) {
        case TargetPriority::First:
        case TargetPriority::Strongest: return candScore > bestScore;
        case TargetPriority::Last:
        case TargetPriority::Weakest:
        case TargetPriority::Closest: return candScore < bestScore;
    }
    return false;
}

float scoreFor(TargetPriority p, const core::Vec2& towerPos, const core::Vec2& enemyPos,
               float pathDistance, float hp) {
    switch (p) {
        case TargetPriority::First: return pathDistance;
        case TargetPriority::Last: return pathDistance;
        case TargetPriority::Strongest:
        case TargetPriority::Weakest: return hp;
        case TargetPriority::Closest: return core::distance(towerPos, enemyPos);
    }
    return pathDistance;
}

}  // namespace

void runTargetingSystem(World& w, float dt) {
    (void)dt;
    auto& r = w.reg();

    r.view<const Position, const TowerStats, TargetRef>().each(
        [&](const Position& tpos, const TowerStats& st, TargetRef& tref) {
            // Sticky targeting: keep the current target while it is alive and in
            // range, so a tower does not thrash between equidistant enemies.
            if (tref.e != entt::null && r.valid(tref.e) && r.all_of<EnemyTag, Position>(tref.e)) {
                const auto& epos = r.get<Position>(tref.e);
                if (core::distance(tpos.v, epos.v) <= st.range) return;
            }
            tref.e = entt::null;

            float best = 0.0f;
            bool haveBest = false;
            r.view<const Position, const PathFollower, const Health, const EnemyTag>().each(
                [&](entt::entity e, const Position& epos, const PathFollower& pf,
                    const Health& hp, const EnemyTag&) {
                    if (core::distance(tpos.v, epos.v) > st.range) return;
                    const float s = scoreFor(st.priority, tpos.v, epos.v, pf.distance, hp.hp);
                    if (!haveBest || better(st.priority, s, best)) {
                        best = s;
                        haveBest = true;
                        tref.e = e;
                    }
                });
        });
}

void runTowerBuffSystem(World& w, float dt) {
    (void)dt;
    auto& r = w.reg();

    // Collect the auras once rather than re-querying per tower: this is O(towers)
    // plus O(auras x towers), and a board holds few auras.
    struct Aura {
        core::Vec2 pos;
        float radius, damagePct, fireRatePct;
    };
    std::vector<Aura> auras;
    r.view<const Position, const TowerBuff>().each(
        [&](const Position& p, const TowerBuff& b) {
            if (b.radius > 0.0f) auras.push_back({p.v, b.radius, b.damagePct, b.fireRatePct});
        });

    r.view<const Position, const TowerTag>().each(
        [&](entt::entity tower, const Position& p, const TowerTag&) {
            float dmg = 1.0f, rate = 1.0f;
            for (const auto& a : auras) {
                if (core::distance(a.pos, p.v) > a.radius) continue;
                // A tower does not buff itself, or a lone forge would be a
                // strictly-better tower rather than a support choice.
                if (a.pos == p.v) continue;
                dmg += a.damagePct;
                rate += a.fireRatePct;
            }
            if (dmg == 1.0f && rate == 1.0f) {
                r.remove<Buffed>(tower);
            } else {
                r.emplace_or_replace<Buffed>(tower, dmg, rate);
            }
        });
}

void runFiringSystem(World& w, float dt) {
    auto& r = w.reg();

    struct Shot {
        core::Vec2 origin;
        core::Vec2 dir;
        TowerStats stats;
        entt::entity source;
        bool crit;
    };
    std::vector<Shot> shots;

    // RampUp is ticked here rather than in its own system because it is a
    // property of firing, and it must reset the instant the target changes.
    r.view<RampUp, const TargetRef>().each([&](RampUp& ramp, const TargetRef& tref) {
        if (tref.e != ramp.lastTarget) {
            ramp.current = 1.0f;
            ramp.lastTarget = tref.e;
        } else if (tref.e != entt::null) {
            ramp.current = std::min(ramp.maxMult, ramp.current + ramp.perSec * dt);
        } else {
            ramp.current = 1.0f;
        }
    });

    r.view<const Position, const TowerStats, const TargetRef, Cooldown>().each(
        [&](entt::entity tower, const Position& tpos, const TowerStats& st, const TargetRef& tref,
            Cooldown& cd) {
            cd.remaining -= dt;
            if (cd.remaining > 0.0f) return;
            if (tref.e == entt::null || !r.valid(tref.e) || !r.all_of<Position>(tref.e)) return;

            const core::Vec2 aim = core::normalized(r.get<Position>(tref.e).v - tpos.v);
            if (aim == core::Vec2{0.0f, 0.0f}) return;

            // Crit is rolled ONCE per shot, not per pierce hit, so a piercing
            // arrow behaves consistently along its whole flight.
            const bool crit = w.rng().chance(st.critChance);

            const auto* buff = r.try_get<Buffed>(tower);
            TowerStats boosted = st;
            if (buff) boosted.damage *= buff->damageMult;

            const int n = std::max(1, st.projectileCount);
            const float spreadStep = 8.0f * 3.14159265f / 180.0f;  // 8 degrees between arrows
            const float base = -spreadStep * static_cast<float>(n - 1) * 0.5f;
            for (int i = 0; i < n; ++i) {
                const float a = base + spreadStep * static_cast<float>(i);
                const float ca = std::cos(a), sa = std::sin(a);
                const core::Vec2 dir{aim.x * ca - aim.y * sa, aim.x * sa + aim.y * ca};
                shots.push_back(Shot{tpos.v, dir, boosted, tower, crit});
            }
            const float rampMult = [&] {
                const auto* ramp = r.try_get<RampUp>(tower);
                return ramp ? ramp->current : 1.0f;
            }();
            const float buffRate = buff ? buff->fireRateMult : 1.0f;
            cd.remaining = 1.0f / std::max(0.0001f, st.fireRate * rampMult * buffRate);
        });

    for (const auto& s : shots) {
        const auto p = r.create();
        r.emplace<Position>(p, s.origin);
        r.emplace<PrevPosition>(p, s.origin);
        Projectile proj;
        proj.damageType = s.stats.damageType;
        proj.dir = s.dir;
        proj.speed = s.stats.projectileSpeed;
        proj.damage = s.stats.damage;
        proj.armorPen = s.stats.armorPen;
        proj.critMult = s.stats.critMult;
        proj.crit = s.crit;
        proj.pierceLeft = s.stats.pierce;
        proj.source = s.source;
        r.emplace<Projectile>(p, std::move(proj));
        r.emplace<Lifetime>(p, kProjectileLifetime);
        if (const auto* er = r.try_get<ElementRef>(s.source); er && er->behavior) {
            er->behavior->onShoot(w, s.source, p);
        }
        w.emit({VisualEvent::Kind::Shot, s.origin, s.dir, 0.0f, s.crit, {}});
    }
}

void runTowerElementTick(World& w, float dt) {
    auto& r = w.reg();
    std::vector<std::pair<entt::entity, ElementBehavior*>> work;
    r.view<const TowerTag, const ElementRef>().each(
        [&](entt::entity e, const TowerTag&, const ElementRef& er) {
            if (er.behavior) work.emplace_back(e, er.behavior);
        });
    for (const auto& [tower, b] : work) b->onTowerTick(w, tower, dt);
}

void runProjectileSystem(World& w, float dt) {
    auto& r = w.reg();
    std::vector<entt::entity> spent;

    r.view<Position, PrevPosition, Projectile, Lifetime>().each(
        [&](entt::entity pe, Position& pos, PrevPosition& prev, Projectile& pr, Lifetime& life) {
            prev.v = pos.v;
            pos.v = pos.v + pr.dir * (pr.speed * dt);

            life.remaining -= dt;
            if (life.remaining <= 0.0f) {
                spent.push_back(pe);
                return;
            }

            bool consumed = false;
            r.view<const Position, Health, const Armor, const EnemyTag>().each(
                [&](entt::entity ee, const Position& epos, Health& hp, const Armor& armor,
                    const EnemyTag&) {
                    if (consumed || hp.hp <= 0.0f) return;
                    if (core::distance(pos.v, epos.v) > kHitRadius) return;
                    if (std::find(pr.alreadyHit.begin(), pr.alreadyHit.end(), ee) !=
                        pr.alreadyHit.end()) {
                        return;
                    }

                    float raw = pr.damage;
                    // Sniper's Execute: heavier against a healthy target.
                    if (const auto* ex = r.try_get<Execute>(pr.source)) {
                        if (hp.maxHp > 0.0f && (hp.hp / hp.maxHp) >= ex->threshold) {
                            raw *= ex->mult;
                        }
                    }
                    // Arcane hex: a marked target takes more from EVERY source,
                    // which is what makes one hexing tower raise a whole line.
                    if (const auto* amped = r.try_get<Amplified>(ee)) {
                        raw *= (1.0f + amped->pct);
                    }
                    // Shadow's wither: accumulated over the whole wave, and it
                    // amplifies every source, not just the tower that applied it.
                    if (const auto* wi = r.try_get<Withered>(ee)) {
                        raw *= (1.0f + wi->pct);
                    }
                    const auto* shred = r.try_get<ArmorShred>(ee);
                    float resist = w.defs().enemy(r.get<EnemyTag>(ee).defId).resistTo(pr.damageType);
                    // Fire's melt peels resistance toward neutral, so a resistant
                    // map stops being a hard wall against one damage type.
                    if (const auto* melted = r.try_get<Melted>(ee)) {
                        resist = std::min(1.0f, resist + melted->amount);
                    }

                    const float dealt =
                        core::computeDamage({.raw = raw,
                                             .crit = pr.crit,
                                             .critMult = pr.critMult,
                                             .targetArmor = armor.value,
                                             .armorShred = shred ? shred->amount : 0.0f,
                                             .armorPen = pr.armorPen,
                                             .resistMult = resist});
                    hp.hp -= dealt;
                    pr.alreadyHit.push_back(ee);
                    // Recorded so on-kill effects fire the KILLER's element.
                    r.emplace_or_replace<LastHitBy>(ee, pr.source);
                    r.emplace_or_replace<HitFlash>(ee, 0.08f);
                    // The tag carries the damage TYPE, and the sign of `value`
                    // is never used for anything, so resistance direction rides
                    // along as a suffix: the renderer needs to know whether this
                    // hit was resisted to draw it dimmer.
                    w.emit({VisualEvent::Kind::Hit, epos.v, pr.dir, dealt, pr.crit,
                            pr.damageType + (resist < 0.95f    ? "-"
                                             : resist > 1.05f  ? "+"
                                                               : "")});
                    // Tower traits fire off the landed hit, so they scale with
                    // whatever the tower and its element already did.
                    if (const auto* sp = r.try_get<Splash>(pr.source);
                        sp && sp->radius > 0.0f) {
                        areaDamage(w, epos.v, sp->radius, dealt * sp->damagePct,
                                   pr.damageType, sp->falloff, {ee});
                    }
                    if (const auto* ch = r.try_get<Chain>(pr.source); ch && ch->jumps > 0) {
                        std::vector<entt::entity> hit{ee};
                        core::Vec2 from = epos.v;
                        float carry = dealt * ch->damagePct;
                        for (int j = 0; j < ch->jumps; ++j) {
                            const auto near = enemiesWithin(w, from, ch->radius, hit);
                            if (near.empty()) break;
                            const auto next = near.front();
                            dealTyped(w, next, carry, pr.damageType);
                            if (const auto* np = r.try_get<Position>(next)) from = np->v;
                            hit.push_back(next);
                            carry *= ch->falloff;
                        }
                    }
                    if (const auto* am = r.try_get<Amplify>(pr.source); am && am->pct > 0.0f) {
                        r.emplace_or_replace<Amplified>(ee, am->pct, am->duration);
                    }
                    // Light's beacon: a marked target shares what it takes with
                    // the crowd around it, from ANY tower's fire.
                    if (const auto* bc = r.try_get<Beaconed>(ee);
                        bc && bc->sharePct > 0.0f && bc->radius > 0.0f) {
                        areaDamage(w, epos.v, bc->radius, dealt * bc->sharePct, pr.damageType,
                                   1.0f, {ee});
                    }
                    if (const auto* so = r.try_get<SlowOnHit>(pr.source); so && so->pct > 0.0f) {
                        // Maximum, never compounding, matching every other slow.
                        if (auto* sl = r.try_get<Slowed>(ee)) {
                            sl->pct = std::max(sl->pct, so->pct);
                            sl->remaining = std::max(sl->remaining, so->duration);
                        } else {
                            r.emplace<Slowed>(ee, so->pct, so->duration);
                        }
                    }

                    if (const auto* er = r.try_get<ElementRef>(pr.source); er && er->behavior) {
                        er->behavior->onHit(w, pe, ee, dealt);
                    }

                    if (pr.pierceLeft <= 0) {
                        spent.push_back(pe);
                        consumed = true;
                    } else {
                        --pr.pierceLeft;
                    }
                });
        });

    // Collected first, destroyed after: destroying while iterating an EnTT view
    // invalidates it.
    for (const auto e : spent) {
        if (r.valid(e)) r.destroy(e);
    }
}

void runDeathSystem(World& w, float dt) {
    (void)dt;
    auto& r = w.reg();

    std::vector<entt::entity> dead;
    std::vector<int> bounties;
    std::vector<int> shards;
    r.view<const Health, const EnemyTag>().each(
        [&](entt::entity e, const Health& hp, const EnemyTag& tag) {
            if (hp.hp <= 0.0f) {
                dead.push_back(e);
                bounties.push_back(tag.bounty);
                shards.push_back(tag.shardValue);
            }
        });

    // onKill runs BEFORE the entity is destroyed, so effects like poison spread
    // can still read the dying enemy's position and stacks. The element used is
    // the one carried by whoever landed the killing blow.
    for (size_t i = 0; i < dead.size(); ++i) {
        const auto* lh = r.try_get<LastHitBy>(dead[i]);
        if (!lh || lh->tower == entt::null || !r.valid(lh->tower)) continue;
        if (const auto* er = r.try_get<ElementRef>(lh->tower); er && er->behavior) {
            er->behavior->onKill(w, dead[i]);
        }
        // Arcane drain pays on top of the bounty, so it scales with kill COUNT
        // rather than with enemy value.
        if (const auto* dr = r.try_get<Drain>(lh->tower); dr && dr->goldPerKill > 0) {
            w.addGold(dr->goldPerKill);
        }
    }
    for (size_t i = 0; i < dead.size(); ++i) {
        if (const auto* p = r.try_get<Position>(dead[i])) {
            w.emit({VisualEvent::Kind::Death, p->v, {}, 0.0f, false,
                    r.get<EnemyTag>(dead[i]).defId});
        }
        w.addGold(bounties[i]);
        w.addShards(shards[i]);
        if (r.valid(dead[i])) r.destroy(dead[i]);
    }
}

}  // namespace td::sim
