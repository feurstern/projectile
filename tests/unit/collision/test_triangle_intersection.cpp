#include "projectile/collision/intersection.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/collision_matchers.hpp"

#include <array>

using namespace projectile;
using projectile::test::require_value;

namespace {

// Lies in the plane z = 0 and faces +z: counter-clockwise when seen from above.
constexpr Triangle triangle{.a = {0.0, 0.0, 0.0}, .b = {2.0, 0.0, 0.0}, .c = {0.0, 2.0, 0.0}};
constexpr Vec3 up{0.0, 0.0, 1.0};
constexpr Vec3 down{0.0, 0.0, -1.0};

[[nodiscard]] Ray downwards_from(double x, double y) {
    return Ray{.origin = {x, y, 5.0}, .direction = down};
}

} // namespace

// --- Rays ---------------------------------------------------------------------------------------

TEST_CASE("a ray hits the front of a triangle", "[collision][triangle]") {
    const HitResult hit = require_value(intersect(downwards_from(0.5, 0.5), triangle));
    CHECK(hit.distance == 5.0);
    CHECK(hit.point == Vec3{0.5, 0.5, 0.0});
    CHECK(hit.normal == up);
    CHECK(hit.front_face);
}

TEST_CASE("a ray hitting a triangle from behind reports a back face", "[collision][triangle]") {
    const Ray ray{.origin = {0.5, 0.5, -5.0}, .direction = up};
    const HitResult hit = require_value(intersect(ray, triangle));
    CHECK(hit.distance == 5.0);
    CHECK(hit.normal == down);
    CHECK_FALSE(hit.front_face);
    CHECK_FALSE(intersect(ray, triangle, 100.0, Culling::back_faces).has_value());
}

TEST_CASE("a ray outside an edge misses the triangle", "[collision][triangle]") {
    CHECK_FALSE(intersect(downwards_from(1.5, 1.5), triangle).has_value()); // beyond the long edge
    CHECK_FALSE(
        intersect(downwards_from(-0.1, 0.5), triangle).has_value()
    ); // beyond the edge x = 0
    CHECK_FALSE(
        intersect(downwards_from(0.5, -0.1), triangle).has_value()
    ); // beyond the edge y = 0
}

TEST_CASE("edges and vertices belong to the triangle", "[collision][triangle]") {
    constexpr std::array<Vec2, 6> boundary_points{{
        {1.0, 0.0}, // on edge a-b
        {0.0, 1.0}, // on edge a-c
        {1.0, 1.0}, // on edge b-c
        {0.0, 0.0}, // vertex a
        {2.0, 0.0}, // vertex b
        {0.0, 2.0}, // vertex c
    }};
    for (const Vec2& point : boundary_points) {
        CAPTURE(point);
        const HitResult hit = require_value(intersect(downwards_from(point.x, point.y), triangle));
        CHECK(hit.point == Vec3{point.x, point.y, 0.0});
    }
}

TEST_CASE("a ray parallel to a triangle never hits it", "[collision][triangle]") {
    CHECK_FALSE(intersect(Ray{.origin = {0.5, 0.5, 5.0}, .direction = {1.0, 0.0, 0.0}}, triangle)
                    .has_value());
    // Not even when it travels within the triangle's plane, across the triangle.
    CHECK_FALSE(intersect(Ray{.origin = {-1.0, 0.5, 0.0}, .direction = {1.0, 0.0, 0.0}}, triangle)
                    .has_value());
}

TEST_CASE("a triangle with zero area is never hit", "[collision][triangle]") {
    const Triangle collinear{.a = {}, .b = {1.0, 1.0, 0.0}, .c = {2.0, 2.0, 0.0}};
    const Triangle coincident{.a = {}, .b = {}, .c = {2.0, 0.0, 0.0}};
    const Triangle point{.a = {1.0, 1.0, 0.0}, .b = {1.0, 1.0, 0.0}, .c = {1.0, 1.0, 0.0}};
    CHECK_FALSE(intersect(downwards_from(1.0, 1.0), collinear).has_value());
    CHECK_FALSE(intersect(downwards_from(1.0, 0.0), coincident).has_value());
    CHECK_FALSE(intersect(downwards_from(1.0, 1.0), point).has_value());
}

TEST_CASE("a triangle behind the ray is not hit", "[collision][triangle]") {
    CHECK_FALSE(intersect(Ray{.origin = {0.5, 0.5, 5.0}, .direction = up}, triangle).has_value());
}

TEST_CASE("max_distance limits triangle hits", "[collision][triangle]") {
    CHECK(intersect(downwards_from(0.5, 0.5), triangle, 5.0).has_value());
    CHECK_FALSE(intersect(downwards_from(0.5, 0.5), triangle, 4.999).has_value());
}

TEST_CASE("a ray starting on a triangle hits it at distance zero", "[collision][triangle]") {
    const HitResult hit =
        require_value(intersect(Ray{.origin = {0.5, 0.5, 0.0}, .direction = down}, triangle));
    CHECK(hit.distance == 0.0);
    CHECK(hit.front_face);
}

// --- Segments ---------------------------------------------------------------------------------

TEST_CASE("a segment crossing a triangle hits it", "[collision][triangle][segment]") {
    const HitResult hit = require_value(
        intersect(Segment{.start = {0.5, 0.5, 5.0}, .end = {0.5, 0.5, -5.0}}, triangle)
    );
    CHECK(hit.distance == 5.0);
    CHECK(hit.point == Vec3{0.5, 0.5, 0.0});
    CHECK(hit.normal == up);
    CHECK(hit.front_face);
}

TEST_CASE("a segment that stops short of a triangle misses it", "[collision][triangle][segment]") {
    CHECK_FALSE(
        intersect(Segment{.start = {0.5, 0.5, 5.0}, .end = {0.5, 0.5, 1.0}}, triangle).has_value()
    );
}

TEST_CASE("a zero-length segment never hits a triangle", "[collision][triangle][segment]") {
    CHECK_FALSE(
        intersect(Segment{.start = {0.5, 0.5, 0.0}, .end = {0.5, 0.5, 0.0}}, triangle).has_value()
    );
}
