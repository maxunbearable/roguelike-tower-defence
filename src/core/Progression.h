#pragma once

#include <string>
#include <vector>

#include "core/SaveGame.h"
#include "core/SkillTree.h"

namespace td::core {

enum class BuyResult { Ok, AlreadyOwned, MissingPrereq, TooPoor, UnknownNode };

// A node is buyable when it is not already owned, every prerequisite is owned,
// and the player can afford it.
BuyResult canBuy(const SkillTree& tree, const MetaSave& meta, const std::string& nodeId);

// Applies the purchase. Returns the same reason on failure and leaves `meta`
// untouched, so a rejected purchase can never half-apply.
BuyResult buyNode(const SkillTree& tree, MetaSave& meta, const std::string& nodeId);

// Whether every prerequisite is owned, ignoring cost. Drives how the tree is
// drawn: reachable nodes look different from ones still gated behind others.
bool prereqsMet(const SkillTree& tree, const MetaSave& meta, const std::string& nodeId);

const char* describe(BuyResult r);

}  // namespace td::core
