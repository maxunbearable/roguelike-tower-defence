#pragma once

#include <filesystem>

namespace td::core {

// Where content/ and assets/ live at runtime. These were compile-time paths into
// the source tree, so an installed copy looked for the build machine's home
// directory.
std::filesystem::path resourceRoot();
std::filesystem::path contentDir();
std::filesystem::path assetDir();
std::filesystem::path executableDir();

// The search `resourceRoot` performs, with its inputs passed in so it can be
// tested against a fabricated layout. First directory containing content/ wins:
// exeDir, exeDir/../Resources (macOS bundle), exeDir/../share/wardstone,
// exeDir/.. (build trees), then sourceFallback.
std::filesystem::path resourceRootFrom(const std::filesystem::path& exeDir,
                                       const std::filesystem::path& sourceFallback);

}  // namespace td::core
