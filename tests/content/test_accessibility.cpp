// Colour must never be the only way to read something.
//
// Measured against Steam's published accessibility feature list -- which now
// appears on store pages and which players can filter by -- this game failed
// Color Alternatives outright: eleven damage types were separated purely by hue
// in the readout that tells a player what to build against. Two pairs are barely
// separable with full colour vision, let alone without it.
//
// The guardrail below is the point of this file: adding a damage type without
// giving it a tag has to fail a test rather than quietly ship a pip that only
// colour can identify.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
#include <set>
#include <string>

#include "content/Registry.h"
#include "core/Settings.h"
#include "core/DamageTags.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// Every damage type any enemy actually resists, plus every type a tower deals.
std::set<std::string> damageTypesInPlay(const content::Registry& reg) {
    std::set<std::string> out;
    for (const auto& [id, e] : reg.enemies()) {
        for (const auto& [type, mult] : e.resist) out.insert(type);
    }
    for (const auto& [id, t] : reg.towers()) {
        if (!t.damageType.empty()) out.insert(t.damageType);
    }
    return out;
}
}  // namespace

TEST_CASE("every damage type in the content has a readable tag", "[a11y]") {
    const auto reg = loadReg();
    const auto types = damageTypesInPlay(reg);
    REQUIRE(types.size() >= 5);  // otherwise this test is not looking at anything

    for (const auto& t : types) {
        INFO("damage type: " << t);
        const std::string tag = core::damageTypeTag(t);
        // "??" is the fallback. A type reaching it is a type the player can only
        // identify by colour.
        CHECK(tag != "??");
        CHECK(tag.size() == 2);
    }
}

TEST_CASE("no two damage types share a tag", "[a11y]") {
    // A duplicated tag is exactly as unreadable as no tag.
    const auto reg = loadReg();
    std::map<std::string, std::string> seen;
    for (const auto& t : damageTypesInPlay(reg)) {
        const std::string tag = core::damageTypeTag(t);
        INFO(t << " tagged " << tag);
        CHECK(seen.count(tag) == 0);
        seen[tag] = t;
    }
}

TEST_CASE("the shake presets cover off, reduced and full", "[a11y]") {
    // Camera Comfort asks for shake to be adjustable OR absent. Offering only
    // on/off would force a player who finds it uncomfortable to give up the
    // feedback entirely, so there is a middle setting.
    CHECK(core::kShakeLevels[0] == 1.0f);
    CHECK(core::kShakeLevels[2] == 0.0f);
    CHECK(core::kShakeLevels[1] > 0.0f);
    CHECK(core::kShakeLevels[1] < 1.0f);

    for (int i = 0; i < 3; ++i) {
        INFO("preset " << i);
        CHECK(core::shakeIndexOf(core::kShakeLevels[i]) == i);
    }
    // A value from an older save that matches no preset must not crash or land
    // out of range.
    CHECK(core::shakeIndexOf(0.77f) >= 0);
    CHECK(core::shakeIndexOf(0.77f) < 3);
}
