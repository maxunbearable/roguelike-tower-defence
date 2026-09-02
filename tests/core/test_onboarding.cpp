// What a brand new profile meets, and what it owns.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "core/Loadout.h"
#include "core/Onboarding.h"
#include "core/SaveGame.h"
#include "sim/World.h"

#include "support/Plots.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

int cheapestNode(const content::Registry& r) {
    int best = 0;
    for (const auto& [id, tree] : r.trees()) {
        for (const auto& n : tree.nodes) {
            if (n.cost > 0 && (best == 0 || n.cost < best)) best = n.cost;
        }
    }
    return best;
}

}  // namespace

TEST_CASE("a default loadout owns nothing", "[onboarding]") {
    // `ownAll` defaulted to true as plan-3 scaffolding and outlived the meta save
    // by twenty-odd plans, so any Loadout built without saying what it owned
    // silently carried the entire skill tree. Asserted through what a player
    // would actually see rather than by reading the flag back.
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");

    sim::World bare(reg, map, 1, core::Loadout{});
    sim::World full(reg, map, 1, tdtest::owningAll());

    INFO("bare " << bare.gold() << "g/" << bare.lives() << " lives, full " << full.gold() << "g/"
                 << full.lives() << " lives");
    // A profile that owns nothing gets exactly what the map grants, with no
    // contribution from the global tree's gold and lives nodes.
    CHECK(bare.gold() == map.startGold);
    CHECK(full.gold() > bare.gold());
    CHECK(full.lives() > bare.lives());
}

TEST_CASE("a new profile is sent to play, not to shop", "[onboarding]") {
    // Opening a profile used to land on the skill trees whatever it owned. For a
    // new one that is a wall: every node locked, every price red, and the slot
    // card that got you there promised "click to begin a new game".
    const auto reg = loadReg();
    const int cheapest = cheapestNode(reg);
    REQUIRE(cheapest > 0);

    core::MetaSave fresh;
    REQUIRE(fresh.shards == 0);
    REQUIRE(fresh.runsPlayed == 0);
    // The premise: there is genuinely nothing a new profile can buy.
    REQUIRE(fresh.shards < cheapest);
    CHECK(core::landingFor(fresh, cheapest) == core::Landing::Maps);

    // Somebody who has played knows where they are, even if they are broke.
    core::MetaSave returning;
    returning.runsPlayed = 1;
    CHECK(core::landingFor(returning, cheapest) == core::Landing::Hub);

    // And a new profile that somehow has shards can spend them.
    core::MetaSave funded;
    funded.shards = cheapest;
    CHECK(core::landingFor(funded, cheapest) == core::Landing::Hub);
}
