#pragma once

#include <filesystem>

#include "core/SaveGame.h"

namespace td::core {

// Where saves live. Honours TD_SAVE_DIR, which is what lets tests run without
// touching the player's real profile directory.
std::filesystem::path saveDir();
std::filesystem::path slotPath(int slot);

// A slot that has never been written, or whose file is corrupt, comes back as
// an unused slot rather than throwing: a damaged save must never stop the game
// from starting.
SaveSlot loadSlot(int slot);
bool writeSlot(int slot, const SaveSlot& data);
bool deleteSlot(int slot);

}  // namespace td::core
