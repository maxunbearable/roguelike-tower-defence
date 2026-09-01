#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "core/Resolve.h"

using namespace td;

static content::Registry loaded() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

static core::Loadout make(const std::string& towerSpec, const std::string& elementSpec) {
    core::Loadout lo;
    lo.towerId = "arrow";
    lo.towerSpec = towerSpec;
    lo.elementId = "earth";
    lo.elementSpec = elementSpec;
    lo.ownAll = true;
    return lo;
}

TEST_CASE("the trunk applies to every spec", "[resolve]") {
    const auto r = loaded();
    // Trunk grants +0.5 range before any spec multiplier, so all three specs
    // must show the trunk's contribution.
    for (const char* spec : {"sniper", "elf", "hunter"}) {
        const auto sb = core::resolveStats(r, make(spec, "poison"));
        REQUIRE(sb.get("arrow.range") > 0.0f);
    }
}

TEST_CASE("a non-equipped branch contributes nothing", "[resolve]") {
    // THE load-bearing property of the whole design: the player owns every
    // branch, but only the equipped one may fold in.
    const auto r = loaded();
    const auto sniper = core::resolveStats(r, make("sniper", "poison"));
    const auto elf = core::resolveStats(r, make("elf", "poison"));

    REQUIRE(sniper.flag("arrow.trait.execute"));
    REQUIRE_FALSE(sniper.flag("arrow.trait.rampUp"));

    REQUIRE(elf.flag("arrow.trait.rampUp"));
    REQUIRE_FALSE(elf.flag("arrow.trait.execute"));

    // Hunter's projectile adds must not leak into the other two.
    REQUIRE(sniper.get("arrow.projectileCount") == 1.0f);
    REQUIRE(elf.get("arrow.projectileCount") == 1.0f);
    REQUIRE(core::resolveStats(r, make("hunter", "poison")).get("arrow.projectileCount") == 4.0f);
}

TEST_CASE("the three tower specs produce genuinely different profiles", "[resolve]") {
    const auto r = loaded();
    const auto sn = core::resolveStats(r, make("sniper", "poison"));
    const auto el = core::resolveStats(r, make("elf", "poison"));
    const auto hu = core::resolveStats(r, make("hunter", "poison"));

    // Sniper: heaviest hit, slowest, longest reach.
    REQUIRE(sn.get("arrow.damage") > el.get("arrow.damage"));
    REQUIRE(sn.get("arrow.damage") > hu.get("arrow.damage"));
    REQUIRE(sn.get("arrow.fireRate") < el.get("arrow.fireRate"));
    REQUIRE(sn.get("arrow.range") > el.get("arrow.range"));
    REQUIRE(sn.get("arrow.armorPen") > 0.0f);

    // Elf: fastest by a wide margin.
    REQUIRE(el.get("arrow.fireRate") > hu.get("arrow.fireRate"));

    // Hunter: the only one that fires more than one arrow, and the only one
    // that pierces.
    REQUIRE(hu.get("arrow.projectileCount") > 1.0f);
    REQUIRE(hu.get("arrow.pierce") > 0.0f);
    REQUIRE(sn.get("arrow.pierce") == 0.0f);
}

TEST_CASE("element specs are likewise isolated", "[resolve]") {
    const auto r = loaded();
    const auto poison = core::resolveStats(r, make("sniper", "poison"));
    const auto rock = core::resolveStats(r, make("sniper", "rock"));

    // poison.core multiplies dpsPerStack; rock loadout must not receive it.
    REQUIRE(poison.get("earth.poison.dpsPerStack") > rock.get("earth.poison.dpsPerStack"));
    REQUIRE(rock.get("earth.rock.shredPerHit") > poison.get("earth.rock.shredPerHit"));
}

TEST_CASE("owning nothing yields base stats only", "[resolve]") {
    const auto r = loaded();
    auto lo = make("sniper", "poison");
    lo.ownAll = false;  // owns no nodes at all
    const auto sb = core::resolveStats(r, lo);
    REQUIRE(sb.get("arrow.damage") == r.tower("arrow").damage);
    REQUIRE_FALSE(sb.flag("arrow.trait.execute"));
}

TEST_CASE("owning a single node applies exactly that node", "[resolve]") {
    const auto r = loaded();
    auto lo = make("sniper", "poison");
    lo.ownAll = false;
    lo.ownedNodes.insert("arrow.trunk.dmg1");
    const auto sb = core::resolveStats(r, lo);
    REQUIRE(sb.get("arrow.damage") == r.tower("arrow").damage * 1.10f);
}

TEST_CASE("resolution is deterministic and repeatable", "[resolve]") {
    const auto r = loaded();
    const auto a = core::resolveStats(r, make("elf", "quake"));
    const auto b = core::resolveStats(r, make("elf", "quake"));
    REQUIRE(a.get("arrow.fireRate") == b.get("arrow.fireRate"));
    REQUIRE(a.flags() == b.flags());
}
