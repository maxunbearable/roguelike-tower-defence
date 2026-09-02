#include "content/Startup.h"

#include <exception>

#include "content/Registry.h"
#include "content/Validate.h"

namespace td::content {

LoadOutcome loadAndValidate(Registry& out, const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return {false, "content folder not found: " + dir.string()};
    }

    try {
        out.loadAll(dir);
    } catch (const std::exception& e) {
        // The loader's message already names the file and the key; it just had
        // nowhere to go.
        return {false, e.what()};
    } catch (...) {
        return {false, "unreadable content in " + dir.string()};
    }

    const auto errors = validate(out);
    if (!errors.empty()) {
        // All of them, not just the first: a content author fixing one at a time
        // through repeated launches is a miserable way to work, and the
        // validator has already done the work of finding them.
        std::string problem = "content failed validation:";
        for (const auto& e : errors) problem += "\n  - " + e;
        return {false, problem};
    }
    return {true, {}};
}

}  // namespace td::content
