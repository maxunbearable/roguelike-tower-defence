#include "core/Paths.h"

#include <cstdlib>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace td::core {
namespace {

bool hasContent(const std::filesystem::path& p) {
    std::error_code ec;
    return !p.empty() && std::filesystem::is_directory(p / "content", ec);
}

}  // namespace

std::filesystem::path executableDir() {
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return {};
    return std::filesystem::path(buf.data()).parent_path();
#elif defined(_WIN32)
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0) return {};
    return std::filesystem::path(std::wstring(buf.data(), n)).parent_path();
#else
    std::error_code ec;
    const auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path{} : p.parent_path();
#endif
}

std::filesystem::path resourceRootFrom(const std::filesystem::path& exeDir,
                                       const std::filesystem::path& sourceFallback) {
    if (!exeDir.empty()) {
        for (const auto& c : {exeDir, exeDir.parent_path() / "Resources",
                              exeDir.parent_path() / "share" / "wardstone",
                              exeDir.parent_path()}) {
            if (hasContent(c)) return c;
        }
    }
    // Last, so a packaged build never reaches into a source tree that happens to
    // exist on the same machine.
    return sourceFallback;
}

std::filesystem::path resourceRoot() {
    if (const char* env = std::getenv("TD_RESOURCE_DIR")) return env;
    return resourceRootFrom(executableDir(), TD_SOURCE_ROOT);
}

std::filesystem::path contentDir() { return resourceRoot() / "content"; }
std::filesystem::path assetDir() { return resourceRoot() / "assets"; }

}  // namespace td::core
