#pragma once

#include <filesystem>
#include <string>

namespace td::content {

class Registry;

// Content loading with the failure told rather than thrown. The loader produces
// a precise message naming the file and key; main() used to call loadAll with no
// try/catch, so malformed content aborted the process and the diagnostic was
// never seen. Validation errors were logged and then ignored.
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
