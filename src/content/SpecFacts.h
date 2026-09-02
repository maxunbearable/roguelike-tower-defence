#pragma once

#include <string>
#include <vector>

#include "core/SkillTree.h"

namespace td::content {

// The exact figures a specialisation gives, derived from its node's modifiers.
// The 33 spec cores describe themselves in prose with no numbers in it, and
// exact figures are what turn a choice from memorisation into thinking.
std::vector<std::string> specNumbers(const core::SkillTree& tree, const std::string& nodeId);

// The same list flattened to one line, for places with a single row to spend.
std::string specNumbersLine(const core::SkillTree& tree, const std::string& nodeId);

}  // namespace td::content
