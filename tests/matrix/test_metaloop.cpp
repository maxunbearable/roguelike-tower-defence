// How many losing runs before map 1 falls?
//
// The design target is a rule of thumb: roughly 8-10 losses. Everything else in
// the balance suite measures a SINGLE run against a fixed profile, which cannot
// answer that -- the answer depends on the whole loop: what a run pays out, what
// nodes that buys, and how much stronger the next run is.
//
// So this simulates the loop: play, bank the shards, buy what is affordable,
// repeat. Reported rather than asserted at a fixed number, because the honest
// output is a curve, and pinning "exactly 9" would break on any tuning change.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#include <vector>

#include "content/Registry.h"
#include "core/Progression.h"
#include "sim/AutoPlayer.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// What a player would plausibly buy next: the cheapest node whose prerequisites
// are already met. Greedy-cheapest is not optimal play, but it is a reasonable
// floor and it is deterministic.
const core::SkillNode* nextPurchase(const content::Registry& reg, const core::MetaSave& meta) {
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

}  // namespace

TEST_CASE("report how many runs it takes to clear map 1", "[balance][.report]") {
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");

    core::MetaSave meta;
    std::ostringstream os;
    os << "\n run  waves  shards earned  banked  nodes owned  cleared\n";

    int clearedOn = -1;
    for (int run = 1; run <= 24; ++run) {
        core::Loadout lo;
        lo.ownAll = false;
        lo.ownedNodes = meta.ownedNodes;

        const auto r = sim::autoPlay(reg, map, lo, static_cast<uint64_t>(run));
        meta.shards += r.shards;

        // Spend down to what is no longer affordable.
        while (const auto* n = nextPurchase(reg, meta)) {
            meta.shards -= n->cost;
            meta.ownedNodes.insert(n->id);
        }

        os << "  " << run << "\t" << r.wavesSurvived << "\t" << r.shards << "\t\t"
           << meta.shards << "\t" << meta.ownedNodes.size() << "\t\t"
           << (r.cleared ? "YES" : "-") << "\n";

        if (r.cleared && clearedOn < 0) {
            clearedOn = run;
            break;
        }
    }
    os << "\n first cleared on run: " << (clearedOn > 0 ? std::to_string(clearedOn)
                                                        : std::string("not within 24"))
       << "   (design target: 8-10)\n";
    WARN(os.str());
}
