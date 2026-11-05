// Properties shared by every intersection query: the conventions of docs/collision.md hold for
// random rays against random shapes, culling only removes back faces, segment queries agree
// with ray queries, and a triangle is hit where its plane is.

#include "projectile/collision/intersection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/collision_matchers.hpp"
#include "support/random.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr int sample_count = 10'000;
// Coordinates stay within about 100 m, where 1e-9 m is several thousand units in the last place.
constexpr double tolerance = 1e-9;

/// A ray from @p origin aimed at a random point within @p spread of @p target.
[[nodiscard]] Ray ray_towards(
    test::Random& random, const Vec3& origin, const Vec3& target, double spread
) {
    return require_value(make_ray(origin, target + random.vec3(-spread, spread) - origin));
}

/// Checks the conventions every hit follows, whatever the shape.
void check_conventions(const HitResult& hit, const Ray& ray, double max_distance) {
    CHECK(hit.distance >= 0.0);
    CHECK(hit.distance <= max_distance);
    CHECK_THAT(hit.point, is_near(point_at(ray, hit.distance), tolerance, 0.0));
    CHECK_THAT(length(hit.normal), WithinAbs(1.0, 1e-12));
    CHECK(dot(hit.normal, ray.direction) <= 1e-12); // the normal faces the query
    CHECK(hit.material_id == 0);
}

/// Culling back faces must remove exactly the back-face hits and nothing else.
void check_culling(
    const std::optional<HitResult>& unculled, const std::optional<HitResult>& culled
) {
    if (unculled && unculled->front_face) {
        CHECK(culled == unculled);
    } else {
        CHECK_FALSE(culled.has_value());
    }
}

/// A segment along @p ray, of length @p length, must find the same contact as the ray limited to
/// @p length. Grazing contacts and contacts at the very end of the segment are excluded: there,
/// rounding may legitimately decide either way.
template <typename Shape>
void check_segment_agrees(const Ray& ray, const Shape& shape, double length) {
    const Segment segment{.start = ray.origin, .end = point_at(ray, length)};
    const std::optional<HitResult> by_ray = intersect(ray, shape, length);
    const std::optional<HitResult> by_segment = intersect(segment, shape);
    const auto well_conditioned = [&](const std::optional<HitResult>& hit) {
        return hit && hit->distance < 0.99 * length &&
               std::abs(dot(hit->normal, ray.direction)) > 1e-3;
    };
    if (well_conditioned(by_ray) || well_conditioned(by_segment)) {
        const HitResult ray_hit = require_value(by_ray);
        const HitResult segment_hit = require_value(by_segment);
        CHECK_THAT(segment_hit.distance, WithinAbs(ray_hit.distance, tolerance));
        CHECK_THAT(segment_hit.point, is_near(ray_hit.point, tolerance, 0.0));
        CHECK_THAT(segment_hit.normal, is_near(ray_hit.normal, 1e-9, 0.0));
        CHECK(segment_hit.front_face == ray_hit.front_face);
    }
}

} // namespace

TEST_CASE("sphere hits follow the query conventions", "[collision][sphere][property]") {
    test::Random random;
    int hits = 0;
    int back_faces = 0;
    for (int i = 0; i < sample_count; ++i) {
        const Sphere sphere{.center = random.vec3(-10.0, 10.0), .radius = random.uniform(0.1, 5.0)};
        // A quarter of the rays start inside the sphere, where the contact is the exit.
        const bool start_inside = random.uniform(0.0, 1.0) < 0.25;
        const Vec3 origin =
            start_inside
                ? sphere.center + random.unit_vec3() * random.uniform(0.0, 0.99 * sphere.radius)
                : random.vec3(-50.0, 50.0);
        // Origins drawn from the whole cube can land inside the sphere too.
        const bool origin_inside =
            length_squared(origin - sphere.center) < sphere.radius * sphere.radius;
        const Ray ray = ray_towards(random, origin, sphere.center, 1.5 * sphere.radius);
        const double max_distance = random.uniform(0.0, 120.0);
        CAPTURE(sphere, ray, max_distance);

        const std::optional<HitResult> hit = intersect(ray, sphere, max_distance);
        check_culling(hit, intersect(ray, sphere, max_distance, Culling::back_faces));
        check_segment_agrees(ray, sphere, max_distance);
        if (!hit) {
            continue;
        }
        ++hits;
        back_faces += hit->front_face ? 0 : 1;
        check_conventions(*hit, ray, max_distance);
        CHECK_THAT(distance(hit->point, sphere.center), WithinAbs(sphere.radius, tolerance));
        const Vec3 outward = (hit->point - sphere.center) / sphere.radius;
        CHECK_THAT(hit->normal, is_near(hit->front_face ? outward : -outward, 1e-9, 0.0));
        CHECK(hit->front_face == !origin_inside);
    }
    CHECK(hits > sample_count / 4);
    CHECK(back_faces > sample_count / 10);
}

TEST_CASE("box hits follow the query conventions", "[collision][aabb][property]") {
    test::Random random;
    int hits = 0;
    int back_faces = 0;
    for (int i = 0; i < sample_count; ++i) {
        const AABB box =
            require_value(make_aabb(random.vec3(-10.0, 10.0), random.vec3(-10.0, 10.0)));
        const bool start_inside = random.uniform(0.0, 1.0) < 0.25;
        const Vec3 origin = start_inside
                                ? Vec3{
                                      random.uniform(box.min.x, box.max.x),
                                      random.uniform(box.min.y, box.max.y),
                                      random.uniform(box.min.z, box.max.z),
                                  }
                                : random.vec3(-50.0, 50.0);
        // Origins drawn from the whole cube can land inside the box too.
        const bool origin_inside = box.min.x < origin.x && origin.x < box.max.x &&
                                   box.min.y < origin.y && origin.y < box.max.y &&
                                   box.min.z < origin.z && origin.z < box.max.z;
        const Ray ray = ray_towards(random, origin, center(box), 1.5 * length(half_extents(box)));
        const double max_distance = random.uniform(0.0, 120.0);
        CAPTURE(box, ray, max_distance);

        const std::optional<HitResult> hit = intersect(ray, box, max_distance);
        check_culling(hit, intersect(ray, box, max_distance, Culling::back_faces));
        check_segment_agrees(ray, box, max_distance);
        if (!hit) {
            continue;
        }
        ++hits;
        back_faces += hit->front_face ? 0 : 1;
        check_conventions(*hit, ray, max_distance);

        // The normal is an axis direction, and the point lies on the face it belongs to.
        const Vec3 outward = hit->front_face ? hit->normal : -hit->normal;
        CHECK(std::abs(outward.x) + std::abs(outward.y) + std::abs(outward.z) == 1.0);
        const Vec3 face = dot(outward, Vec3{1.0, 1.0, 1.0}) > 0.0 ? box.max : box.min;
        CHECK_THAT(dot(hit->point - face, outward), WithinAbs(0.0, tolerance));
        const AABB tolerant{
            .min = box.min - Vec3{tolerance, tolerance, tolerance},
            .max = box.max + Vec3{tolerance, tolerance, tolerance}
        };
        CHECK(contains(tolerant, hit->point));
        CHECK(hit->front_face == !origin_inside);
    }
    CHECK(hits > sample_count / 4);
    CHECK(back_faces > sample_count / 10);
}

TEST_CASE("triangle hits follow the query conventions", "[collision][triangle][property]") {
    test::Random random;
    int hits = 0;
    int back_faces = 0;
    for (int i = 0; i < sample_count; ++i) {
        const Triangle triangle{
            .a = random.vec3(-10.0, 10.0),
            .b = random.vec3(-10.0, 10.0),
            .c = random.vec3(-10.0, 10.0)
        };
        const Vec3 centroid = (triangle.a + triangle.b + triangle.c) / 3.0;
        const Ray ray = ray_towards(random, random.vec3(-50.0, 50.0), centroid, 4.0);
        const double max_distance = random.uniform(0.0, 120.0);
        CAPTURE(triangle, ray, max_distance);

        const std::optional<HitResult> hit = intersect(ray, triangle, max_distance);
        check_culling(hit, intersect(ray, triangle, max_distance, Culling::back_faces));
        check_segment_agrees(ray, triangle, max_distance);
        if (!hit) {
            continue;
        }
        ++hits;
        back_faces += hit->front_face ? 0 : 1;
        check_conventions(*hit, ray, max_distance);

        // On the triangle's plane, on the side the normal says, and inside the edges.
        const Vec3 front = require_value(face_normal(triangle));
        CHECK_THAT(dot(hit->point - triangle.a, front), WithinAbs(0.0, tolerance));
        CHECK_THAT(hit->normal, is_near(hit->front_face ? front : -front, 1e-12, 0.0));
        const Vec3 ab = triangle.b - triangle.a;
        const Vec3 bc = triangle.c - triangle.b;
        const Vec3 ca = triangle.a - triangle.c;
        // The cross products below scale with the square of the longest edge.
        const double scale =
            (std::max)({length_squared(ab), length_squared(bc), length_squared(ca)});
        CHECK(dot(cross(ab, hit->point - triangle.a), front) >= -tolerance * scale);
        CHECK(dot(cross(bc, hit->point - triangle.b), front) >= -tolerance * scale);
        CHECK(dot(cross(ca, hit->point - triangle.c), front) >= -tolerance * scale);

        // The triangle's plane is hit at the same place. Grazing rays are excluded: there the two
        // algorithms' rounding errors grow like epsilon / cos(angle), and may decide the side
        // differently.
        if (std::abs(dot(front, ray.direction)) > 1e-3) {
            const Plane plane =
                require_value(plane_through_points(triangle.a, triangle.b, triangle.c));
            const HitResult plane_hit = require_value(intersect(ray, plane, max_distance + 1.0));
            CHECK_THAT(plane_hit.distance, WithinAbs(hit->distance, tolerance));
            CHECK(plane_hit.front_face == hit->front_face);
        }
    }
    CHECK(hits > sample_count / 10);
    CHECK(back_faces > sample_count / 50);
}

TEST_CASE("plane hits follow the query conventions", "[collision][plane][property]") {
    test::Random random;
    int hits = 0;
    int back_faces = 0;
    for (int i = 0; i < sample_count; ++i) {
        const Plane plane = require_value(make_plane(random.unit_vec3(), random.vec3(-10.0, 10.0)));
        const Ray ray =
            ray_towards(random, random.vec3(-50.0, 50.0), random.vec3(-10.0, 10.0), 0.0);
        const double max_distance = random.uniform(0.0, 120.0);
        CAPTURE(plane, ray, max_distance);

        const std::optional<HitResult> hit = intersect(ray, plane, max_distance);
        check_culling(hit, intersect(ray, plane, max_distance, Culling::back_faces));
        check_segment_agrees(ray, plane, max_distance);
        if (!hit) {
            continue;
        }
        ++hits;
        back_faces += hit->front_face ? 0 : 1;
        check_conventions(*hit, ray, max_distance);
        CHECK_THAT(signed_distance(plane, hit->point), WithinAbs(0.0, tolerance));
        CHECK(hit->normal == (hit->front_face ? plane.normal : -plane.normal));
        CHECK(hit->front_face == (dot(plane.normal, ray.direction) < 0.0));
    }
    CHECK(hits > sample_count / 4);
    CHECK(back_faces > sample_count / 10);
}
