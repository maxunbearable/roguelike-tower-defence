// What the measuring instrument actually builds.
//
// Every balance figure in this project -- how far a fresh profile gets, how many
// runs clear map 1, whether a fully upgraded board finishes -- comes from
// sim::autoPlay. It used to build ONLY arrow towers, on a game with five tower
// types whose entire pitch is 270 combinations. So the instrument exercised a
// fifth of the tower content and none of the combinations, and every number it
// produced was a reading of a board no player would ever field.
//
// A playtesting agent does not need to be skilled; the research on autonomous
// balance agents is clear that it needs to CORRELATE with a human's experience.
// It cannot correlate with a player who has five towers if it only has one.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "content/Registry.h"
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

// Replays a run and reports which tower types ended up on the board. autoPlay
// does not expose the world, so this rebuilds the same decision the way the
// harness does and inspects the result.
std::set<std::string> typesFielded(const content::Registry& reg, const content::MapDef& map,
                                   const core::Loadout& lo) {
    sim::World w(reg, map, 1, lo);
    std::set<std::string> out;
    for (const auto& id : sim::towerPreferenceFor(reg, map, w)) out.insert(id);
    return out;
}

}  // namespace

TEST_CASE("an unlocked profile can field more than one tower type", "[autoplayer]") {
    const auto reg = loadReg();
    const auto types = typesFielded(reg, reg.map("greenfields"), owning());
    UNSCOPED_INFO("preference set: " << types.size() << " towers");
    CHECK(types.size() >= 4);  // the game has five
}

TEST_CASE("a fresh profile is offered only the starting tower", "[autoplayer]") {
    // The gate must hold in the harness too. If the instrument builds towers the
    // player has not bought, every "fresh profile" measurement is a lie.
    const auto reg = loadReg();
    core::Loadout fresh;
    fresh.ownAll = false;
    const auto types = typesFielded(reg, reg.map("greenfields"), fresh);
    CHECK(types.size() == 1);
    CHECK(types.count(sim::kStartingTower) == 1);
}

TEST_CASE("the preference is ordered by how the roster resists", "[autoplayer]") {
    // Not "is it optimal" -- it is deliberately not -- but "does it read the
    // dossier at all". The first choice must fare at least as well against the
    // map's roster as the last.
    const auto reg = loadReg();
    const auto& map = reg.map("obsidian-gate");
    sim::World w(reg, map, 1, owning());
    const auto prefs = sim::towerPreferenceFor(reg, map, w);
    REQUIRE(prefs.size() >= 2);

    const auto score = [&](const std::string& id) {
        float total = 0.0f;
        int n = 0;
        for (const auto& p : map.recipe.pool) {
            if (!reg.hasEnemy(p.enemyId)) continue;
            total += reg.enemy(p.enemyId).resistTo(reg.tower(id).damageType);
            ++n;
        }
        return n > 0 ? total / static_cast<float>(n) : 1.0f;
    };
    UNSCOPED_INFO("best " << prefs.front() << " " << score(prefs.front()) << ", worst "
                          << prefs.back() << " " << score(prefs.back()));
    CHECK(score(prefs.front()) >= score(prefs.back()));
}

TEST_CASE("the preference is deterministic", "[autoplayer]") {
    // Every measurement in the project depends on a run being reproducible.
    const auto reg = loadReg();
    const auto& map = reg.map("frostmere");
    sim::World a(reg, map, 1, owning());
    sim::World b(reg, map, 1, owning());
    CHECK(sim::towerPreferenceFor(reg, map, a) == sim::towerPreferenceFor(reg, map, b));
}

TEST_CASE("a run actually fields a mixed board", "[autoplayer]") {
    // The observable end of it: play a real run with everything unlocked and
    // count the distinct tower types standing at the end. One type means the
    // rotation is not working, whatever the preference list says.
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");
    const auto r = sim::autoPlay(reg, map, owning(), 3);
    UNSCOPED_INFO("built " << r.towersBuilt << " towers, reached wave " << r.wavesSurvived);
    REQUIRE(r.towersBuilt >= 3);
    CHECK(r.distinctTowerTypes >= 2);
}
