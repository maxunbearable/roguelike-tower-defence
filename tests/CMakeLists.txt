file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS *.cpp)
add_executable(td_tests ${TEST_SRC})
target_link_libraries(td_tests PRIVATE td_core Catch2::Catch2WithMain)
target_compile_definitions(td_tests PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content")

# CPM does not put Catch2's CMake helpers on the module path; without this,
# include(Catch) fails with "Could not find Catch.cmake".
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(td_tests)
