#pragma once

#include <filesystem>

#include "content/Defs.h"
#include "core/SkillTree.h"

namespace td::content {

core::SkillTree loadTree(const std::filesystem::path& file);
ElementDef loadElement(const std::filesystem::path& file);

}  // namespace td::content
