#include <catch2/catch_test_macros.hpp>

TEST_CASE("Smoke: Catch2 + CMake wired up", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
