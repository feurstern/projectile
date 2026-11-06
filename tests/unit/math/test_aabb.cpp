#include "projectile/math/aabb.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/math_matchers.hpp"

#include <limits>

using namespace projectile;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

// The corner of the box [low, high] selected by the three bits of `mask`: a set bit picks the
// high coordinate on that axis (bit 0 = x, bit 1 = y, bit 2 = z).
constexpr Vec3 corner(const Vec3& low, const Vec3& high, unsigned mask) {
    return Vec3{
        (mask & 1U) != 0U ? high.x : low.x,
        (mask & 2U) != 0U ? high.y : low.y,
        (mask & 4U) != 0U ? high.z : low.z,
    };
}

} // namespace

TEST_CASE("make_aabb orders corners given in any order", "[math][aabb]") {
    constexpr Vec3 low{-1.0, -2.0, -3.0};
    constexpr Vec3 high{4.0, 5.0, 6.0};
    constexpr AABB expected{.min = low, .max = high};

    // All eight corners, each paired with the opposite corner (all mask bits flipped).
    for (unsigned mask = 0U; mask < 8U; ++mask) {
        const Vec3 a = corner(low, high, mask);
        const Vec3 b = corner(low, high, ~mask & 7U);
        CAPTURE(a, b);
        CHECK(require_value(make_aabb(a, b)) == expected);
    }
}

TEST_CASE("make_aabb accepts flat boxes", "[math][aabb]") {
    const AABB flat = require_value(make_aabb(Vec3{0.0, 1.0, 0.0}, Vec3{5.0, 1.0, 5.0}));
    CHECK(is_valid(flat));
    CHECK(flat.min.y == flat.max.y);

    const AABB point = require_value(make_aabb(Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 3.0}));
    CHECK(is_valid(point));
    CHECK(contains(point, Vec3{1.0, 2.0, 3.0}));
}

TEST_CASE("make_aabb rejects non-finite corners", "[math][aabb]") {
    CHECK_FALSE(make_aabb(Vec3{not_a_number, 0.0, 0.0}, Vec3{1.0, 1.0, 1.0}).has_value());
    CHECK_FALSE(make_aabb(Vec3{0.0, 0.0, 0.0}, Vec3{1.0, infinity, 1.0}).has_value());
}

TEST_CASE("is_valid checks the AABB invariants", "[math][aabb]") {
    CHECK(is_valid(AABB{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}}));
    CHECK_FALSE(is_valid(AABB{.min = {0.0, 2.0, 0.0}, .max = {1.0, 1.0, 1.0}})); // inverted on y
    CHECK_FALSE(is_valid(AABB{.min = {not_a_number, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}}));
    CHECK_FALSE(is_valid(AABB{.min = {0.0, 0.0, 0.0}, .max = {infinity, 1.0, 1.0}}));
}

TEST_CASE("center and half_extents describe the box", "[math][aabb]") {
    constexpr AABB box{.min = {-1.0, 2.0, -6.0}, .max = {3.0, 4.0, 0.0}};
    STATIC_REQUIRE(center(box) == Vec3{1.0, 3.0, -3.0});
    STATIC_REQUIRE(half_extents(box) == Vec3{2.0, 1.0, 3.0});
    STATIC_REQUIRE(
        AABB{.min = center(box) - half_extents(box), .max = center(box) + half_extents(box)} == box
    );
}

TEST_CASE("center and half_extents do not overflow for huge boxes", "[math][aabb]") {
    const double max = std::numeric_limits<double>::max();
    const AABB box{.min = {-max, -max, 0.0}, .max = {max, max, max}};
    CHECK(center(box) == Vec3{0.0, 0.0, max / 2.0});
    CHECK(half_extents(box) == Vec3{max, max, max / 2.0});
}

TEST_CASE("contains includes the faces, edges and corners of the box", "[math][aabb]") {
    constexpr AABB box{.min = {0.0, 0.0, 0.0}, .max = {2.0, 3.0, 4.0}};
    STATIC_REQUIRE(contains(box, Vec3{1.0, 1.0, 1.0}));
    STATIC_REQUIRE(contains(box, Vec3{0.0, 1.5, 2.0})); // face
    STATIC_REQUIRE(contains(box, Vec3{2.0, 3.0, 2.0})); // edge
    STATIC_REQUIRE(contains(box, Vec3{2.0, 3.0, 4.0})); // corner
    STATIC_REQUIRE_FALSE(contains(box, Vec3{-0.001, 1.0, 1.0}));
    STATIC_REQUIRE_FALSE(contains(box, Vec3{1.0, 3.001, 1.0}));
    STATIC_REQUIRE_FALSE(contains(box, Vec3{1.0, 1.0, 4.001}));
    CHECK_FALSE(contains(box, Vec3{not_a_number, 1.0, 1.0}));
}

TEST_CASE("closest_point clamps a point to the box", "[math][aabb]") {
    constexpr AABB box{.min = {-1.0, -2.0, -3.0}, .max = {1.0, 2.0, 3.0}};
    // Inside, or on the surface: the point itself.
    STATIC_REQUIRE(closest_point(box, Vec3{0.5, -1.0, 2.0}) == Vec3{0.5, -1.0, 2.0});
    STATIC_REQUIRE(closest_point(box, Vec3{1.0, 2.0, 3.0}) == Vec3{1.0, 2.0, 3.0});
    // Outside a face, an edge and a corner.
    STATIC_REQUIRE(closest_point(box, Vec3{5.0, 1.0, 0.0}) == Vec3{1.0, 1.0, 0.0});
    STATIC_REQUIRE(closest_point(box, Vec3{5.0, -7.0, 0.0}) == Vec3{1.0, -2.0, 0.0});
    STATIC_REQUIRE(closest_point(box, Vec3{-5.0, 7.0, 9.0}) == Vec3{-1.0, 2.0, 3.0});
    // A flat box is a rectangle.
    constexpr AABB flat{.min = {0.0, 0.0, 0.0}, .max = {2.0, 2.0, 0.0}};
    STATIC_REQUIRE(closest_point(flat, Vec3{1.0, 1.0, -4.0}) == Vec3{1.0, 1.0, 0.0});
}
