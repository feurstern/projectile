#include "projectile/math/segment.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/math_matchers.hpp"

#include <limits>
#include <type_traits>

using namespace projectile;

TEST_CASE("Segment is a plain aggregate with equality", "[math][segment]") {
    STATIC_REQUIRE(std::is_aggregate_v<Segment>);
    constexpr Segment segment{.start = {1.0, 2.0, 3.0}, .end = {4.0, 6.0, 3.0}};
    STATIC_REQUIRE(segment == Segment{.start = {1.0, 2.0, 3.0}, .end = {4.0, 6.0, 3.0}});
    STATIC_REQUIRE(segment != Segment{.start = {4.0, 6.0, 3.0}, .end = {1.0, 2.0, 3.0}});
}

TEST_CASE("length of a segment is the distance between its endpoints", "[math][segment]") {
    CHECK(length(Segment{.start = {1.0, 2.0, 3.0}, .end = {4.0, 6.0, 3.0}}) == 5.0);
    CHECK(length(Segment{.start = {1.0, 2.0, 3.0}, .end = {1.0, 2.0, 3.0}}) == 0.0);
}

TEST_CASE("is_valid requires finite endpoints", "[math][segment]") {
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    CHECK(is_valid(Segment{.start = {}, .end = {1.0, 0.0, 0.0}}));
    CHECK(is_valid(Segment{.start = {}, .end = {}})); // a zero-length segment is valid
    CHECK_FALSE(is_valid(Segment{.start = {not_a_number, 0.0, 0.0}, .end = {}}));
    CHECK_FALSE(is_valid(Segment{.start = {}, .end = {0.0, infinity, 0.0}}));
}
