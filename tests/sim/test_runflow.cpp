#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "sim/World.h"

using namespace td;

static content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
static void advance(sim::World& w, float s) {
    const int n = static_cast<int>(s / sim::kFixedDt);
    for (int i = 0; i < n; ++i) w.tick(sim::kFixedDt);
}

TEST_CASE("the build timer auto-starts the wave when it expires", "[runflow]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.phase() == sim::Phase::Build);
    advance(w, 13.0f);  // buildTime is 12s
    REQUIRE(w.phase() == sim::Phase::Wave);
}

TEST_CASE("starting early awards bonus gold", "[runflow]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    advance(w, 4.0f);
    const int before = w.gold();
    const int bonus = w.earlyStartBonus();
    REQUIRE(bonus > 0);
    w.startNextWave();
    REQUIRE(w.gold() == before + bonus);
    REQUIRE(w.earlyStartBonus() == 0);  // no bonus once the wave is running
}

TEST_CASE("letting the timer expire awards no bonus", "[runflow]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    const int before = w.gold();
    advance(w, 13.0f);
    REQUIRE(w.phase() == sim::Phase::Wave);
    REQUIRE(w.gold() == before);
}

TEST_CASE("a defended map survives deep into the wave schedule", "[runflow]") {
    // Plan 1 has no skill trees, so clearing all 50 waves is not yet achievable
    // by design -- that is what Plan 3's power curve exists for. What must hold
    // now is that the combat loop SCALES: a heavily defended map should push far
    // past the opening waves rather than collapsing immediately.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/6000);
    // Fill every buildable tile the board offers rather than assuming a layout.
    const auto& m = reg.map("greenfields");
    int built = 0;
    for (int y = 0; y < m.gridH && built < 40; ++y) {
        for (int x = 0; x < m.gridW && built < 40; ++x) {
            if (w.placeTower(x, y, "arrow") == sim::World::PlaceResult::Ok) {
                w.upgradeTower(x, y);
                ++built;
            }
        }
    }
    REQUIRE(built > 10);
    for (int i = 0; i < 1200 && w.phase() != sim::Phase::Cleared &&
                    w.phase() != sim::Phase::Defeated;
         ++i) {
        advance(w, 1.0f);
    }
    UNSCOPED_INFO("reached wave " << w.waveIndex() << " with " << w.lives() << " lives");
    REQUIRE(w.waveIndex() >= 10);
}

TEST_CASE("an undefended map loses quickly", "[runflow]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    for (int i = 0; i < 600 && w.phase() != sim::Phase::Defeated; ++i) advance(w, 1.0f);
    REQUIRE(w.phase() == sim::Phase::Defeated);
    REQUIRE(w.waveIndex() < 10);
}
