#include "projectile/math/triangle.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/math_matchers.hpp"

#include <limits>
#include <type_traits>

using namespace projectile;
using projectile::test::require_value;

TEST_CASE("Triangle is a plain aggregate with equality", "[math][triangle]") {
    STATIC_REQUIRE(std::is_aggregate_v<Triangle>);
    constexpr Triangle triangle{.a = {0.0, 0.0, 0.0}, .b = {1.0, 0.0, 0.0}, .c = {0.0, 1.0, 0.0}};
    STATIC_REQUIRE(triangle == Triangle{.a = {}, .b = {1.0, 0.0, 0.0}, .c = {0.0, 1.0, 0.0}});
    // Swapping two vertices flips the triangle to face the other way: a different triangle.
    STATIC_REQUIRE(triangle != Triangle{.a = {}, .b = {0.0, 1.0, 0.0}, .c = {1.0, 0.0, 0.0}});
}

TEST_CASE("face_normal follows the right-hand rule", "[math][triangle]") {
    const Triangle counter_clockwise{.a = {}, .b = {2.0, 0.0, 0.0}, .c = {0.0, 2.0, 0.0}};
    CHECK(require_value(face_normal(counter_clockwise)) == Vec3{0.0, 0.0, 1.0});

    const Triangle clockwise{.a = {}, .b = {0.0, 2.0, 0.0}, .c = {2.0, 0.0, 0.0}};
    CHECK(require_value(face_normal(clockwise)) == Vec3{0.0, 0.0, -1.0});
}

TEST_CASE("face_normal is empty for triangles with zero area", "[math][triangle]") {
    const Triangle collinear{.a = {}, .b = {1.0, 1.0, 1.0}, .c = {2.0, 2.0, 2.0}};
    const Triangle coincident{.a = {1.0, 2.0, 3.0}, .b = {1.0, 2.0, 3.0}, .c = {4.0, 5.0, 6.0}};
    CHECK_FALSE(face_normal(collinear).has_value());
    CHECK_FALSE(face_normal(coincident).has_value());
}

TEST_CASE("is_valid requires finite vertices and accepts zero area", "[math][triangle]") {
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();
    CHECK(is_valid(Triangle{.a = {}, .b = {1.0, 0.0, 0.0}, .c = {0.0, 1.0, 0.0}}));
    CHECK(is_valid(Triangle{.a = {}, .b = {1.0, 1.0, 1.0}, .c = {2.0, 2.0, 2.0}}));
    CHECK_FALSE(is_valid(Triangle{.a = {}, .b = {not_a_number, 0.0, 0.0}, .c = {}}));
}
