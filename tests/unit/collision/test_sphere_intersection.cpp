#include "projectile/collision/intersection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/collision_matchers.hpp"

#include <cmath>
#include <numbers>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr Sphere sphere{.center = {0.0, 0.0, 0.0}, .radius = 2.0};
constexpr Vec3 plus_x{1.0, 0.0, 0.0};
constexpr Vec3 minus_x{-1.0, 0.0, 0.0};

} // namespace

// --- Rays from outside --------------------------------------------------------------------------

TEST_CASE("a ray hits a sphere head-on", "[collision][sphere]") {
    const HitResult hit =
        require_value(intersect(Ray{.origin = {-10.0, 0.0, 0.0}, .direction = plus_x}, sphere));
    CHECK(hit.distance == 8.0);
    CHECK(hit.point == Vec3{-2.0, 0.0, 0.0});
    CHECK(hit.normal == minus_x);
    CHECK(hit.front_face);
}

TEST_CASE("an off-center ray hits a sphere where it enters", "[collision][sphere]") {
    // Offset by 1 from the center line: the ray enters at x = -sqrt(3).
    constexpr double root3 = std::numbers::sqrt3;
    const HitResult hit =
        require_value(intersect(Ray{.origin = {-10.0, 1.0, 0.0}, .direction = plus_x}, sphere));
    CHECK_THAT(hit.distance, WithinAbs(10.0 - root3, 1e-14));
    CHECK_THAT(hit.point, is_near(Vec3{-root3, 1.0, 0.0}, 1e-14, 0.0));
    CHECK_THAT(hit.normal, is_near(Vec3{-root3 / 2.0, 0.5, 0.0}, 1e-15, 0.0));
}

TEST_CASE("a ray passing beside a sphere misses it", "[collision][sphere]") {
    CHECK_FALSE(
        intersect(Ray{.origin = {-10.0, 3.0, 0.0}, .direction = plus_x}, sphere).has_value()
    );
}

TEST_CASE("a ray grazing a sphere touches it at one point", "[collision][sphere]") {
    const HitResult hit =
        require_value(intersect(Ray{.origin = {-10.0, 2.0, 0.0}, .direction = plus_x}, sphere));
    CHECK(hit.distance == 10.0);
    CHECK(hit.point == Vec3{0.0, 2.0, 0.0});
    CHECK(hit.normal == Vec3{0.0, 1.0, 0.0}); // perpendicular to the ray
    CHECK(hit.front_face);
}

TEST_CASE("a sphere behind the ray is not hit", "[collision][sphere]") {
    CHECK_FALSE(
        intersect(Ray{.origin = {10.0, 0.0, 0.0}, .direction = plus_x}, sphere).has_value()
    );
}

TEST_CASE("max_distance limits sphere hits", "[collision][sphere]") {
    const Ray ray{.origin = {-10.0, 0.0, 0.0}, .direction = plus_x};
    CHECK(intersect(ray, sphere, 8.0).has_value());
    CHECK_FALSE(intersect(ray, sphere, 7.999).has_value());
}

// --- Rays starting inside or on the surface ----------------------------------------------------

TEST_CASE(
    "a ray starting inside a sphere hits the back face where it leaves", "[collision][sphere]"
) {
    const Ray from_center{.origin = {}, .direction = plus_x};
    const HitResult hit = require_value(intersect(from_center, sphere));
    CHECK(hit.distance == 2.0);
    CHECK(hit.point == Vec3{2.0, 0.0, 0.0});
    CHECK(hit.normal == minus_x); // facing back into the sphere, towards where the ray came from
    CHECK_FALSE(hit.front_face);

    const HitResult off_center =
        require_value(intersect(Ray{.origin = {1.0, 0.0, 0.0}, .direction = plus_x}, sphere));
    CHECK(off_center.distance == 1.0);

    CHECK_FALSE(intersect(from_center, sphere, 100.0, Culling::back_faces).has_value());
}

TEST_CASE("a ray starting on a sphere hits it only when moving inwards", "[collision][sphere]") {
    const HitResult inwards =
        require_value(intersect(Ray{.origin = {-2.0, 0.0, 0.0}, .direction = plus_x}, sphere));
    CHECK(inwards.distance == 0.0);
    CHECK(inwards.normal == minus_x);
    CHECK(inwards.front_face);

    CHECK_FALSE(
        intersect(Ray{.origin = {-2.0, 0.0, 0.0}, .direction = minus_x}, sphere).has_value()
    );
    CHECK_FALSE(
        intersect(Ray{.origin = {-2.0, 0.0, 0.0}, .direction = {0.0, 1.0, 0.0}}, sphere).has_value()
    );
}

// --- Degenerate spheres and precision
// -------------------------------------------------------------

TEST_CASE("a sphere of radius zero is hit only through its center", "[collision][sphere]") {
    const Sphere point{.center = {5.0, 0.0, 0.0}, .radius = 0.0};
    const HitResult hit = require_value(intersect(Ray{.origin = {}, .direction = plus_x}, point));
    CHECK(hit.distance == 5.0);
    CHECK(hit.point == point.center);
    CHECK(hit.normal == minus_x); // a point has no surface normal, so it faces the ray
    CHECK_FALSE(intersect(Ray{.origin = {0.0, 1e-9, 0.0}, .direction = plus_x}, point).has_value());
}

TEST_CASE("a small sphere far away is hit accurately", "[collision][sphere]") {
    // At this distance, the textbook discriminant b^2 - c loses all but a few digits (an error of
    // about 3e-5 here); computing it from the distance to the line keeps full precision.
    const Sphere far_sphere{.center = {1e6, 0.0, 0.0}, .radius = 1.0};
    const HitResult hit =
        require_value(intersect(Ray{.origin = {0.0, 0.3, 0.0}, .direction = plus_x}, far_sphere));
    CHECK_THAT(hit.distance, WithinAbs(1e6 - std::sqrt(1.0 - 0.09), 1e-9));
}

// --- Segments ---------------------------------------------------------------------------------

TEST_CASE("a segment through a sphere hits it where it enters", "[collision][sphere][segment]") {
    const HitResult hit = require_value(
        intersect(Segment{.start = {-10.0, 0.0, 0.0}, .end = {10.0, 0.0, 0.0}}, sphere)
    );
    // Points inside a segment come from std::lerp, whose rounding varies between standard
    // libraries; compare them within a few units in the last place.
    CHECK_THAT(hit.distance, WithinAbs(8.0, 1e-14));
    CHECK_THAT(hit.point, is_near(Vec3{-2.0, 0.0, 0.0}, 1e-14, 0.0));
    CHECK(hit.normal == minus_x);
    CHECK(hit.front_face);
}

TEST_CASE("a segment that stops short of a sphere misses it", "[collision][sphere][segment]") {
    CHECK_FALSE(
        intersect(Segment{.start = {-10.0, 0.0, 0.0}, .end = {-3.0, 0.0, 0.0}}, sphere).has_value()
    );
}

TEST_CASE(
    "a segment starting inside a sphere hits where it leaves", "[collision][sphere][segment]"
) {
    const Segment segment{.start = {}, .end = {5.0, 0.0, 0.0}};
    const HitResult hit = require_value(intersect(segment, sphere));
    CHECK_THAT(hit.distance, WithinAbs(2.0, 1e-14));
    CHECK_THAT(hit.point, is_near(Vec3{2.0, 0.0, 0.0}, 1e-14, 0.0));
    CHECK_FALSE(hit.front_face);
    CHECK_FALSE(intersect(segment, sphere, Culling::back_faces).has_value());
}

TEST_CASE("a zero-length segment never hits a sphere", "[collision][sphere][segment]") {
    CHECK_FALSE(intersect(Segment{.start = {}, .end = {}}, sphere).has_value());
    CHECK_FALSE(
        intersect(Segment{.start = {-2.0, 0.0, 0.0}, .end = {-2.0, 0.0, 0.0}}, sphere).has_value()
    );
}
