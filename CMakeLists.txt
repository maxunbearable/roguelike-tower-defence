cmake_minimum_required(VERSION 3.24)
project(td LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# A plain `cmake -B build` is what the README tells a player to run, so it has to
# produce a binary worth playing: optimised, with NDEBUG set. Debug remains one
# flag away (-DCMAKE_BUILD_TYPE=Debug) and RelWithDebInfo keeps symbols, so
# nothing is lost for development.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Several dependencies still declare cmake_minimum_required below 3.5, which
# CMake 4 rejects outright. This floors them instead of forking each one.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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

# --- Architecture boundary ------------------------------------------------
# td_core holds all game logic and must stay headlessly testable, which means
# it must never gain a raylib dependency. Enforced at configure time.
file(GLOB_RECURSE TD_CORE_TREE CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/src/core/*
     ${CMAKE_SOURCE_DIR}/src/sim/*
     ${CMAKE_SOURCE_DIR}/src/content/*)
foreach(f ${TD_CORE_TREE})
  file(READ ${f} _contents)
  if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]raylib\\.h[>\"]")
    message(FATAL_ERROR "td_core boundary violated: ${f} includes raylib.h")
  endif()
endforeach()

# --- td_core : game logic, MUST NOT link raylib ---------------------------
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS
     src/core/*.cpp src/sim/*.cpp src/content/*.cpp)
add_library(td_core STATIC ${CORE_SRC})
target_include_directories(td_core PUBLIC src)
target_link_libraries(td_core PUBLIC
    EnTT::EnTT
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json)

# --- td_app : executable --------------------------------------------------
file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS
     src/render/*.cpp src/ui/*.cpp src/app/*.cpp src/audio/*.cpp)
add_executable(td_app ${APP_SRC})
target_link_libraries(td_app PRIVATE td_core raylib)
target_compile_definitions(td_app PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    TD_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# --- td_shot : the same game, rendered without a display -------------------
# Eleven rounds shipped UI changes that could not be looked at, because macOS
# denies a locked screen's processes an active display: CoreGraphics reports the
# monitor online but not active, GLFW then enumerates zero monitors, and raylib
# aborts InitWindow -- purely because it cannot decide where to centre a window.
# raylib ships a software rasteriser (rlsw) and a display-free backend for
# exactly this, so the capture tool is the identical game code linked against
# raylib built on PLATFORM_MEMORY. Verification no longer depends on the screen.
add_library(raylib_sw STATIC
    ${raylib_SOURCE_DIR}/src/rcore.c
    ${raylib_SOURCE_DIR}/src/rshapes.c
    ${raylib_SOURCE_DIR}/src/rtext.c
    ${raylib_SOURCE_DIR}/src/rtextures.c
    ${raylib_SOURCE_DIR}/src/rmodels.c
    ${raylib_SOURCE_DIR}/src/raudio.c)
target_include_directories(raylib_sw PUBLIC ${raylib_SOURCE_DIR}/src)
# rlsw's texture pool defaults to 128 entries and the game loads well over
# three hundred, so every texture past the 128th silently failed and its art
# rendered as nothing -- which is why the terrain and every UI panel came back
# black on the first capture. The pool holds descriptors, not pixels, so a
# generous ceiling costs almost nothing.
target_compile_definitions(raylib_sw PUBLIC PLATFORM_MEMORY GRAPHICS_API_OPENGL_SOFTWARE
                           SW_MAX_TEXTURES=1024)
if(APPLE)
  target_link_libraries(raylib_sw PUBLIC "-framework CoreAudio" "-framework AudioToolbox")
endif()

add_executable(td_shot ${APP_SRC})
target_link_libraries(td_shot PRIVATE td_core raylib_sw)
target_compile_definitions(td_shot PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    TD_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    TD_HEADLESS=1)

enable_testing()
add_subdirectory(tests)
add_subdirectory(uitests)
