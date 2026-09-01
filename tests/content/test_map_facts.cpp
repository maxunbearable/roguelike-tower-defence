// Each map's element bias, derived from its own roster.
//
// The game's replayability claim is that every map resists a different element,
// so the build that cleared the last map is the wrong build for the next. The
// map select screen never stated it, and stating it by hand would let the screen
// drift out of step with the enemies. These tests pin the derivation AND the
// design claim it advertises.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>

#include "content/MapFacts.h"
#include "content/Registry.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
}  // namespace

TEST_CASE("a reported bias is a real one", "[mapfacts]") {
    const auto reg = loadReg();
    int biased = 0;
    for (const auto& [id, map] : reg.maps()) {
        const auto b = content::mapBias(reg, map);
        UNSCOPED_INFO(id << ": resists " << b.resistant << " (" << b.resistantMult
                         << "), weak to " << b.vulnerable << " (" << b.vulnerableMult
                         << ") valid=" << b.valid);
        if (!b.valid) continue;  // an even roster is allowed to have no bias
        ++biased;
        CHECK(!b.resistant.empty());
        CHECK(!b.vulnerable.empty());
        CHECK(b.resistant != b.vulnerable);
        // Resistant must actually resist and vulnerable must actually hurt more,
        // or the label is worse than saying nothing.
        CHECK(b.resistantMult < b.vulnerableMult);
    }
    // Most of the campaign must be biased or the replayability claim is empty.
    UNSCOPED_INFO("maps with a real elemental bias: " << biased);
    CHECK(biased >= 4);
}

TEST_CASE("the opening map does not resist the starting tower", "[mapfacts]") {
    // Greenfields is the tutorial and the arrow tower is the only tower a new
    // profile can build. If map 1 advertises a piercing resistance, the first
    // thing the game tells a new player is that their only weapon is the wrong
    // one -- and it was doing exactly that off a 0.93 average.
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");
    const auto b = content::mapBias(reg, map);

    // Two separate requirements. First: the card must never ADVERTISE a piercing
    // resistance on the opening map -- that is what the player is actually told.
    CHECK_FALSE((b.valid && b.resistant == "piercing"));

    // Second, and the real guardrail: the roster must not drift into genuinely
    // punishing the starting tower as enemies are added. Measured directly so
    // this cannot be satisfied by merely hiding the label.
    float total = 0.0f;
    int counted = 0;
    for (const auto& p : map.recipe.pool) {
        if (!reg.hasEnemy(p.enemyId)) continue;
        const auto& r = reg.enemy(p.enemyId).resist;
        const auto it = r.find("piercing");
        total += it == r.end() ? 1.0f : it->second;
        ++counted;
    }
    REQUIRE(counted > 0);
    const float avg = total / static_cast<float>(counted);
    UNSCOPED_INFO("greenfields pool piercing multiplier: " << avg);
    CHECK(avg >= 0.88f);
}

TEST_CASE("the maps do not all share one resistance", "[mapfacts]") {
    // If every map resisted the same thing, one build would clear the campaign
    // and the five maps would be one map five times.
    const auto reg = loadReg();
    std::set<std::string> resistances;
    for (const auto& [id, map] : reg.maps()) {
        const auto b = content::mapBias(reg, map);
        if (b.valid) resistances.insert(b.resistant);
    }
    UNSCOPED_INFO("distinct resistances across the campaign: " << resistances.size());
    CHECK(resistances.size() >= 3);
}

TEST_CASE("bias counts a silent enemy as neutral", "[mapfacts]") {
    // The trap this guards: summing only the enemies that MENTION a type makes
    // one enemy with a single 0.5 entry read as the whole roster resisting it.
    const auto reg = loadReg();
    const auto b = content::mapBias(reg, reg.map("greenfields"));
    // Averages including neutral entries must land near 1, never at the extreme
    // of any single enemy's table.
    CHECK(b.resistantMult > 0.4f);
    CHECK(b.vulnerableMult < 1.6f);
}
