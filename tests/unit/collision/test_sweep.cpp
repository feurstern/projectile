// Swept-sphere queries against single shapes: exact cases for each shape, then properties checked
// on random input against the geometry itself.

#include "projectile/collision/intersection.hpp"
#include "projectile/collision/sweep.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/collision_matchers.hpp"
#include "support/random.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr Segment path(const Vec3& start, const Vec3& end) {
    return Segment{.start = start, .end = end};
}

// --- Distances from a point to each shape, from the closest-point functions -----------------

double distance_to(const Plane& plane, const Vec3& point) {
    return std::abs(signed_distance(plane, point));
}

double distance_to(const Sphere& sphere, const Vec3& point) {
    return (std::max)(distance(point, sphere.center) - sphere.radius, 0.0);
}

double distance_to(const AABB& box, const Vec3& point) {
    return distance(point, closest_point(box, point));
}

double distance_to(const Triangle& triangle, const Vec3& point) {
    return distance(point, closest_point(triangle, point));
}

/// The twelve triangles of the surface of @p box, facing outwards.
std::array<Triangle, 12> surface_of(const AABB& box) {
    const auto corner = [&box](unsigned bits) {
        return Vec3{
            (bits & 1U) != 0U ? box.max.x : box.min.x,
            (bits & 2U) != 0U ? box.max.y : box.min.y,
            (bits & 4U) != 0U ? box.max.z : box.min.z,
        };
    };
    // Each face as four corners, counter-clockwise seen from outside.
    constexpr std::array<std::array<unsigned, 4>, 6> faces{{
        {0U, 4U, 6U, 2U}, // -x
        {1U, 3U, 7U, 5U}, // +x
        {0U, 1U, 5U, 4U}, // -y
        {2U, 6U, 7U, 3U}, // +y
        {0U, 2U, 3U, 1U}, // -z
        {4U, 5U, 7U, 6U}, // +z
    }};
    std::array<Triangle, 12> triangles{};
    std::size_t next = 0;
    for (const auto& face : faces) {
        const Vec3 a = corner(face.at(0));
        const Vec3 b = corner(face.at(1));
        const Vec3 c = corner(face.at(2));
        const Vec3 d = corner(face.at(3));
        triangles.at(next++) = Triangle{.a = a, .b = b, .c = c};
        triangles.at(next++) = Triangle{.a = a, .b = c, .c = d};
    }
    return triangles;
}

} // namespace

// --- Planes -------------------------------------------------------------------------------------

TEST_CASE("a sphere sweeping onto a plane stops one radius above it", "[collision][sweep]") {
    const Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    const HitResult hit =
        require_value(sweep_sphere(path({1.0, 3.0, 2.0}, {1.0, -3.0, 2.0}), 0.5, ground));
    CHECK(hit.distance == 2.5);
    CHECK(hit.point == Vec3{1.0, 0.0, 2.0});
    CHECK(hit.normal == Vec3{0.0, 1.0, 0.0});
    CHECK(hit.front_face);

    // From behind: the back face, with the normal facing the sphere.
    const HitResult behind =
        require_value(sweep_sphere(path({1.0, -3.0, 2.0}, {1.0, 3.0, 2.0}), 0.5, ground));
    CHECK(behind.distance == 2.5);
    CHECK(behind.normal == Vec3{0.0, -1.0, 0.0});
    CHECK_FALSE(behind.front_face);
    CHECK_FALSE(
        sweep_sphere(path({1.0, -3.0, 2.0}, {1.0, 3.0, 2.0}), 0.5, ground, Culling::back_faces)
    );
}

TEST_CASE("a sphere overlapping a plane hits where it stops overlapping", "[collision][sweep]") {
    const Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    // Moving up from 0.25 above the plane, the sphere of radius 0.5 clears it at 0.5.
    const HitResult up =
        require_value(sweep_sphere(path({0.0, 0.25, 0.0}, {0.0, 2.25, 0.0}), 0.5, ground));
    CHECK(up.distance == 0.25);
    CHECK(up.point == Vec3{});
    CHECK(up.normal == Vec3{0.0, -1.0, 0.0}); // faces against the motion
    CHECK_FALSE(up.front_face);
    // Moving down, it clears the plane on the other side.
    const HitResult down =
        require_value(sweep_sphere(path({0.0, 0.25, 0.0}, {0.0, -1.75, 0.0}), 0.5, ground));
    CHECK(down.distance == 0.75);
    CHECK(down.normal == Vec3{0.0, 1.0, 0.0});
    CHECK_FALSE(down.front_face);
    // Culling back faces ignores both, and a path too short to clear the plane hits nothing.
    CHECK_FALSE(
        sweep_sphere(path({0.0, 0.25, 0.0}, {0.0, 2.25, 0.0}), 0.5, ground, Culling::back_faces)
    );
    CHECK_FALSE(sweep_sphere(path({0.0, 0.25, 0.0}, {0.0, 0.5 - 1e-9, 0.0}), 0.5, ground));
}

TEST_CASE("a sphere touching a plane hits it only when moving into it", "[collision][sweep]") {
    const Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    const HitResult into =
        require_value(sweep_sphere(path({0.0, 0.5, 0.0}, {1.0, -1.0, 0.0}), 0.5, ground));
    CHECK(into.distance == 0.0);
    CHECK(into.front_face);
    CHECK_FALSE(sweep_sphere(path({0.0, 0.5, 0.0}, {0.0, 3.0, 0.0}), 0.5, ground)); // away
    CHECK_FALSE(sweep_sphere(path({0.0, 0.5, 0.0}, {9.0, 0.5, 0.0}), 0.5, ground)); // sliding
}

TEST_CASE("a plane sweep ending exactly at the contact hits it", "[collision][sweep]") {
    const Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    const HitResult hit =
        require_value(sweep_sphere(path({0.0, 3.0, 0.0}, {0.0, 0.5, 0.0}), 0.5, ground));
    CHECK(hit.distance == 2.5);
    CHECK_FALSE(sweep_sphere(path({0.0, 3.0, 0.0}, {0.0, 0.5000001, 0.0}), 0.5, ground));
}

// --- Spheres ------------------------------------------------------------------------------------

TEST_CASE("a sphere sweeping onto a sphere stops when their surfaces touch", "[collision][sweep]") {
    const Sphere target{.center = {5.0, 0.0, 0.0}, .radius = 1.0};
    const HitResult hit =
        require_value(sweep_sphere(path({0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}), 0.5, target));
    CHECK(hit.distance == 3.5);
    CHECK(hit.point == Vec3{4.0, 0.0, 0.0});
    CHECK(hit.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK(hit.front_face);

    // Grazing: the sphere passes at exactly the sum of the radii.
    const HitResult graze =
        require_value(sweep_sphere(path({0.0, 1.5, 0.0}, {10.0, 1.5, 0.0}), 0.5, target));
    CHECK(graze.distance == 5.0);
    CHECK(graze.point == Vec3{5.0, 1.0, 0.0});
    CHECK(graze.normal == Vec3{0.0, 1.0, 0.0});
    CHECK_FALSE(sweep_sphere(path({0.0, 1.6, 0.0}, {10.0, 1.6, 0.0}), 0.5, target));

    // A sphere of radius zero is a point.
    const Sphere point{.center = {5.0, 0.0, 0.0}, .radius = 0.0};
    const HitResult at_point =
        require_value(sweep_sphere(path({0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}), 0.5, point));
    CHECK(at_point.distance == 4.5);
    CHECK(at_point.point == point.center);
}

TEST_CASE("a sphere overlapping a sphere hits where it leaves it", "[collision][sweep]") {
    const Sphere target{.center = {0.0, 0.0, 0.0}, .radius = 1.0};
    const HitResult hit =
        require_value(sweep_sphere(path({0.0, 0.0, 0.0}, {5.0, 0.0, 0.0}), 0.5, target));
    CHECK(hit.distance == 1.5);
    CHECK(hit.point == Vec3{1.0, 0.0, 0.0});
    CHECK(hit.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK_FALSE(hit.front_face);
    CHECK_FALSE(
        sweep_sphere(path({0.0, 0.0, 0.0}, {5.0, 0.0, 0.0}), 0.5, target, Culling::back_faces)
    );
}

// --- Boxes --------------------------------------------------------------------------------------

TEST_CASE("a sphere sweeping onto a box face stops one radius from it", "[collision][sweep]") {
    const AABB box{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
    const HitResult hit =
        require_value(sweep_sphere(path({-5.0, 0.25, 0.5}, {5.0, 0.25, 0.5}), 0.5, box));
    CHECK(hit.distance == 3.5);
    CHECK(hit.point == Vec3{-1.0, 0.25, 0.5});
    CHECK(hit.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK(hit.front_face);
}

TEST_CASE("a box's edges and corners are rounded by the radius", "[collision][sweep]") {
    const AABB box{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
    const double r = 0.5;

    SECTION("towards an edge") {
        // Moving diagonally onto the edge along x at y = z = 1: the sphere touches the edge, not
        // the corner of the box grown by the radius, which it would reach sooner.
        const HitResult hit =
            require_value(sweep_sphere(path({0.0, 3.0, 3.0}, {0.0, -3.0, -3.0}), r, box));
        const double reach = r / std::numbers::sqrt2; // per axis, from the edge to the center
        CHECK_THAT(hit.distance, WithinAbs(std::numbers::sqrt2 * (2.0 - reach), 1e-12));
        CHECK_THAT(hit.point, is_near(Vec3{0.0, 1.0, 1.0}, 1e-12));
        CHECK_THAT(hit.normal, is_near(Vec3{0.0, 1.0, 1.0} / std::numbers::sqrt2, 1e-12));
    }
    SECTION("towards a corner") {
        const HitResult hit =
            require_value(sweep_sphere(path({3.0, 3.0, 3.0}, {0.0, 0.0, 0.0}), r, box));
        CHECK_THAT(hit.distance, WithinAbs(2.0 * std::numbers::sqrt3 - r, 1e-12));
        CHECK_THAT(hit.point, is_near(Vec3{1.0, 1.0, 1.0}, 1e-12));
        CHECK_THAT(hit.normal, is_near(Vec3{1.0, 1.0, 1.0} / std::numbers::sqrt3, 1e-12));
    }
    SECTION("past an edge") {
        // 0.4 from the faces on two axes is 0.57 from the edge: inside the grown box, but clear
        // of the rounded one.
        CHECK_FALSE(sweep_sphere(path({-5.0, 1.4, 1.4}, {5.0, 1.4, 1.4}), r, box));
        // 0.3 from the faces is 0.42 from the edge: the sphere touches the corner region first.
        const HitResult hit =
            require_value(sweep_sphere(path({-5.0, 1.3, 1.3}, {5.0, 1.3, 1.3}), r, box));
        const double overhang = std::sqrt(r * r - 0.18); // beyond the corner, along x
        CHECK_THAT(hit.distance, WithinAbs(4.0 - overhang, 1e-12));
        CHECK_THAT(hit.point, is_near(Vec3{-1.0, 1.0, 1.0}, 1e-12));
    }
}

TEST_CASE("a sphere inside a box hits where it leaves it", "[collision][sweep]") {
    const AABB box{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
    const HitResult hit = require_value(sweep_sphere(path({}, {5.0, 0.0, 0.0}), 0.5, box));
    CHECK(hit.distance == 1.5);
    CHECK(hit.point == Vec3{1.0, 0.0, 0.0});
    CHECK(hit.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK_FALSE(hit.front_face);
    CHECK_FALSE(sweep_sphere(path({}, {5.0, 0.0, 0.0}), 0.5, box, Culling::back_faces));
}

TEST_CASE("a sphere touching a box hits it only when moving into it", "[collision][sweep]") {
    const AABB box{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
    const HitResult into =
        require_value(sweep_sphere(path({-1.5, 0.0, 0.0}, {0.0, 0.0, 0.0}), 0.5, box));
    CHECK(into.distance == 0.0);
    CHECK(into.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK(into.front_face);
    CHECK_FALSE(sweep_sphere(path({-1.5, 0.0, 0.0}, {-4.0, 0.0, 0.0}), 0.5, box)); // away
    // Sliding along the face, at exactly the radius from it, then past the rounded edge.
    CHECK_FALSE(sweep_sphere(path({-1.5, -0.5, 0.0}, {-1.5, 3.0, 0.0}), 0.5, box));

    // Arriving from outside to slide along the face is grazing, which counts as a hit, as for
    // a ray along a box's face.
    const HitResult graze =
        require_value(sweep_sphere(path({-1.5, -3.0, 0.0}, {-1.5, 3.0, 0.0}), 0.5, box));
    CHECK(graze.distance == 2.0);
    CHECK(graze.point == Vec3{-1.0, -1.0, 0.0});
    CHECK(graze.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK(graze.front_face);
}

TEST_CASE("flat boxes are hit like rectangles with rounded rims", "[collision][sweep]") {
    const AABB flat{.min = {0.0, 0.0, 0.0}, .max = {2.0, 2.0, 0.0}};
    const HitResult face =
        require_value(sweep_sphere(path({1.0, 1.0, 3.0}, {1.0, 1.0, -3.0}), 0.5, flat));
    CHECK(face.distance == 2.5);
    CHECK(face.normal == Vec3{0.0, 0.0, 1.0});
    // In the rectangle's plane, towards its edge.
    const HitResult rim =
        require_value(sweep_sphere(path({-3.0, 1.0, 0.0}, {3.0, 1.0, 0.0}), 0.5, flat));
    CHECK(rim.distance == 2.5);
    CHECK(rim.normal == Vec3{-1.0, 0.0, 0.0});
}

// --- Triangles ----------------------------------------------------------------------------------

TEST_CASE(
    "a sphere sweeping onto a triangle's face stops one radius from it", "[collision][sweep]"
) {
    const Triangle triangle{.a = {0.0, 0.0, 0.0}, .b = {2.0, 0.0, 0.0}, .c = {0.0, 2.0, 0.0}};
    const HitResult front =
        require_value(sweep_sphere(path({0.5, 0.5, 3.0}, {0.5, 0.5, -3.0}), 0.5, triangle));
    CHECK(front.distance == 2.5);
    CHECK_THAT(front.point, is_near(Vec3{0.5, 0.5, 0.0}, 1e-15));
    CHECK_THAT(front.normal, is_near(Vec3{0.0, 0.0, 1.0}, 1e-15));
    CHECK(front.front_face);

    const HitResult back =
        require_value(sweep_sphere(path({0.5, 0.5, -3.0}, {0.5, 0.5, 3.0}), 0.5, triangle));
    CHECK(back.distance == 2.5);
    CHECK_THAT(back.normal, is_near(Vec3{0.0, 0.0, -1.0}, 1e-15));
    CHECK_FALSE(back.front_face);
    CHECK_FALSE(
        sweep_sphere(path({0.5, 0.5, -3.0}, {0.5, 0.5, 3.0}), 0.5, triangle, Culling::back_faces)
    );
}

TEST_CASE("a triangle's edges and vertices are rounded by the radius", "[collision][sweep]") {
    const Triangle triangle{.a = {0.0, 0.0, 0.0}, .b = {2.0, 0.0, 0.0}, .c = {0.0, 2.0, 0.0}};

    // In the triangle's plane, towards edge ab.
    const HitResult edge =
        require_value(sweep_sphere(path({1.0, -3.0, 0.0}, {1.0, 3.0, 0.0}), 0.5, triangle));
    CHECK(edge.distance == 2.5);
    CHECK(edge.point == Vec3{1.0, 0.0, 0.0});
    CHECK(edge.normal == Vec3{0.0, -1.0, 0.0});
    CHECK(edge.front_face); // the center is not behind the plane

    // Towards vertex a, along the diagonal.
    const HitResult vertex =
        require_value(sweep_sphere(path({-3.0, -3.0, 0.0}, {1.0, 1.0, 0.0}), 0.5, triangle));
    CHECK_THAT(vertex.distance, WithinAbs(3.0 * std::numbers::sqrt2 - 0.5, 1e-12));
    CHECK(vertex.point == triangle.a);
    CHECK_THAT(vertex.normal, is_near(Vec3{-1.0, -1.0, 0.0} / std::numbers::sqrt2, 1e-12));

    // Passing 0.4 above the plane, the sphere meets the rounded edge ac before the face.
    const HitResult over =
        require_value(sweep_sphere(path({-3.0, 1.0, 0.4}, {3.0, 1.0, 0.4}), 0.5, triangle));
    CHECK_THAT(over.distance, WithinAbs(2.7, 1e-12));
    CHECK_THAT(over.point, is_near(Vec3{0.0, 1.0, 0.0}, 1e-12));
    CHECK_THAT(over.normal, is_near(Vec3{-0.6, 0.0, 0.8}, 1e-12));
    CHECK(over.front_face);

    // Passing 0.6 above it, the sphere misses.
    CHECK_FALSE(sweep_sphere(path({-3.0, 1.0, 0.6}, {3.0, 1.0, 0.6}), 0.5, triangle));
}

TEST_CASE("triangles of zero area are never hit by sweeps", "[collision][sweep]") {
    const Triangle collinear{.a = {0.0, 0.0, 0.0}, .b = {1.0, 0.0, 0.0}, .c = {2.0, 0.0, 0.0}};
    CHECK_FALSE(sweep_sphere(path({1.0, -3.0, 0.0}, {1.0, 3.0, 0.0}), 0.5, collinear));
}

// --- Common rules -------------------------------------------------------------------------------

TEST_CASE("paths of zero length never hit", "[collision][sweep]") {
    const Vec3 inside{0.1, 0.1, 0.1};
    const Segment still = path(inside, inside);
    CHECK_FALSE(sweep_sphere(still, 0.5, Plane{.normal = {0.0, 0.0, 1.0}, .distance = 0.0}));
    CHECK_FALSE(sweep_sphere(still, 0.5, Sphere{.center = {}, .radius = 1.0}));
    CHECK_FALSE(sweep_sphere(still, 0.5, AABB{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}}));
    CHECK_FALSE(
        sweep_sphere(still, 0.5, Triangle{.a = {}, .b = {1.0, 0.0, 0.0}, .c = {0.0, 1.0, 0.0}})
    );
}

TEST_CASE("contact normals have no components of negative zero", "[collision][sweep]") {
    // Negating a normal such as {0, 1, 0} gives {-0, -1, -0}, which compares equal but prints as
    // "-0". Results never carry those.
    const auto negative_zero = [](double value) { return value == 0.0 && std::signbit(value); };
    const auto no_negative_zero = [&negative_zero](const Vec3& normal) {
        return !negative_zero(normal.x) && !negative_zero(normal.y) && !negative_zero(normal.z);
    };
    const Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    const AABB box{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
    const Segment upwards = path({0.0, -3.0, 0.0}, {0.0, 3.0, 0.0});
    const Segment eastwards = path({-3.0, 0.0, 0.0}, {3.0, 0.0, 0.0});
    const Ray ray{.origin = {-3.0, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}};
    CHECK(no_negative_zero(require_value(intersect(upwards, ground)).normal));
    CHECK(no_negative_zero(require_value(intersect(ray, box)).normal));
    CHECK(no_negative_zero(
        require_value(intersect(Ray{.origin = {}, .direction = {1.0, 0.0, 0.0}}, box)).normal
    ));
    CHECK(no_negative_zero(require_value(sweep_sphere(upwards, 0.5, ground)).normal));
    CHECK(no_negative_zero(require_value(sweep_sphere(eastwards, 0.5, box)).normal));
    CHECK(
        no_negative_zero(require_value(sweep_sphere(path({}, {3.0, 0.0, 0.0}), 0.5, box)).normal)
    );
}

TEST_CASE("a sweep with radius zero is a segment query", "[collision][sweep][property]") {
    test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Segment segment = path(random.vec3(-10.0, 10.0), random.vec3(-10.0, 10.0));
        const Plane plane = require_value(make_plane(random.unit_vec3(), random.vec3(-5.0, 5.0)));
        const Sphere sphere{.center = random.vec3(-5.0, 5.0), .radius = random.uniform(0.0, 3.0)};
        const AABB box = require_value(make_aabb(random.vec3(-5.0, 5.0), random.vec3(-5.0, 5.0)));
        const Triangle triangle{
            .a = random.vec3(-5.0, 5.0), .b = random.vec3(-5.0, 5.0), .c = random.vec3(-5.0, 5.0)
        };
        CHECK(sweep_sphere(segment, 0.0, plane) == intersect(segment, plane));
        CHECK(sweep_sphere(segment, 0.0, sphere) == intersect(segment, sphere));
        CHECK(sweep_sphere(segment, 0.0, box) == intersect(segment, box));
        CHECK(sweep_sphere(segment, 0.0, triangle) == intersect(segment, triangle));
    }
}

namespace {

/// Checks that @p hit, the result of sweeping a sphere of @p radius along @p segment against
/// @p shape, describes the geometry: the sphere touches the shape at the contact and not before
/// (or, if it overlapped the shape at the start, overlaps it until the contact), and a miss means
/// that it never touches the shape.
template <typename Shape>
void check_sweep(
    const Segment& segment, double radius, const Shape& shape, const std::optional<HitResult>& hit
) {
    constexpr double tolerance = 1e-9;
    const double path_length = length(segment);
    const bool overlapping = distance_to(shape, segment.start) < radius;
    CAPTURE(shape, segment.start, segment.end, radius, overlapping);
    INFO("hit: " << (hit ? Catch::StringMaker<HitResult>::convert(*hit) : "none"));

    const double end = hit ? hit->distance / path_length : 1.0;
    constexpr int samples = 32;
    for (int k = 0; k < samples; ++k) {
        const double fraction = end * k / samples;
        const double gap = distance_to(shape, lerp(segment.start, segment.end, fraction));
        CAPTURE(fraction, gap);
        if (overlapping) {
            CHECK(gap <= radius + tolerance);
        } else {
            CHECK(gap >= radius - tolerance);
        }
    }
    if (!hit) {
        if (!overlapping) {
            CHECK(distance_to(shape, segment.end) >= radius - tolerance);
        }
        return;
    }
    const Vec3 center = lerp(segment.start, segment.end, end);
    CHECK_THAT(distance_to(shape, center), WithinAbs(radius, tolerance));
    CHECK_THAT(distance_to(shape, hit->point), WithinAbs(0.0, tolerance));
    CHECK(is_normalized(hit->normal));
    // The normal points from the contact to the center, or the other way for a sphere leaving
    // the shape, and faces against the motion.
    const Vec3 outward = (center - hit->point) / radius;
    CHECK_THAT(hit->normal, is_near(overlapping ? -outward : outward, 1e-6, 0.0));
    CHECK(dot(hit->normal, segment.end - segment.start) <= tolerance);
}

} // namespace

TEST_CASE(
    "sweeps report exactly where a moving sphere first touches a shape",
    "[collision][sweep][property]"
) {
    test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        // Paths are aimed at the shapes' neighbourhood, so that most of them hit.
        const Vec3 start = random.vec3(-10.0, 10.0);
        const Segment segment = path(start, random.vec3(-3.0, 3.0) * 2.0 - start * 0.2);
        const double radius = random.uniform(0.01, 2.0);

        const Plane plane = require_value(make_plane(random.unit_vec3(), random.vec3(-3.0, 3.0)));
        check_sweep(segment, radius, plane, sweep_sphere(segment, radius, plane));
        const Sphere sphere{.center = random.vec3(-3.0, 3.0), .radius = random.uniform(0.0, 2.0)};
        check_sweep(segment, radius, sphere, sweep_sphere(segment, radius, sphere));
        // One box in eight is flat on an axis.
        Vec3 extent = random.vec3(0.0, 2.0);
        if (random.integer(0, 7) == 0) {
            extent.y = 0.0;
        }
        const Vec3 middle = random.vec3(-3.0, 3.0);
        const AABB box{.min = middle - extent, .max = middle + extent};
        check_sweep(segment, radius, box, sweep_sphere(segment, radius, box));
        const Triangle triangle{
            .a = random.vec3(-4.0, 4.0), .b = random.vec3(-4.0, 4.0), .c = random.vec3(-4.0, 4.0)
        };
        if (length_squared(cross(triangle.b - triangle.a, triangle.c - triangle.a)) > 1e-6) {
            check_sweep(segment, radius, triangle, sweep_sphere(segment, radius, triangle));
        }
    }
}

TEST_CASE("a box is hit where its surface triangles are hit", "[collision][sweep][property]") {
    // Two independent computations of the same contact: the rounded box, and the rounded
    // triangles that make up its surface. From outside, the first contact is the same.
    test::Random random;
    int compared = 0;
    for (int i = 0; i < 10'000; ++i) {
        const Vec3 middle = random.vec3(-3.0, 3.0);
        const Vec3 extent = random.vec3(0.1, 2.0);
        const AABB box{.min = middle - extent, .max = middle + extent};
        const double radius = random.uniform(0.01, 2.0);
        const Vec3 start = random.vec3(-10.0, 10.0);
        if (distance_to(box, start) <= radius) {
            continue; // the triangles describe the surface only, not the inside
        }
        const Segment segment = path(start, random.vec3(-4.0, 4.0));

        const std::optional<HitResult> box_hit = sweep_sphere(segment, radius, box);
        std::optional<HitResult> surface_hit;
        for (const Triangle& triangle : surface_of(box)) {
            const std::optional<HitResult> hit = sweep_sphere(segment, radius, triangle);
            if (hit && (!surface_hit || hit->distance < surface_hit->distance)) {
                surface_hit = hit;
            }
        }
        CAPTURE(box, segment.start, segment.end, radius);
        INFO("box hit: " << (box_hit ? Catch::StringMaker<HitResult>::convert(*box_hit) : "none"));
        INFO(
            "surface hit: "
            << (surface_hit ? Catch::StringMaker<HitResult>::convert(*surface_hit) : "none")
        );
        if (box_hit && surface_hit) {
            ++compared;
            CHECK_THAT(box_hit->distance, WithinAbs(surface_hit->distance, 1e-9));
            CHECK_THAT(box_hit->point, is_near(surface_hit->point, 1e-6, 0.0));
            CHECK(box_hit->front_face);
        } else if (box_hit || surface_hit) {
            // Only a sphere that just grazes the shape may be found by one and not the other.
            const HitResult& only = box_hit ? *box_hit : *surface_hit;
            const Vec3 direction = require_value(try_normalize(segment.end - segment.start));
            CHECK(std::abs(dot(only.normal, direction)) < 1e-6);
        }
    }
    CHECK(compared > 1'000);
}

TEST_CASE(
    "culling back faces removes exactly the back-face contacts", "[collision][sweep][property]"
) {
    test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Segment segment = path(random.vec3(-6.0, 6.0), random.vec3(-6.0, 6.0));
        const double radius = random.uniform(0.01, 2.0);
        const Plane plane = require_value(make_plane(random.unit_vec3(), random.vec3(-2.0, 2.0)));
        const AABB box = require_value(make_aabb(random.vec3(-3.0, 3.0), random.vec3(-3.0, 3.0)));
        const Triangle triangle{
            .a = random.vec3(-4.0, 4.0), .b = random.vec3(-4.0, 4.0), .c = random.vec3(-4.0, 4.0)
        };
        const auto check_culling = [&](const auto& shape) {
            const std::optional<HitResult> both = sweep_sphere(segment, radius, shape);
            const std::optional<HitResult> front =
                sweep_sphere(segment, radius, shape, Culling::back_faces);
            if (both && both->front_face) {
                CHECK(front == both);
            } else {
                CHECK_FALSE(front);
            }
        };
        check_culling(plane);
        check_culling(Sphere{.center = random.vec3(-3.0, 3.0), .radius = random.uniform(0.0, 2.0)});
        check_culling(box);
        check_culling(triangle);
    }
}
