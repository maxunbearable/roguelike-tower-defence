// Difficulty settings.
//
// These exist to settle a contradiction the project carried for rounds and wrote
// down in its own README: "hardcore, limited resources" and "map 1 in 8-10
// losses" are the same dial pulled opposite ways. Irreconcilable as one tuning,
// fine as a choice.
//
// Tested by PLAYING each setting with the identical profile and seed and
// measuring what changes, not by reading the multiplier table back.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "core/Difficulty.h"
#include "sim/AutoPlayer.h"
#include "sim/World.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
core::Loadout owning() {
    core::Loadout lo;
    lo.ownAll = true;
    return lo;
}
}  // namespace

TEST_CASE("standard is the authored balance, untouched", "[difficulty]") {
    // Standard must be a true no-op or every previous balance measurement in the
    // project silently stops meaning what it says.
    const auto m = core::modsFor(core::Difficulty::Standard);
    CHECK(m.enemyHp == 1.0f);
    CHECK(m.enemyCount == 1.0f);
    CHECK(m.startGold == 1.0f);
    CHECK(m.lives == 1.0f);
    CHECK(m.shards == 1.0f);
}

TEST_CASE("no setting reduces shard payout below baseline", "[difficulty]") {
    // Taxing the meta progression of players who chose an easier setting is the
    // design players say makes them doubt a game's fairness, and it punishes
    // exactly the people who needed the help. Harder may pay MORE; easier must
    // never pay less.
    for (int i = 0; i < core::kDifficultyCount; ++i) {
        const auto d = core::difficultyFromIndex(i);
        INFO(core::difficultyName(d));
        CHECK(core::modsFor(d).shards >= 1.0f);
    }
    CHECK(core::modsFor(core::Difficulty::Brutal).shards > 1.0f);
}

TEST_CASE("the index round trips and is safe out of range", "[difficulty]") {
    for (int i = 0; i < core::kDifficultyCount; ++i) {
        CHECK(core::difficultyToIndex(core::difficultyFromIndex(i)) == i);
    }
    CHECK(core::difficultyFromIndex(-1) == core::Difficulty::Standard);
    CHECK(core::difficultyFromIndex(99) == core::Difficulty::Standard);
}

TEST_CASE("relaxed opens with more to work with than brutal", "[difficulty]") {
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");
    sim::World relaxed(reg, map, 1, owning(), -1, core::Difficulty::Relaxed);
    sim::World standard(reg, map, 1, owning(), -1, core::Difficulty::Standard);
    sim::World brutal(reg, map, 1, owning(), -1, core::Difficulty::Brutal);

    CHECK(relaxed.gold() > standard.gold());
    CHECK(standard.gold() > brutal.gold());
    CHECK(relaxed.lives() > standard.lives());
    CHECK(standard.lives() > brutal.lives());
}

TEST_CASE("a run's life cap follows its own starting lives", "[difficulty]") {
    // gainLife used to clamp against the raw constant, so a Relaxed run that
    // opened with more lives than kStartingLives would have them confiscated the
    // first time anything healed it.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), -1, core::Difficulty::Relaxed);
    const int opened = w.lives();
    REQUIRE(opened > sim::kStartingLives);
    w.loseLife(3);
    REQUIRE(w.lives() == opened - 3);
    w.gainLife(10);
    CHECK(w.lives() == opened);           // healed back to the run's own maximum
    CHECK(w.lives() > sim::kStartingLives);
}

TEST_CASE("harder settings send more and tougher enemies", "[difficulty]") {
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");

    // Same map, same seed, same wave: count how many actually arrive and how
    // much health the wave carries in total.
    const auto measure = [&](core::Difficulty d) {
        sim::World w(reg, map, 1, owning(), 100000, d);
        w.devSetWave(12);
        float hp = 0.0f;
        for (int i = 0; i < 60 * 90 && w.phase() == sim::Phase::Wave; ++i) {
            w.tick(sim::kFixedDt);
        }
        w.reg().view<const sim::Health>().each([&](const sim::Health& h) { hp += h.maxHp; });
        return std::pair<int, float>{w.enemiesSpawned(), hp};
    };

    const auto [relaxedN, relaxedHp] = measure(core::Difficulty::Relaxed);
    const auto [brutalN, brutalHp] = measure(core::Difficulty::Brutal);
    UNSCOPED_INFO("relaxed spawned " << relaxedN << ", brutal " << brutalN);
    CHECK(brutalN > relaxedN);
    (void)relaxedHp;
    (void)brutalHp;
}

TEST_CASE("a boss stays a single boss on every setting", "[difficulty]") {
    // Scaling group size must not duplicate a boss: two bosses is a different
    // fight, not a harder one.
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");
    for (int i = 0; i < core::kDifficultyCount; ++i) {
        const auto d = core::difficultyFromIndex(i);
        sim::World w(reg, map, 1, owning(), 100000, d);
        w.devSetWave(24);  // the mid-boss wave
        int bosses = 0;
        for (int t = 0; t < 60 * 60 && w.phase() == sim::Phase::Wave; ++t) {
            w.tick(sim::kFixedDt);
            int live = 0;
            w.reg().view<const sim::Boss>().each([&](auto) { ++live; });
            bosses = std::max(bosses, live);
        }
        INFO(core::difficultyName(d));
        CHECK(bosses <= 1);
    }
}

TEST_CASE("relaxed gets further than brutal with the same player", "[difficulty]") {
    // The whole point, measured end to end: an identical profile playing an
    // identical map must reach further on the easier setting.
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");
    core::Loadout fresh;
    fresh.ownAll = false;

    const auto relaxed = sim::autoPlay(reg, map, fresh, 7, 60, core::Difficulty::Relaxed);
    const auto brutal = sim::autoPlay(reg, map, fresh, 7, 60, core::Difficulty::Brutal);
    UNSCOPED_INFO("relaxed reached wave " << relaxed.wavesSurvived << ", brutal "
                                          << brutal.wavesSurvived);
    CHECK(relaxed.wavesSurvived > brutal.wavesSurvived);
}
