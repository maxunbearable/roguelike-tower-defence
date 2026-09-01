#include "core/Progression.h"

namespace td::core {

bool prereqsMet(const SkillTree& tree, const MetaSave& meta, const std::string& nodeId) {
    const auto* n = tree.find(nodeId);
    if (!n) return false;
    for (const auto& req : n->prereqs) {
        if (meta.ownedNodes.count(req) == 0) return false;
    }
    return true;
}

BuyResult canBuy(const SkillTree& tree, const MetaSave& meta, const std::string& nodeId) {
    const auto* n = tree.find(nodeId);
    if (!n) return BuyResult::UnknownNode;
    if (meta.ownedNodes.count(nodeId) > 0) return BuyResult::AlreadyOwned;
    if (!prereqsMet(tree, meta, nodeId)) return BuyResult::MissingPrereq;
    if (meta.shards < n->cost) return BuyResult::TooPoor;
    return BuyResult::Ok;
}

BuyResult buyNode(const SkillTree& tree, MetaSave& meta, const std::string& nodeId) {
    const auto r = canBuy(tree, meta, nodeId);
    if (r != BuyResult::Ok) return r;
    const auto* n = tree.find(nodeId);
    meta.shards -= n->cost;
    meta.ownedNodes.insert(nodeId);
    return BuyResult::Ok;
}

const char* describe(BuyResult r) {
    switch (r) {
        case BuyResult::Ok: return "";
        case BuyResult::AlreadyOwned: return "already learned";
        case BuyResult::MissingPrereq: return "requires an earlier skill";
        case BuyResult::TooPoor: return "not enough shards";
        case BuyResult::UnknownNode: return "unknown skill";
    }
    return "";
}

}  // namespace td::core
