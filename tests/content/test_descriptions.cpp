#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "content/Registry.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
// Long enough to actually say something. "Fire" is a name, not a description.
constexpr size_t kMinUseful = 20;
}  // namespace

TEST_CASE("every tower explains what it is for", "[content][desc]") {
    const auto reg = loadReg();
    REQUIRE_FALSE(reg.towers().empty());
    for (const auto& [id, def] : reg.towers()) {
        UNSCOPED_INFO("tower " << id << ": \"" << def.desc << "\"");
        REQUIRE(def.desc.size() >= kMinUseful);
    }
}

TEST_CASE("every element explains what imbuing it buys", "[content][desc]") {
    const auto reg = loadReg();
    REQUIRE_FALSE(reg.elements().empty());
    for (const auto& [id, def] : reg.elements()) {
        UNSCOPED_INFO("element " << id << ": \"" << def.desc << "\"");
        REQUIRE(def.desc.size() >= kMinUseful);
    }
}

TEST_CASE("every specialisation has a core node that describes it", "[content][desc]") {
    // The menu reads `<tree>.<spec>.core` for the text it shows when the player
    // is choosing a specialisation. A spec whose core node is missing or has an
    // empty desc would silently fall back to a generic sentence, which is what
    // every spec used to show.
    const auto reg = loadReg();
    int checked = 0;
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& spec : tree.specs) {
            const std::string coreId = treeId + "." + spec + ".core";
            const auto* node = tree.find(coreId);
            UNSCOPED_INFO("looking for " << coreId);
            REQUIRE(node != nullptr);
            UNSCOPED_INFO(coreId << ": \"" << node->desc << "\"");
            REQUIRE(node->desc.size() >= kMinUseful);
            ++checked;
        }
    }
    REQUIRE(checked == 33);  // 15 tower specs + 18 element specs
}

TEST_CASE("every skill tree node describes its effect", "[content][desc]") {
    const auto reg = loadReg();
    int nodes = 0;
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& n : tree.nodes) {
            UNSCOPED_INFO(n.id << ": name=\"" << n.name << "\" desc=\"" << n.desc << "\"");
            REQUIRE_FALSE(n.name.empty());
            // Shorter bar than the others: "+35% fire rate" is genuinely useful
            // at 14 characters. Below that a node is naming a mechanic without
            // explaining it, which is what "+1 pierce" did.
            REQUIRE(n.desc.size() >= 14);
            ++nodes;
        }
    }
    REQUIRE(nodes >= 126);
}

TEST_CASE("specialisation descriptions are not all the same", "[content][desc]") {
    // The regression this whole test file exists for: the menu replaced all 33
    // spec descriptions with one identical sentence about uniqueness, so
    // choosing between sniper, elf and hunter said nothing about any of them.
    const auto reg = loadReg();
    for (const auto& [treeId, tree] : reg.trees()) {
        if (tree.specs.size() < 2) continue;
        std::set<std::string> seen;
        for (const auto& spec : tree.specs) {
            const auto* n = tree.find(treeId + "." + spec + ".core");
            REQUIRE(n != nullptr);
            UNSCOPED_INFO(treeId << "/" << spec);
            REQUIRE(seen.insert(n->desc).second);  // must be distinct within a tree
        }
    }
}
