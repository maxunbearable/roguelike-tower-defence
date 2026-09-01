#include "content/Registry.h"

#include <functional>
#include <stdexcept>

#include "content/TomlLoader.h"
#include "content/TreeLoader.h"

namespace td::content {
namespace {

template <typename Map>
const typename Map::mapped_type& lookup(const Map& m, const std::string& id, const char* kind) {
    const auto it = m.find(id);
    if (it == m.end()) throw std::runtime_error(std::string("unknown ") + kind + " id: " + id);
    return it->second;
}

void loadDir(const std::filesystem::path& dir, const std::function<void(const std::filesystem::path&)>& fn) {
    if (!std::filesystem::exists(dir)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".toml") fn(entry.path());
    }
}

}  // namespace

void Registry::loadAll(const std::filesystem::path& contentDir) {
    if (!std::filesystem::exists(contentDir)) {
        throw std::runtime_error("content directory not found: " + contentDir.string());
    }
    loadDir(contentDir / "enemies", [&](const auto& f) {
        for (auto& e : loadEnemies(f)) enemies_[e.id] = std::move(e);
    });
    loadDir(contentDir / "towers", [&](const auto& f) {
        auto t = loadTower(f);
        towers_[t.id] = std::move(t);
    });
    loadDir(contentDir / "maps", [&](const auto& f) {
        auto m = loadMap(f);
        maps_[m.id] = std::move(m);
    });
    loadDir(contentDir / "elements", [&](const auto& f) {
        auto e = loadElement(f);
        elements_[e.id] = std::move(e);
    });
    loadDir(contentDir / "trees", [&](const auto& f) {
        auto t = loadTree(f);
        trees_[t.id] = std::move(t);
    });
}

const EnemyDef& Registry::enemy(const std::string& id) const { return lookup(enemies_, id, "enemy"); }
const TowerDef& Registry::tower(const std::string& id) const { return lookup(towers_, id, "tower"); }
const MapDef& Registry::map(const std::string& id) const { return lookup(maps_, id, "map"); }
const ElementDef& Registry::element(const std::string& id) const {
    return lookup(elements_, id, "element");
}
const core::SkillTree& Registry::tree(const std::string& id) const {
    return lookup(trees_, id, "tree");
}

}  // namespace td::content
