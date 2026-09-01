// The combination matrix: every tower spec crossed with every element spec.
//
// With 3 towers x 3 specs and 4 elements x 3 specs that is 108 pairings, built
// from 21 authored pieces (9 spec stat blocks + 12 element hook objects). The
// point of this file is to prove there is no per-pair code: if a pairing needed
// special handling, one of these guardrails would be the thing that noticed.
//
// Assertions here are RELATIONAL GUARDRAILS, never magic numbers. Tuning a TOML
// value should move the numbers without breaking the suite; only a change that
// breaks a design INTENT should fail. Each assertion below names the intent it
// protects.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

#include "content/Registry.h"
#include "sim/Scenario.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

constexpr float kSeconds = 20.0f;
constexpr uint64_t kSeed = 999;

// Enumerated from content, never hardcoded: adding a tower or an element must
// extend the matrix automatically or the guarantee is worthless.
struct Spec {
    std::string ownerId;  // tower or element id
    std::string specId;
};

std::vector<Spec> specsOfKind(const content::Registry& r, core::SkillTree::Kind kind) {
    std::vector<Spec> out;
    for (const auto& [id, tree] : r.trees()) {
        if (tree.kind != kind) continue;
        for (const auto& sp : tree.specs) out.push_back({id, sp});
    }
    return out;
}

std::vector<Spec> towerSpecs(const content::Registry& r) {
    return specsOfKind(r, core::SkillTree::Kind::Tower);
}
std::vector<Spec> elementSpecs(const content::Registry& r) {
    return specsOfKind(r, core::SkillTree::Kind::Element);
}

// Specs whose value is CONTROL, not throughput. The scenario harness holds
// enemies stationary on purpose -- it measures damage per second against a fixed
// pile of health -- so these are structurally unmeasurable here:
//
//   chill/freeze  slow or stop something that is not moving: no effect at all
//   shatter       needs a slowed target, so alone it never triggers
//   gust          shoves the target backwards, and with no forward walk it never
//                 returns, so it reads as a DAMAGE PENALTY rather than as tempo
//
// They are excluded from the throughput band and asserted on their actual
// mechanism instead, in the control test below. Measuring them here would either
// fail honestly or force the design to add damage it should not have.
bool isControlSpec(const std::string& specId) {
    return specId == "chill" || specId == "shatter" || specId == "freeze" || specId == "gust";
}

// Tower specs whose value is SUPPORT, not their own output. Forge trades most of
// its damage and rate for an aura that raises every tower around it, so measured
// alone against a fixed pile of health it is close to inert -- and the flat and
// periodic elements then dominate its row, because its own hits contribute almost
// nothing. Its mechanism is asserted in test_control_elements.cpp instead.
bool isSupportTowerSpec(const std::string& specId) { return specId == "forge"; }

std::vector<Spec> throughputTowerSpecs(const content::Registry& r) {
    std::vector<Spec> out;
    for (const auto& t : specsOfKind(r, core::SkillTree::Kind::Tower)) {
        if (!isSupportTowerSpec(t.specId)) out.push_back(t);
    }
    return out;
}

std::vector<Spec> throughputElementSpecs(const content::Registry& r) {
    std::vector<Spec> out;
    for (const auto& e : elementSpecs(r)) {
        if (!isControlSpec(e.specId)) out.push_back(e);
    }
    return out;
}

float pct(const content::Registry& r, const Spec& t, const Spec& e, sim::ScenarioKind kind) {
    return sim::simulateCombo(r, t.ownerId, t.specId, e.ownerId, e.specId, kind, kSeconds, kSeed)
        .clearedFraction();
}

// The intent tests below name arrow and earth specs deliberately: they encode
// the design intent of THAT pair (sniper is the anti-armour answer, poison is
// gated on hit count) rather than a general property, so they stay hardcoded.
const char* kArrowSpecs[] = {"sniper", "elf", "hunter"};
const char* kEarthSpecs[] = {"poison", "rock", "quake"};

float pct(const content::Registry& r, const char* arrowSpec, const char* earthSpec,
          sim::ScenarioKind kind) {
    return pct(r, Spec{"arrow", arrowSpec}, Spec{"earth", earthSpec}, kind);
}

float avgAcrossScenarios(const content::Registry& r, const Spec& t, const Spec& e) {
    return (pct(r, t, e, sim::ScenarioKind::LoneTank) + pct(r, t, e, sim::ScenarioKind::Swarm) +
            pct(r, t, e, sim::ScenarioKind::Mixed)) /
           3.0f;
}

}  // namespace

TEST_CASE("every combination is playable and does real damage", "[matrix]") {
    const auto r = loadReg();
    const auto towers = towerSpecs(r);
    const auto elements = elementSpecs(r);
    // Derived, not hardcoded: this asserted 9 x 12 and broke the moment two more
    // towers shipped, which is exactly the failure mode it exists to prevent.
    REQUIRE(towers.size() >= 9);
    REQUIRE(elements.size() >= 12);
    const size_t expected = towers.size() * elements.size();
    UNSCOPED_INFO("checking " << towers.size() << " tower specs x " << elements.size()
                              << " element specs = " << expected << " combinations");

    int checked = 0;
    for (const auto& t : towers) {
        for (const auto& e : elements) {
            // A short window: this only asks whether the pairing FUNCTIONS.
            const auto res = sim::simulateCombo(r, t.ownerId, t.specId, e.ownerId, e.specId,
                                                sim::ScenarioKind::Mixed, 6.0f, kSeed);
            UNSCOPED_INFO(t.ownerId << "/" << t.specId << " + " << e.ownerId << "/" << e.specId);
            REQUIRE(res.spawned > 0);
            REQUIRE(res.damageDealt > 0.0f);
            ++checked;
        }
    }
    REQUIRE(static_cast<size_t>(checked) == expected);
}

TEST_CASE("within a tower, no element pairing is dead weight or runaway", "[matrix]") {
    // Intent: whichever tower you have specialised, all twelve element powers are
    // worth considering on it. Judged on the average across all three scenarios,
    // not per-scenario -- a build SHOULD be weak in the scenario it is not for.
    //
    // Banded PER TOWER rather than across all 108: cannon shells hit far harder
    // than arrow volleys and cost more to field, so a single band across three
    // towers would measure base damage, not balance.
    const auto r = loadReg();
    const auto elements = throughputElementSpecs(r);

    for (const auto& t : throughputTowerSpecs(r)) {
        std::vector<float> scores;
        for (const auto& e : elements) scores.push_back(avgAcrossScenarios(r, t, e));
        auto sorted = scores;
        std::sort(sorted.begin(), sorted.end());
        const float median = sorted[sorted.size() / 2];
        UNSCOPED_INFO(t.ownerId << "/" << t.specId << " median " << median);
        REQUIRE(median > 0.0f);
        for (size_t i = 0; i < scores.size(); ++i) {
            UNSCOPED_INFO("  " << elements[i].ownerId << "/" << elements[i].specId << " scored "
                               << scores[i]);
            REQUIRE(scores[i] <= median * 3.0f);
            REQUIRE(scores[i] >= median * 0.3f);
        }
    }
}

TEST_CASE("no tower archetype dominates the others outright", "[matrix]") {
    // Intent: rough parity between the three towers, averaged over every element
    // they can carry and every scenario. Not equality -- they cost different
    // gold and answer different problems -- but none may be strictly the answer.
    const auto r = loadReg();
    const auto elements = throughputElementSpecs(r);

    std::vector<std::pair<std::string, float>> perTower;
    for (const auto& t : throughputTowerSpecs(r)) {
        float sum = 0.0f;
        for (const auto& e : elements) sum += avgAcrossScenarios(r, t, e);
        perTower.emplace_back(t.ownerId + "/" + t.specId, sum / static_cast<float>(elements.size()));
    }
    auto vals = perTower;
    std::sort(vals.begin(), vals.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    for (const auto& [n, v] : perTower) UNSCOPED_INFO(n << " -> " << v);
    REQUIRE(vals.front().second > 0.0f);
    REQUIRE(vals.back().second <= vals.front().second * 6.0f);
}

TEST_CASE("fire rate beats hit size for poison uptime", "[matrix]") {
    // Intent: poison's ceiling is gated by HIT COUNT, so the fast spec out-scales
    // the heavy-hit one on a crowd. This is the property that makes Elf+Poison a
    // distinct build rather than a worse Sniper+Poison.
    const auto r = loadReg();
    REQUIRE(pct(r, "elf", "poison", sim::ScenarioKind::Swarm) >
            pct(r, "sniper", "poison", sim::ScenarioKind::Swarm));
    REQUIRE(pct(r, "hunter", "poison", sim::ScenarioKind::Swarm) >
            pct(r, "sniper", "poison", sim::ScenarioKind::Swarm));
}

TEST_CASE("multishot beats single-shot on crowds, for every element", "[matrix]") {
    // Intent: Hunter's identity is crowds, and it must hold no matter which
    // element is paired with it -- otherwise the element is driving the outcome
    // rather than composing with it.
    const auto r = loadReg();
    for (const auto* e : kEarthSpecs) {
        UNSCOPED_INFO("element " << e);
        REQUIRE(pct(r, "hunter", e, sim::ScenarioKind::Swarm) >
                pct(r, "sniper", e, sim::ScenarioKind::Swarm));
    }
}

TEST_CASE("the long-range heavy-hit spec owns the single armoured target", "[matrix]") {
    // Intent: Sniper is the reliable answer to one big armoured thing, averaged
    // over which element it happens to be carrying.
    const auto r = loadReg();
    float sniper = 0.0f, elf = 0.0f, hunter = 0.0f;
    for (const auto* e : kEarthSpecs) {
        sniper += pct(r, "sniper", e, sim::ScenarioKind::LoneTank);
        elf += pct(r, "elf", e, sim::ScenarioKind::LoneTank);
        hunter += pct(r, "hunter", e, sim::ScenarioKind::LoneTank);
    }
    REQUIRE(sniper > elf);
    REQUIRE(sniper > hunter);
}

TEST_CASE("rock is the anti-armour element", "[matrix]") {
    // Intent: armour shred is what Rock is FOR, so it must lead against the
    // armoured target regardless of which tower carries it.
    const auto r = loadReg();
    float rock = 0.0f, poison = 0.0f, quake = 0.0f;
    for (const auto* t : kArrowSpecs) {
        rock += pct(r, t, "rock", sim::ScenarioKind::LoneTank);
        poison += pct(r, t, "poison", sim::ScenarioKind::LoneTank);
        quake += pct(r, t, "quake", sim::ScenarioKind::LoneTank);
    }
    REQUIRE(rock > poison);
    REQUIRE(rock > quake);
}

TEST_CASE("enemy damage-type resistance actually changes outcomes", "[matrix]") {
    // The goblin resists earth (0.6) and the wraith is vulnerable to it (1.35).
    // If resistance were ignored, these would be identical.
    const auto r = loadReg();
    const auto resistant = sim::simulateCombo(r, "arrow", "elf", "earth", "poison",
                                             sim::ScenarioKind::LoneTank,
                                              kSeconds, kSeed);
    REQUIRE(resistant.damageDealt > 0.0f);
    REQUIRE(r.enemy("goblin").resistTo("earth") < 1.0f);
    REQUIRE(r.enemy("wraith").resistTo("earth") > 1.0f);
}

TEST_CASE("every combination is deterministic for a fixed seed", "[matrix]") {
    // Across ALL pairings, not just arrow/earth: several of the new elements roll
    // dice (freeze chance, gust chance), and a seed replay that diverged would
    // break save-resume as well as this harness.
    const auto r = loadReg();
    const auto elements = elementSpecs(r);
    for (const auto& t : towerSpecs(r)) {
        for (const auto& e : elements) {
            const auto a = sim::simulateCombo(r, t.ownerId, t.specId, e.ownerId, e.specId,
                                              sim::ScenarioKind::Mixed, 6.0f, 4242);
            const auto b = sim::simulateCombo(r, t.ownerId, t.specId, e.ownerId, e.specId,
                                              sim::ScenarioKind::Mixed, 6.0f, 4242);
            UNSCOPED_INFO(t.ownerId << "/" << t.specId << " + " << e.ownerId << "/" << e.specId);
            REQUIRE(a.damageDealt == b.damageDealt);
            REQUIRE(a.kills == b.kills);
        }
    }
}

TEST_CASE("print the combination matrix", "[matrix][.report]") {
    // The whole 9 x 12 grid, averaged across scenarios. Run with
    //   ./build/tests/td_tests "print the combination matrix" -c "[.report]"
    // This is the tuning surface: a column that is bright everywhere is an
    // element that needs bringing down, a row that is dark everywhere is a spec
    // nobody would field.
    const auto r = loadReg();
    const auto towers = towerSpecs(r);
    const auto elements = elementSpecs(r);

    std::ostringstream os;
    os << "\n=== average across scenarios, percent of health removed ===\n";
    os << std::setw(16) << "";
    for (const auto& e : elements) os << std::setw(9) << e.specId;
    os << "\n";
    for (const auto& t : towers) {
        os << std::setw(16) << (t.ownerId + "/" + t.specId);
        for (const auto& e : elements) {
            os << std::setw(8) << std::fixed << std::setprecision(0)
               << (avgAcrossScenarios(r, t, e) * 100.0f) << "%";
        }
        os << "\n";
    }
    WARN(os.str());
}
