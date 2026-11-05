#include "projectile/math/ray.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/math_matchers.hpp"
#include "support/random.hpp"

#include <cmath>
#include <limits>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double epsilon = std::numeric_limits<double>::epsilon();

} // namespace

TEST_CASE("Ray supports designated initialization and equality", "[math][ray]") {
    constexpr Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    STATIC_REQUIRE(ray.origin == Vec3{});
    STATIC_REQUIRE(ray.direction == Vec3{0.0, 0.0, -1.0});
    STATIC_REQUIRE(ray == Ray{.origin = {}, .direction = {0.0, 0.0, -1.0}});
    STATIC_REQUIRE(ray != Ray{.origin = {}, .direction = {0.0, 0.0, 1.0}});
    STATIC_REQUIRE(ray != Ray{.origin = {0.0, 1.0, 0.0}, .direction = {0.0, 0.0, -1.0}});
}

TEST_CASE("point_at walks along the ray", "[math][ray]") {
    constexpr Ray ray{.origin = {1.0, 2.0, 3.0}, .direction = {0.0, 0.0, -1.0}};
    STATIC_REQUIRE(point_at(ray, 0.0) == ray.origin);
    STATIC_REQUIRE(point_at(ray, 10.0) == Vec3{1.0, 2.0, -7.0});
    STATIC_REQUIRE(point_at(ray, -2.0) == Vec3{1.0, 2.0, 5.0}); // behind the origin
}

TEST_CASE("make_ray normalizes the direction", "[math][ray]") {
    const Ray ray = require_value(make_ray(Vec3{1.0, 1.0, 1.0}, Vec3{0.0, 3.0, 4.0}));
    CHECK(ray.origin == Vec3{1.0, 1.0, 1.0});
    CHECK_THAT(ray.direction, is_near(Vec3{0.0, 0.6, 0.8}, 2 * epsilon, 0.0));
    CHECK(is_valid(ray));
}

TEST_CASE("make_ray rejects a missing direction and non-finite input", "[math][ray]") {
    CHECK_FALSE(make_ray(Vec3{}, Vec3{}).has_value());
    CHECK_FALSE(make_ray(Vec3{not_a_number, 0.0, 0.0}, Vec3{1.0, 0.0, 0.0}).has_value());
    CHECK_FALSE(make_ray(Vec3{0.0, infinity, 0.0}, Vec3{1.0, 0.0, 0.0}).has_value());
    CHECK_FALSE(make_ray(Vec3{}, Vec3{infinity, 0.0, 0.0}).has_value());
}

TEST_CASE("is_valid checks the Ray invariants", "[math][ray]") {
    CHECK(is_valid(Ray{.origin = {}, .direction = {1.0, 0.0, 0.0}}));
    CHECK_FALSE(is_valid(Ray{.origin = {}, .direction = {2.0, 0.0, 0.0}})); // not unit length
    CHECK_FALSE(is_valid(Ray{.origin = {}, .direction = {}}));
    CHECK_FALSE(is_valid(Ray{.origin = {not_a_number, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}}));
    CHECK_FALSE(is_valid(Ray{.origin = {}, .direction = {not_a_number, 0.0, 0.0}}));
}

TEST_CASE("for a valid ray, t is the distance travelled", "[math][ray][property]") {
    projectile::test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Vec3 origin = random.vec3(-1e3, 1e3);
        const Ray ray = require_value(make_ray(origin, random.vec3(-1.0, 1.0)));
        const double t = random.uniform(0.0, 1e3);
        CAPTURE(ray, t);
        CHECK_THAT(distance(ray.origin, point_at(ray, t)), WithinAbs(t, 1e-9));
    }
}
