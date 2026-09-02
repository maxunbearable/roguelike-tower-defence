#pragma once

#include <filesystem>
#include <string>

namespace td::content {

class Registry;

// Loading the game's content, with the failure told rather than thrown.
//
// The loader already produces a precise message -- it names the file and the key
// -- and main() threw it away: `registry.loadAll(...)` was called with no
// try/catch, so malformed content aborted the process with
//
//     libc++abi: terminating due to uncaught exception of type std::runtime_error
//
// which is a crash, not an error. The diagnostic existed and nobody ever saw it.
//
// Validation was worse: `content::validate()` returned its errors, main() logged
// them at LOG_ERROR and then started the game anyway. A tree modifier pointing at
// a stat path that does not exist -- exactly the mistake that was nearly shipped
// two rounds ago -- would run in an undefined state instead of refusing.
struct LoadOutcome {
    bool ok = false;
    // Empty when ok. Otherwise a message fit to show a player: it names the file
    // and what is wrong with it.
    std::string problem;
};

// Loads every content file under `dir` into `out` and validates the result.
// Never throws: a malformed file, a missing directory and a failed validation
// all come back as `ok == false` with a description.
LoadOutcome loadAndValidate(Registry& out, const std::filesystem::path& dir);

}  // namespace td::content
