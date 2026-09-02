#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"

#include "support/Plots.h"
#include "sim/World.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
core::Loadout owning(std::initializer_list<const char*> nodes) {
    core::Loadout lo;
    lo.ownAll = false;
    for (const char* n : nodes) lo.ownedNodes.insert(n);
    return lo;
}
}  // namespace

TEST_CASE("a fresh profile can build the arrow tower and nothing else",
          "[towers][unlock]") {
    // Without one buildable tower a first run has no game at all, so arrow is
    // free; the rest of the roster is progression.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning({}), 1000000);
    REQUIRE(w.towerUnlocked("arrow"));
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);

    for (const char* id : {"cannon", "arcane", "ballista", "brazier"}) {
        UNSCOPED_INFO(id);
        REQUIRE_FALSE(w.towerUnlocked(id));
        REQUIRE(w.placeTower(PLOT(1), id) == sim::World::PlaceResult::Locked);
    }
}

TEST_CASE("buying a charter unlocks exactly that tower", "[towers][unlock]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning({"cannon.unlock"}), 1000000);
    REQUIRE(w.towerUnlocked("cannon"));
    REQUIRE(w.placeTower(PLOT(1), "cannon") == sim::World::PlaceResult::Ok);
    // And no others come along with it.
    REQUIRE_FALSE(w.towerUnlocked("arcane"));
    REQUIRE(w.placeTower(PLOT(2), "arcane") == sim::World::PlaceResult::Locked);
}

TEST_CASE("a locked tower is rejected before its cost is charged",
          "[towers][unlock]") {
    // Order matters: charging for a purchase that is then refused would quietly
    // eat the player's gold.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning({}), 1000000);
    const int before = w.gold();
    REQUIRE(w.placeTower(PLOT(1), "arcane") == sim::World::PlaceResult::Locked);
    REQUIRE(w.gold() == before);
}

TEST_CASE("every non-starting tower has a charter node to buy", "[towers][unlock]") {
    // A tower with no charter would be permanently unbuildable, which is worse
    // than not shipping it.
    const auto reg = loadReg();
    for (const auto& [id, def] : reg.towers()) {
        if (id == sim::kStartingTower) continue;
        UNSCOPED_INFO("tower " << id);
        REQUIRE(reg.hasTree(id));
        const auto* node = reg.tree(id).find(id + ".unlock");
        REQUIRE(node != nullptr);
        REQUIRE(node->cost > 0);
        REQUIRE(node->prereqs.empty());  // it is the root: nothing gates the gate
    }
}

TEST_CASE("a refusal says why, and a locked tower is not a money problem", "[unlocks]") {
    // The build menu offers a new profile all five towers, four of them greyed.
    // A first run carries 275 gold and the cannon costs 166, so it can AFFORD
    // three of the four it cannot build -- and the only message the game had for
    // a refused build was "not enough gold", which is the one explanation that
    // is false here.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{});

    const int purse = w.gold();
    const int cannon = reg.tower("cannon").buildCost;
    INFO("purse " << purse << ", cannon " << cannon);
    REQUIRE(cannon <= purse);  // affordable, so gold cannot be the reason

    const auto r = w.placeTower(PLOT(0), "cannon");
    REQUIRE(r == sim::World::PlaceResult::Locked);
    const std::string why = sim::World::describe(r);
    CHECK_FALSE(why.empty());
    CHECK(why.find("gold") == std::string::npos);
    CHECK(why.find("skill trees") != std::string::npos);

    // ...and the tower it does own is placeable, so the map is not the problem.
    CHECK(w.placeTower(PLOT(1), "arrow") == sim::World::PlaceResult::Ok);
}

TEST_CASE("every placement outcome has something to say", "[unlocks]") {
    // `Locked` was added to the enum and not to the message mapping, so a
    // refused placement returned an empty string. The compiler warned; nothing
    // failed. This is the test that would have.
    using R = sim::World::PlaceResult;
    const R all[] = {R::Ok,          R::NotBuildable, R::Occupied, R::TooPoor,
                     R::OutOfBounds, R::UnknownTower, R::Locked};
    for (const auto r : all) {
        const std::string msg = sim::World::describe(r);
        INFO("outcome index " << static_cast<int>(r) << " -> \"" << msg << "\"");
        if (r == R::Ok) {
            CHECK(msg.empty());  // nothing to report when it worked
        } else {
            CHECK_FALSE(msg.empty());
        }
    }
}
