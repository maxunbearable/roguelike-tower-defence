#pragma once

#include <set>
#include <string>

#include "core/Loadout.h"
#include "core/StatBlock.h"

namespace td::content {
class Registry;
}

namespace td::core {

// Folds every source of power into one block, in the spec's fixed order:
//   base tower + element stats
//     -> global tree
//     -> tower trunk -> equipped tower spec branch
//     -> element trunk -> equipped element spec branch
//
// Owned nodes in a branch that is not equipped are skipped. That single rule is
// what makes "you own every branch, but only one plays" work with no
// per-spec special-casing anywhere else in the codebase.
StatBlock resolveStats(const content::Registry& reg, const Loadout& lo);

// Every stat path the current content declares. Used by content validation to
// reject a skill node that modifies a stat nothing reads -- the most common
// authoring mistake once trees get large.
std::set<std::string> knownStatPaths(const content::Registry& reg);

}  // namespace td::core
