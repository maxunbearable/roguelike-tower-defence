#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "content/Validate.h"

static td::content::Registry loaded() {
    td::content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

TEST_CASE("shipped content passes validation", "[content][validate]") {
    const auto r = loaded();
    const auto errors = td::content::validate(r);
    for (const auto& e : errors) UNSCOPED_INFO("validation error: " << e);
    REQUIRE(errors.empty());
}

// The const_casts below are deliberate and confined to tests: they corrupt a
// loaded registry to prove the validator actually rejects bad data, without
// needing broken fixture files on disk.

TEST_CASE("a wave referencing an unknown enemy is reported", "[content][validate]") {
    auto r = loaded();
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    m.waves[0].groups[0].enemyId = "ghost";
    REQUIRE_FALSE(td::content::validate(r).empty());
}

TEST_CASE("a non-axis-aligned path segment is reported", "[content][validate]") {
    auto r = loaded();
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    m.pathWaypoints[1] = td::core::Vec2{13, 3};
    REQUIRE_FALSE(td::content::validate(r).empty());
}

TEST_CASE("a route crossing a non-path tile is reported", "[content][validate]") {
    auto r = loaded();
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    // Blank out a tile the route actually walks over, found from the waypoints
    // rather than hardcoded -- the board size is no longer fixed.
    const auto a = m.pathWaypoints[0];
    const auto b = m.pathWaypoints[1];
    const int mx = static_cast<int>((a.x + b.x) / 2.0f);
    const int my = static_cast<int>((a.y + b.y) / 2.0f);
    m.tileRows[static_cast<size_t>(my)][static_cast<size_t>(mx)] = '.';
    REQUIRE_FALSE(td::content::validate(r).empty());
}

TEST_CASE("an orphan path tile off the route is reported", "[content][validate]") {
    auto r = loaded();
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    m.tileRows[0][0] = '=';
    REQUIRE_FALSE(td::content::validate(r).empty());
}

TEST_CASE("a mismatched grid height is reported", "[content][validate]") {
    auto r = loaded();
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    m.tileRows.pop_back();
    REQUIRE_FALSE(td::content::validate(r).empty());
}

TEST_CASE("a non-positive tower stat is reported", "[content][validate]") {
    auto r = loaded();
    auto& t = const_cast<td::content::TowerDef&>(r.tower("arrow"));
    t.fireRate = 0.0f;
    REQUIRE_FALSE(td::content::validate(r).empty());
}

TEST_CASE("an unknown targetPriority is reported", "[content][validate]") {
    auto r = loaded();
    auto& t = const_cast<td::content::TowerDef&>(r.tower("arrow"));
    t.targetPriority = "sideways";
    REQUIRE_FALSE(td::content::validate(r).empty());
}
