#include "projectile/collision/intersection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/collision_matchers.hpp"

#include <array>
#include <cmath>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr AABB box{.min = {-1.0, -2.0, -3.0}, .max = {1.0, 2.0, 3.0}};
constexpr Vec3 plus_x{1.0, 0.0, 0.0};
constexpr Vec3 minus_x{-1.0, 0.0, 0.0};

struct FaceCase {
    Vec3 origin;
    Vec3 direction;
    Vec3 expected_point;
    Vec3 expected_normal;
};

} // namespace

// --- Rays from outside --------------------------------------------------------------------------

TEST_CASE("a ray enters a box through each of its six faces", "[collision][aabb]") {
    // Each origin is 4 m from the face it aims at.
    constexpr std::array cases{
        FaceCase{{-5.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
        FaceCase{{5.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
        FaceCase{{0.0, -6.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, -2.0, 0.0}, {0.0, -1.0, 0.0}},
        FaceCase{{0.0, 6.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 1.0, 0.0}},
        FaceCase{{0.0, 0.0, -7.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -3.0}, {0.0, 0.0, -1.0}},
        FaceCase{{0.0, 0.0, 7.0}, {0.0, 0.0, -1.0}, {0.0, 0.0, 3.0}, {0.0, 0.0, 1.0}},
    };
    for (const FaceCase& face : cases) {
        CAPTURE(face.origin, face.direction);
        const HitResult hit =
            require_value(intersect(Ray{.origin = face.origin, .direction = face.direction}, box));
        CHECK(hit.distance == 4.0);
        CHECK(hit.point == face.expected_point);
        CHECK(hit.normal == face.expected_normal);
        CHECK(hit.front_face);
    }
}

TEST_CASE("a ray passing beside a box misses it", "[collision][aabb]") {
    // Parallel to the y slab but outside it.
    CHECK_FALSE(intersect(Ray{.origin = {-5.0, 3.0, 0.0}, .direction = plus_x}, box).has_value());
    // Diagonal: inside the x slab only once it has left the y slab.
    const double half = std::sqrt(0.5);
    CHECK_FALSE(
        intersect(Ray{.origin = {-5.0, 0.0, 0.0}, .direction = {half, half, 0.0}}, box).has_value()
    );
}

TEST_CASE("a ray parallel to two slabs hits a box when inside both", "[collision][aabb]") {
    const HitResult hit =
        require_value(intersect(Ray{.origin = {-5.0, 1.5, 2.5}, .direction = plus_x}, box));
    CHECK(hit.distance == 4.0);
    CHECK(hit.point == Vec3{-1.0, 1.5, 2.5});
}

TEST_CASE("a ray grazing a box along a face touches it at the edge", "[collision][aabb]") {
    // Travels in the plane y = 2 of the box's top face.
    const HitResult hit =
        require_value(intersect(Ray{.origin = {-5.0, 2.0, 0.0}, .direction = plus_x}, box));
    CHECK(hit.distance == 4.0);
    CHECK(hit.point == Vec3{-1.0, 2.0, 0.0});
    CHECK(hit.normal == minus_x);
}

TEST_CASE("a ray through a corner reports the normal of one adjacent face", "[collision][aabb]") {
    const Ray ray = require_value(make_ray(Vec3{-5.0, -6.0, -7.0}, Vec3{1.0, 1.0, 1.0}));
    const HitResult hit = require_value(intersect(ray, box));
    CHECK_THAT(hit.point, is_near(box.min, 1e-14, 0.0));
    CHECK(hit.normal == minus_x); // the x face, which is checked first
    CHECK(hit.front_face);
}

TEST_CASE("a flat box is hit where the ray crosses it", "[collision][aabb]") {
    constexpr AABB flat{.min = {-1.0, 0.0, -1.0}, .max = {1.0, 0.0, 1.0}};
    const HitResult hit = require_value(
        intersect(Ray{.origin = {0.0, 5.0, 0.0}, .direction = {0.0, -1.0, 0.0}}, flat)
    );
    CHECK(hit.distance == 5.0);
    CHECK(hit.normal == Vec3{0.0, 1.0, 0.0});
    CHECK(hit.front_face);
}

TEST_CASE("a box behind the ray is not hit", "[collision][aabb]") {
    CHECK_FALSE(intersect(Ray{.origin = {5.0, 0.0, 0.0}, .direction = plus_x}, box).has_value());
}

TEST_CASE("max_distance limits box hits", "[collision][aabb]") {
    const Ray ray{.origin = {-5.0, 0.0, 0.0}, .direction = plus_x};
    CHECK(intersect(ray, box, 4.0).has_value());
    CHECK_FALSE(intersect(ray, box, 3.999).has_value());
}

// --- Rays starting inside or on the surface ----------------------------------------------------

TEST_CASE("a ray starting inside a box hits the back face where it leaves", "[collision][aabb]") {
    const Ray ray{.origin = {}, .direction = plus_x};
    const HitResult hit = require_value(intersect(ray, box));
    CHECK(hit.distance == 1.0);
    CHECK(hit.point == Vec3{1.0, 0.0, 0.0});
    CHECK(hit.normal == minus_x); // facing back into the box
    CHECK_FALSE(hit.front_face);
    CHECK_FALSE(intersect(ray, box, 100.0, Culling::back_faces).has_value());
}

TEST_CASE("a ray starting on a box hits it only when moving inwards", "[collision][aabb]") {
    const HitResult inwards =
        require_value(intersect(Ray{.origin = {-1.0, 0.0, 0.0}, .direction = plus_x}, box));
    CHECK(inwards.distance == 0.0);
    CHECK(inwards.normal == minus_x);
    CHECK(inwards.front_face);

    // Moving outwards, or sliding along the face.
    CHECK_FALSE(intersect(Ray{.origin = {-1.0, 0.0, 0.0}, .direction = minus_x}, box).has_value());
    CHECK_FALSE(
        intersect(Ray{.origin = {-1.0, 0.0, 0.0}, .direction = {0.0, 1.0, 0.0}}, box).has_value()
    );
}

// --- Segments ---------------------------------------------------------------------------------

TEST_CASE("a segment through a box hits it where it enters", "[collision][aabb][segment]") {
    const HitResult hit =
        require_value(intersect(Segment{.start = {-5.0, 0.0, 0.0}, .end = {5.0, 0.0, 0.0}}, box));
    CHECK_THAT(hit.distance, WithinAbs(4.0, 1e-14));
    CHECK_THAT(hit.point, is_near(Vec3{-1.0, 0.0, 0.0}, 1e-14, 0.0));
    CHECK(hit.normal == minus_x);
    CHECK(hit.front_face);
}

TEST_CASE("a segment that stops short of a box misses it", "[collision][aabb][segment]") {
    CHECK_FALSE(
        intersect(Segment{.start = {-5.0, 0.0, 0.0}, .end = {-2.0, 0.0, 0.0}}, box).has_value()
    );
}

TEST_CASE("a segment starting inside a box hits where it leaves", "[collision][aabb][segment]") {
    const Segment segment{.start = {}, .end = {0.0, 4.0, 0.0}};
    const HitResult hit = require_value(intersect(segment, box));
    CHECK(hit.distance == 2.0);
    CHECK(hit.point == Vec3{0.0, 2.0, 0.0});
    CHECK(hit.normal == Vec3{0.0, -1.0, 0.0});
    CHECK_FALSE(hit.front_face);
}

TEST_CASE("a zero-length segment never hits a box", "[collision][aabb][segment]") {
    CHECK_FALSE(intersect(Segment{.start = {}, .end = {}}, box).has_value()); // inside
    CHECK_FALSE(
        intersect(Segment{.start = {1.0, 0.0, 0.0}, .end = {1.0, 0.0, 0.0}}, box).has_value()
    ); // on a face
}
