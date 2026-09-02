// Simulation cost at the top of the game.
//
// The fixed timestep is 1/60s, so one tick has 16.67ms before the game cannot
// keep up at 1x -- and the speed control goes to 4x, which needs four ticks
// inside one frame. Targeting is O(towers x enemies) per tick and has been
// noted as such twice without ever being measured.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "content/Registry.h"
#include "core/Loadout.h"
#include "sim/World.h"

#include "support/Plots.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// A board at the ceiling: every plot filled and levelled, at a late wave.
struct Loaded {
    int towers = 0;
    int enemies = 0;
    double usPerTick = 0.0;
};

Loaded measure(const content::Registry& reg, const std::string& mapId, int wave, int towerCap) {
    Loaded out;
    core::Loadout lo;
    lo.ownAll = true;
    sim::World w(reg, reg.map(mapId), 7, lo, /*goldOverride=*/100000000);
    for (int y = 0; y < w.map().gridH && out.towers < towerCap; ++y) {
        for (int x = 0; x < w.map().gridW && out.towers < towerCap; ++x) {
            if (!w.map().buildableAt(x, y)) continue;
            const char* types[] = {"arrow", "cannon", "ballista", "arcane", "brazier"};
            if (w.placeTower(x, y, types[out.towers % 5]) != sim::World::PlaceResult::Ok) continue;
            while (w.upgradeCost(x, y) > 0 && w.upgradeTower(x, y)) {
            }
            w.attachElement(x, y, "earth");
            ++out.towers;
        }
    }
    w.devSetWave(wave);
    w.startNextWave();
    // Let the wave populate the field before timing anything.
    for (int i = 0; i < 600; ++i) w.tick(sim::kFixedDt);
    out.enemies = static_cast<int>(w.reg().view<const sim::EnemyTag>().size());

    constexpr int kTicks = 1200;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kTicks; ++i) w.tick(sim::kFixedDt);
    const auto t1 = std::chrono::steady_clock::now();
    out.usPerTick =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / static_cast<double>(kTicks);
    return out;
}

}  // namespace

TEST_CASE("report simulation cost by board size", "[perf][.report]") {
    const auto reg = loadReg();
    std::string s = "\n  towers  enemies   us/tick   % of a 60Hz frame   % of a 4x frame\n";
    for (const int cap : {4, 10, 20, 40}) {
        const auto m = measure(reg, "greenfields", 44, cap);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  %6d  %7d  %8.1f  %17.1f  %16.1f\n", m.towers,
                      m.enemies, m.usPerTick, m.usPerTick / 16667.0 * 100.0,
                      m.usPerTick / 4167.0 * 100.0);
        s += buf;
    }
    UNSCOPED_INFO(s);
    CHECK(true);
}
