#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"

static td::content::Registry loaded() {
    td::content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

TEST_CASE("enemies load with their stats", "[content]") {
    const auto r = loaded();
    REQUIRE(r.hasEnemy("slime"));
    REQUIRE(r.enemy("goblin").armor == 6.0f);
    REQUIRE_FALSE(r.enemy("wraith").flying);  // the armoured heavy walks
    REQUIRE_FALSE(r.enemy("slime").flying);
    // Not a hardcoded total: bosses ship in their own file and more maps add
    // more of them, so assert the roster CONTAINS what this test is about.
    REQUIRE(r.enemies().size() >= 4);
    REQUIRE(r.hasEnemy("wolf"));
}

TEST_CASE("the arrow tower loads with two upgrade levels", "[content]") {
    const auto r = loaded();
    const auto& t = r.tower("arrow");
    REQUIRE(t.buildCost == 60);
    REQUIRE(t.levels.size() == 2);
    REQUIRE(t.levels[1].cost == 140);
    REQUIRE(t.targetPriority == "first");
}

TEST_CASE("the map loads with a correctly sized tile grid", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    // Read the declared size rather than asserting a specific board: the grid
    // changed when the art moved to a 64px tile.
    REQUIRE(m.gridW > 0);
    REQUIRE(m.gridH > 0);
    REQUIRE(m.tileRows.size() == static_cast<size_t>(m.gridH));
    for (const auto& row : m.tileRows) REQUIRE(row.size() == static_cast<size_t>(m.gridW));
}

TEST_CASE("buildableAt respects tile type and bounds", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    // Derived from the map rather than hardcoded coordinates.
    const auto spawn = m.pathWaypoints.front();
    const auto exitAt = m.pathWaypoints.back();
    REQUIRE_FALSE(m.buildableAt(static_cast<int>(spawn.x), static_cast<int>(spawn.y)));
    REQUIRE_FALSE(m.buildableAt(static_cast<int>(exitAt.x), static_cast<int>(exitAt.y)));
    REQUIRE_FALSE(m.buildableAt(-1, 0));
    REQUIRE_FALSE(m.buildableAt(m.gridW, 0));
    REQUIRE_FALSE(m.buildableAt(0, m.gridH));
    int buildable = 0;
    for (int y = 0; y < m.gridH; ++y)
        for (int x = 0; x < m.gridW; ++x)
            if (m.buildableAt(x, y)) ++buildable;
    REQUIRE(buildable > 20);   // a playable board needs somewhere to build
}

TEST_CASE("the wave recipe expands to the requested number of waves", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.hasRecipe);
    REQUIRE(m.recipe.count == 50);
    REQUIRE(m.waves.size() == 50);
}

TEST_CASE("wave 1 is a pure opener at authored strength", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.waves[0].groups.size() == 1);
    REQUIRE(m.waves[0].groups[0].enemyId == "slime");
    REQUIRE(m.waves[0].groups[0].count == 8);
    // wave 1 must carry no scaling at all, or the opener is secretly buffed
    REQUIRE(m.waves[0].groups[0].hpMult == 1.0f);
    REQUIRE(m.waves[0].groups[0].armorAdd == 0.0f);
    REQUIRE(m.waves[0].groups[0].bountyMult == 1.0f);
}

TEST_CASE("waves become mixed and harder as they go on", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.waves[3].groups.size() == 1);   // wave 4, before secondaryFromWave
    REQUIRE(m.waves[9].groups.size() == 2);   // wave 10, mixed
    REQUIRE(m.waves[49].groups[0].hpMult > m.waves[0].groups[0].hpMult);
    REQUIRE(m.waves[49].groups[0].count > m.waves[0].groups[0].count);
    REQUIRE(m.waves[49].groups[0].interval < m.waves[0].groups[0].interval);
}

TEST_CASE("enemies only appear once their pool entry unlocks", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    for (int w = 0; w < 50; ++w) {
        for (const auto& g : m.waves[static_cast<size_t>(w)].groups) {
            const int wave = w + 1;
            if (g.enemyId == "wolf") REQUIRE(wave >= 3);
            if (g.enemyId == "goblin") REQUIRE(wave >= 6);
            if (g.enemyId == "wraith") REQUIRE(wave >= 9);
        }
    }
}

TEST_CASE("wave generation is deterministic", "[content]") {
    const auto a = loaded();
    const auto b = loaded();
    const auto& ma = a.map("greenfields");
    const auto& mb = b.map("greenfields");
    REQUIRE(ma.waves.size() == mb.waves.size());
    for (size_t i = 0; i < ma.waves.size(); ++i) {
        REQUIRE(ma.waves[i].groups.size() == mb.waves[i].groups.size());
        for (size_t g = 0; g < ma.waves[i].groups.size(); ++g) {
            REQUIRE(ma.waves[i].groups[g].enemyId == mb.waves[i].groups[g].enemyId);
            REQUIRE(ma.waves[i].groups[g].count == mb.waves[i].groups[g].count);
        }
    }
}

TEST_CASE("path waypoints load as tile coordinates", "[content]") {
    const auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.pathWaypoints.size() >= 4);
    // Every waypoint must sit inside the declared grid.
    for (const auto& wp : m.pathWaypoints) {
        REQUIRE(wp.x >= 0.0f);
        REQUIRE(wp.y >= 0.0f);
        REQUIRE(wp.x < static_cast<float>(m.gridW));
        REQUIRE(wp.y < static_cast<float>(m.gridH));
    }
}

TEST_CASE("a missing id throws rather than returning garbage", "[content]") {
    const auto r = loaded();
    REQUIRE_THROWS(r.enemy("does_not_exist"));
    REQUIRE_THROWS(r.tower("nope"));
    REQUIRE_THROWS(r.map("nowhere"));
}
