#include <catch2/catch_test_macros.hpp>

#include "core/SaveGame.h"

using namespace td::core;

TEST_CASE("map progress round-trips through JSON", "[save][maps]") {
    SaveSlot in;
    in.used = true;
    in.profileName = "Slot 1";
    in.meta.shards = 240;
    in.meta.mapProgress["greenfields"] = {50, true};
    in.meta.mapProgress["ashen-wastes"] = {31, false};

    const auto out = fromJson(toJson(in));
    REQUIRE(out.meta.mapProgress.size() == 2);
    REQUIRE(out.meta.mapProgress.at("greenfields").bestWave == 50);
    REQUIRE(out.meta.mapProgress.at("greenfields").cleared);
    REQUIRE(out.meta.mapProgress.at("ashen-wastes").bestWave == 31);
    REQUIRE_FALSE(out.meta.mapProgress.at("ashen-wastes").cleared);
}

TEST_CASE("a save written before map progress existed still loads", "[save][maps]") {
    // Exactly the shape kSaveVersion 1 wrote: no mapProgress key at all. A
    // player mid-profile must not lose their shards to a format change.
    const std::string v1 = R"({
        "version": 1,
        "used": true,
        "profileName": "Slot 1",
        "meta": { "shards": 77, "runsPlayed": 4, "bestWave": 18,
                  "ownedNodes": ["global.gold1"] }
    })";
    const auto out = fromJson(v1);
    REQUIRE(out.used);
    REQUIRE(out.meta.shards == 77);
    REQUIRE(out.meta.bestWave == 18);
    REQUIRE(out.meta.ownedNodes.count("global.gold1") == 1);
    REQUIRE(out.meta.mapProgress.empty());  // absent, not garbage
}

TEST_CASE("clearing a map is what unlocks the next one", "[save][maps]") {
    SaveSlot s;
    const std::vector<std::string> order = {"greenfields", "ashen-wastes", "frostmere"};

    // Nothing cleared: only the first map is playable.
    REQUIRE(mapUnlocked(s.meta, order, 0));
    REQUIRE_FALSE(mapUnlocked(s.meta, order, 1));
    REQUIRE_FALSE(mapUnlocked(s.meta, order, 2));

    // Reaching a high wave without clearing does NOT unlock the next map.
    s.meta.mapProgress["greenfields"] = {49, false};
    REQUIRE_FALSE(mapUnlocked(s.meta, order, 1));

    s.meta.mapProgress["greenfields"] = {50, true};
    REQUIRE(mapUnlocked(s.meta, order, 1));
    REQUIRE_FALSE(mapUnlocked(s.meta, order, 2));
}
