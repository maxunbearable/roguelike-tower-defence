#include "core/StatBlock.h"

namespace td::core {

void StatBlock::setBase(const std::string& path, float v) { base_[path] = v; }

void StatBlock::apply(const Modifier& m) {
    switch (m.op) {
        case ModOp::Add: adds_[m.target] += m.value; break;
        case ModOp::Mult:
            if (mults_.find(m.target) == mults_.end()) mults_[m.target] = 1.0f;
            mults_[m.target] *= m.value;
            break;
        case ModOp::Set: sets_[m.target] = m.value; break;
        case ModOp::Flag: flags_.insert(m.target); break;
    }
}

void StatBlock::apply(const std::vector<Modifier>& mods) {
    for (const auto& m : mods) apply(m);
}

bool StatBlock::has(const std::string& path) const {
    return base_.count(path) || adds_.count(path) || mults_.count(path) || sets_.count(path);
}

float StatBlock::get(const std::string& path, float fallback) const {
    const auto s = sets_.find(path);
    if (s != sets_.end()) return s->second;
    if (!has(path)) return fallback;

    const auto b = base_.find(path);
    const auto a = adds_.find(path);
    const auto m = mults_.find(path);
    const float baseV = b != base_.end() ? b->second : 0.0f;
    const float addV = a != adds_.end() ? a->second : 0.0f;
    const float mulV = m != mults_.end() ? m->second : 1.0f;
    return (baseV + addV) * mulV;
}

bool StatBlock::flag(const std::string& name) const { return flags_.count(name) > 0; }

}  // namespace td::core
