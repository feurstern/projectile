// PrimitiveWorld: the library's collision world made of planes, spheres, boxes and triangles.

#include "projectile/collision/primitive_world.hpp"
#include "projectile/collision/raycast.hpp"
#include "projectile/collision/sweep.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/ballistics_support.hpp"
#include "support/collision_matchers.hpp"
#include "support/random.hpp"

#include <cstdint>
#include <limits>
#include <optional>

using namespace projectile;
using projectile::test::require_value;

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();

constexpr Ray along_x{.origin = {0.0, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}};

/// A wall of each kind across the x axis: a triangle at x = 2, a box from x = 4, a sphere from
/// x = 6 and a plane at x = 8, made of materials 1 to 4.
PrimitiveWorld row_of_walls() {
    PrimitiveWorld world;
    REQUIRE_FALSE(world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -8.0}, 4));
    REQUIRE_FALSE(world.add(Sphere{.center = {7.0, 0.0, 0.0}, .radius = 1.0}, 3));
    REQUIRE_FALSE(world.add(AABB{.min = {4.0, -1.0, -1.0}, .max = {5.0, 1.0, 1.0}}, 2));
    REQUIRE_FALSE(
        world.add(Triangle{.a = {2.0, -1.0, -1.0}, .b = {2.0, 1.0, -1.0}, .c = {2.0, 0.0, 2.0}}, 1)
    );
    return world;
}

} // namespace

TEST_CASE("an empty world is never hit", "[collision][world]") {
    const PrimitiveWorld world;
    CHECK(world.empty());
    CHECK_FALSE(raycast(world, along_x, infinity));
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{100.0, 0.0, 0.0}, 1.0));
}

TEST_CASE(
    "a world reports the nearest shape of any kind, with its material", "[collision][world]"
) {
    const PrimitiveWorld world = row_of_walls();
    CHECK(world.size() == 4);

    const HitResult ray = require_value(raycast(world, along_x, infinity));
    CHECK(ray.distance == 2.0);
    CHECK(ray.material_id == 1);

    const HitResult sweep = require_value(sweep_sphere(world, Vec3{}, Vec3{100.0, 0.0, 0.0}, 0.5));
    CHECK(sweep.distance == 1.5);
    CHECK(sweep.material_id == 1);

    // Starting past each wall in turn finds the next one.
    const Ray from_three{.origin = {3.0, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}};
    CHECK(require_value(raycast(world, from_three, infinity)).material_id == 2);
    const Ray from_five_and_a_half{.origin = {5.5, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}};
    CHECK(require_value(raycast(world, from_five_and_a_half, infinity)).material_id == 3);
    CHECK(
        require_value(sweep_sphere(world, Vec3{8.5, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}, 0.0))
            .material_id == 4
    );

    // A range that ends before the first wall finds nothing.
    CHECK_FALSE(raycast(world, along_x, 1.5));
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{1.4, 0.0, 0.0}, 0.5));
}

TEST_CASE("a world's results are those of the shape queries", "[collision][world][property]") {
    test::Random random;
    for (int i = 0; i < 2'000; ++i) {
        const Sphere sphere{.center = random.vec3(-3.0, 3.0), .radius = random.uniform(0.1, 2.0)};
        const AABB box = require_value(make_aabb(random.vec3(-3.0, 3.0), random.vec3(-3.0, 3.0)));
        PrimitiveWorld world;
        REQUIRE_FALSE(world.add(sphere, 1));
        REQUIRE_FALSE(world.add(box, 2));

        const Vec3 start = random.vec3(-8.0, 8.0);
        const Vec3 end = random.vec3(-8.0, 8.0);
        const double radius = random.uniform(0.0, 1.0);
        const Segment path{.start = start, .end = end};
        std::optional<HitResult> expected = sweep_sphere(path, radius, sphere);
        if (expected) {
            expected->material_id = 1;
        }
        if (std::optional<HitResult> box_hit = sweep_sphere(path, radius, box);
            box_hit && (!expected || box_hit->distance < expected->distance)) {
            box_hit->material_id = 2;
            expected = box_hit;
        }
        CHECK(sweep_sphere(world, start, end, radius) == expected);
    }
}

TEST_CASE("shapes hit at the same distance resolve in a fixed order", "[collision][world]") {
    // A box face and a plane at x = 4: the plane, which comes first, wins.
    PrimitiveWorld world;
    REQUIRE_FALSE(world.add(AABB{.min = {4.0, -1.0, -1.0}, .max = {5.0, 1.0, 1.0}}, 2));
    REQUIRE_FALSE(world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -4.0}, 4));
    REQUIRE_FALSE(world.add(Plane{.normal = {1.0, 0.0, 0.0}, .distance = 4.0}, 5));
    CHECK(require_value(raycast(world, along_x, infinity)).material_id == 4);
}

TEST_CASE("a world rejects invalid shapes", "[collision][world]") {
    PrimitiveWorld world;
    CHECK(world.add(Plane{.normal = {0.0, 2.0, 0.0}, .distance = 0.0}) == Error::invalid_shape);
    CHECK(world.add(Sphere{.center = {}, .radius = -1.0}) == Error::invalid_shape);
    CHECK(world.add(AABB{.min = {1.0, 0.0, 0.0}, .max = {0.0, 1.0, 1.0}}) == Error::invalid_shape);
    CHECK(
        world.add(Triangle{.a = {not_a_number, 0.0, 0.0}, .b = {}, .c = {}}) == Error::invalid_shape
    );
    CHECK(world.empty());
}

TEST_CASE("clearing a world removes every shape", "[collision][world]") {
    PrimitiveWorld world = row_of_walls();
    world.clear();
    CHECK(world.empty());
    CHECK_FALSE(raycast(world, along_x, infinity));
}

TEST_CASE("queries through the entry points are validated", "[collision][world]") {
    const PrimitiveWorld world = row_of_walls();
    CHECK_FALSE(sweep_sphere(world, Vec3{not_a_number, 0.0, 0.0}, Vec3{9.0, 0.0, 0.0}, 0.5));
    CHECK_FALSE(sweep_sphere(world, Vec3{}, Vec3{9.0, 0.0, 0.0}, -0.5));
    CHECK_FALSE(raycast(world, Ray{.origin = {}, .direction = {}}, infinity));
}
