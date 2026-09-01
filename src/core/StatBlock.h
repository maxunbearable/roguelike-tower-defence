#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/Modifier.h"

namespace td::core {

// Accumulates modifiers and resolves them on demand as
//     (base + sum(adds)) * product(mults)
// with `set` overriding outright and flags forming a union.
//
// Adds and mults are kept in separate buckets rather than applied as they
// arrive, which makes the result independent of the order modifiers were
// applied in. That property is what lets skill trees be folded in any order and
// still produce identical stats -- and it is the first thing that breaks if this
// is "simplified" into a single running value.
class StatBlock {
public:
    void setBase(const std::string& path, float v);
    void apply(const Modifier& m);
    void apply(const std::vector<Modifier>& mods);

    float get(const std::string& path, float fallback = 0.0f) const;
    bool flag(const std::string& name) const;
    bool has(const std::string& path) const;

    const std::set<std::string>& flags() const { return flags_; }

private:
    std::map<std::string, float> base_;
    std::map<std::string, float> adds_;
    std::map<std::string, float> mults_;
    std::map<std::string, float> sets_;
    std::set<std::string> flags_;
};

}  // namespace td::core
