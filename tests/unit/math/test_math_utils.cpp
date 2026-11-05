#include "projectile/math/math_utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/random.hpp"

#include <limits>
#include <numbers>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinULP;
using projectile::approx_equal;
using projectile::degrees_to_radians;
using projectile::radians_to_degrees;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr int sample_count = 10'000;

} // namespace

TEST_CASE("approx_equal uses the absolute tolerance near zero", "[math][approx_equal]") {
    CHECK(approx_equal(0.0, 1e-13));
    CHECK(approx_equal(0.0, -1e-12)); // the boundary is inclusive
    CHECK_FALSE(approx_equal(0.0, 1e-11));
    CHECK(approx_equal(0.0, 1e-7, 1e-6));            // custom absolute tolerance
    CHECK_FALSE(approx_equal(0.0, 1e-13, 0.0, 0.0)); // zero tolerances demand equality
}

TEST_CASE("approx_equal scales the tolerance with the magnitude", "[math][approx_equal]") {
    CHECK(approx_equal(1e6, 1e6 + 1e-4));       // relative difference 1e-10
    CHECK_FALSE(approx_equal(1e6, 1e6 + 1e-2)); // relative difference 1e-8
    CHECK(approx_equal(-1e300, -1e300 * (1.0 + 1e-12)));
    CHECK(approx_equal(1.0, 2.0, 0.0, 0.5)); // |1 - 2| <= 0.5 * max(1, 2)
    CHECK_FALSE(approx_equal(1.0, 2.0, 0.0, 0.4));
}

TEST_CASE("approx_equal never matches NaN, and infinities only exactly", "[math][approx_equal]") {
    CHECK_FALSE(approx_equal(not_a_number, not_a_number));
    CHECK_FALSE(approx_equal(not_a_number, 0.0));
    CHECK_FALSE(approx_equal(0.0, not_a_number, 1e300, 1.0)); // not even with a huge tolerance
    CHECK(approx_equal(infinity, infinity));
    CHECK(approx_equal(-infinity, -infinity));
    CHECK_FALSE(approx_equal(infinity, -infinity));
    CHECK_FALSE(approx_equal(infinity, std::numeric_limits<double>::max(), 0.0, 1.0));
}

TEST_CASE("approx_equal treats positive and negative zero as equal", "[math][approx_equal]") {
    CHECK(approx_equal(0.0, -0.0, 0.0, 0.0));
}

TEST_CASE("approx_equal is symmetric", "[math][approx_equal][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const double a = random.uniform(-1e3, 1e3);
        // Differences straddling the default tolerance, so both outcomes occur.
        const double b = a + random.uniform(-2e-6, 2e-6);
        CAPTURE(a, b);
        CHECK(approx_equal(a, b) == approx_equal(b, a));
    }
}

TEST_CASE("degrees_to_radians converts known angles", "[math][angles]") {
    STATIC_REQUIRE(degrees_to_radians(0.0) == 0.0);
    CHECK_THAT(degrees_to_radians(180.0), WithinULP(std::numbers::pi, 1));
    CHECK_THAT(degrees_to_radians(90.0), WithinULP(std::numbers::pi / 2.0, 1));
    CHECK_THAT(degrees_to_radians(-45.0), WithinULP(-std::numbers::pi / 4.0, 1));
    CHECK_THAT(degrees_to_radians(360.0), WithinULP(2.0 * std::numbers::pi, 1));
}

TEST_CASE("radians_to_degrees converts known angles", "[math][angles]") {
    STATIC_REQUIRE(radians_to_degrees(0.0) == 0.0);
    CHECK_THAT(radians_to_degrees(std::numbers::pi), WithinULP(180.0, 1));
    CHECK_THAT(radians_to_degrees(std::numbers::pi / 6.0), WithinRel(30.0, 1e-15));
    CHECK_THAT(radians_to_degrees(-std::numbers::pi / 2.0), WithinULP(-90.0, 1));
}

TEST_CASE("angle conversions round-trip", "[math][angles][property]") {
    projectile::test::Random random;
    for (int i = 0; i < sample_count; ++i) {
        const double degrees = random.uniform(-720.0, 720.0);
        CAPTURE(degrees);
        CHECK_THAT(
            radians_to_degrees(degrees_to_radians(degrees)),
            WithinRel(degrees, 1e-15) || WithinAbs(degrees, 1e-12)
        );
    }
}
