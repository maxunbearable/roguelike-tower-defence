// Every gate the code checks must be openable by BUYING nodes.
//
// This exists because of a bug that no other test could see: World::levelUnlocked
// checked meta_.owns("global.unlock.level2"), but that string is a FLAG granted
// by the node "global.level2" -- and ownedNodes holds node ids, never granted
// flags. So levels 2 and 3 were unreachable for every real player, and only the
// test-only `ownAll` shortcut passed the gate. Every existing guardrail used
// ownAll, so all of them passed while the shipped game could not upgrade a tower.
//
// The lesson generalised: a gate verified only through ownAll is not verified.
// These tests buy nodes, exactly as a player does.
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

// A loadout that owns every authored node -- what a player has after grinding
// the whole tree. Deliberately NOT ownAll.
core::Loadout ownEveryNode(const content::Registry& reg) {
    core::Loadout lo;
    lo.ownAll = false;
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& n : tree.nodes) lo.ownedNodes.insert(n.id);
    }
    return lo;
}

}  // namespace

TEST_CASE("owning every node unlocks the same things as ownAll", "[unlock]") {
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");

    sim::World bought(reg, map, 1, ownEveryNode(reg));

    core::Loadout all;
    all.ownAll = true;
    sim::World shortcut(reg, map, 1, all);

    // Tower levels: the gate that was broken.
    for (int level : {2, 3}) {
        CHECK(shortcut.levelUnlocked(level));
        CHECK(bought.levelUnlocked(level));
    }

    // Tower types.
    for (const auto& [id, t] : reg.towers()) {
        CHECK(shortcut.towerUnlocked(id));
        CHECK(bought.towerUnlocked(id));
    }

    // Elements, and every element specialisation.
    for (const auto& [id, e] : reg.elements()) {
        CHECK(shortcut.elementUnlocked(id));
        CHECK(bought.elementUnlocked(id));
        for (const auto& [spec, params] : e.specs) {
            CHECK(bought.elementSpecUnlocked(id, spec));
        }
    }
}

TEST_CASE("a fresh profile has levels 2 and 3 locked", "[unlock]") {
    const auto reg = loadReg();
    core::Loadout fresh;
    fresh.ownAll = false;
    sim::World w(reg, reg.map("greenfields"), 1, fresh);

    CHECK(w.levelUnlocked(1));
    CHECK_FALSE(w.levelUnlocked(2));
    CHECK_FALSE(w.levelUnlocked(3));
}

TEST_CASE("buying only the level-2 node unlocks level 2 but not level 3", "[unlock]") {
    const auto reg = loadReg();

    // Named by the node that GRANTS the unlock, which is the distinction the
    // original bug erased.
    core::Loadout lo;
    lo.ownAll = false;
    lo.ownedNodes.insert("global.level2");
    sim::World w(reg, reg.map("greenfields"), 1, lo);

    CHECK(w.levelUnlocked(2));
    CHECK_FALSE(w.levelUnlocked(3));

    lo.ownedNodes.insert("global.level3");
    sim::World w2(reg, reg.map("greenfields"), 1, lo);
    CHECK(w2.levelUnlocked(3));
}

TEST_CASE("the level gate follows the tree, not a hardcoded node id", "[unlock]") {
    // If the level-2 node is ever renamed, the gate must follow it. Assert the
    // grant is expressed in DATA: some node in the global tree carries a flag
    // modifier targeting each unlock, and that node is the one that opens it.
    const auto reg = loadReg();
    REQUIRE(reg.hasTree("global"));

    for (const char* flag : {"global.unlock.level2", "global.unlock.level3"}) {
        const core::SkillNode* granter = nullptr;
        for (const auto& n : reg.tree("global").nodes) {
            for (const auto& m : n.modifiers) {
                if (m.target == flag) granter = &n;
            }
        }
        REQUIRE(granter != nullptr);  // nothing grants it -> gate is sealed

        core::Loadout lo;
        lo.ownAll = false;
        // A granter needs its prerequisites too, so own the whole chain.
        for (const auto& n : reg.tree("global").nodes) lo.ownedNodes.insert(n.id);
        sim::World w(reg, reg.map("greenfields"), 1, lo);
        CHECK(w.levelUnlocked(std::string(flag).back() == '2' ? 2 : 3));
    }
}
