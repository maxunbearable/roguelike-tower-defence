#include "content/TreeLoader.h"

#include <stdexcept>
#include <string>

#include <toml++/toml.hpp>

namespace td::content {
namespace {

[[noreturn]] void fail(const std::filesystem::path& file, const std::string& what) {
    throw std::runtime_error(file.string() + ": " + what);
}

core::ModOp parseOp(const std::string& s, const std::filesystem::path& file) {
    if (s == "add") return core::ModOp::Add;
    if (s == "mult") return core::ModOp::Mult;
    if (s == "set") return core::ModOp::Set;
    if (s == "flag") return core::ModOp::Flag;
    fail(file, "unknown modifier op '" + s + "'");
}

core::SkillTree::Kind parseKind(const std::string& s, const std::filesystem::path& file) {
    if (s == "global") return core::SkillTree::Kind::Global;
    if (s == "tower") return core::SkillTree::Kind::Tower;
    if (s == "element") return core::SkillTree::Kind::Element;
    fail(file, "unknown tree kind '" + s + "'");
}

}  // namespace

core::SkillTree loadTree(const std::filesystem::path& file) {
    const toml::table tbl = toml::parse_file(file.string());
    const auto* t = tbl["tree"].as_table();
    if (!t) fail(file, "expected a [tree] table");

    core::SkillTree tree;
    const auto id = (*t)["id"].value<std::string>();
    const auto kind = (*t)["kind"].value<std::string>();
    if (!id || !kind) fail(file, "[tree] needs both id and kind");
    tree.id = *id;
    tree.kind = parseKind(*kind, file);

    if (const auto* specs = (*t)["specs"].as_array()) {
        for (const auto& sp : *specs) {
            if (auto v = sp.value<std::string>()) tree.specs.push_back(*v);
        }
    }

    const auto* nodes = tbl["node"].as_array();
    if (!nodes) fail(file, "tree has no [[node]] entries");
    for (const auto& elem : *nodes) {
        const auto* nt = elem.as_table();
        if (!nt) fail(file, "[[node]] entry is not a table");

        core::SkillNode n;
        const auto nid = (*nt)["id"].value<std::string>();
        if (!nid) fail(file, "node is missing an id");
        n.id = *nid;
        n.name = (*nt)["name"].value<std::string>().value_or(n.id);
        n.desc = (*nt)["desc"].value_or<std::string>("");
        n.branch = (*nt)["branch"].value_or<std::string>("trunk");
        n.cost = static_cast<int>((*nt)["cost"].value_or<int64_t>(1));

        if (const auto* pos = (*nt)["pos"].as_array()) {
            if (pos->size() == 2) {
                n.x = static_cast<int>((*pos)[0].value_or<int64_t>(0));
                n.y = static_cast<int>((*pos)[1].value_or<int64_t>(0));
            }
        }
        if (const auto* req = (*nt)["requires"].as_array()) {
            for (const auto& r : *req) {
                if (auto v = r.value<std::string>()) n.prereqs.push_back(*v);
            }
        }
        if (const auto* mods = (*nt)["modifiers"].as_array()) {
            for (const auto& m : *mods) {
                const auto* mt = m.as_table();
                if (!mt) fail(file, "modifier is not a table in node '" + n.id + "'");
                const auto target = (*mt)["target"].value<std::string>();
                const auto op = (*mt)["op"].value<std::string>();
                if (!target || !op) {
                    fail(file, "modifier in node '" + n.id + "' needs target and op");
                }
                core::Modifier mod;
                mod.target = *target;
                mod.op = parseOp(*op, file);
                // Flag modifiers carry no value, so it is legitimately absent.
                if (auto d = (*mt)["value"].value<double>()) mod.value = static_cast<float>(*d);
                else if (auto i = (*mt)["value"].value<int64_t>())
                    mod.value = static_cast<float>(*i);
                else if (mod.op != core::ModOp::Flag)
                    fail(file, "modifier '" + mod.target + "' in node '" + n.id +
                                   "' needs a value");
                n.modifiers.push_back(std::move(mod));
            }
        }
        tree.nodes.push_back(std::move(n));
    }
    return tree;
}

ElementDef loadElement(const std::filesystem::path& file) {
    const toml::table tbl = toml::parse_file(file.string());
    const auto* e = tbl["element"].as_table();
    if (!e) fail(file, "expected an [element] table");

    ElementDef d;
    const auto id = (*e)["id"].value<std::string>();
    if (!id) fail(file, "[element] needs an id");
    d.id = *id;
    d.name = (*e)["name"].value<std::string>().value_or(d.id);
    d.damageType = (*e)["damageType"].value_or<std::string>("physical");
    d.attachCost = static_cast<int>((*e)["attachCost"].value_or<int64_t>(0));
    d.specCost = static_cast<int>((*e)["specCost"].value_or<int64_t>(0));

    if (const auto* base = (*e)["base"].as_table()) {
        for (const auto& [k, v] : *base) {
            if (auto f = v.value<double>()) d.base[std::string(k.str())] = static_cast<float>(*f);
            else if (auto i = v.value<int64_t>())
                d.base[std::string(k.str())] = static_cast<float>(*i);
        }
    }
    if (const auto* specs = tbl["spec"].as_table()) {
        for (const auto& [specName, specNode] : *specs) {
            const auto* st = specNode.as_table();
            if (!st) fail(file, "spec '" + std::string(specName.str()) + "' is not a table");
            std::map<std::string, float> params;
            for (const auto& [k, v] : *st) {
                if (auto f = v.value<double>()) params[std::string(k.str())] = static_cast<float>(*f);
                else if (auto i = v.value<int64_t>())
                    params[std::string(k.str())] = static_cast<float>(*i);
            }
            d.specs[std::string(specName.str())] = std::move(params);
        }
    }
    if (d.specs.empty()) fail(file, "element declares no [spec.*] tables");
    return d;
}

}  // namespace td::content
