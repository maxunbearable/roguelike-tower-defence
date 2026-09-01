#pragma once

#include <string>
#include <vector>

namespace td::content {

class Registry;

// Returns one human-readable message per problem; empty means valid. Asserted by
// a unit test over the shipped content, so malformed data fails CI rather than a
// player.
std::vector<std::string> validate(const Registry& reg);

}  // namespace td::content
