// The in-run build-up: place a tower, imbue it with an element, specialise the
// tower, specialise the element. The run commits to one tower spec and one
// element spec on the first purchase that picks them.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "content/Registry.h"

#include "support/Plots.h"
#include "sim/World.h"

using namespace td;

static content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
static sim::World rich(const content::Registry& r) {
    return sim::World(r, r.map("greenfields"), 1, tdtest::owningAll(), /*goldOverride=*/100000);
}
static bool has(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

// Specialising now requires a fully levelled tower, so tests must level first.
static void maxOut(sim::World& w, int x, int y) {
    while (w.upgradeCost(x, y) > 0) REQUIRE(w.upgradeTower(x, y));
    REQUIRE(w.atMaxLevel(x, y));
}

TEST_CASE("a freshly built tower has no element and no specs", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    const auto& tag = w.reg().get<sim::TowerTag>(w.towerAt(PLOT(0)));
    REQUIRE(tag.elementId.empty());
    REQUIRE(tag.towerSpec.empty());
    REQUIRE(tag.elementSpec.empty());
    REQUIRE(w.activeTowerSpecs().empty());
    REQUIRE(w.activeElementSpecs().empty());
}

TEST_CASE("an element must be attached before it can be specialised", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");
    REQUIRE(w.availableElementSpecs(PLOT(0)).empty());
    REQUIRE_FALSE(w.specialiseElement(PLOT(0), "poison"));

    REQUIRE(w.attachElement(PLOT(0), "earth"));
    REQUIRE(w.availableElementSpecs(PLOT(0)).size() == 3);
    REQUIRE(w.specialiseElement(PLOT(0), "poison"));
}

TEST_CASE("each purchase costs gold and is recorded for refunds", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    const int start = w.gold();
    w.placeTower(PLOT(0), "arrow");
    const int afterBuild = w.gold();
    REQUIRE(afterBuild == start - reg.tower("arrow").buildCost);

    REQUIRE(w.attachElement(PLOT(0), "earth"));
    REQUIRE(w.gold() == afterBuild - reg.element("earth").attachCost);

    maxOut(w, PLOT(0));
    const int beforeSpec = w.gold();
    REQUIRE(w.specialiseTower(PLOT(0), "elf"));
    REQUIRE(w.gold() == beforeSpec - reg.tower("arrow").specCost);

    const int invested = start - w.gold();
    const int beforeSell = w.gold();
    REQUIRE(w.sellTower(PLOT(0)));
    // Refund is proportional to EVERYTHING invested, not just the build cost.
    REQUIRE(w.gold() - beforeSell ==
            static_cast<int>(static_cast<float>(invested) * reg.tower("arrow").sellRefundPct));
}

TEST_CASE("an element cannot be attached twice", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");
    REQUIRE(w.attachElement(PLOT(0), "earth"));
    REQUIRE_FALSE(w.attachElement(PLOT(0), "earth"));
}

TEST_CASE("a specialisation is unique on the map", "[build]") {
    // One sniper, one elf, one hunter -- and any number of plain arrow towers.
    // The decision is which pairings to field, not one spec for the whole run.
    const auto reg = loadReg();
    auto w = rich(reg);
    for (int i = 0; i < 4; ++i) w.placeTower(PLOT(i), "arrow");
    for (int i = 0; i < 4; ++i) maxOut(w, PLOT(i));

    auto avail = w.availableTowerSpecs(PLOT(0));
    REQUIRE(avail.size() == 3);
    REQUIRE(has(avail, "sniper"));
    REQUIRE(has(avail, "elf"));
    REQUIRE(has(avail, "hunter"));

    REQUIRE(w.specialiseTower(PLOT(0), "elf"));
    REQUIRE(w.towerSpecInUse("elf"));

    // Elf is now taken, but the other two are still open on another tower.
    avail = w.availableTowerSpecs(PLOT(1));
    REQUIRE(avail.size() == 2);
    REQUIRE_FALSE(has(avail, "elf"));
    REQUIRE_FALSE(w.specialiseTower(PLOT(1), "elf"));

    REQUIRE(w.specialiseTower(PLOT(1), "sniper"));
    REQUIRE(w.specialiseTower(PLOT(2), "hunter"));

    // All three fielded: a fourth tower has nothing left to become.
    REQUIRE(w.availableTowerSpecs(4, 1).empty());
    REQUIRE(w.activeTowerSpecs().size() == 3);
}

TEST_CASE("selling a specialised tower frees its specialisation", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");
    w.placeTower(PLOT(1), "arrow");
    maxOut(w, PLOT(0));
    maxOut(w, PLOT(1));
    REQUIRE(w.specialiseTower(PLOT(0), "elf"));
    REQUIRE_FALSE(has(w.availableTowerSpecs(PLOT(1)), "elf"));

    REQUIRE(w.sellTower(PLOT(0)));
    REQUIRE_FALSE(w.towerSpecInUse("elf"));
    REQUIRE(has(w.availableTowerSpecs(PLOT(1)), "elf"));  // available again
}

TEST_CASE("unspecialised arrow towers are unlimited", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    for (int i = 0; i < 12; ++i) {
        REQUIRE(w.placeTower(PLOT(i), "arrow") == sim::World::PlaceResult::Ok);
    }
    REQUIRE(w.activeTowerSpecs().empty());
}

TEST_CASE("an element power is unique on the map too", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    for (int i = 0; i < 4; ++i) {
        w.placeTower(PLOT(i), "arrow");
        w.attachElement(PLOT(i), "earth");
    }
    REQUIRE(w.specialiseElement(PLOT(0), "quake"));
    REQUIRE(w.elementSpecInUse("quake"));
    REQUIRE_FALSE(has(w.availableElementSpecs(PLOT(1)), "quake"));
    REQUIRE_FALSE(w.specialiseElement(PLOT(1), "quake"));

    REQUIRE(w.specialiseElement(PLOT(1), "poison"));
    REQUIRE(w.specialiseElement(PLOT(2), "rock"));
    REQUIRE(w.availableElementSpecs(4, 1).empty());
}

TEST_CASE("all three pairings can be fielded at once", "[build]") {
    // The whole point of the new rule: three different combinations live on the
    // board simultaneously.
    const auto reg = loadReg();
    auto w = rich(reg);
    const char* towerSpecs[3] = {"sniper", "elf", "hunter"};
    const char* elemSpecs[3] = {"poison", "rock", "quake"};
    for (int i = 0; i < 3; ++i) {
        const int x = tdtest::plotX(i), y = tdtest::plotY(i);
        w.placeTower(x, y, "arrow");
        maxOut(w, x, y);
        REQUIRE(w.attachElement(x, y, "earth"));
        REQUIRE(w.specialiseTower(x, y, towerSpecs[i]));
        REQUIRE(w.specialiseElement(x, y, elemSpecs[i]));
    }
    REQUIRE(w.activeTowerSpecs().size() == 3);
    REQUIRE(w.activeElementSpecs().size() == 3);
}

TEST_CASE("a tower cannot be specialised twice", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");
    maxOut(w, PLOT(0));
    REQUIRE(w.specialiseTower(PLOT(0), "hunter"));
    REQUIRE(w.availableTowerSpecs(PLOT(0)).empty());
    REQUIRE_FALSE(w.specialiseTower(PLOT(0), "hunter"));
}

TEST_CASE("specialising visibly changes the tower's stats", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");
    maxOut(w, PLOT(0));
    const auto t = w.towerAt(PLOT(0));
    const float baseRate = w.reg().get<sim::TowerStats>(t).fireRate;
    const float baseDamage = w.reg().get<sim::TowerStats>(t).damage;

    REQUIRE(w.specialiseTower(PLOT(0), "elf"));
    const auto& after = w.reg().get<sim::TowerStats>(t);
    REQUIRE(after.fireRate > baseRate * 2.0f);   // elf is the rate spec
    REQUIRE(after.damage < baseDamage);          // paid for with damage
    REQUIRE(w.reg().all_of<sim::RampUp>(t));     // and it gains the ramp trait
}

TEST_CASE("the element behaviour attaches only once a spec is chosen", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");
    const auto t = w.towerAt(PLOT(0));
    REQUIRE_FALSE(w.reg().all_of<sim::ElementRef>(t));

    w.attachElement(PLOT(0), "earth");
    REQUIRE_FALSE(w.reg().all_of<sim::ElementRef>(t));  // element but no spec yet

    w.specialiseElement(PLOT(0), "poison");
    REQUIRE(w.reg().all_of<sim::ElementRef>(t));
    REQUIRE(w.reg().get<sim::ElementRef>(t).behavior != nullptr);
}

TEST_CASE("each element power gets its own behaviour instance", "[build]") {
    // Two towers can no longer share a power, so what matters is that distinct
    // powers resolve to distinct behaviours and each tower carries its own.
    const auto reg = loadReg();
    auto w = rich(reg);
    const char* specs[3] = {"poison", "rock", "quake"};
    std::vector<const void*> behaviours;
    for (int i = 0; i < 3; ++i) {
        const int x = tdtest::plotX(i), y = tdtest::plotY(i);
        w.placeTower(x, y, "arrow");
        REQUIRE(w.attachElement(x, y, "earth"));
        REQUIRE(w.specialiseElement(x, y, specs[i]));
        const auto t = w.towerAt(x, y);
        REQUIRE(w.reg().all_of<sim::ElementRef>(t));
        const auto* b = w.reg().get<sim::ElementRef>(t).behavior;
        REQUIRE(b != nullptr);
        behaviours.push_back(b);
    }
    REQUIRE(behaviours[0] != behaviours[1]);
    REQUIRE(behaviours[1] != behaviours[2]);
    REQUIRE(behaviours[0] != behaviours[2]);
}

TEST_CASE("an unspecialised tower still fights, just plainly", "[build]") {
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(2), "arrow");
    w.startNextWave();
    const int before = w.gold();
    for (int i = 0; i < static_cast<int>(50.0f / sim::kFixedDt); ++i) w.tick(sim::kFixedDt);
    REQUIRE(w.gold() > before);  // it killed something and earned bounty
}

TEST_CASE("purchases are unaffordable when gold runs out", "[build]") {
    const auto reg = loadReg();
    // Just enough for the tower itself and nothing more.
    sim::World w(reg, reg.map("greenfields"), 1, tdtest::owningAll(),
                 /*goldOverride=*/reg.tower("arrow").buildCost);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.gold() == 0);
    REQUIRE_FALSE(w.attachElement(PLOT(0), "earth"));
    REQUIRE_FALSE(w.specialiseTower(PLOT(0), "elf"));  // unaffordable AND unlevelled
    REQUIRE(w.activeTowerSpecs().empty());  // a failed purchase must not commit anything
}

TEST_CASE("a tower must be fully levelled before it can specialise", "[build]") {
    // The upgrade path is the commitment; specialising is what it buys.
    const auto reg = loadReg();
    auto w = rich(reg);
    w.placeTower(PLOT(0), "arrow");

    REQUIRE_FALSE(w.atMaxLevel(PLOT(0)));
    REQUIRE(w.availableTowerSpecs(PLOT(0)).empty());
    REQUIRE(w.towerSpecCost(PLOT(0)) == -1);
    REQUIRE_FALSE(w.specialiseTower(PLOT(0), "elf"));

    REQUIRE(w.upgradeTower(PLOT(0)));                 // level 2
    REQUIRE_FALSE(w.atMaxLevel(PLOT(0)));
    REQUIRE(w.availableTowerSpecs(PLOT(0)).empty());  // still not enough

    REQUIRE(w.upgradeTower(PLOT(0)));                 // level 3 = max
    REQUIRE(w.atMaxLevel(PLOT(0)));
    REQUIRE(w.availableTowerSpecs(PLOT(0)).size() == 3);
    REQUIRE(w.towerSpecCost(PLOT(0)) > 0);
    REQUIRE(w.specialiseTower(PLOT(0), "elf"));
}
