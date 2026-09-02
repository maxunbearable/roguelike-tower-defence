// Value per gold, per scenario.
//
// Plan 18 found that arrow -- the FREE starting tower -- is the best value per
// gold in the game, and that every tower a player spends shards to unlock is
// worth 75-80% of the one they already had. That is a progression defect: the
// reward for buying a charter is a worse tower. Before changing it, plan 18
// named the check that has to come first: does arrow's lead survive per
// SCENARIO, or is the 100% an artefact of averaging a specialist's good matchup
// with its bad one?
//
// This file answers that. It is a report plus one guardrail.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#include "content/Registry.h"
#include "sim/Scenario.h"
#include "sim/World.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

constexpr float kSeconds = 20.0f;
constexpr uint64_t kSeed = 999;

struct Spec {
    std::string ownerId, specId;
};

bool isControlSpec(const std::string& s) {
    return s == "chill" || s == "shatter" || s == "freeze" || s == "gust";
}
bool isSupportTowerSpec(const std::string& s) { return s == "forge"; }

std::vector<Spec> specsOfKind(const content::Registry& r, core::SkillTree::Kind kind) {
    std::vector<Spec> out;
    for (const auto& [id, tree] : r.trees()) {
        if (tree.kind != kind) continue;
        for (const auto& sp : tree.specs) out.push_back({id, sp});
    }
    return out;
}

// The top-five mean, as plan 18 settled on: a single best cell is noise, and a
// flat mean punishes a tower for the elements it is not meant to carry. A
// player picks a good element for the tower they bought, not a random one.
float bestOf(const content::Registry& r, const std::string& towerId, sim::ScenarioKind kind) {
    std::vector<float> cells;
    for (const auto& t : specsOfKind(r, core::SkillTree::Kind::Tower)) {
        if (t.ownerId != towerId || isSupportTowerSpec(t.specId)) continue;
        for (const auto& e : specsOfKind(r, core::SkillTree::Kind::Element)) {
            if (isControlSpec(e.specId)) continue;
            cells.push_back(sim::simulateCombo(r, t.ownerId, t.specId, e.ownerId, e.specId, kind,
                                               kSeconds, kSeed)
                                .clearedFraction());
        }
    }
    std::sort(cells.rbegin(), cells.rend());
    const size_t n = std::min<size_t>(5, cells.size());
    if (n == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) sum += cells[i];
    return sum / static_cast<float>(n);
}

// What a maxed tower actually costs: build price PLUS every upgrade to reach the
// level the matrix measures. Dividing by buildCost alone -- which is what the
// first pass of this report did -- prices a tower as though its upgrades were
// free, and the matrix only ever measures MAXED towers. A tower with a cheap
// charter and expensive levels reads far better than it plays.
int maxedCost(const content::Registry& r, const std::string& towerId) {
    core::Loadout lo;
    lo.ownAll = true;
    sim::World w(r, r.map("greenfields"), 1, lo, /*goldOverride=*/10000000);
    int px = -1, py = -1;
    for (int y = 0; y < w.map().gridH && px < 0; ++y)
        for (int x = 0; x < w.map().gridW && px < 0; ++x)
            if (w.map().buildableAt(x, y)) { px = x; py = y; }
    if (px < 0) return r.tower(towerId).buildCost;
    const int before = w.gold();
    if (w.placeTower(px, py, towerId) != sim::World::PlaceResult::Ok)
        return r.tower(towerId).buildCost;
    while (w.upgradeCost(px, py) > 0 && w.upgradeTower(px, py)) {
    }
    return before - w.gold();
}

std::vector<std::string> towerIds(const content::Registry& r) {
    std::vector<std::string> ids;
    for (const auto& [id, tree] : r.trees())
        if (tree.kind == core::SkillTree::Kind::Tower) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

}  // namespace

TEST_CASE("report value per gold for every tower, per scenario", "[matrix][.report]") {
    const auto r = loadReg();
    const sim::ScenarioKind kinds[] = {sim::ScenarioKind::LoneTank, sim::ScenarioKind::Swarm,
                                       sim::ScenarioKind::Mixed};

    std::ostringstream out;
    out << "\n" << std::left << std::setw(11) << "tower" << std::right << std::setw(7) << "gold";
    for (auto k : kinds) out << std::setw(12) << sim::name(k);
    out << std::setw(12) << "mean\n";

    // Per scenario, normalised so the best tower in that column reads 100%.
    std::map<std::string, std::vector<float>> perGold;
    for (const auto& id : towerIds(r)) {
        const float gold = static_cast<float>(maxedCost(r, id));
        for (auto k : kinds) perGold[id].push_back(bestOf(r, id, k) / gold);
    }
    std::vector<float> colMax(3, 0.0f);
    for (const auto& [id, v] : perGold)
        for (size_t i = 0; i < 3; ++i) colMax[i] = std::max(colMax[i], v[i]);

    for (const auto& id : towerIds(r)) {
        out << std::left << std::setw(11) << id << std::right << std::setw(7)
            << maxedCost(r, id) << std::fixed << std::setprecision(0);
        float mean = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            const float rel = colMax[i] > 0.0f ? perGold[id][i] / colMax[i] * 100.0f : 0.0f;
            out << std::setw(11) << rel << "%";
            mean += rel;
        }
        out << std::setw(11) << mean / 3.0f << "%\n";
    }
    // Per SITE, not per gold: a map has a finite number of build plots, so if
    // plots are the binding constraint rather than gold then absolute output is
    // the number that decides which tower a player wants.
    out << "\nabsolute output per tower (per build site), same normalisation\n";
    out << std::left << std::setw(11) << "tower" << std::right << std::setw(7) << "maxed";
    for (auto k : kinds) out << std::setw(12) << sim::name(k);
    out << std::setw(12) << "mean\n";
    std::map<std::string, std::vector<float>> raw;
    for (const auto& id : towerIds(r))
        for (auto k : kinds) raw[id].push_back(bestOf(r, id, k));
    std::vector<float> rawMax(3, 0.0f);
    for (const auto& [id, v] : raw)
        for (size_t i = 0; i < 3; ++i) rawMax[i] = std::max(rawMax[i], v[i]);
    for (const auto& id : towerIds(r)) {
        out << std::left << std::setw(11) << id << std::right << std::setw(7)
            << r.tower(id).buildCost << std::fixed << std::setprecision(0);
        float mean = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            const float rel = rawMax[i] > 0.0f ? raw[id][i] / rawMax[i] * 100.0f : 0.0f;
            out << std::setw(11) << rel << "%";
            mean += rel;
        }
        out << std::setw(11) << mean / 3.0f << "%\n";
    }
    UNSCOPED_INFO(out.str());
    CHECK(true);
}
