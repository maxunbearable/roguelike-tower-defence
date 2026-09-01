#pragma once

#include <filesystem>
#include <vector>

#include "content/Defs.h"

namespace td::content {

// Each loader throws std::runtime_error naming the file and the reason on any
// missing key, wrong type, or structural problem. A half-populated definition is
// never returned.
std::vector<EnemyDef> loadEnemies(const std::filesystem::path& file);
TowerDef loadTower(const std::filesystem::path& file);
MapDef loadMap(const std::filesystem::path& file);

}  // namespace td::content
