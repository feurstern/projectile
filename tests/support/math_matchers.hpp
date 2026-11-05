#pragma once

// Catch2 support for Projectile's math types: exact values in failure messages, matchers that
// compare vectors within a tolerance, and a helper for unwrapping std::optional results.

#include "projectile/math/aabb.hpp"
#include "projectile/math/math_utils.hpp"
#include "projectile/math/plane.hpp"
#include "projectile/math/ray.hpp"
#include "projectile/math/sphere.hpp"
#include "projectile/math/vec2.hpp"
#include "projectile/math/vec3.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace projectile::test {

/// Formats @p value with enough digits to identify the double exactly.
[[nodiscard]] inline std::string format(double value) {
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

[[nodiscard]] inline std::string format(const Vec2& v) {
    return "{" + format(v.x) + ", " + format(v.y) + "}";
}

[[nodiscard]] inline std::string format(const Vec3& v) {
    return "{" + format(v.x) + ", " + format(v.y) + ", " + format(v.z) + "}";
}

/// Matches a vector that is approx_equal() to the expected one.
template <typename Vector>
class IsNear final : public Catch::Matchers::MatcherBase<Vector> {
public:
    IsNear(const Vector& expected, double absolute_tolerance, double relative_tolerance)
        : expected_(expected),
          absolute_tolerance_(absolute_tolerance),
          relative_tolerance_(relative_tolerance) {}

    [[nodiscard]] bool match(const Vector& actual) const override {
        return approx_equal(actual, expected_, absolute_tolerance_, relative_tolerance_);
    }

    [[nodiscard]] std::string describe() const override {
        return "is near " + format(expected_) + " (absolute tolerance " +
               format(absolute_tolerance_) + ", relative tolerance " + format(relative_tolerance_) +
               ")";
    }

private:
    Vector expected_;
    double absolute_tolerance_;
    double relative_tolerance_;
};

/// `CHECK_THAT(actual, is_near(expected))`: the vectors are approx_equal() with the given
/// tolerances, which default to the library defaults.
[[nodiscard]] inline IsNear<Vec3> is_near(
    const Vec3& expected,
    double absolute_tolerance = default_absolute_tolerance,
    double relative_tolerance = default_relative_tolerance
) {
    return {expected, absolute_tolerance, relative_tolerance};
}

[[nodiscard]] inline IsNear<Vec2> is_near(
    const Vec2& expected,
    double absolute_tolerance = default_absolute_tolerance,
    double relative_tolerance = default_relative_tolerance
) {
    return {expected, absolute_tolerance, relative_tolerance};
}

/// Returns the value held by @p optional, failing the current test if it is empty.
template <typename T>
[[nodiscard]] T require_value(const std::optional<T>& optional) {
    REQUIRE(optional.has_value());
    return optional.value();
}

} // namespace projectile::test

template <>
struct Catch::StringMaker<projectile::Vec2> {
    static std::string convert(const projectile::Vec2& v) { return projectile::test::format(v); }
};

template <>
struct Catch::StringMaker<projectile::Vec3> {
    static std::string convert(const projectile::Vec3& v) { return projectile::test::format(v); }
};

template <>
struct Catch::StringMaker<projectile::Ray> {
    static std::string convert(const projectile::Ray& ray) {
        using projectile::test::format;
        return "Ray{origin " + format(ray.origin) + ", direction " + format(ray.direction) + "}";
    }
};

template <>
struct Catch::StringMaker<projectile::Plane> {
    static std::string convert(const projectile::Plane& plane) {
        using projectile::test::format;
        return "Plane{normal " + format(plane.normal) + ", distance " + format(plane.distance) +
               "}";
    }
};

template <>
struct Catch::StringMaker<projectile::Sphere> {
    static std::string convert(const projectile::Sphere& sphere) {
        using projectile::test::format;
        return "Sphere{center " + format(sphere.center) + ", radius " + format(sphere.radius) + "}";
    }
};

template <>
struct Catch::StringMaker<projectile::AABB> {
    static std::string convert(const projectile::AABB& box) {
        using projectile::test::format;
        return "AABB{min " + format(box.min) + ", max " + format(box.max) + "}";
    }
};
