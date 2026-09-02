#include "core/SaveIO.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace td::core {

namespace {

// Where the platform wants an application's data, for a given product name.
std::filesystem::path productDir(const char* name) {
    const char* home = std::getenv("HOME");
    const std::filesystem::path base =
        home ? std::filesystem::path(home) : std::filesystem::path(".");
#if defined(__APPLE__)
    return base / "Library" / "Application Support" / name;
#elif defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / name;
    }
    return base / name;
#else
    return base / ".local" / "share" / name;
#endif
}

}  // namespace

std::filesystem::path adoptLegacySaveDir(const std::filesystem::path& current,
                                         const std::filesystem::path& legacy) {
    std::error_code ec;
    if (!std::filesystem::exists(current, ec) && std::filesystem::is_directory(legacy, ec)) {
        std::filesystem::create_directories(current.parent_path(), ec);
        std::filesystem::rename(legacy, current, ec);
    }
    return current;
}

std::filesystem::path saveDir() {
    if (const char* override = std::getenv("TD_SAVE_DIR")) return std::filesystem::path(override);
    static const std::filesystem::path dir =
        adoptLegacySaveDir(productDir("Wardstone"), productDir("PixelTD"));
    return dir;
}

std::filesystem::path slotPath(int slot) {
    return saveDir() / ("slot" + std::to_string(slot) + ".json");
}

SaveSlot loadSlot(int slot) {
    const auto path = slotPath(slot);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return SaveSlot{};

    std::ifstream in(path);
    if (!in) return SaveSlot{};
    std::ostringstream ss;
    ss << in.rdbuf();

    try {
        return fromJson(ss.str());
    } catch (const std::exception&) {
        // A corrupt or future-version save reads as empty. Losing a save is bad;
        // refusing to launch the game because of one is worse.
        return SaveSlot{};
    }
}

bool writeSlot(int slot, const SaveSlot& data) {
    std::error_code ec;
    std::filesystem::create_directories(saveDir(), ec);

    const auto path = slotPath(slot);
    const auto tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << toJson(data);
        if (!out) return false;
    }
    // Write to a temp file and rename, so a crash mid-write cannot leave a
    // half-written save where a good one used to be.
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

bool deleteSlot(int slot) {
    std::error_code ec;
    return std::filesystem::remove(slotPath(slot), ec);
}

}  // namespace td::core
