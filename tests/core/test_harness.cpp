#include <catch2/catch_test_macros.hpp>
#include <string>

#include "core/Version.h"

TEST_CASE("test harness runs and core is linkable", "[harness]") {
    REQUIRE(std::string(td::core::versionString()) == "0.1.0");
}
