#include "projectile/math/vec3.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/math_matchers.hpp"
#include "support/random.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <type_traits>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double epsilon = std::numeric_limits<double>::epsilon();
constexpr int sample_count = 10'000;

constexpr Vec3 unit_x{1.0, 0.0, 0.0};
constexpr Vec3 unit_y{0.0, 1.0, 0.0};
constexpr Vec3 unit_z{0.0, 0.0, 1.0};

} // namespace

// --- Type and arithmetic --------------------------------------------------------------------

TEST_CASE("Vec3 is a plain aggregate of three doubles", "[math][vec3]") {
    STATIC_REQUIRE(std::is_aggregate_v<Vec3>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<Vec3>);
    STATIC_REQUIRE(std::is_standard_layout_v<Vec3>);
    STATIC_REQUIRE(sizeof(Vec3) == 3 * sizeof(double));

    constexpr Vec3 v{1.0, 2.0, 3.0};
    STATIC_REQUIRE(v.x == 1.0);
    STATIC_REQUIRE(v.y == 2.0);
    STATIC_REQUIRE(v.z == 3.0);
    STATIC_REQUIRE(Vec3{} == Vec3{0.0, 0.0, 0.0});
    STATIC_REQUIRE(Vec3{1.0, 2.0, 3.0} != Vec3{1.0, 2.0, 4.0});
}

TEST_CASE("Vec3 arithmetic operators work component-wise", "[math][vec3]") {
    constexpr Vec3 a{1.0, -2.0, 3.0};
    constexpr Vec3 b{4.0, 5.0, -6.0};

    STATIC_REQUIRE(a + b == Vec3{5.0, 3.0, -3.0});
    STATIC_REQUIRE(a - b == Vec3{-3.0, -7.0, 9.0});
    STATIC_REQUIRE(-a == Vec3{-1.0, 2.0, -3.0});
    STATIC_REQUIRE(a * 2.0 == Vec3{2.0, -4.0, 6.0});
    STATIC_REQUIRE(2.0 * a == a * 2.0);
    STATIC_REQUIRE(b / 2.0 == Vec3{2.0, 2.5, -3.0});
}

TEST_CASE("Vec3 compound assignment matches the binary operators", "[math][vec3]") {
    const Vec3 a{1.5, -2.0, 0.25};
    const Vec3 b{-4.0, 8.0, 3.0};

    Vec3 v = a;
    v += b;
    CHECK(v == a + b);

    v = a;
    v -= b;
    CHECK(v == a - b);

    v = a;
    v *= -3.0;
    CHECK(v == a * -3.0);

    v = a;
    v /= 4.0;
    CHECK(v == a / 4.0);

    v = a;
    (v += b) *= 2.0; // compound assignment returns the vector itself
    CHECK(v == (a + b) * 2.0);
}

TEST_CASE("Vec3 division by zero follows IEEE-754 instead of hiding the error", "[math][vec3]") {
    const Vec3 v = Vec3{1.0, -1.0, 0.0} / 0.0;
    CHECK(v.x == infinity);
    CHECK(v.y == -infinity);
    CHECK(std::isnan(v.z));
    CHECK_FALSE(is_finite(v));
}

TEST_CASE("is_finite rejects NaN and infinite components", "[math][vec3]") {
    CHECK(is_finite(Vec3{1.0, -2.0, std::numeric_limits<double>::max()}));
    CHECK_FALSE(is_finite(Vec3{not_a_number, 0.0, 0.0}));
    CHECK_FALSE(is_finite(Vec3{0.0, infinity, 0.0}));
    CHECK_FALSE(is_finite(Vec3{0.0, 0.0, -infinity}));
}

// --- Products, lengths and distances --------------------------------------------------------

TEST_CASE("dot multiplies and sums the components", "[math][vec3]") {
    STATIC_REQUIRE(dot(Vec3{1.0, 2.0, 3.0}, Vec3{4.0, -5.0, 6.0}) == 12.0);
    STATIC_REQUIRE(dot(unit_x, unit_y) == 0.0);
    STATIC_REQUIRE(dot(Vec3{2.0, 3.0, 4.0}, Vec3{2.0, 3.0, 4.0}) == 29.0);
    STATIC_REQUIRE(
        dot(Vec3{1.0, 2.0, 3.0}, Vec3{-2.0, 0.5, 7.0}) ==
        dot(Vec3{-2.0, 0.5, 7.0}, Vec3{1.0, 2.0, 3.0})
    );
}

TEST_CASE("cross follows the right-hand rule", "[math][vec3]") {
    STATIC_REQUIRE(cross(unit_x, unit_y) == unit_z);
    STATIC_REQUIRE(cross(unit_y, unit_z) == unit_x);
    STATIC_REQUIRE(cross(unit_z, unit_x) == unit_y);
    STATIC_REQUIRE(cross(unit_y, unit_x) == -unit_z);
}

TEST_CASE("cross of parallel vectors is zero", "[math][vec3]") {
    STATIC_REQUIRE(cross(Vec3{1.0, 2.0, 3.0}, Vec3{2.0, 4.0, 6.0}) == Vec3{});
    STATIC_REQUIRE(cross(Vec3{1.0, 2.0, 3.0}, Vec3{-3.0, -6.0, -9.0}) == Vec3{});
    STATIC_REQUIRE(cross(Vec3{1.0, 2.0, 3.0}, Vec3{}) == Vec3{});
}

TEST_CASE("cross has length |a| |b| sin(angle)", "[math][vec3]") {
    const double angle = std::numbers::pi / 6.0; // 30 degrees
    const Vec3 a{2.0, 0.0, 0.0};
    const Vec3 b{3.0 * std::cos(angle), 3.0 * std::sin(angle), 0.0};
    CHECK_THAT(length(cross(a, b)), WithinRel(2.0 * 3.0 * 0.5, 1e-15));
}

TEST_CASE("cross is perpendicular to its inputs and anticommutative", "[math][vec3][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const Vec3 a = random.vec3(-100.0, 100.0);
        const Vec3 b = random.vec3(-100.0, 100.0);
        const Vec3 c = cross(a, b);
        // Rounding errors scale with |a| |b|.
        const double scale = length(a) * length(b);
        CAPTURE(a, b);
        CHECK_THAT(dot(c, a), WithinAbs(0.0, 1e-12 * scale * length(a)));
        CHECK_THAT(dot(c, b), WithinAbs(0.0, 1e-12 * scale * length(b)));
        CHECK_THAT(cross(b, a), is_near(-c, 1e-12 * scale, 0.0));
        // Lagrange's identity: |a x b|^2 + (a . b)^2 = |a|^2 |b|^2.
        CHECK_THAT(
            length_squared(c) + dot(a, b) * dot(a, b),
            WithinRel(length_squared(a) * length_squared(b), 1e-12)
        );
    }
}

TEST_CASE("length and length_squared measure the magnitude", "[math][vec3]") {
    STATIC_REQUIRE(length_squared(Vec3{3.0, 4.0, 0.0}) == 25.0);
    STATIC_REQUIRE(length_squared(Vec3{}) == 0.0);
    CHECK(length(Vec3{3.0, 4.0, 0.0}) == 5.0);
    CHECK(length(Vec3{1.0, -2.0, 2.0}) == 3.0);
    CHECK(length(Vec3{0.0, 0.0, -7.0}) == 7.0);
    CHECK(length(Vec3{}) == 0.0);
}

TEST_CASE("distance and distance_squared measure between points", "[math][vec3]") {
    constexpr Vec3 a{1.0, 1.0, 1.0};
    constexpr Vec3 b{4.0, 5.0, 1.0};
    STATIC_REQUIRE(distance_squared(a, b) == 25.0);
    STATIC_REQUIRE(distance_squared(a, b) == distance_squared(b, a));
    CHECK(distance(a, b) == 5.0);
    CHECK(distance(b, a) == 5.0);
    CHECK(distance(a, a) == 0.0);
}

// --- Interpolation, reflection and projection ---------------------------------------------

TEST_CASE("lerp interpolates and extrapolates linearly", "[math][vec3]") {
    constexpr Vec3 a{1.0, -2.0, 10.0};
    constexpr Vec3 b{3.0, 6.0, -10.0};
    STATIC_REQUIRE(lerp(a, b, 0.0) == a);
    STATIC_REQUIRE(lerp(a, b, 1.0) == b);
    STATIC_REQUIRE(lerp(a, b, 0.5) == Vec3{2.0, 2.0, 0.0});
    STATIC_REQUIRE(lerp(a, b, 0.25) == Vec3{1.5, 0.0, 5.0});
    STATIC_REQUIRE(lerp(a, b, 2.0) == Vec3{5.0, 14.0, -30.0});
    STATIC_REQUIRE(lerp(a, b, -1.0) == Vec3{-1.0, -10.0, 30.0});
}

TEST_CASE("lerp is exact at both ends", "[math][vec3][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const Vec3 a = random.vec3(-1e6, 1e6);
        const Vec3 b = random.vec3(-1e6, 1e6);
        CAPTURE(a, b);
        CHECK(lerp(a, b, 0.0) == a);
        CHECK(lerp(a, b, 1.0) == b);
    }
}

TEST_CASE("reflect bounces a vector off a surface", "[math][vec3]") {
    STATIC_REQUIRE(reflect(Vec3{1.0, -1.0, 0.0}, unit_y) == Vec3{1.0, 1.0, 0.0});  // 45 degrees
    STATIC_REQUIRE(reflect(Vec3{0.0, -5.0, 0.0}, unit_y) == Vec3{0.0, 5.0, 0.0});  // head-on
    STATIC_REQUIRE(reflect(Vec3{3.0, 0.0, -2.0}, unit_y) == Vec3{3.0, 0.0, -2.0}); // grazing
    // The side the normal points to does not matter.
    STATIC_REQUIRE(reflect(Vec3{1.0, -1.0, 0.0}, -unit_y) == Vec3{1.0, 1.0, 0.0});
}

TEST_CASE("reflect preserves length and undoes itself", "[math][vec3][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const Vec3 v = random.vec3(-100.0, 100.0);
        const Vec3 normal = random.unit_vec3();
        const Vec3 reflected = reflect(v, normal);
        const double tolerance = 1e-12 * length(v);
        CAPTURE(v, normal);
        CHECK_THAT(length(reflected), WithinAbs(length(v), tolerance));
        // The normal component flips; the tangential component is kept.
        CHECK_THAT(dot(reflected, normal), WithinAbs(-dot(v, normal), tolerance));
        CHECK_THAT(reflect(reflected, normal), is_near(v, tolerance, 0.0));
    }
}

TEST_CASE("project keeps the component parallel to the target", "[math][vec3]") {
    constexpr Vec3 v{3.0, 4.0, 5.0};
    STATIC_REQUIRE(project(v, unit_x) == Vec3{3.0, 0.0, 0.0});
    STATIC_REQUIRE(project(v, Vec3{4.0, 0.0, 0.0}) == Vec3{3.0, 0.0, 0.0});  // any length
    STATIC_REQUIRE(project(v, Vec3{-4.0, 0.0, 0.0}) == Vec3{3.0, 0.0, 0.0}); // either sign
    STATIC_REQUIRE(project(Vec3{2.0, 4.0, 6.0}, Vec3{1.0, 2.0, 3.0}) == Vec3{2.0, 4.0, 6.0});
    STATIC_REQUIRE(project(Vec3{0.0, 4.0, 5.0}, unit_x) == Vec3{});
}

TEST_CASE("project onto the zero vector yields the zero vector", "[math][vec3]") {
    STATIC_REQUIRE(project(Vec3{3.0, 4.0, 5.0}, Vec3{}) == Vec3{});
}

TEST_CASE("project leaves a perpendicular remainder", "[math][vec3][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const Vec3 v = random.vec3(-100.0, 100.0);
        const Vec3 onto = random.vec3(-100.0, 100.0);
        const Vec3 parallel = project(v, onto);
        const double tolerance = 1e-12 * length(v) * length(onto);
        CAPTURE(v, onto);
        CHECK_THAT(dot(v - parallel, onto), WithinAbs(0.0, tolerance));
        CHECK_THAT(length(cross(parallel, onto)), WithinAbs(0.0, tolerance));
        CHECK_THAT(project(parallel, onto), is_near(parallel, 1e-12 * length(v), 0.0));
    }
}

// --- Normalization ----------------------------------------------------------------------------

TEST_CASE("try_normalize returns the unit vector in the same direction", "[math][vec3]") {
    CHECK_THAT(
        require_value(try_normalize(Vec3{3.0, 4.0, 0.0})),
        is_near(Vec3{0.6, 0.8, 0.0}, 2 * epsilon, 0.0)
    );
    CHECK(require_value(try_normalize(Vec3{0.0, 0.0, -2.5})) == -unit_z);
    CHECK(require_value(try_normalize(unit_x)) == unit_x);
}

TEST_CASE("try_normalize rejects vectors without a direction", "[math][vec3]") {
    CHECK_FALSE(try_normalize(Vec3{}).has_value());
    CHECK_FALSE(try_normalize(Vec3{-0.0, 0.0, -0.0}).has_value());
    CHECK_FALSE(try_normalize(Vec3{not_a_number, 1.0, 1.0}).has_value());
    CHECK_FALSE(try_normalize(Vec3{1.0, infinity, 1.0}).has_value());
    CHECK_FALSE(try_normalize(Vec3{1.0, 1.0, -infinity}).has_value());
}

TEST_CASE("try_normalize handles the extremes of the double range", "[math][vec3]") {
    // Naive normalization would overflow (max^2 = inf) or underflow (tiny^2 = 0) here.
    const double max = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double third = std::numbers::inv_sqrt3;
    const double half = std::sqrt(0.5);

    CHECK_THAT(
        require_value(try_normalize(Vec3{max, max, max})),
        is_near(Vec3{third, third, third}, 4 * epsilon, 0.0)
    );
    CHECK_THAT(
        require_value(try_normalize(Vec3{max, -max, 0.0})),
        is_near(Vec3{half, -half, 0.0}, 4 * epsilon, 0.0)
    );
    CHECK_THAT(
        require_value(try_normalize(Vec3{1e-300, -1e-300, 1e-300})),
        is_near(Vec3{third, -third, third}, 4 * epsilon, 0.0)
    );
    CHECK(require_value(try_normalize(Vec3{tiny, 0.0, 0.0})) == unit_x);
    CHECK(require_value(try_normalize(Vec3{0.0, -tiny, 0.0})) == -unit_y);
}

TEST_CASE(
    "try_normalize is accurate across the whole range of magnitudes", "[math][vec3][property]"
) {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        // A well-conditioned direction, scaled by a power of two (an exact operation) anywhere
        // between 2^-1000 and 2^1000. The expected result is computed from the unscaled vector.
        const Vec3 direction = random.vec3(-1.0, 1.0);
        if (length_squared(direction) < 1e-4) {
            continue;
        }
        const int exponent = random.integer(-1000, 1000);
        const Vec3 scaled{
            std::ldexp(direction.x, exponent),
            std::ldexp(direction.y, exponent),
            std::ldexp(direction.z, exponent),
        };
        CAPTURE(scaled, exponent);
        const Vec3 unit = require_value(try_normalize(scaled));
        CHECK_THAT(unit, is_near(direction / length(direction), 8 * epsilon, 0.0));
        CHECK_THAT(length(unit), WithinAbs(1.0, 8 * epsilon));
    }
}

TEST_CASE(
    "try_normalize rejects every vector with a non-finite component", "[math][vec3][property]"
) {
    constexpr std::array specials{not_a_number, infinity, -infinity};
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        Vec3 v = random.vec3(-1e10, 1e10);
        const double special = specials.at(static_cast<std::size_t>(random.integer(0, 2)));
        switch (random.integer(0, 2)) {
        case 0:
            v.x = special;
            break;
        case 1:
            v.y = special;
            break;
        default:
            v.z = special;
            break;
        }
        CAPTURE(v);
        CHECK_FALSE(try_normalize(v).has_value());
    }
}

TEST_CASE("is_normalized checks for unit length within a tolerance", "[math][vec3]") {
    CHECK(is_normalized(unit_x));
    CHECK(is_normalized(Vec3{0.6, 0.8, 0.0}));
    CHECK(is_normalized(Vec3{1.0 + 1e-7, 0.0, 0.0}));
    CHECK_FALSE(is_normalized(Vec3{1.01, 0.0, 0.0}));
    CHECK(is_normalized(Vec3{1.01, 0.0, 0.0}, 0.05)); // custom tolerance
    CHECK_FALSE(is_normalized(Vec3{}));
    CHECK_FALSE(is_normalized(Vec3{not_a_number, 0.0, 0.0}));
    CHECK_FALSE(is_normalized(Vec3{infinity, 0.0, 0.0}));
}

TEST_CASE("is_normalized accepts directions normalized in single precision", "[math][vec3]") {
    // Engines often store directions as floats; converted to double they are off by ~1e-7.
    const auto as_double = static_cast<double>(std::numbers::inv_sqrt3_v<float>);
    CHECK(is_normalized(Vec3{as_double, as_double, as_double}));
}

// --- Approximate equality -------------------------------------------------------------------

TEST_CASE(
    "approx_equal for Vec3 measures differences against the whole vector",
    "[math][vec3][approx_equal]"
) {
    CHECK(approx_equal(Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 3.0}));
    CHECK(approx_equal(Vec3{}, Vec3{1e-13, -1e-13, 0.0})); // absolute tolerance near zero
    CHECK_FALSE(approx_equal(Vec3{}, Vec3{1e-11, 0.0, 0.0}));
    // Rounding noise in the small component of a large vector is tolerated...
    CHECK(approx_equal(Vec3{1e6, 1e-6, 0.0}, Vec3{1e6, 2e-6, 0.0}));
    // ...but a difference that is significant relative to the vector is not.
    CHECK_FALSE(approx_equal(Vec3{1e6, 0.0, 0.0}, Vec3{1e6, 0.01, 0.0}));
    CHECK(approx_equal(Vec3{1.0, 1.0, 1.0}, Vec3{1.05, 1.0, 1.0}, 0.1, 0.0)); // custom tolerance
}

TEST_CASE(
    "approx_equal for Vec3 never matches NaN, and infinities only exactly",
    "[math][vec3][approx_equal]"
) {
    CHECK_FALSE(approx_equal(Vec3{not_a_number, 0.0, 0.0}, Vec3{not_a_number, 0.0, 0.0}));
    CHECK(approx_equal(Vec3{infinity, 0.0, 0.0}, Vec3{infinity, 0.0, 0.0}));
    CHECK_FALSE(approx_equal(Vec3{infinity, 0.0, 0.0}, Vec3{infinity, 1e-20, 0.0}));
    CHECK_FALSE(approx_equal(Vec3{infinity, 0.0, 0.0}, Vec3{1.0, 0.0, 0.0}, 1e300, 1.0));
}
