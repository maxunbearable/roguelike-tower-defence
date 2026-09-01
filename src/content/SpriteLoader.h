#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "content/SpriteDef.h"

namespace td::content {

// Decodes the indexed-pixel art file into RGBA sprites. Throws with the sprite
// id and row number on any malformed row, so bad art fails at load rather than
// rendering as garbage.
std::map<std::string, SpriteDef> loadSprites(const std::filesystem::path& file);

}  // namespace td::content
