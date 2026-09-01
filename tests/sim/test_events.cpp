#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>

#include "content/Registry.h"
#include "sim/World.h"

using namespace td;

static content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

TEST_CASE("combat emits the visual events the renderer needs", "[events]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/5000);
    w.placeTower(3, 0, "arrow");
    w.placeTower(6, 0, "arrow");
    w.startNextWave();

    std::map<int, int> counts;
    for (int i = 0; i < static_cast<int>(40.0f / sim::kFixedDt); ++i) {
        w.tick(sim::kFixedDt);
        for (const auto& e : w.drainEvents()) counts[static_cast<int>(e.kind)]++;
    }

    REQUIRE(counts[static_cast<int>(sim::VisualEvent::Kind::Shot)] > 0);
    REQUIRE(counts[static_cast<int>(sim::VisualEvent::Kind::Hit)] > 0);
    REQUIRE(counts[static_cast<int>(sim::VisualEvent::Kind::Death)] > 0);
}

TEST_CASE("hit events carry damage and a position", "[events]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/5000);
    w.placeTower(3, 0, "arrow");
    w.startNextWave();

    bool sawHit = false;
    for (int i = 0; i < static_cast<int>(30.0f / sim::kFixedDt) && !sawHit; ++i) {
        w.tick(sim::kFixedDt);
        for (const auto& e : w.drainEvents()) {
            if (e.kind == sim::VisualEvent::Kind::Hit) {
                REQUIRE(e.value > 0.0f);
                REQUIRE(e.pos.x > 0.0f);
                sawHit = true;
            }
        }
    }
    REQUIRE(sawHit);
}

TEST_CASE("the quake element announces its eruptions", "[events]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/5000);
    w.placeTower(3, 0, "arrow");
    while (w.upgradeCost(3, 0) > 0) w.upgradeTower(3, 0);
    w.attachElement(3, 0, "earth");
    w.specialiseElement(3, 0, "quake");
    w.startNextWave();

    int quakes = 0;
    for (int i = 0; i < static_cast<int>(30.0f / sim::kFixedDt); ++i) {
        w.tick(sim::kFixedDt);
        for (const auto& e : w.drainEvents()) {
            if (e.kind == sim::VisualEvent::Kind::Quake) {
                REQUIRE(e.value > 0.0f);  // radius
                ++quakes;
            }
        }
    }
    REQUIRE(quakes > 0);
}

TEST_CASE("a headless run never accumulates events without bound", "[events]") {
    // Nothing drains in a headless run, so the queue must be capped or a long
    // simulation would grow it forever.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/5000);
    w.placeTower(3, 0, "arrow");
    w.startNextWave();
    for (int i = 0; i < static_cast<int>(120.0f / sim::kFixedDt); ++i) w.tick(sim::kFixedDt);
    REQUIRE(w.drainEvents().size() <= 512);
}
