#pragma once

#include <string>
#include <vector>

#include "core/SkillTree.h"

namespace td::content {

// The exact numbers a specialisation actually gives you.
//
// All 33 specialisation cores -- the authored pieces the whole "270 builds"
// claim rests on -- described themselves in prose alone: "Far fewer, far heavier
// shots", "Hits inject stacking venom". Not one contained a number, so the
// choice that defines a build was made on flavour text.
//
// The tower defence design writing is blunt about this: state "5 damage per
// second for 5 seconds", never "50% more damage", because exact figures are what
// turn a game from memorisation into thinking.
//
// Derived from the node's own modifiers rather than written by hand, so the text
// cannot drift away from what the game does when a value is retuned -- which is
// the failure mode hand-written numbers always eventually hit.
std::vector<std::string> specNumbers(const core::SkillTree& tree, const std::string& nodeId);

// The same list flattened to one line, for places with a single row to spend.
std::string specNumbersLine(const core::SkillTree& tree, const std::string& nodeId);

}  // namespace td::content
