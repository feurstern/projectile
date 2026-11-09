// Unit tests of the raylib adapter's conversions. They use raylib's types only, so they need no
// window and run anywhere.

#include <projectile/raylib.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <optional>

using projectile::AABB;
using projectile::Vec3;

namespace adapter = projectile::raylib;

namespace {

constexpr float float_infinity = std::numeric_limits<float>::infinity();
constexpr float float_nan = std::numeric_limits<float>::quiet_NaN();

/// Whether @p converted is @p original rounded to the nearest float, component by component.
bool rounds_to_nearest_float(const Vec3& original, const Vector3& converted) {
    return converted.x == static_cast<float>(original.x) &&
           converted.y == static_cast<float>(original.y) &&
           converted.z == static_cast<float>(original.z);
}

} // namespace

TEST_CASE("Vectors convert to raylib's single precision and back", "[raylib][conversions]") {
    SECTION("values that a float holds exactly survive the round trip") {
        const Vec3 exact{1.5, -2.25, 1'048'576.0};
        const Vector3 converted = adapter::to_vector3(exact);
        CHECK(converted.x == 1.5F);
        CHECK(converted.y == -2.25F);
        CHECK(converted.z == 1'048'576.0F);
        CHECK(adapter::to_vec3(converted) == exact);
    }

    SECTION("other values round to the nearest float, within float precision") {
        const Vec3 inexact{0.1, 1.0 / 3.0, 1'000.0 + 1e-4};
        const Vector3 converted = adapter::to_vector3(inexact);
        CHECK(rounds_to_nearest_float(inexact, converted));
        const Vec3 back = adapter::to_vec3(converted);
        const auto epsilon = static_cast<double>(std::numeric_limits<float>::epsilon());
        CHECK(std::abs(back.x - inexact.x) <= epsilon * std::abs(inexact.x));
        CHECK(std::abs(back.y - inexact.y) <= epsilon * std::abs(inexact.y));
        CHECK(std::abs(back.z - inexact.z) <= epsilon * std::abs(inexact.z));
    }

    SECTION("widening to double is exact") {
        const Vector3 single{0.1F, -7.3F, 3.0e38F};
        const Vec3 widened = adapter::to_vec3(single);
        CHECK(widened.x == static_cast<double>(0.1F));
        CHECK(widened.y == static_cast<double>(-7.3F));
        CHECK(widened.z == static_cast<double>(3.0e38F));
        CHECK(adapter::to_vector3(widened).x == single.x);
    }

    SECTION("the conversions work at compile time") {
        static_assert(
            adapter::to_vec3(adapter::to_vector3(Vec3{2.0, 4.0, 8.0})) == Vec3{2.0, 4.0, 8.0}
        );
    }
}

TEST_CASE("Rays convert between raylib and Projectile", "[raylib][conversions]") {
    SECTION("a raylib ray becomes a Projectile ray with a unit direction") {
        // Like a ray from GetScreenToWorldRay(), but with an unnormalized direction.
        const ::Ray ray{.position = {1.0F, 2.0F, 3.0F}, .direction = {0.0F, 0.0F, -2.0F}};
        const std::optional<projectile::Ray> converted = adapter::to_ray(ray);
        const projectile::Ray expected{.origin = {1.0, 2.0, 3.0}, .direction = {0.0, 0.0, -1.0}};
        CHECK(converted == expected);
        CHECK((converted.has_value() && projectile::is_valid(*converted)));
    }

    SECTION("a ray without a direction or with a non-finite component is rejected") {
        CHECK(adapter::to_ray(::Ray{.position = {}, .direction = {}}) == std::nullopt);
        CHECK(
            adapter::to_ray(::Ray{.position = {}, .direction = {float_nan, 1.0F, 0.0F}}) ==
            std::nullopt
        );
        CHECK(
            adapter::to_ray(
                ::Ray{.position = {float_infinity, 0.0F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}}
            ) == std::nullopt
        );
    }

    SECTION("a Projectile ray converts to raylib component by component") {
        const projectile::Ray ray{.origin = {0.5, -1.0, 2.0}, .direction = {0.0, 1.0, 0.0}};
        const ::Ray converted = adapter::to_raylib_ray(ray);
        CHECK(rounds_to_nearest_float(ray.origin, converted.position));
        CHECK(rounds_to_nearest_float(ray.direction, converted.direction));
    }
}

TEST_CASE("Boxes convert between raylib and Projectile", "[raylib][conversions]") {
    SECTION("a box survives the round trip") {
        const AABB box{.min = {-1.0, 0.0, -2.5}, .max = {1.0, 3.0, 2.5}};
        const BoundingBox converted = adapter::to_bounding_box(box);
        CHECK(rounds_to_nearest_float(box.min, converted.min));
        CHECK(rounds_to_nearest_float(box.max, converted.max));
        CHECK(adapter::to_aabb(converted) == box);
    }

    SECTION("corners may come in either order") {
        const BoundingBox swapped{.min = {1.0F, 3.0F, 2.5F}, .max = {-1.0F, 0.0F, -2.5F}};
        const AABB expected{.min = {-1.0, 0.0, -2.5}, .max = {1.0, 3.0, 2.5}};
        CHECK(adapter::to_aabb(swapped) == expected);
    }

    SECTION("a box with a non-finite corner is rejected") {
        const BoundingBox infinite{.min = {}, .max = {float_infinity, 1.0F, 1.0F}};
        CHECK(adapter::to_aabb(infinite) == std::nullopt);
    }
}
