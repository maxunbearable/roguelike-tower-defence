// Starting up with content that is not perfect.
//
// The loader always produced a precise message -- it names the file and the key
// -- and main() discarded it: loadAll was called with no try/catch, so a
// malformed file ended the process with
//
//     libc++abi: terminating due to uncaught exception of type std::runtime_error
//
// A player saw a crash. The diagnostic existed and nobody ever read it.
//
// Validation was worse than that. content::validate() returned its errors,
// main() logged them and started the game anyway, so content the validator had
// already declared broken ran in an undefined state.
//
// These tests corrupt a real copy of the game's content and check what comes
// back, rather than asserting on a hand-built fixture that might not resemble it.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "content/Registry.h"
#include "content/Startup.h"

using namespace td;

namespace {

// A throwaway copy of the real content tree, so a test can break one file
// without touching the game's own.
struct ContentCopy {
    std::filesystem::path dir;
    explicit ContentCopy(const std::string& tag) {
        dir = std::filesystem::temp_directory_path() / ("td_content_" + tag);
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::copy(std::filesystem::path(TD_CONTENT_DIR), dir,
                              std::filesystem::copy_options::recursive, ec);
    }
    ~ContentCopy() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    // Rewrites one file, replacing the first occurrence of `from` with `to`.
    void corrupt(const std::string& relative, const std::string& from, const std::string& to) {
        const auto path = dir / relative;
        std::string text;
        {
            std::ifstream in(path);
            text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        const auto at = text.find(from);
        REQUIRE(at != std::string::npos);
        text.replace(at, from.size(), to);
        std::ofstream(path, std::ios::trunc) << text;
    }
};

bool mentions(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("the game's own content loads and validates", "[startup]") {
    content::Registry reg;
    const auto out = content::loadAndValidate(reg, TD_CONTENT_DIR);
    INFO(out.problem);
    CHECK(out.ok);
    CHECK(out.problem.empty());
    CHECK(reg.towers().size() >= 5);  // it really did load something
}

TEST_CASE("a malformed file is reported, not thrown", "[startup]") {
    ContentCopy c("malformed");
    c.corrupt("towers/arrow.toml", "buildCost = ", "buildCostTYPO = ");

    content::Registry reg;
    const auto out = content::loadAndValidate(reg, c.dir);  // must not throw
    INFO(out.problem);
    REQUIRE_FALSE(out.ok);
    // The message has to be actionable: which file, and what about it.
    CHECK(mentions(out.problem, "arrow.toml"));
    CHECK(mentions(out.problem, "buildCost"));
}

TEST_CASE("unparseable TOML is reported, not thrown", "[startup]") {
    // Not a missing key -- syntactically broken, which fails deeper in the parser.
    ContentCopy c("garbage");
    c.corrupt("towers/cannon.toml", "[[level]]", "[[[[not toml");

    content::Registry reg;
    const auto out = content::loadAndValidate(reg, c.dir);
    INFO(out.problem);
    REQUIRE_FALSE(out.ok);
    CHECK(!out.problem.empty());
}

TEST_CASE("a table header with an invalid key starter is reported", "[startup]") {
    // toml++ 3.4.0 entered parse_key() on '[' without checking the next
    // character, which asserted in Debug and was UB in Release. These are the
    // shapes that triggered it; each must now come back as a normal error.
    const auto broken = GENERATE(as<std::string>{},
                                 "[=tower]", "[.tower]", "[#tower]",
                                 "[\\tower]", "[&tower]", "[", "[[[level]]");
    ContentCopy c("badheader");
    c.corrupt("towers/cannon.toml", "[[level]]", broken);

    content::Registry reg;
    const auto out = content::loadAndValidate(reg, c.dir);
    INFO("header: " << broken << " -> " << out.problem);
    REQUIRE_FALSE(out.ok);
    CHECK(!out.problem.empty());
}

TEST_CASE("content that fails validation refuses to start", "[startup]") {
    // A modifier pointing at a stat path that does not exist. The validator has
    // always caught this; main() used to log it and carry on.
    ContentCopy c("invalid");
    c.corrupt("trees/global.toml", "global.strike.damage", "global.strike.NOTASTAT");

    content::Registry reg;
    const auto out = content::loadAndValidate(reg, c.dir);
    INFO(out.problem);
    REQUIRE_FALSE(out.ok);
    CHECK(mentions(out.problem, "NOTASTAT"));
    CHECK(mentions(out.problem, "global"));
}

TEST_CASE("every validation error is reported at once", "[startup]") {
    // Fixing content one failed launch at a time is a miserable way to work, and
    // the validator has already found them all.
    ContentCopy c("many");
    c.corrupt("trees/global.toml", "global.strike.damage", "global.strike.NOPE1");
    c.corrupt("trees/global.toml", "global.ward.slow", "global.ward.NOPE2");

    content::Registry reg;
    const auto out = content::loadAndValidate(reg, c.dir);
    INFO(out.problem);
    REQUIRE_FALSE(out.ok);
    CHECK(mentions(out.problem, "NOPE1"));
    CHECK(mentions(out.problem, "NOPE2"));
}

TEST_CASE("a missing content folder is reported, not thrown", "[startup]") {
    content::Registry reg;
    const auto out = content::loadAndValidate(reg, "/no/such/content/folder/at/all");
    INFO(out.problem);
    REQUIRE_FALSE(out.ok);
    CHECK(mentions(out.problem, "not found"));
}
