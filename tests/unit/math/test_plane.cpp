#include "projectile/math/plane.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/math_matchers.hpp"
#include "support/random.hpp"

#include <cmath>
#include <limits>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

// The plane y = 2, facing +y.
constexpr Plane raised_floor{.normal = {0.0, 1.0, 0.0}, .distance = 2.0};

} // namespace

TEST_CASE("Plane equality compares the normal and the distance", "[math][plane]") {
    STATIC_REQUIRE(raised_floor == Plane{.normal = {0.0, 1.0, 0.0}, .distance = 2.0});
    STATIC_REQUIRE(raised_floor != Plane{.normal = {0.0, 1.0, 0.0}, .distance = 3.0});
    STATIC_REQUIRE(raised_floor != Plane{.normal = {0.0, -1.0, 0.0}, .distance = 2.0});
}

TEST_CASE("make_plane builds the plane through a point", "[math][plane]") {
    const Plane plane = require_value(make_plane(Vec3{0.0, 2.0, 0.0}, Vec3{5.0, 3.0, -1.0}));
    CHECK(plane.normal == Vec3{0.0, 1.0, 0.0}); // normalized
    CHECK(plane.distance == 3.0);
    CHECK(is_valid(plane));
    CHECK(signed_distance(plane, Vec3{5.0, 3.0, -1.0}) == 0.0);
}

TEST_CASE("make_plane rejects degenerate input", "[math][plane]") {
    const double max = std::numeric_limits<double>::max();
    CHECK_FALSE(make_plane(Vec3{}, Vec3{1.0, 2.0, 3.0}).has_value()); // no normal
    CHECK_FALSE(make_plane(Vec3{not_a_number, 1.0, 0.0}, Vec3{}).has_value());
    CHECK_FALSE(make_plane(Vec3{0.0, 1.0, 0.0}, Vec3{0.0, infinity, 0.0}).has_value());
    // The distance from the origin, about 1.41 * max, is not representable.
    CHECK_FALSE(make_plane(Vec3{1.0, 1.0, 0.0}, Vec3{max, max, 0.0}).has_value());
}

TEST_CASE("plane_through_points orients the normal by the right-hand rule", "[math][plane]") {
    const Plane counter_clockwise = require_value(
        plane_through_points(Vec3{0.0, 0.0, 0.0}, Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0})
    );
    CHECK(counter_clockwise.normal == Vec3{0.0, 0.0, 1.0});
    CHECK(counter_clockwise.distance == 0.0);

    const Plane clockwise = require_value(
        plane_through_points(Vec3{0.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{1.0, 0.0, 0.0})
    );
    CHECK(clockwise.normal == Vec3{0.0, 0.0, -1.0});

    const Plane raised = require_value(
        plane_through_points(Vec3{0.0, 0.0, 5.0}, Vec3{2.0, 0.0, 5.0}, Vec3{0.0, 3.0, 5.0})
    );
    CHECK(raised.normal == Vec3{0.0, 0.0, 1.0});
    CHECK(raised.distance == 5.0);
}

TEST_CASE("plane_through_points rejects points that span no plane", "[math][plane]") {
    // Collinear points.
    CHECK_FALSE(plane_through_points(Vec3{0.0, 0.0, 0.0}, Vec3{1.0, 1.0, 1.0}, Vec3{2.0, 2.0, 2.0})
                    .has_value());
    // Two or three coincident points.
    CHECK_FALSE(plane_through_points(Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 3.0}, Vec3{4.0, 5.0, 6.0})
                    .has_value());
    CHECK_FALSE(plane_through_points(Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 3.0})
                    .has_value());
    // Non-finite points.
    CHECK_FALSE(
        plane_through_points(Vec3{not_a_number, 0.0, 0.0}, Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0})
            .has_value()
    );
}

TEST_CASE("plane_through_points contains all three points", "[math][plane][property]") {
    projectile::test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Vec3 a = random.vec3(-100.0, 100.0);
        const Vec3 b = random.vec3(-100.0, 100.0);
        const Vec3 c = random.vec3(-100.0, 100.0);
        // Nearly collinear points have a legitimately imprecise normal (the error grows like
        // epsilon / sin(angle)); this property is about well-conditioned triangles.
        const Vec3 ab = b - a;
        const Vec3 ac = c - a;
        if (length(cross(ab, ac)) < 1e-3 * length(ab) * length(ac)) {
            continue;
        }
        CAPTURE(a, b, c);
        const Plane plane = require_value(plane_through_points(a, b, c));
        CHECK(is_valid(plane));
        CHECK_THAT(signed_distance(plane, a), WithinAbs(0.0, 1e-9));
        CHECK_THAT(signed_distance(plane, b), WithinAbs(0.0, 1e-9));
        CHECK_THAT(signed_distance(plane, c), WithinAbs(0.0, 1e-9));
    }
}

TEST_CASE(
    "signed_distance is positive in front of the plane and negative behind it", "[math][plane]"
) {
    STATIC_REQUIRE(signed_distance(raised_floor, Vec3{5.0, 7.0, -3.0}) == 5.0);
    STATIC_REQUIRE(signed_distance(raised_floor, Vec3{5.0, -1.0, -3.0}) == -3.0);
    STATIC_REQUIRE(signed_distance(raised_floor, Vec3{100.0, 2.0, 100.0}) == 0.0);
}

TEST_CASE("closest_point projects a point onto the plane", "[math][plane]") {
    STATIC_REQUIRE(closest_point(raised_floor, Vec3{5.0, 7.0, -3.0}) == Vec3{5.0, 2.0, -3.0});
    STATIC_REQUIRE(closest_point(raised_floor, Vec3{5.0, -1.0, -3.0}) == Vec3{5.0, 2.0, -3.0});
    STATIC_REQUIRE(closest_point(raised_floor, Vec3{5.0, 2.0, -3.0}) == Vec3{5.0, 2.0, -3.0});
}

TEST_CASE("closest_point lies on the plane, straight along the normal", "[math][plane][property]") {
    projectile::test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Plane plane =
            require_value(make_plane(random.vec3(-1.0, 1.0), random.vec3(-100.0, 100.0)));
        const Vec3 point = random.vec3(-100.0, 100.0);
        const Vec3 closest = closest_point(plane, point);
        CAPTURE(plane, point);
        CHECK_THAT(signed_distance(plane, closest), WithinAbs(0.0, 1e-12));
        CHECK_THAT(length(cross(point - closest, plane.normal)), WithinAbs(0.0, 1e-12));
        CHECK_THAT(
            distance(point, closest), WithinAbs(std::abs(signed_distance(plane, point)), 1e-12)
        );
    }
}

TEST_CASE("is_valid checks the Plane invariants", "[math][plane]") {
    CHECK(is_valid(raised_floor));
    CHECK_FALSE(is_valid(Plane{.normal = {0.0, 2.0, 0.0}, .distance = 1.0})); // not unit length
    CHECK_FALSE(is_valid(Plane{.normal = {}, .distance = 0.0}));
    CHECK_FALSE(is_valid(Plane{.normal = {0.0, 1.0, 0.0}, .distance = not_a_number}));
    CHECK_FALSE(is_valid(Plane{.normal = {0.0, 1.0, 0.0}, .distance = infinity}));
}
