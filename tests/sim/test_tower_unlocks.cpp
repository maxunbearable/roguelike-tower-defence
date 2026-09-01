#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
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
    REQUIRE(w.placeTower(1, 0, "arrow") == sim::World::PlaceResult::Ok);

    for (const char* id : {"cannon", "arcane", "ballista", "brazier"}) {
        UNSCOPED_INFO(id);
        REQUIRE_FALSE(w.towerUnlocked(id));
        REQUIRE(w.placeTower(3, 0, id) == sim::World::PlaceResult::Locked);
    }
}

TEST_CASE("buying a charter unlocks exactly that tower", "[towers][unlock]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning({"cannon.unlock"}), 1000000);
    REQUIRE(w.towerUnlocked("cannon"));
    REQUIRE(w.placeTower(3, 0, "cannon") == sim::World::PlaceResult::Ok);
    // And no others come along with it.
    REQUIRE_FALSE(w.towerUnlocked("arcane"));
    REQUIRE(w.placeTower(5, 0, "arcane") == sim::World::PlaceResult::Locked);
}

TEST_CASE("a locked tower is rejected before its cost is charged",
          "[towers][unlock]") {
    // Order matters: charging for a purchase that is then refused would quietly
    // eat the player's gold.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning({}), 1000000);
    const int before = w.gold();
    REQUIRE(w.placeTower(3, 0, "arcane") == sim::World::PlaceResult::Locked);
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
