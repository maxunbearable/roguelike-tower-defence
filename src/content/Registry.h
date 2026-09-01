#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "content/Defs.h"
#include "core/SkillTree.h"

namespace td::content {

// Owns every gameplay definition loaded from the content directory. Lookups
// throw rather than returning a default, so a typo'd id fails loudly at the
// call site instead of silently producing a zero-stat entity.
class Registry {
public:
    void loadAll(const std::filesystem::path& contentDir);

    const EnemyDef& enemy(const std::string& id) const;
    const TowerDef& tower(const std::string& id) const;
    const MapDef& map(const std::string& id) const;
    const ElementDef& element(const std::string& id) const;
    const core::SkillTree& tree(const std::string& id) const;

    bool hasEnemy(const std::string& id) const { return enemies_.count(id) > 0; }
    bool hasTower(const std::string& id) const { return towers_.count(id) > 0; }
    bool hasMap(const std::string& id) const { return maps_.count(id) > 0; }
    bool hasElement(const std::string& id) const { return elements_.count(id) > 0; }
    bool hasTree(const std::string& id) const { return trees_.count(id) > 0; }

    const std::map<std::string, EnemyDef>& enemies() const { return enemies_; }
    const std::map<std::string, TowerDef>& towers() const { return towers_; }
    const std::map<std::string, MapDef>& maps() const { return maps_; }
    const std::map<std::string, ElementDef>& elements() const { return elements_; }
    const std::map<std::string, core::SkillTree>& trees() const { return trees_; }

private:
    std::map<std::string, EnemyDef> enemies_;
    std::map<std::string, TowerDef> towers_;
    std::map<std::string, MapDef> maps_;
    std::map<std::string, ElementDef> elements_;
    std::map<std::string, core::SkillTree> trees_;
};

}  // namespace td::content
