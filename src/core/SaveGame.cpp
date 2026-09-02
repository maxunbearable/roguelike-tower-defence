#include "core/SaveGame.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

namespace td::core {
namespace {
using json = nlohmann::json;
}

std::string toJson(const SaveSlot& slot) {
    json j;
    j["version"] = slot.version;
    j["used"] = slot.used;
    j["profileName"] = slot.profileName;

    j["meta"]["shards"] = slot.meta.shards;
    j["meta"]["seenHints"] = slot.meta.seenHints;
    j["meta"]["tutorialStep"] = slot.meta.tutorialStep;
    j["meta"]["difficulty"] = slot.meta.difficulty;
    j["meta"]["colorAlternatives"] = slot.meta.colorAlternatives;
    j["meta"]["shake"] = slot.meta.shake;
    j["meta"]["integerScaling"] = slot.meta.integerScaling;
    j["meta"]["runsPlayed"] = slot.meta.runsPlayed;
    j["meta"]["bestWave"] = slot.meta.bestWave;
    j["meta"]["ownedNodes"] = std::vector<std::string>(slot.meta.ownedNodes.begin(),
                                                       slot.meta.ownedNodes.end());
    j["meta"]["musicVolume"] = slot.meta.musicVolume;
    j["meta"]["sfxVolume"] = slot.meta.sfxVolume;
    for (const auto& [mapId, p] : slot.meta.mapProgress) {
        j["meta"]["mapProgress"][mapId] = {{"bestWave", p.bestWave}, {"cleared", p.cleared}};
    }

    if (slot.run) {
        const auto& r = *slot.run;
        json jr;
        jr["mapId"] = r.mapId;
        jr["seed"] = r.seed;
        jr["rngState"] = r.rngState;
        jr["waveIndex"] = r.waveIndex;
        jr["gold"] = r.gold;
        jr["lives"] = r.lives;
        jr["buildTimer"] = r.buildTimer;
        jr["enemiesKilled"] = r.enemiesKilled;
        jr["leaked"] = r.leaked;
        jr["goldEarned"] = r.goldEarned;
        jr["towersBuilt"] = r.towersBuilt;
        jr["abilityCooldowns"] = r.abilityCooldowns;
        for (const auto& t : r.towers) {
            jr["towers"].push_back({{"x", t.x},
                                    {"y", t.y},
                                    {"towerId", t.towerId},
                                    {"priority", t.priority},
                                    {"level", t.level},
                                    {"goldSpent", t.goldSpent},
                                    {"elementId", t.elementId},
                                    {"towerSpec", t.towerSpec},
                                    {"elementSpec", t.elementSpec}});
        }
        if (r.towers.empty()) jr["towers"] = json::array();
        j["run"] = jr;
    } else {
        j["run"] = nullptr;
    }
    return j.dump(2);
}

SaveSlot fromJson(const std::string& text) {
    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("save file is not valid JSON: ") + e.what());
    }

    SaveSlot s;
    s.version = j.value("version", 0);
    if (s.version != kSaveVersion) {
        // A future migration would live here. Refusing loudly beats silently
        // loading a save whose fields mean something different.
        throw std::runtime_error("save version " + std::to_string(s.version) +
                                 " is not supported (expected " +
                                 std::to_string(kSaveVersion) + ")");
    }
    s.used = j.value("used", false);
    s.profileName = j.value("profileName", std::string{});

    if (j.contains("meta")) {
        const auto& m = j["meta"];
        s.meta.shards = m.value("shards", 0);
        s.meta.seenHints = m.value("seenHints", std::set<std::string>{});
        s.meta.tutorialStep = m.value("tutorialStep", 0);
        s.meta.difficulty = m.value("difficulty", 1);
        s.meta.colorAlternatives = m.value("colorAlternatives", false);
        s.meta.shake = m.value("shake", 1.0f);
        s.meta.integerScaling = m.value("integerScaling", true);
        s.meta.runsPlayed = m.value("runsPlayed", 0);
        s.meta.bestWave = m.value("bestWave", 0);
        s.meta.musicVolume = m.value("musicVolume", 0.5f);
        s.meta.sfxVolume = m.value("sfxVolume", 0.85f);
        // Absent in a version-1 save, which must still load: a format change
        // must never cost a player their profile.
        if (m.contains("mapProgress")) {
            for (const auto& [mapId, p] : m["mapProgress"].items()) {
                s.meta.mapProgress[mapId] = {p.value("bestWave", 0), p.value("cleared", false)};
            }
        }
        for (const auto& n : m.value("ownedNodes", std::vector<std::string>{})) {
            s.meta.ownedNodes.insert(n);
        }
    }

    if (j.contains("run") && !j["run"].is_null()) {
        const auto& jr = j["run"];
        RunSave r;
        r.mapId = jr.value("mapId", std::string{});
        r.seed = jr.value("seed", uint64_t{0});
        r.rngState = jr.value("rngState", std::string{});
        r.waveIndex = jr.value("waveIndex", 0);
        r.gold = jr.value("gold", 0);
        r.lives = jr.value("lives", 0);
        r.buildTimer = jr.value("buildTimer", 0.0f);
        // Defaults keep saves written before these fields loadable.
        r.enemiesKilled = jr.value("enemiesKilled", 0);
        r.leaked = jr.value("leaked", 0);
        r.goldEarned = jr.value("goldEarned", 0);
        r.towersBuilt = jr.value("towersBuilt", 0);
        r.abilityCooldowns = jr.value("abilityCooldowns", std::vector<float>{});
        if (jr.contains("towers")) {
            for (const auto& jt : jr["towers"]) {
                TowerSave t;
                t.x = jt.value("x", 0);
                t.y = jt.value("y", 0);
                // Defaults keep pre-existing saves loadable.
                t.towerId = jt.value("towerId", std::string{"arrow"});
                t.priority = jt.value("priority", std::string{"first"});
                t.level = jt.value("level", 1);
                t.goldSpent = jt.value("goldSpent", 0);
                t.elementId = jt.value("elementId", std::string{});
                t.towerSpec = jt.value("towerSpec", std::string{});
                t.elementSpec = jt.value("elementSpec", std::string{});
                r.towers.push_back(std::move(t));
            }
        }
        s.run = std::move(r);
    }
    return s;
}

bool mapUnlocked(const MetaSave& meta, const std::vector<std::string>& order, int index) {
    if (index <= 0) return index == 0 && !order.empty();
    if (index >= static_cast<int>(order.size())) return false;
    const auto it = meta.mapProgress.find(order[static_cast<size_t>(index) - 1]);
    return it != meta.mapProgress.end() && it->second.cleared;
}

}  // namespace td::core
