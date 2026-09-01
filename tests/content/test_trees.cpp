#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "content/Validate.h"
#include "core/Resolve.h"

using namespace td;

static content::Registry loaded() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

TEST_CASE("all three trees load and declare their branches", "[trees]") {
    const auto r = loaded();
    REQUIRE(r.hasTree("global"));
    REQUIRE(r.hasTree("arrow"));
    REQUIRE(r.hasTree("earth"));

    const auto& arrow = r.tree("arrow");
    REQUIRE(arrow.kind == core::SkillTree::Kind::Tower);
    REQUIRE(arrow.specs == std::vector<std::string>{"sniper", "elf", "hunter"});

    const auto& earth = r.tree("earth");
    REQUIRE(earth.kind == core::SkillTree::Kind::Element);
    REQUIRE(earth.specs == std::vector<std::string>{"poison", "rock", "quake"});
}

TEST_CASE("flag modifiers load without a value", "[trees]") {
    const auto r = loaded();
    const auto* n = r.tree("arrow").find("arrow.sniper.core");
    REQUIRE(n != nullptr);
    bool sawFlag = false;
    for (const auto& m : n->modifiers) {
        if (m.op == core::ModOp::Flag && m.target == "arrow.trait.execute") sawFlag = true;
    }
    REQUIRE(sawFlag);
}

TEST_CASE("the earth element loads its base and per-spec parameters", "[trees]") {
    const auto r = loaded();
    const auto& e = r.element("earth");
    REQUIRE(e.damageType == "earth");
    REQUIRE(e.specs.count("poison") == 1);
    REQUIRE(e.specs.count("rock") == 1);
    REQUIRE(e.specs.count("quake") == 1);
    REQUIRE(e.base.at("potency") == 1.0f);
    // Presence, not value: the exact tuning is expected to move.
    REQUIRE(e.specs.at("poison").count("maxStacks") == 1);
    REQUIRE(e.specs.at("poison").at("maxStacks") > 0.0f);
}

TEST_CASE("enemy resistances load and default to neutral", "[trees]") {
    const auto r = loaded();
    REQUIRE(r.enemy("goblin").resistTo("earth") == 0.6f);
    REQUIRE(r.enemy("wraith").resistTo("earth") == 1.35f);
    REQUIRE(r.enemy("slime").resistTo("earth") == 1.0f);   // undeclared is neutral
    REQUIRE(r.enemy("slime").resistTo("nonsense") == 1.0f);
}

TEST_CASE("shipped trees pass validation", "[trees][validate]") {
    const auto r = loaded();
    const auto errors = content::validate(r);
    for (const auto& e : errors) UNSCOPED_INFO("validation error: " << e);
    REQUIRE(errors.empty());
}

TEST_CASE("a node modifying an unknown stat path is reported", "[trees][validate]") {
    auto r = loaded();
    auto& t = const_cast<core::SkillTree&>(r.tree("arrow"));
    t.nodes[0].modifiers.push_back({"arrow.nonexistentStat", core::ModOp::Add, 1.0f});
    REQUIRE_FALSE(content::validate(r).empty());
}

TEST_CASE("a node requiring a missing prerequisite is reported", "[trees][validate]") {
    auto r = loaded();
    auto& t = const_cast<core::SkillTree&>(r.tree("arrow"));
    t.nodes[1].prereqs.push_back("arrow.does.not.exist");
    REQUIRE_FALSE(content::validate(r).empty());
}

TEST_CASE("a prerequisite cycle is reported", "[trees][validate]") {
    auto r = loaded();
    auto& t = const_cast<core::SkillTree&>(r.tree("arrow"));
    t.nodes[0].prereqs.push_back(t.nodes[1].id);  // 0 -> 1 and 1 -> 0
    REQUIRE_FALSE(content::validate(r).empty());
}

TEST_CASE("a node on an undeclared branch is reported", "[trees][validate]") {
    auto r = loaded();
    auto& t = const_cast<core::SkillTree&>(r.tree("arrow"));
    t.nodes[0].branch = "necromancer";
    REQUIRE_FALSE(content::validate(r).empty());
}
