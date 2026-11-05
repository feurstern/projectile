// The collision-world abstraction: the validated entry points raycast() and sweep_sphere(),
// and an application-side world implemented on top of the intersection functions.

#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/intersection.hpp"
#include "projectile/collision/raycast.hpp"
#include "projectile/collision/sweep.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/collision_matchers.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

/// Records the queries it receives and answers each with `answer`.
class RecordingWorld final : public ICollisionWorld {
public:
    std::optional<HitResult> answer;
    mutable int raycast_calls = 0;
    mutable int sweep_calls = 0;
    mutable Ray last_ray;
    mutable double last_max_distance = 0.0;
    mutable Vec3 last_start;
    mutable Vec3 last_end;
    mutable double last_radius = 0.0;

    [[nodiscard]] std::optional<HitResult> raycast(
        const Ray& ray, double max_distance
    ) const override {
        ++raycast_calls;
        last_ray = ray;
        last_max_distance = max_distance;
        return answer;
    }

    [[nodiscard]] std::optional<HitResult> sweep_sphere(
        const Vec3& start, const Vec3& end, double radius
    ) const override {
        ++sweep_calls;
        last_start = start;
        last_end = end;
        last_radius = radius;
        return answer;
    }
};

/// A world of spheres with materials, implemented on top of the intersection functions: the
/// example in docs/collision.md.
class SphereWorld final : public ICollisionWorld {
public:
    struct Body {
        Sphere sphere;
        std::uint32_t material_id = 0;
    };

    explicit SphereWorld(std::vector<Body> bodies) : bodies_(std::move(bodies)) {}

    [[nodiscard]] std::optional<HitResult> raycast(
        const Ray& ray, double max_distance
    ) const override {
        std::optional<HitResult> nearest;
        for (const Body& body : bodies_) {
            std::optional<HitResult> hit = intersect(ray, body.sphere, max_distance);
            if (hit && (!nearest || hit->distance < nearest->distance)) {
                hit->material_id = body.material_id;
                nearest = hit;
            }
        }
        return nearest;
    }

    [[nodiscard]] std::optional<HitResult> sweep_sphere(
        const Vec3& start, const Vec3& end, double radius
    ) const override {
        // A sphere of radius r touches a sphere of radius R when their centers are R + r apart,
        // so sweeping it is a segment query of its center against the enlarged sphere.
        std::optional<HitResult> nearest;
        for (const Body& body : bodies_) {
            const Sphere enlarged{
                .center = body.sphere.center, .radius = body.sphere.radius + radius
            };
            std::optional<HitResult> hit = intersect(Segment{.start = start, .end = end}, enlarged);
            if (hit && (!nearest || hit->distance < nearest->distance)) {
                // The segment query found the center at the contact; move to the body's surface.
                const Vec3 outward = hit->front_face ? hit->normal : -hit->normal;
                hit->point -= outward * radius;
                hit->material_id = body.material_id;
                nearest = hit;
            }
        }
        return nearest;
    }

private:
    std::vector<Body> bodies_;
};

constexpr Ray valid_ray{.origin = {1.0, 2.0, 3.0}, .direction = {0.0, 0.0, -1.0}};
constexpr HitResult sample_hit{
    .distance = 4.0,
    .point = {1.0, 2.0, -1.0},
    .normal = {0.0, 0.0, 1.0},
    .material_id = 7,
    .front_face = true,
};

} // namespace

// --- raycast() --------------------------------------------------------------------------------

TEST_CASE("raycast forwards a valid query and returns the world's answer", "[collision][world]") {
    RecordingWorld world;
    world.answer = sample_hit;
    CHECK(raycast(world, valid_ray, 10.0) == sample_hit);
    CHECK(world.raycast_calls == 1);
    CHECK(world.last_ray == valid_ray);
    CHECK(world.last_max_distance == 10.0);
}

TEST_CASE("raycast accepts zero and infinite distances", "[collision][world]") {
    const RecordingWorld world;
    CHECK_FALSE(raycast(world, valid_ray, 0.0).has_value());
    CHECK_FALSE(raycast(world, valid_ray, infinity).has_value());
    CHECK(world.raycast_calls == 2);
    CHECK(world.last_max_distance == infinity);
}

TEST_CASE("raycast rejects invalid queries without querying the world", "[collision][world]") {
    RecordingWorld world;
    world.answer = sample_hit;
    // Invalid rays: no direction, a direction of the wrong length, non-finite values.
    CHECK_FALSE(raycast(world, Ray{.origin = {}, .direction = {}}, 10.0).has_value());
    CHECK_FALSE(raycast(world, Ray{.origin = {}, .direction = {2.0, 0.0, 0.0}}, 10.0).has_value());
    CHECK_FALSE(
        raycast(world, Ray{.origin = {not_a_number, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}}, 10.0)
            .has_value()
    );
    CHECK_FALSE(
        raycast(world, Ray{.origin = {}, .direction = {infinity, 0.0, 0.0}}, 10.0).has_value()
    );
    // Invalid distances.
    CHECK_FALSE(raycast(world, valid_ray, -1.0).has_value());
    CHECK_FALSE(raycast(world, valid_ray, not_a_number).has_value());
    CHECK(world.raycast_calls == 0);
}

// --- sweep_sphere() ---------------------------------------------------------------------------

TEST_CASE(
    "sweep_sphere forwards a valid query and returns the world's answer", "[collision][world]"
) {
    RecordingWorld world;
    world.answer = sample_hit;
    CHECK(sweep_sphere(world, Vec3{1.0, 2.0, 3.0}, Vec3{4.0, 5.0, 6.0}, 0.25) == sample_hit);
    CHECK(world.sweep_calls == 1);
    CHECK(world.last_start == Vec3{1.0, 2.0, 3.0});
    CHECK(world.last_end == Vec3{4.0, 5.0, 6.0});
    CHECK(world.last_radius == 0.25);

    CHECK(sweep_sphere(world, Vec3{}, Vec3{1.0, 0.0, 0.0}, 0.0) == sample_hit); // radius zero
    CHECK(world.sweep_calls == 2);
}

TEST_CASE("sweep_sphere rejects invalid queries without querying the world", "[collision][world]") {
    RecordingWorld world;
    world.answer = sample_hit;
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{1.0, 0.0, 0.0}, -0.5).has_value());
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{1.0, 0.0, 0.0}, not_a_number).has_value());
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{1.0, 0.0, 0.0}, infinity).has_value());
    CHECK_FALSE(
        sweep_sphere(world, Vec3{not_a_number, 0.0, 0.0}, Vec3{1.0, 0.0, 0.0}, 0.5).has_value()
    );
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{0.0, infinity, 0.0}, 0.5).has_value());
    CHECK(world.sweep_calls == 0);
}

// --- An application-side world ----------------------------------------------------------------

TEST_CASE("a world reports the nearest body and its material", "[collision][world]") {
    const SphereWorld world({
        {.sphere = {.center = {10.0, 0.0, 0.0}, .radius = 1.0}, .material_id = 1},
        {.sphere = {.center = {5.0, 0.0, 0.0}, .radius = 1.0}, .material_id = 2},
        {.sphere = {.center = {0.0, 5.0, 0.0}, .radius = 1.0}, .material_id = 3},
    });
    const Ray ray{.origin = {}, .direction = {1.0, 0.0, 0.0}};

    const HitResult hit = require_value(raycast(world, ray, 100.0));
    CHECK(hit.distance == 4.0);
    CHECK(hit.point == Vec3{4.0, 0.0, 0.0});
    CHECK(hit.material_id == 2);

    CHECK_FALSE(raycast(world, ray, 3.5).has_value());
}

TEST_CASE("a swept sphere stops where it first touches a body", "[collision][world]") {
    const SphereWorld world({{.sphere = {.center = {}, .radius = 2.0}, .material_id = 4}});

    // A sphere of radius 0.5 touches the body when its center reaches x = -2.5.
    const HitResult hit =
        require_value(sweep_sphere(world, Vec3{-10.0, 0.0, 0.0}, Vec3{10.0, 0.0, 0.0}, 0.5));
    CHECK(hit.distance == 7.5);
    CHECK(hit.point == Vec3{-2.0, 0.0, 0.0}); // on the body's surface
    CHECK(hit.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK(hit.front_face);
    CHECK(hit.material_id == 4);

    // Passing 3 m from the center, a sphere misses with radius 0.5 and hits with radius 1.5.
    CHECK_FALSE(sweep_sphere(world, Vec3{-10.0, 3.0, 0.0}, Vec3{10.0, 3.0, 0.0}, 0.5).has_value());
    CHECK(sweep_sphere(world, Vec3{-10.0, 3.0, 0.0}, Vec3{10.0, 3.0, 0.0}, 1.5).has_value());
}

TEST_CASE("a swept sphere of radius zero is a segment query", "[collision][world]") {
    const Sphere body{.center = {1.0, 2.0, 0.0}, .radius = 1.5};
    const SphereWorld world({{.sphere = body, .material_id = 9}});
    const Segment path{.start = {-8.0, 1.0, 0.5}, .end = {7.0, 2.5, -0.5}};

    HitResult expected = require_value(intersect(path, body));
    expected.material_id = 9;
    CHECK(sweep_sphere(world, path.start, path.end, 0.0) == expected);
}

TEST_CASE(
    "a swept sphere that starts overlapping a body reports where it gets free", "[collision][world]"
) {
    const SphereWorld world({{.sphere = {.center = {}, .radius = 2.0}, .material_id = 1}});
    // A sphere of radius 1 centered inside the body stops overlapping it when its center is 3 m
    // out.
    const HitResult hit = require_value(sweep_sphere(world, Vec3{}, Vec3{10.0, 0.0, 0.0}, 1.0));
    CHECK_THAT(hit.distance, WithinAbs(3.0, 1e-14));
    CHECK_THAT(hit.point, is_near(Vec3{2.0, 0.0, 0.0}, 1e-14, 0.0));
    CHECK(hit.normal == Vec3{-1.0, 0.0, 0.0});
    CHECK_FALSE(hit.front_face);
}
