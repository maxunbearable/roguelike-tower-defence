# Rendering tests link the software raylib, so they need no display. They are a
# separate binary from td_tests on purpose: td_tests must never gain a raylib
# dependency, because that boundary is what keeps the simulation headless.
file(GLOB_RECURSE UI_TEST_SRC CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE UI_LIB_SRC CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/src/render/*.cpp
     ${CMAKE_SOURCE_DIR}/src/ui/*.cpp)
add_executable(td_uitests ${UI_TEST_SRC} ${UI_LIB_SRC})
target_link_libraries(td_uitests PRIVATE td_core raylib_sw Catch2::Catch2WithMain)
target_compile_definitions(td_uitests PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    TD_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    TD_HEADLESS=1)
# CMAKE_MODULE_PATH set in tests/ does not reach a sibling directory.
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(td_uitests)
