// Per-run wave variation.
//
// Every run of a map used to face the identical fifty waves, in a game whose
// meta loop asks the player to replay maps many times over to bank shards. What
// varies now is COMPOSITION; what must not vary is difficulty.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "content/Registry.h"
#include "core/Loadout.h"
#include "sim/World.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// The primary enemy of each wave, which is what a player reads off the board.
std::vector<std::string> primaries(const sim::World& w) {
    std::vector<std::string> out;
    for (const auto& wave : w.waves()) {
        out.push_back(wave.groups.empty() ? std::string{} : wave.groups.front().enemyId);
    }
    return out;
}

std::vector<std::string> primariesForSeed(const content::Registry& r, uint64_t seed) {
    sim::World w(r, r.map("greenfields"), seed);
    return primaries(w);
}

}  // namespace

TEST_CASE("two runs of the same map meet its enemies in a different order", "[waves]") {
    const auto reg = loadReg();
    const auto a = primariesForSeed(reg, 1);
    const auto b = primariesForSeed(reg, 2);
    REQUIRE(a.size() == b.size());
    REQUIRE(a.size() >= 50);

    int differing = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) ++differing;
    }
    INFO("waves whose lead enemy differs: " << differing << " of " << a.size());
    // Not merely "not identical": a couple of swapped waves would be a technical
    // pass and no player would notice the run was different.
    CHECK(differing >= 10);
}

TEST_CASE("the same seed always plays the same waves", "[waves]") {
    // The whole project rests on a run being reproducible from its seed, and a
    // resumed run rebuilds its waves rather than restoring them.
    const auto reg = loadReg();
    CHECK(primariesForSeed(reg, 7) == primariesForSeed(reg, 7));
    CHECK(primariesForSeed(reg, 7) != primariesForSeed(reg, 8));
}

TEST_CASE("a seed changes what a wave is made of, never what it weighs", "[waves]") {
    // Per WAVE, not just per run. A run could balance a brutal wave 8 against a
    // gentle wave 9 and still total correctly, and the player would feel the
    // spike. Health and payout are matched wave by wave.
    //
    // Note what is deliberately NOT invariant: count and the health multiplier.
    // Those are the compensation. Wave 8 fields 3 of a heavy creature under one
    // seed and 14 of a light one under another -- the same weight of enemy,
    // asking for a different board.
    const auto reg = loadReg();
    sim::World ref(reg, reg.map("greenfields"), 1);
    const auto weigh = [&](const content::WaveDef& wave) {
        float hp = 0.0f, gold = 0.0f;
        for (const auto& g : wave.groups) {
            hp += static_cast<float>(g.count) * reg.enemy(g.enemyId).maxHp * g.hpMult;
            gold += static_cast<float>(g.count) * static_cast<float>(reg.enemy(g.enemyId).bounty) *
                    g.bountyMult;
        }
        return std::pair<float, float>{hp, gold};
    };

    for (uint64_t seed = 2; seed <= 12; ++seed) {
        sim::World w(reg, reg.map("greenfields"), seed);
        REQUIRE(w.waves().size() == ref.waves().size());
        for (size_t i = 0; i < w.waves().size(); ++i) {
            INFO("wave " << i + 1 << " under seed " << seed);
            const auto [hpA, goldA] = weigh(ref.waves()[i]);
            const auto [hpB, goldB] = weigh(w.waves()[i]);
            REQUIRE(hpA > 0.0f);
            CHECK(std::abs(hpB - hpA) <= hpA * 0.02f);
            CHECK(std::abs(goldB - goldA) <= goldA * 0.02f);
            // Pacing is the recipe's, not the seed's.
            REQUIRE(ref.waves()[i].groups.size() == w.waves()[i].groups.size());
            for (size_t g = 0; g < w.waves()[i].groups.size(); ++g) {
                CHECK(ref.waves()[i].groups[g].interval == w.waves()[i].groups[g].interval);
                CHECK(ref.waves()[i].groups[g].armorAdd == w.waves()[i].groups[g].armorAdd);
            }
        }
    }
}

TEST_CASE("every run meets each unlocked enemy about equally often", "[waves]") {
    // The cycle spends every unlocked type once before repeating, which is the
    // property the difficulty calibration rests on: a seed may not hand one run
    // a campaign of slimes and another a campaign of goblins.
    const auto reg = loadReg();
    for (uint64_t seed = 1; seed <= 8; ++seed) {
        std::map<std::string, int> tally;
        for (const auto& p : primariesForSeed(reg, seed)) ++tally[p];
        int lo = 1 << 30, hi = 0;
        for (const auto& [id, n] : tally) {
            lo = std::min(lo, n);
            hi = std::max(hi, n);
        }
        INFO("seed " << seed << ": " << tally.size() << " types, " << lo << ".." << hi
                     << " appearances");
        // Types unlock at different waves, so the earliest-unlocked appears more
        // often. What must not happen is one type crowding out another that
        // unlocked alongside it.
        CHECK(tally.size() >= 5);
        CHECK(hi <= lo * 4);
    }
}

TEST_CASE("bosses still arrive on their authored waves", "[waves]") {
    // Bosses are set pieces, not part of the shuffle.
    const auto reg = loadReg();
    const auto& recipe = reg.map("greenfields").recipe;
    REQUIRE_FALSE(recipe.bosses.empty());
    for (uint64_t seed = 1; seed <= 6; ++seed) {
        sim::World w(reg, reg.map("greenfields"), seed);
        for (const auto& b : recipe.bosses) {
            const auto& wave = w.waves()[static_cast<size_t>(b.wave - 1)];
            bool found = false;
            for (const auto& g : wave.groups) {
                if (g.enemyId == b.enemyId) found = true;
            }
            INFO("seed " << seed << " wave " << b.wave << " should field " << b.enemyId);
            CHECK(found);
        }
    }
}

TEST_CASE("a run's total health does not depend on its seed", "[waves]") {
    // The envelope test above proves count and multipliers are seed-independent,
    // but the health a wave actually brings is the enemy's own maximum times
    // that multiplier -- and a slime has 40 where a goblin has 160. If the
    // shuffle could hand one run the heavy creature more often than another,
    // every difficulty number in this project would become a lottery. This is
    // the assertion that the cycle exists to make true.
    const auto reg = loadReg();
    std::vector<float> totals;
    for (uint64_t seed = 1; seed <= 12; ++seed) {
        sim::World w(reg, reg.map("greenfields"), seed);
        float total = 0.0f;
        for (const auto& wave : w.waves()) {
            for (const auto& g : wave.groups) {
                total += static_cast<float>(g.count) * reg.enemy(g.enemyId).maxHp * g.hpMult;
            }
        }
        totals.push_back(total);
    }
    const float lo = *std::min_element(totals.begin(), totals.end());
    const float hi = *std::max_element(totals.begin(), totals.end());
    INFO("total health across 12 seeds: " << lo << " .. " << hi << " (" << (hi / lo - 1.0f) * 100.0f
                                          << "% spread)");
    CHECK(lo > 0.0f);
    // Exactly, not approximately. Reordering alone left 51% of the first twelve
    // waves' health riding on the seed; compensating the count and taking up the
    // remainder in the health multiplier removes it entirely.
    CHECK(hi <= lo * 1.001f);
}

TEST_CASE("a run's total payout does not depend on its seed either", "[waves]") {
    // Health parity alone would still leave a lottery: a run that drew heavy
    // creatures fields fewer of them, and bounty is paid per kill, so it would
    // finish poorer than a run that drew light ones. Gold is the binding
    // constraint in this game, so that gap would matter more than the health one.
    const auto reg = loadReg();
    std::vector<float> totals;
    for (uint64_t seed = 1; seed <= 12; ++seed) {
        sim::World w(reg, reg.map("greenfields"), seed);
        float gold = 0.0f;
        for (const auto& wave : w.waves()) {
            for (const auto& g : wave.groups) {
                gold += static_cast<float>(g.count) *
                        static_cast<float>(reg.enemy(g.enemyId).bounty) * g.bountyMult;
            }
        }
        totals.push_back(gold);
    }
    const float lo = *std::min_element(totals.begin(), totals.end());
    const float hi = *std::max_element(totals.begin(), totals.end());
    INFO("total bounty across 12 seeds: " << lo << " .. " << hi);
    CHECK(lo > 0.0f);
    CHECK(hi <= lo * 1.001f);
}

TEST_CASE("a wave that draws a heavier creature fields fewer of them", "[waves]") {
    // The mechanism, asserted rather than assumed: somewhere across these seeds
    // the same wave number must field a different creature AND a different
    // count, or the compensation is not doing anything.
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 1);
    bool sawHeavierAndFewer = false;
    for (uint64_t seed = 2; seed <= 16 && !sawHeavierAndFewer; ++seed) {
        sim::World b(reg, reg.map("greenfields"), seed);
        for (size_t i = 0; i < a.waves().size(); ++i) {
            const auto& ga = a.waves()[i].groups.front();
            const auto& gb = b.waves()[i].groups.front();
            if (ga.enemyId == gb.enemyId) continue;
            const float ha = reg.enemy(ga.enemyId).maxHp;
            const float hb = reg.enemy(gb.enemyId).maxHp;
            if (hb > ha * 1.5f && gb.count < ga.count) {
                INFO("wave " << i + 1 << ": " << ga.count << "x" << ga.enemyId << " vs "
                             << gb.count << "x" << gb.enemyId);
                sawHeavierAndFewer = true;
                break;
            }
        }
    }
    CHECK(sawHeavierAndFewer);
}

TEST_CASE("a resumed run continues the waves it was playing", "[waves]") {
    // Waves are rebuilt from the seed rather than stored in the save, so this is
    // the assertion that makes that safe. A resume that quietly reshuffled the
    // rest of the campaign would be a nasty bug to find from a bug report.
    const auto reg = loadReg();
    sim::World before(reg, reg.map("greenfields"), 4242);
    const auto expected = primaries(before);

    const auto snap = before.snapshot();
    sim::World after(reg, reg.map("greenfields"), snap.seed);
    after.restore(snap);

    REQUIRE(snap.seed == 4242u);
    CHECK(primaries(after) == expected);
}
