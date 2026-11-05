#pragma once

/// @file
/// @brief Three-dimensional vector type and vector operations.
///
/// See docs/mathematics.md for the conventions: right-handed coordinates, SI units, IEEE-754
/// double precision, and how degenerate input such as the zero vector is handled.

#include "projectile/math/math_utils.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace projectile {

/// A three-dimensional vector of doubles, used for positions, displacements and velocities.
///
/// Vec3 is a plain aggregate: trivially copyable, standard layout and exactly three doubles. It
/// can be brace-initialized (`Vec3{1.0, 2.0, 3.0}`) and converted to and from engine vector types
/// member by member. The coordinate system is right-handed, and no axis is assumed to point up.
struct Vec3 {
    double x = 0.0; ///< X component.
    double y = 0.0; ///< Y component.
    double z = 0.0; ///< Z component.

    /// Exact component-wise comparison. Compare computed values with approx_equal() instead.
    constexpr bool operator==(const Vec3&) const = default;

    /// Adds @p other component-wise.
    constexpr Vec3& operator+=(const Vec3& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    /// Subtracts @p other component-wise.
    constexpr Vec3& operator-=(const Vec3& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    /// Multiplies every component by @p scalar.
    constexpr Vec3& operator*=(double scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    /// Divides every component by @p divisor. Division by zero follows IEEE-754 and produces
    /// infinities or NaN; use try_normalize() to turn a vector into a direction safely.
    constexpr Vec3& operator/=(double divisor) noexcept {
        x /= divisor;
        y /= divisor;
        z /= divisor;
        return *this;
    }
};

/// Component-wise sum.
[[nodiscard]] constexpr Vec3 operator+(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

/// Component-wise difference.
[[nodiscard]] constexpr Vec3 operator-(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

/// Negation.
[[nodiscard]] constexpr Vec3 operator-(const Vec3& v) noexcept {
    return Vec3{-v.x, -v.y, -v.z};
}

/// Multiplies every component by @p scalar.
[[nodiscard]] constexpr Vec3 operator*(const Vec3& v, double scalar) noexcept {
    return Vec3{v.x * scalar, v.y * scalar, v.z * scalar};
}

/// Multiplies every component by @p scalar.
[[nodiscard]] constexpr Vec3 operator*(double scalar, const Vec3& v) noexcept {
    return v * scalar;
}

/// Divides every component by @p divisor. Division by zero follows IEEE-754 and produces
/// infinities or NaN; use try_normalize() to turn a vector into a direction safely.
[[nodiscard]] constexpr Vec3 operator/(const Vec3& v, double divisor) noexcept {
    return Vec3{v.x / divisor, v.y / divisor, v.z / divisor};
}

/// Dot product: `a.x·b.x + a.y·b.y + a.z·b.z`.
[[nodiscard]] constexpr double dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// Cross product, following the right-hand rule: `cross({1, 0, 0}, {0, 1, 0}) == {0, 0, 1}`.
/// The result is perpendicular to both inputs, with length `‖a‖ ‖b‖ sin θ`.
[[nodiscard]] constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// Squared length (magnitude): `dot(v, v)`. Cheaper than length() and exact for comparisons.
[[nodiscard]] constexpr double length_squared(const Vec3& v) noexcept {
    return dot(v, v);
}

/// Length (Euclidean magnitude). Squaring overflows for components beyond about 1e154, where
/// the result becomes infinity; physical quantities in SI units are far from that range.
[[nodiscard]] inline double length(const Vec3& v) noexcept {
    return std::sqrt(length_squared(v));
}

/// Squared distance between two points.
[[nodiscard]] constexpr double distance_squared(const Vec3& a, const Vec3& b) noexcept {
    return length_squared(b - a);
}

/// Distance between two points.
[[nodiscard]] inline double distance(const Vec3& a, const Vec3& b) noexcept {
    return length(b - a);
}

/// Returns whether every component is finite (neither NaN nor infinite).
[[nodiscard]] inline bool is_finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/// @cond PROJECTILE_DETAIL
namespace detail {

/// Largest component magnitude, i.e. the maximum norm. Expects finite components.
[[nodiscard]] inline double max_abs_component(const Vec3& v) noexcept {
    return (std::max)({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

/// Component-wise minimum. Expects components that are not NaN.
[[nodiscard]] constexpr Vec3 component_min(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{(std::min)(a.x, b.x), (std::min)(a.y, b.y), (std::min)(a.z, b.z)};
}

/// Component-wise maximum. Expects components that are not NaN.
[[nodiscard]] constexpr Vec3 component_max(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{(std::max)(a.x, b.x), (std::max)(a.y, b.y), (std::max)(a.z, b.z)};
}

} // namespace detail
/// @endcond

/// Returns the unit vector in the direction of @p v, or `std::nullopt` when @p v has no
/// direction: when it is the zero vector, or a component is NaN or infinite.
///
/// Every other vector is normalized accurately, from subnormal components up to the largest
/// finite double: the components are scaled by the largest magnitude before squaring, so the
/// computation can neither overflow nor underflow.
[[nodiscard]] inline std::optional<Vec3> try_normalize(const Vec3& v) noexcept {
    if (!is_finite(v)) {
        return std::nullopt;
    }
    const double largest = detail::max_abs_component(v);
    if (largest == 0.0) {
        return std::nullopt;
    }
    // Divide rather than multiply by 1 / largest: that reciprocal overflows for subnormal input.
    const Vec3 scaled = v / largest;
    return scaled / std::sqrt(length_squared(scaled));
}

/// Returns whether @p v has unit length within @p tolerance, measured on the squared length:
/// `|‖v‖² − 1| ≤ tolerance`. False for vectors with NaN or infinite components.
[[nodiscard]] inline bool is_normalized(
    const Vec3& v, double tolerance = unit_length_tolerance
) noexcept {
    return std::abs(length_squared(v) - 1.0) <= tolerance;
}

/// Linear interpolation from @p a (at t = 0) to @p b (at t = 1). Values of @p t outside [0, 1]
/// extrapolate. Exact at both ends; uses std::lerp for each component.
[[nodiscard]] constexpr Vec3 lerp(const Vec3& a, const Vec3& b, double t) noexcept {
    return Vec3{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}

/// Reflects @p v off a surface with unit normal @p normal: `v − 2 (v · normal) normal`.
///
/// The result has the same length as @p v, and it does not matter which side of the surface
/// @p normal points to. @p normal must have unit length.
[[nodiscard]] constexpr Vec3 reflect(const Vec3& v, const Vec3& normal) noexcept {
    return v - normal * (2.0 * dot(v, normal));
}

/// Projects @p v onto the line through the origin along @p onto, which need not be a unit
/// vector: the result is the component of @p v parallel to @p onto. Projecting onto the zero
/// vector, which spans no line, yields the zero vector.
[[nodiscard]] constexpr Vec3 project(const Vec3& v, const Vec3& onto) noexcept {
    const double onto_length_squared = length_squared(onto);
    if (onto_length_squared == 0.0) {
        return Vec3{};
    }
    return onto * (dot(v, onto) / onto_length_squared);
}

/// Returns whether @p a and @p b are equal within a tolerance.
///
/// Every component of `a − b` must be within `max(absolute_tolerance, relative_tolerance × s)`,
/// where `s` is the largest component magnitude of either vector. Because the relative part is
/// measured against the whole vector, rounding noise in a small component of a large vector is
/// tolerated. Vectors with NaN or infinite components are approximately equal only when they
/// are exactly equal.
[[nodiscard]] inline bool approx_equal(
    const Vec3& a,
    const Vec3& b,
    double absolute_tolerance = default_absolute_tolerance,
    double relative_tolerance = default_relative_tolerance
) noexcept {
    if (a == b) {
        return true;
    }
    if (!is_finite(a) || !is_finite(b)) {
        return false;
    }
    const double scale = (std::max)(detail::max_abs_component(a), detail::max_abs_component(b));
    const double tolerance = (std::max)(absolute_tolerance, relative_tolerance * scale);
    return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance &&
           std::abs(a.z - b.z) <= tolerance;
}

} // namespace projectile
