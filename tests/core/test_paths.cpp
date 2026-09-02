// Resource location for an installed copy. These paths were compiled in as
// absolute source-tree paths, so a packaged build looked for the build
// machine's home directory and refused to start anywhere else.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/Paths.h"

using namespace td;
namespace fs = std::filesystem;

namespace {

struct TempTree {
    fs::path root;
    explicit TempTree(const std::string& tag) {
        root = fs::temp_directory_path() / ("td_paths_" + tag);
        fs::remove_all(root);
    }
    ~TempTree() { fs::remove_all(root); }
    // Creates dir/content and returns dir.
    fs::path withContent(const fs::path& dir) {
        fs::create_directories(root / dir / "content");
        return root / dir;
    }
    fs::path at(const fs::path& dir) {
        fs::create_directories(root / dir);
        return root / dir;
    }
};

}  // namespace

TEST_CASE("resources are found beside the executable", "[paths]") {
    TempTree t("portable");
    const auto exe = t.withContent("bin");
    CHECK(core::resourceRootFrom(exe, "/nonexistent") == exe);
}

TEST_CASE("resources are found inside a macOS bundle", "[paths]") {
    // Wardstone.app/Contents/MacOS/Wardstone alongside Contents/Resources.
    TempTree t("bundle");
    const auto exe = t.at("Wardstone.app/Contents/MacOS");
    const auto res = t.withContent("Wardstone.app/Contents/Resources");
    CHECK(core::resourceRootFrom(exe, "/nonexistent") == res);
}

TEST_CASE("resources are found in a Unix install prefix", "[paths]") {
    TempTree t("prefix");
    const auto exe = t.at("bin");
    const auto share = t.withContent("share/wardstone");
    CHECK(core::resourceRootFrom(exe, "/nonexistent") == share);
}

TEST_CASE("resources are found one level up, as in a build tree", "[paths]") {
    TempTree t("buildtree");
    const auto exe = t.at("build");
    const auto root = t.withContent(".");
    CHECK(fs::equivalent(core::resourceRootFrom(exe, "/nonexistent"), root));
}

TEST_CASE("a packaged layout wins over the source tree it was built from",
          "[paths]") {
    // The failure this ordering exists to prevent: with the source tree present
    // on the same machine, a packaged copy that found its own resources must not
    // silently read the developer's instead.
    TempTree t("precedence");
    const auto exe = t.withContent("bin");
    const auto source = t.withContent("elsewhere/src");
    CHECK(core::resourceRootFrom(exe, source) == exe);
}

TEST_CASE("with nothing installed it falls back to the source tree", "[paths]") {
    TempTree t("fallback");
    const auto exe = t.at("bin");
    const auto source = t.withContent("elsewhere/src");
    CHECK(core::resourceRootFrom(exe, source) == source);
}

TEST_CASE("the running test binary can find the real content", "[paths]") {
    // End to end, against the actual repository rather than a fixture.
    REQUIRE(fs::is_directory(core::contentDir()));
    CHECK(fs::is_regular_file(core::contentDir() / "maps" / "greenfields.toml"));
}
