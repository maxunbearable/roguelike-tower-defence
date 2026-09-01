#include "sim/systems/StatusSystem.h"

#include <vector>

#include "sim/AreaEffects.h"
#include "sim/World.h"

namespace td::sim {

void runStatusSystem(World& w, float dt) {
    auto& r = w.reg();

    std::vector<entt::entity> clearPoison, clearShred, clearSlow, clearPetrify;
    std::vector<entt::entity> clearAmped, clearMelted, clearFrozen, clearBurn;
    std::vector<entt::entity> clearBeacon, expiredRifts;

    r.view<Poisoned, Health, const EnemyTag>().each(
        [&](entt::entity e, Poisoned& p, Health& hp, const EnemyTag& tag) {
            const float resist = w.defs().enemy(tag.defId).resistTo(p.damageType);
            hp.hp -= p.stacks * p.dpsPerStack * resist * dt;
            p.remaining -= dt;
            if (p.remaining <= 0.0f) clearPoison.push_back(e);
        });

    r.view<ArmorShred>().each([&](entt::entity e, ArmorShred& s) {
        s.remaining -= dt;
        if (s.remaining <= 0.0f) clearShred.push_back(e);
    });

    r.view<Slowed>().each([&](entt::entity e, Slowed& s) {
        s.remaining -= dt;
        if (s.remaining <= 0.0f) clearSlow.push_back(e);
    });

    std::vector<entt::entity> clearFlash;
    r.view<HitFlash>().each([&](entt::entity e, HitFlash& f) {
        f.remaining -= dt;
        if (f.remaining <= 0.0f) clearFlash.push_back(e);
    });
    for (const auto e : clearFlash) r.remove<HitFlash>(e);

    r.view<Petrified>().each([&](entt::entity e, Petrified& s) {
        s.remaining -= dt;
        if (s.remaining <= 0.0f) clearPetrify.push_back(e);
    });

    r.view<Burning, Health, const EnemyTag>().each(
        [&](entt::entity e, Burning& b, Health& hp, const EnemyTag& tag) {
            hp.hp -= b.dps * w.defs().enemy(tag.defId).resistTo(b.damageType) * dt;
            b.remaining -= dt;
            if (b.remaining <= 0.0f) clearBurn.push_back(e);
        });

    r.view<Beaconed>().each([&](entt::entity e, Beaconed& b) {
        b.remaining -= dt;
        if (b.remaining <= 0.0f) clearBeacon.push_back(e);
    });

    // Rifts live on their own entities and damage whatever stands in them. Note
    // Withered is deliberately absent from this system: it has no timer.
    r.view<Rift>().each([&](entt::entity e, Rift& rift) {
        for (const auto victim : enemiesWithin(w, rift.pos, rift.radius)) {
            dealTyped(w, victim, rift.dps * dt, rift.damageType);
        }
        rift.remaining -= dt;
        if (rift.remaining <= 0.0f) expiredRifts.push_back(e);
    });

    r.view<Amplified>().each([&](entt::entity e, Amplified& a) {
        a.remaining -= dt;
        if (a.remaining <= 0.0f) clearAmped.push_back(e);
    });

    r.view<Melted>().each([&](entt::entity e, Melted& m) {
        m.remaining -= dt;
        if (m.remaining <= 0.0f) clearMelted.push_back(e);
    });

    r.view<Frozen>().each([&](entt::entity e, Frozen& f) {
        f.remaining -= dt;
        if (f.remaining <= 0.0f) clearFrozen.push_back(e);
    });

    // Components removed after iteration, never during it.
    for (const auto e : clearPoison) r.remove<Poisoned>(e);
    for (const auto e : clearShred) r.remove<ArmorShred>(e);
    for (const auto e : clearSlow) r.remove<Slowed>(e);
    for (const auto e : clearPetrify) r.remove<Petrified>(e);
    for (const auto e : clearAmped) r.remove<Amplified>(e);
    for (const auto e : clearMelted) r.remove<Melted>(e);
    for (const auto e : clearFrozen) r.remove<Frozen>(e);
    for (const auto e : clearBurn) r.remove<Burning>(e);
    for (const auto e : clearBeacon) r.remove<Beaconed>(e);
    for (const auto e : expiredRifts) {
        if (r.valid(e)) r.destroy(e);
    }
}

}  // namespace td::sim
