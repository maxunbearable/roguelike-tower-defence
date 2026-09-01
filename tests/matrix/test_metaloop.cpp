// How many losing runs before map 1 falls?
//
// The design target is a rule of thumb: roughly 8-10 losses. Everything else in
// the balance suite measures a SINGLE run against a fixed profile, which cannot
// answer that -- the answer depends on the whole loop: what a run pays out, what
// nodes that buys, and how much stronger the next run is.
//
// So this simulates the loop: play, bank the shards, buy, repeat. Reported
// rather than asserted at a fixed number, because the honest output is a curve,
// and pinning "exactly 9" would break on any tuning change.
//
// TWO purchase policies are reported, because the answer depends enormously on
// how the shards are spent and a single policy would misrepresent the game:
//
//   greedy   buys the cheapest prereq-met node anywhere. A floor, and a bad
//            model of a person: the six element trunks are the cheapest nodes in
//            the game, so it buys all six before anything else. Under a gold
//            deficit that makes it WORSE -- an unlocked element is an invitation
//            to spend scarce gold imbuing instead of building, so measured waves
//            drop from 14 to 10 over the first few runs.
//   planned  pushes one line and SAVES for it: global first, then the starting
//            tower, then a single element. Never buys a consolation node just
//            because it is affordable. This is what a person does, and it is the
//            number the 8-10 target should be read against.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "content/Registry.h"
#include "core/Difficulty.h"
#include "core/Progression.h"
#include "sim/AutoPlayer.h"
#include "sim/World.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// The cheapest node whose prerequisites are met, anywhere. Deterministic.
const core::SkillNode* greedyPurchase(const content::Registry& reg, const core::MetaSave& meta) {
    const core::SkillNode* best = nullptr;
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& n : tree.nodes) {
            if (meta.ownedNodes.count(n.id)) continue;
            if (!core::prereqsMet(tree, meta, n.id)) continue;
            if (n.cost > meta.shards) continue;
            if (!best || n.cost < best->cost || (n.cost == best->cost && n.id < best->id)) {
                best = &n;
            }
        }
    }
    return best;
}

// A player with a plan. Walks a priority order of trees and, within a tree,
// authored order -- trees are authored trunk-first, so that approximates a line.
// Returns nullptr when the next node on the line is unaffordable, which is the
// whole point: it BANKS instead of buying a consolation prize elsewhere.
const core::SkillNode* plannedPurchase(const content::Registry& reg, const core::MetaSave& meta) {
    // A second TOWER, not just a second element. The line used to be global ->
    // arrow -> earth, so the modelled player never bought a tower charter and
    // finished a 24-run campaign having only ever fielded arrow towers -- on a
    // game that sells five and gates them behind cheap charters. That made the
    // loss counts a reading of a player nobody is.
    static const std::vector<std::string> priority = {"global", sim::kStartingTower, "cannon",
                                                      "earth"};
    for (const auto& treeId : priority) {
        if (!reg.trees().count(treeId)) continue;
        const auto& tree = reg.trees().at(treeId);
        for (const auto& n : tree.nodes) {
            if (meta.ownedNodes.count(n.id)) continue;
            if (!core::prereqsMet(tree, meta, n.id)) continue;
            return n.cost <= meta.shards ? &n : nullptr;  // affordable, or save for it
        }
    }
    return greedyPurchase(reg, meta);  // the line is exhausted; stop stalling
}

using Policy = const core::SkillNode* (*)(const content::Registry&, const core::MetaSave&);

// Plays runs until map 1 falls or the budget runs out. Returns the run it fell
// on, or -1, and appends a per-run table to `os`.
int runLoop(const content::Registry& reg, const content::MapDef& map, Policy policy,
            const char* label, std::ostringstream& os,
            core::Difficulty difficulty = core::Difficulty::Standard) {
    core::MetaSave meta;
    os << "\n " << label << "\n run  waves  earned  banked  nodes  cleared\n";
    for (int run = 1; run <= 24; ++run) {
        core::Loadout lo;
        lo.ownAll = false;
        lo.ownedNodes = meta.ownedNodes;

        const auto r = sim::autoPlay(reg, map, lo, static_cast<uint64_t>(run), 60,
                                     difficulty);
        meta.shards += r.shards;

        while (const auto* n = policy(reg, meta)) {
            meta.shards -= n->cost;
            meta.ownedNodes.insert(n->id);
        }

        os << "  " << run << "\t" << r.wavesSurvived << "\t" << r.shards << "\t"
           << meta.shards << "\t" << meta.ownedNodes.size() << "\t"
           << (r.cleared ? "YES" : "-") << "\n";

        if (r.cleared) return run;
    }
    return -1;
}

}  // namespace

TEST_CASE("report how many runs it takes to clear map 1", "[balance][.report]") {
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");

    std::ostringstream os;
    const int greedy = runLoop(reg, map, &greedyPurchase, "greedy (floor)", os);
    const int planned = runLoop(reg, map, &plannedPurchase, "planned (realistic)", os);
    // The whole reason difficulty exists: "hardcore" and "clearable in 8-10
    // losses" are the same dial pulled opposite ways, so they became a choice.
    // This is the measurement that says whether the choice actually delivers.
    const int relaxed = runLoop(reg, map, &plannedPurchase, "planned on RELAXED", os,
                                core::Difficulty::Relaxed);
    const int brutal = runLoop(reg, map, &plannedPurchase, "planned on BRUTAL", os,
                               core::Difficulty::Brutal);

    const auto say = [](int n) {
        return n > 0 ? std::to_string(n) : std::string("not within 24");
    };
    os << "\n first cleared -- greedy: " << say(greedy) << "   planned: " << say(planned)
       << "\n   by difficulty -- relaxed: " << say(relaxed) << "   standard: " << say(planned)
       << "   brutal: " << say(brutal)
       << "\n   (the 8-10 loss target belongs to RELAXED; brutal is meant to hurt)\n";
    WARN(os.str());
}
