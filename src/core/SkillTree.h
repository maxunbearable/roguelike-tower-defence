#pragma once

#include <string>
#include <vector>

#include "core/Modifier.h"

namespace td::core {

struct SkillNode {
    std::string id;
    std::string name;
    std::string desc;
    std::string branch;  // "trunk" or one of the tree's declared specs
    int cost = 1;
    std::vector<std::string> prereqs;  // 'requires' is a C++20 keyword
    int x = 0;
    int y = 0;
    std::vector<Modifier> modifiers;
};

// Global, tower and element trees all share this one shape, which is what lets a
// new element ship as a TOML file rather than as new C++.
struct SkillTree {
    enum class Kind { Global, Tower, Element };

    std::string id;
    Kind kind = Kind::Global;
    std::vector<std::string> specs;  // branch names, excluding "trunk"
    std::vector<SkillNode> nodes;

    const SkillNode* find(const std::string& nodeId) const {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
        }
        return nullptr;
    }
};

}  // namespace td::core
