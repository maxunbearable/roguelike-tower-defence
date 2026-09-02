// The exact numbers behind every specialisation.
//
// All 33 specialisation cores described themselves in prose alone -- "Far fewer,
// far heavier shots", "Hits inject stacking venom" -- with not one number
// between them. The choice that defines a whole build was made on flavour text,
// which is the thing tower defence design writing warns about most directly:
// state the figures, or the game is memorisation rather than thinking.
//
// These are derived from each node's own modifiers, so the tests check the
// DERIVATION against the content rather than checking a string against itself.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

#include "content/Registry.h"
#include "content/SpecFacts.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
bool hasDigit(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
}  // namespace

TEST_CASE("every specialisation states real numbers", "[specfacts]") {
    const auto reg = loadReg();
    int cores = 0;
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& spec : tree.specs) {
            const std::string id = treeId + "." + spec + ".core";
            if (!tree.find(id)) continue;
            ++cores;
            const auto line = content::specNumbersLine(tree, id);
            INFO(id << " -> " << line);
            CHECK(!line.empty());
            CHECK(hasDigit(line));
        }
    }
    UNSCOPED_INFO("specialisation cores covered: " << cores);
    CHECK(cores == 33);  // the count the game's own pitch rests on
}

TEST_CASE("the numbers come from the modifiers, not from prose", "[specfacts]") {
    // Sniper's authored modifiers are damage x2.6, fireRate x0.45, range x1.7,
    // critChance +0.20, armorPen +4. If the formatter is reading anything other
    // than those, these fail.
    const auto reg = loadReg();
    REQUIRE(reg.hasTree("arrow"));
    const auto line = content::specNumbersLine(reg.tree("arrow"), "arrow.sniper.core");
    INFO(line);
    CHECK(line.find("damage x2.6") != std::string::npos);
    CHECK(line.find("fire rate x0.45") != std::string::npos);
    CHECK(line.find("range x1.7") != std::string::npos);
    // A fraction is shown as a percentage: 0.20 must read as 20%, never 0.2.
    CHECK(line.find("crit +20%") != std::string::npos);
    CHECK(line.find("armour pen +4") != std::string::npos);
}

TEST_CASE("a multiplier on a fraction is not turned into a percentage", "[specfacts]") {
    // earth.poison.core multiplies dpsPerStack by 1.4. Multiplying a fraction is
    // still a multiplier -- showing "x140%" would be wrong.
    const auto reg = loadReg();
    REQUIRE(reg.hasTree("earth"));
    const auto line = content::specNumbersLine(reg.tree("earth"), "earth.poison.core");
    INFO(line);
    CHECK(line.find("x140") == std::string::npos);
}

TEST_CASE("trait flags are left to the prose", "[specfacts]") {
    // A line reading "trait.execute" teaches a player nothing. The flag turns the
    // trait on; the description says what it is; these say how much.
    const auto reg = loadReg();
    const auto line = content::specNumbersLine(reg.tree("arrow"), "arrow.sniper.core");
    CHECK(line.find("trait") == std::string::npos);
    CHECK(line.find("flag") == std::string::npos);
}

TEST_CASE("an unknown node yields nothing rather than nonsense", "[specfacts]") {
    const auto reg = loadReg();
    CHECK(content::specNumbers(reg.tree("arrow"), "arrow.does.not.exist").empty());
    CHECK(content::specNumbersLine(reg.tree("arrow"), "").empty());
}

TEST_CASE("the derivation tracks the content", "[specfacts]") {
    // The property that makes this worth doing: every number shown appears in the
    // node's modifiers. Hand-written descriptions drift when values are retuned;
    // a derivation cannot.
    const auto reg = loadReg();
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& spec : tree.specs) {
            const std::string id = treeId + "." + spec + ".core";
            const auto* node = tree.find(id);
            if (!node) continue;
            // One line per non-flag modifier that has a label.
            const auto lines = content::specNumbers(tree, id);
            size_t expected = 0;
            for (const auto& m : node->modifiers) {
                if (m.op != core::ModOp::Flag) ++expected;
            }
            INFO(id << ": " << lines.size() << " lines for " << expected << " modifiers");
            CHECK(lines.size() <= expected);   // never invents one
            CHECK(!lines.empty());             // never silent on a real spec
        }
    }
}
