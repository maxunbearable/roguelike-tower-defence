#pragma once

#include <string>
#include <vector>

namespace td::core {

enum class ModOp { Add, Mult, Set, Flag };

// A single stat change from a skill node. `target` is a dotted stat path such as
// "arrow.fireRate" or "earth.poison.dpsPerStack". For Flag, the target names the
// flag and value is ignored.
struct Modifier {
    std::string target;
    ModOp op = ModOp::Add;
    float value = 0.0f;
};

}  // namespace td::core
