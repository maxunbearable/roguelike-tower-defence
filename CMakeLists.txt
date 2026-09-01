cmake_minimum_required(VERSION 3.24)
project(td LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
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
CPMAddPackage("gh:marzer/tomlplusplus@3.4.0")
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

enable_testing()
add_subdirectory(tests)
