#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "content/Registry.h"

#include "support/Plots.h"
#include "core/Progression.h"
#include "core/SaveGame.h"
#include "core/SaveIO.h"
#include "sim/World.h"

using namespace td;

namespace {

// Every test here points the save directory at a scratch path, so a test run
// can never touch a real player profile.
struct TempSaveDir {
    std::filesystem::path dir;
    TempSaveDir() {
        dir = std::filesystem::temp_directory_path() / "td_save_test";
        std::filesystem::remove_all(dir);
        setenv("TD_SAVE_DIR", dir.c_str(), 1);
    }
    ~TempSaveDir() {
        std::filesystem::remove_all(dir);
        unsetenv("TD_SAVE_DIR");
    }
};

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

}  // namespace

TEST_CASE("a save slot survives a JSON round trip", "[save]") {
    core::SaveSlot s;
    s.used = true;
    s.profileName = "Slot A";
    s.meta.shards = 137;
    s.meta.runsPlayed = 4;
    s.meta.bestWave = 22;
    s.meta.ownedNodes = {"arrow.trunk.dmg1", "global.gold1"};

    core::RunSave r;
    r.mapId = "greenfields";
    r.seed = 4242;
    r.rngState = "1 2 3";
    r.waveIndex = 12;
    r.gold = 340;
    r.lives = 17;
    r.buildTimer = 7.5f;
    r.towers.push_back({3, 1, "cannon", 2, 195, "earth", "elf", "poison", "strongest"});
    s.run = r;

    const auto back = core::fromJson(core::toJson(s));
    REQUIRE(back.used);
    REQUIRE(back.meta.shards == 137);
    REQUIRE(back.meta.ownedNodes == s.meta.ownedNodes);
    REQUIRE(back.hasRunInProgress());
    REQUIRE(back.run->waveIndex == 12);
    REQUIRE(back.run->towers.size() == 1);
    REQUIRE(back.run->towers[0].elementSpec == "poison");
    // A saved tower must remember WHAT it is and how it was aimed. Both used to
    // be dropped, and restore() rebuilt everything as a level-N arrow tower.
    REQUIRE(back.run->towers[0].towerId == "cannon");
    REQUIRE(back.run->towers[0].priority == "strongest");
}

TEST_CASE("a slot with no run in progress round trips too", "[save]") {
    core::SaveSlot s;
    s.used = true;
    s.meta.shards = 10;
    const auto back = core::fromJson(core::toJson(s));
    REQUIRE(back.used);
    REQUIRE_FALSE(back.hasRunInProgress());
}

TEST_CASE("slots write, read back and delete", "[save]") {
    TempSaveDir tmp;
    core::SaveSlot s;
    s.used = true;
    s.meta.shards = 55;

    REQUIRE(core::writeSlot(1, s));
    REQUIRE(std::filesystem::exists(core::slotPath(1)));

    const auto back = core::loadSlot(1);
    REQUIRE(back.used);
    REQUIRE(back.meta.shards == 55);

    REQUIRE(core::deleteSlot(1));
    REQUIRE_FALSE(core::loadSlot(1).used);
}

TEST_CASE("an unwritten slot reads as empty rather than failing", "[save]") {
    TempSaveDir tmp;
    for (int i = 0; i < core::kSlotCount; ++i) REQUIRE_FALSE(core::loadSlot(i).used);
}

TEST_CASE("a corrupt save reads as empty rather than crashing the game", "[save]") {
    TempSaveDir tmp;
    std::filesystem::create_directories(core::saveDir());
    {
        std::ofstream out(core::slotPath(0));
        out << "{ this is not valid json";
    }
    REQUIRE_FALSE(core::loadSlot(0).used);  // must not throw
}

TEST_CASE("a save from an unsupported version is refused", "[save]") {
    REQUIRE_THROWS(core::fromJson(R"({"version": 999, "used": true})"));
}

TEST_CASE("a run snapshot restores an identical world", "[save]") {
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 7, core::Loadout{}, /*goldOverride=*/5000);
    a.placeTower(PLOT(0), "arrow");
    while (a.upgradeCost(PLOT(0)) > 0) a.upgradeTower(PLOT(0));
    a.attachElement(PLOT(0), "earth");
    a.specialiseTower(PLOT(0), "elf");
    a.specialiseElement(PLOT(0), "poison");
    a.upgradeTower(PLOT(0));
    a.placeTower(PLOT(1), "arrow");

    REQUIRE(a.canSnapshot());
    const auto snap = a.snapshot();

    sim::World b(reg, reg.map("greenfields"), 1);
    b.restore(snap);

    REQUIRE(b.gold() == a.gold());
    REQUIRE(b.lives() == a.lives());
    REQUIRE(b.waveIndex() == a.waveIndex());
    REQUIRE(b.activeTowerSpecs() == a.activeTowerSpecs());
    REQUIRE(b.activeElementSpecs() == a.activeElementSpecs());

    const auto ta = a.towerAt(PLOT(0));
    const auto tb = b.towerAt(PLOT(0));
    REQUIRE((ta != entt::null));
    REQUIRE((tb != entt::null));
    const auto& tagA = a.reg().get<sim::TowerTag>(ta);
    const auto& tagB = b.reg().get<sim::TowerTag>(tb);
    REQUIRE(tagB.level == tagA.level);
    REQUIRE(tagB.towerSpec == tagA.towerSpec);
    REQUIRE(tagB.elementSpec == tagA.elementSpec);
    REQUIRE(tagB.goldSpent == tagA.goldSpent);
    // Restored towers must fight identically, not merely look the same.
    REQUIRE(b.reg().get<sim::TowerStats>(tb).damage ==
            a.reg().get<sim::TowerStats>(ta).damage);
    REQUIRE(b.reg().all_of<sim::ElementRef>(tb));
    REQUIRE((b.towerAt(PLOT(1)) != entt::null));
}

TEST_CASE("restoring does not charge for the towers again", "[save]") {
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 7, core::Loadout{}, /*goldOverride=*/5000);
    a.placeTower(PLOT(0), "arrow");
    while (a.upgradeCost(PLOT(0)) > 0) a.upgradeTower(PLOT(0));
    a.specialiseTower(PLOT(0), "sniper");
    const int goldBefore = a.gold();

    sim::World b(reg, reg.map("greenfields"), 1);
    b.restore(a.snapshot());
    REQUIRE(b.gold() == goldBefore);
}

TEST_CASE("a resumed run continues the same random sequence", "[save]") {
    const auto reg = loadReg();
    auto play = [&](bool viaSave) {
        sim::World w(reg, reg.map("greenfields"), 99, core::Loadout{}, /*goldOverride=*/5000);
        w.placeTower(PLOT(0), "arrow");
        if (viaSave) {
            sim::World other(reg, reg.map("greenfields"), 1);
            other.restore(w.snapshot());
            other.startNextWave();
            for (int i = 0; i < 1200; ++i) other.tick(sim::kFixedDt);
            return std::make_pair(other.gold(), other.lives());
        }
        w.startNextWave();
        for (int i = 0; i < 1200; ++i) w.tick(sim::kFixedDt);
        return std::make_pair(w.gold(), w.lives());
    };
    REQUIRE(play(true) == play(false));
}

TEST_CASE("a losing run still earns shards", "[save][progression]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    for (int i = 0; i < 400 && w.phase() != sim::Phase::Defeated; ++i) {
        if (w.phase() == sim::Phase::Build) w.startNextWave();
        for (int k = 0; k < 60; ++k) w.tick(sim::kFixedDt);
    }
    REQUIRE(w.phase() == sim::Phase::Defeated);
    REQUIRE(w.shardsForRun() > 0);  // a failed run must not feel wasted
}

TEST_CASE("skill nodes obey prerequisites and cost", "[progression]") {
    const auto reg = loadReg();
    const auto& tree = reg.tree("arrow");
    core::MetaSave meta;

    // A node behind a prerequisite is refused before the prerequisite is owned.
    REQUIRE(core::canBuy(tree, meta, "arrow.trunk.rate1") == core::BuyResult::MissingPrereq);

    const int price = tree.find("arrow.trunk.dmg1")->cost;  // read the price, do not assume it
    meta.shards = price - 1;
    REQUIRE(core::canBuy(tree, meta, "arrow.trunk.dmg1") == core::BuyResult::TooPoor);

    meta.shards = price + 50;
    REQUIRE(core::buyNode(tree, meta, "arrow.trunk.dmg1") == core::BuyResult::Ok);
    REQUIRE(meta.ownedNodes.count("arrow.trunk.dmg1") == 1);
    REQUIRE(meta.shards == 50);

    REQUIRE(core::buyNode(tree, meta, "arrow.trunk.dmg1") == core::BuyResult::AlreadyOwned);
    REQUIRE(meta.shards == 50);  // a refused purchase must not charge

    REQUIRE(core::canBuy(tree, meta, "arrow.trunk.rate1") == core::BuyResult::Ok);
    REQUIRE(core::canBuy(tree, meta, "nope.nope") == core::BuyResult::UnknownNode);
}

TEST_CASE("owned nodes actually change a run's stats", "[progression]") {
    const auto reg = loadReg();
    core::Loadout none;
    none.ownAll = false;
    core::Loadout some = none;
    some.ownedNodes.insert("arrow.trunk.dmg1");

    sim::World a(reg, reg.map("greenfields"), 1, none, 5000);
    sim::World b(reg, reg.map("greenfields"), 1, some, 5000);
    a.placeTower(PLOT(0), "arrow");
    b.placeTower(PLOT(0), "arrow");
    REQUIRE(b.reg().get<sim::TowerStats>(b.towerAt(PLOT(0))).damage >
            a.reg().get<sim::TowerStats>(a.towerAt(PLOT(0))).damage);
}
