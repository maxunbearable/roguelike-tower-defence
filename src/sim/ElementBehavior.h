#pragma once

#include <memory>
#include <string>

#include <entt/entt.hpp>

#include "core/StatBlock.h"

namespace td::sim {

class World;

// The element half of the combination engine. A tower spec changes NUMBERS AND
// GEOMETRY (damage size, rate, arrow count); an element spec changes EVENTS.
// Because element potency scales with hit magnitude and hit frequency, the three
// firing profiles differentiate the three elements on their own -- which is why
// nine combinations exist without nine implementations.
class ElementBehavior {
public:
    virtual ~ElementBehavior() = default;

    virtual void onShoot(World&, entt::entity tower, entt::entity projectile) {}
    virtual void onHit(World&, entt::entity projectile, entt::entity target, float dealt) {}
    virtual void onKill(World&, entt::entity target) {}
    virtual void onTowerTick(World&, entt::entity tower, float dt) {}
};

// Exactly one behaviour is live per run, chosen at run start.
std::unique_ptr<ElementBehavior> makeElement(const std::string& elementId,
                                             const std::string& spec,
                                             const core::StatBlock& stats,
                                             const std::string& damageType);

}  // namespace td::sim
