# Dependency pins, kept in their own file so CI can key its cache on them:
# hashing the whole of CMakeLists.txt meant any edit rebuilt every dependency.
include(cmake/get_cpm.cmake)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Tags verified against upstream on 2026-08-30. raylib tags carry no "v" prefix
# so it uses CPM's verbatim "#<tag>" form. EnTT 4.0.0 exists but is a fresh major
# with API churn; pinned to the latest 3.x.
CPMAddPackage("gh:raysan5/raylib#6.0")
CPMAddPackage("gh:skypjack/entt@3.16.0")
# toml++ is pinned past its last release (3.4.0, Oct 2023) to a specific commit.
# 3.4.0 enters parse_key() on a '[' table header without checking the next
# character is a valid key starter, so malformed content is an assert in Debug
# and a violated __builtin_assume -- real UB -- in Release. Upstream fixed it
# (issues #294, #301); there has been no release since. Measured across all 33
# content files with 13,200 sampled corruptions: 25 crashes on 3.4.0, 0 here.
CPMAddPackage(NAME tomlplusplus GITHUB_REPOSITORY marzer/tomlplusplus
              GIT_TAG 1e8829b793b66ad17011732a146b8077d379b011)
CPMAddPackage("gh:nlohmann/json@3.12.0")
CPMAddPackage("gh:catchorg/Catch2@3.16.0")
