#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

#include "content/WaveGen.h"

using td::content::generateWaves;
using td::content::WaveRecipe;
using Catch::Matchers::WithinAbs;

namespace {

// A recipe with two boss waves and a two-enemy pool, small enough to reason
// about by hand.
WaveRecipe recipeWithBosses() {
    WaveRecipe r;
    r.count = 12;
    r.countBase = 4;
    r.countPerWave = 0.0f;   // constant count, so a boss group is unmistakable
    r.hpPerWave = 1.0f;      // no scaling, so hpMult stays 1 except where stated
    r.armorPerWave = 0.0f;
    r.bountyPerWave = 1.0f;
    r.secondaryFromWave = 99;  // keep waves pure
    r.pool = {{"slime", 1}, {"wolf", 1}};
    r.bosses = {{4, "boss_mid"}, {12, "boss_final"}};
    return r;
}

bool hasEnemy(const td::content::WaveDef& w, const std::string& id) {
    return std::any_of(w.groups.begin(), w.groups.end(),
                       [&](const auto& g) { return g.enemyId == id; });
}

}  // namespace

TEST_CASE("a boss appears on exactly its named wave", "[content][boss]") {
    const auto waves = generateWaves(recipeWithBosses());
    REQUIRE(waves.size() == 12);

    // waves[i] is wave i+1.
    REQUIRE(hasEnemy(waves[3], "boss_mid"));     // wave 4
    REQUIRE(hasEnemy(waves[11], "boss_final"));  // wave 12
    for (size_t i = 0; i < waves.size(); ++i) {
        if (i == 3 || i == 11) continue;
        REQUIRE_FALSE(hasEnemy(waves[i], "boss_mid"));
        REQUIRE_FALSE(hasEnemy(waves[i], "boss_final"));
    }
}

TEST_CASE("a boss arrives as a single enemy, after the wave it escorts",
          "[content][boss]") {
    const auto waves = generateWaves(recipeWithBosses());
    const auto& w = waves[3];
    const auto it = std::find_if(w.groups.begin(), w.groups.end(),
                                 [](const auto& g) { return g.enemyId == "boss_mid"; });
    REQUIRE(it != w.groups.end());
    REQUIRE(it->count == 1);
    // Behind the escort, so the player fights the wave before the boss lands.
    REQUIRE(it->startDelay > 0.0f);
}

TEST_CASE("a boss wave keeps its normal escort wave", "[content][boss]") {
    const auto waves = generateWaves(recipeWithBosses());
    // Wave 4 still sends its regular group; the boss is an addition, not a
    // replacement, or a boss wave would be easier than the wave before it.
    REQUIRE(waves[3].groups.size() >= 2);
    REQUIRE((hasEnemy(waves[3], "slime") || hasEnemy(waves[3], "wolf")));
}

TEST_CASE("boss waves do not disturb the difficulty curve of other waves",
          "[content][boss]") {
    WaveRecipe with = recipeWithBosses();
    WaveRecipe without = with;
    without.bosses.clear();

    const auto a = generateWaves(with);
    const auto b = generateWaves(without);
    REQUIRE(a.size() == b.size());

    for (size_t i = 0; i < a.size(); ++i) {
        // The non-boss groups must match exactly: adding a boss must not shift
        // the enemy rotation or rescale the wave it lands on.
        std::vector<td::content::WaveGroup> plain;
        for (const auto& g : a[i].groups) {
            if (g.enemyId.rfind("boss_", 0) != 0) plain.push_back(g);
        }
        REQUIRE(plain.size() == b[i].groups.size());
        for (size_t k = 0; k < plain.size(); ++k) {
            REQUIRE(plain[k].enemyId == b[i].groups[k].enemyId);
            REQUIRE(plain[k].count == b[i].groups[k].count);
            REQUIRE_THAT(plain[k].hpMult, WithinAbs(b[i].groups[k].hpMult, 1e-5f));
        }
    }
}

TEST_CASE("a boss scales with the wave it lands on", "[content][boss]") {
    WaveRecipe r = recipeWithBosses();
    r.hpPerWave = 1.10f;  // now scaling is on
    const auto waves = generateWaves(r);

    const auto find = [](const td::content::WaveDef& w, const std::string& id) {
        return std::find_if(w.groups.begin(), w.groups.end(),
                            [&](const auto& g) { return g.enemyId == id; });
    };
    const auto mid = find(waves[3], "boss_mid");
    const auto fin = find(waves[11], "boss_final");
    REQUIRE(mid != waves[3].groups.end());
    REQUIRE(fin != waves[11].groups.end());
    // The wave-12 boss must be meaningfully tougher than the wave-4 one, or a
    // map's final boss is a pushover next to its mid-boss.
    REQUIRE(fin->hpMult > mid->hpMult);
}

TEST_CASE("a boss named on a wave beyond the recipe is ignored, not crashed",
          "[content][boss]") {
    WaveRecipe r = recipeWithBosses();
    r.bosses.push_back({999, "boss_nowhere"});
    const auto waves = generateWaves(r);
    REQUIRE(waves.size() == 12);
    for (const auto& w : waves) REQUIRE_FALSE(hasEnemy(w, "boss_nowhere"));
}
