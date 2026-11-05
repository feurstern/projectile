#include "projectile/math/vec2.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/math_matchers.hpp"
#include "support/random.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double epsilon = std::numeric_limits<double>::epsilon();
constexpr int sample_count = 10'000;

constexpr Vec2 unit_x{1.0, 0.0};
constexpr Vec2 unit_y{0.0, 1.0};

} // namespace

TEST_CASE("Vec2 is a plain aggregate of two doubles", "[math][vec2]") {
    STATIC_REQUIRE(std::is_aggregate_v<Vec2>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<Vec2>);
    STATIC_REQUIRE(std::is_standard_layout_v<Vec2>);
    STATIC_REQUIRE(sizeof(Vec2) == 2 * sizeof(double));
    STATIC_REQUIRE(Vec2{} == Vec2{0.0, 0.0});
}

TEST_CASE("Vec2 arithmetic operators work component-wise", "[math][vec2]") {
    constexpr Vec2 a{1.0, -2.0};
    constexpr Vec2 b{4.0, 5.0};
    STATIC_REQUIRE(a + b == Vec2{5.0, 3.0});
    STATIC_REQUIRE(a - b == Vec2{-3.0, -7.0});
    STATIC_REQUIRE(-a == Vec2{-1.0, 2.0});
    STATIC_REQUIRE(a * 2.0 == Vec2{2.0, -4.0});
    STATIC_REQUIRE(2.0 * a == a * 2.0);
    STATIC_REQUIRE(b / 2.0 == Vec2{2.0, 2.5});

    Vec2 v = a;
    (v += b) *= 2.0;
    CHECK(v == (a + b) * 2.0);
    v -= b;
    CHECK(v == (a + b) * 2.0 - b);
    v /= 2.0;
    CHECK(v == ((a + b) * 2.0 - b) / 2.0);
}

TEST_CASE("Vec2 dot, length and distance", "[math][vec2]") {
    STATIC_REQUIRE(dot(Vec2{1.0, 2.0}, Vec2{3.0, -4.0}) == -5.0);
    STATIC_REQUIRE(dot(unit_x, unit_y) == 0.0);
    STATIC_REQUIRE(length_squared(Vec2{3.0, 4.0}) == 25.0);
    STATIC_REQUIRE(distance_squared(Vec2{1.0, 1.0}, Vec2{4.0, 5.0}) == 25.0);
    CHECK(length(Vec2{3.0, -4.0}) == 5.0);
    CHECK(distance(Vec2{1.0, 1.0}, Vec2{4.0, 5.0}) == 5.0);
    CHECK(length(Vec2{}) == 0.0);
}

TEST_CASE("Vec2 cross gives the signed area and the turning direction", "[math][vec2]") {
    STATIC_REQUIRE(cross(unit_x, unit_y) == 1.0);  // counter-clockwise
    STATIC_REQUIRE(cross(unit_y, unit_x) == -1.0); // clockwise
    STATIC_REQUIRE(cross(Vec2{2.0, 0.0}, Vec2{0.0, 3.0}) == 6.0);
    STATIC_REQUIRE(cross(Vec2{1.0, 2.0}, Vec2{-2.0, -4.0}) == 0.0); // parallel
}

TEST_CASE("Vec2 lerp, reflect and project", "[math][vec2]") {
    constexpr Vec2 a{1.0, -2.0};
    constexpr Vec2 b{3.0, 6.0};
    STATIC_REQUIRE(lerp(a, b, 0.0) == a);
    STATIC_REQUIRE(lerp(a, b, 1.0) == b);
    STATIC_REQUIRE(lerp(a, b, 0.5) == Vec2{2.0, 2.0});
    STATIC_REQUIRE(lerp(a, b, 2.0) == Vec2{5.0, 14.0});

    STATIC_REQUIRE(reflect(Vec2{1.0, -1.0}, unit_y) == Vec2{1.0, 1.0});
    STATIC_REQUIRE(reflect(Vec2{1.0, -1.0}, -unit_y) == Vec2{1.0, 1.0});

    STATIC_REQUIRE(project(Vec2{3.0, 4.0}, Vec2{4.0, 0.0}) == Vec2{3.0, 0.0});
    STATIC_REQUIRE(project(Vec2{3.0, 4.0}, Vec2{}) == Vec2{});
}

TEST_CASE("Vec2 reflect preserves length", "[math][vec2][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const Vec2 v = random.vec2(-100.0, 100.0);
        const Vec2 normal = random.unit_vec2();
        CAPTURE(v, normal);
        CHECK_THAT(length(reflect(v, normal)), WithinAbs(length(v), 1e-12 * length(v)));
    }
}

TEST_CASE("Vec2 try_normalize", "[math][vec2]") {
    const double max = std::numeric_limits<double>::max();
    const double half = std::sqrt(0.5);

    CHECK_THAT(
        require_value(try_normalize(Vec2{3.0, -4.0})), is_near(Vec2{0.6, -0.8}, 2 * epsilon, 0.0)
    );
    CHECK_THAT(
        require_value(try_normalize(Vec2{max, max})), is_near(Vec2{half, half}, 4 * epsilon, 0.0)
    );
    CHECK(
        require_value(try_normalize(Vec2{std::numeric_limits<double>::denorm_min(), 0.0})) == unit_x
    );
    CHECK_FALSE(try_normalize(Vec2{}).has_value());
    CHECK_FALSE(try_normalize(Vec2{not_a_number, 1.0}).has_value());
    CHECK_FALSE(try_normalize(Vec2{1.0, -infinity}).has_value());
}

TEST_CASE("Vec2 is_normalized and is_finite", "[math][vec2]") {
    CHECK(is_normalized(unit_y));
    CHECK(is_normalized(Vec2{0.6, 0.8}));
    CHECK_FALSE(is_normalized(Vec2{1.01, 0.0}));
    CHECK_FALSE(is_normalized(Vec2{not_a_number, 0.0}));
    CHECK(is_finite(Vec2{1.0, -1.0}));
    CHECK_FALSE(is_finite(Vec2{infinity, 0.0}));
    CHECK_FALSE(is_finite(Vec2{0.0, not_a_number}));
}

TEST_CASE("Vec2 approx_equal", "[math][vec2][approx_equal]") {
    CHECK(approx_equal(Vec2{1.0, 2.0}, Vec2{1.0, 2.0}));
    CHECK(approx_equal(Vec2{}, Vec2{1e-13, -1e-13}));
    CHECK_FALSE(approx_equal(Vec2{}, Vec2{1e-11, 0.0}));
    CHECK(approx_equal(Vec2{1e6, 1e-6}, Vec2{1e6, 2e-6}));
    CHECK_FALSE(approx_equal(Vec2{not_a_number, 0.0}, Vec2{not_a_number, 0.0}));
    CHECK(approx_equal(Vec2{infinity, 0.0}, Vec2{infinity, 0.0}));
    CHECK_FALSE(approx_equal(Vec2{infinity, 0.0}, Vec2{1.0, 0.0}, 1e300, 1.0));
}
