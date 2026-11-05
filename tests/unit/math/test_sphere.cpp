#include "projectile/math/sphere.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/math_matchers.hpp"

#include <limits>

using namespace projectile;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

} // namespace

TEST_CASE("Sphere equality compares the center and the radius", "[math][sphere]") {
    constexpr Sphere sphere{.center = {1.0, 2.0, 3.0}, .radius = 0.5};
    STATIC_REQUIRE(sphere == Sphere{.center = {1.0, 2.0, 3.0}, .radius = 0.5});
    STATIC_REQUIRE(sphere != Sphere{.center = {1.0, 2.0, 3.0}, .radius = 0.25});
    STATIC_REQUIRE(sphere != Sphere{.center = {1.0, 2.0, 4.0}, .radius = 0.5});
}

TEST_CASE("is_valid checks the Sphere invariants", "[math][sphere]") {
    CHECK(is_valid(Sphere{.center = {1.0, 2.0, 3.0}, .radius = 0.5}));
    CHECK(is_valid(Sphere{.center = {}, .radius = 0.0})); // a single point
    CHECK_FALSE(is_valid(Sphere{.center = {}, .radius = -1.0}));
    CHECK_FALSE(is_valid(Sphere{.center = {}, .radius = not_a_number}));
    CHECK_FALSE(is_valid(Sphere{.center = {}, .radius = infinity}));
    CHECK_FALSE(is_valid(Sphere{.center = {not_a_number, 0.0, 0.0}, .radius = 1.0}));
}

TEST_CASE("contains includes the surface of the sphere", "[math][sphere]") {
    constexpr Sphere sphere{.center = {1.0, 1.0, 1.0}, .radius = 2.0};
    STATIC_REQUIRE(contains(sphere, Vec3{1.0, 1.0, 1.0}));  // center
    STATIC_REQUIRE(contains(sphere, Vec3{3.0, 1.0, 1.0}));  // on the surface
    STATIC_REQUIRE(contains(sphere, Vec3{1.0, -1.0, 1.0})); // on the surface
    STATIC_REQUIRE_FALSE(contains(sphere, Vec3{3.0001, 1.0, 1.0}));
    // Inside the bounding box but outside the sphere.
    STATIC_REQUIRE_FALSE(contains(sphere, Vec3{2.5, 2.5, 1.0}));
    CHECK_FALSE(contains(sphere, Vec3{not_a_number, 1.0, 1.0}));
}

TEST_CASE("a sphere of radius zero contains only its center", "[math][sphere]") {
    constexpr Sphere point{.center = {1.0, 2.0, 3.0}, .radius = 0.0};
    STATIC_REQUIRE(contains(point, Vec3{1.0, 2.0, 3.0}));
    STATIC_REQUIRE_FALSE(contains(point, Vec3{1.0, 2.0, 3.000001}));
}
