#pragma once

/// @file
/// @brief Two-dimensional vector type and vector operations.
///
/// Vec2 mirrors Vec3 (see projectile/math/vec3.hpp) for planar problems. The only difference is
/// cross(), which returns a scalar in two dimensions.

#include "projectile/math/math_utils.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace projectile {

/// A two-dimensional vector of doubles.
///
/// Vec2 is a plain aggregate: trivially copyable, standard layout and exactly two doubles, so it
/// can be brace-initialized (`Vec2{1.0, 2.0}`) and converted to and from engine vector types
/// member by member.
struct Vec2 {
    double x = 0.0; ///< X component.
    double y = 0.0; ///< Y component.

    /// Exact component-wise comparison. Compare computed values with approx_equal() instead.
    constexpr bool operator==(const Vec2&) const = default;

    /// Adds @p other component-wise.
    constexpr Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    /// Subtracts @p other component-wise.
    constexpr Vec2& operator-=(const Vec2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    /// Multiplies every component by @p scalar.
    constexpr Vec2& operator*=(double scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    /// Divides every component by @p divisor. Division by zero follows IEEE-754 and produces
    /// infinities or NaN; use try_normalize() to turn a vector into a direction safely.
    constexpr Vec2& operator/=(double divisor) noexcept {
        x /= divisor;
        y /= divisor;
        return *this;
    }
};

/// Component-wise sum.
[[nodiscard]] constexpr Vec2 operator+(const Vec2& a, const Vec2& b) noexcept {
    return Vec2{a.x + b.x, a.y + b.y};
}

/// Component-wise difference.
[[nodiscard]] constexpr Vec2 operator-(const Vec2& a, const Vec2& b) noexcept {
    return Vec2{a.x - b.x, a.y - b.y};
}

/// Negation.
[[nodiscard]] constexpr Vec2 operator-(const Vec2& v) noexcept {
    return Vec2{-v.x, -v.y};
}

/// Multiplies every component by @p scalar.
[[nodiscard]] constexpr Vec2 operator*(const Vec2& v, double scalar) noexcept {
    return Vec2{v.x * scalar, v.y * scalar};
}

/// Multiplies every component by @p scalar.
[[nodiscard]] constexpr Vec2 operator*(double scalar, const Vec2& v) noexcept {
    return v * scalar;
}

/// Divides every component by @p divisor. Division by zero follows IEEE-754 and produces
/// infinities or NaN; use try_normalize() to turn a vector into a direction safely.
[[nodiscard]] constexpr Vec2 operator/(const Vec2& v, double divisor) noexcept {
    return Vec2{v.x / divisor, v.y / divisor};
}

/// Dot product: `a.x·b.x + a.y·b.y`.
[[nodiscard]] constexpr double dot(const Vec2& a, const Vec2& b) noexcept {
    return a.x * b.x + a.y * b.y;
}

/// Two-dimensional cross product: the z component of the cross product of `{a.x, a.y, 0}` and
/// `{b.x, b.y, 0}`, i.e. `a.x·b.y − a.y·b.x`. Positive when @p b is counter-clockwise from @p a,
/// negative when clockwise, and zero when they are parallel.
[[nodiscard]] constexpr double cross(const Vec2& a, const Vec2& b) noexcept {
    return a.x * b.y - a.y * b.x;
}

/// Squared length (magnitude): `dot(v, v)`.
[[nodiscard]] constexpr double length_squared(const Vec2& v) noexcept {
    return dot(v, v);
}

/// Length (Euclidean magnitude). Squaring overflows for components beyond about 1e154, where
/// the result becomes infinity.
[[nodiscard]] inline double length(const Vec2& v) noexcept {
    return std::sqrt(length_squared(v));
}

/// Squared distance between two points.
[[nodiscard]] constexpr double distance_squared(const Vec2& a, const Vec2& b) noexcept {
    return length_squared(b - a);
}

/// Distance between two points.
[[nodiscard]] inline double distance(const Vec2& a, const Vec2& b) noexcept {
    return length(b - a);
}

/// Returns whether every component is finite (neither NaN nor infinite).
[[nodiscard]] inline bool is_finite(const Vec2& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

/// @cond PROJECTILE_DETAIL
namespace detail {

/// Largest component magnitude, i.e. the maximum norm. Expects finite components.
[[nodiscard]] inline double max_abs_component(const Vec2& v) noexcept {
    return (std::max)(std::abs(v.x), std::abs(v.y));
}

} // namespace detail
/// @endcond

/// Returns the unit vector in the direction of @p v, or `std::nullopt` when @p v has no
/// direction: when it is the zero vector, or a component is NaN or infinite.
///
/// Every other vector is normalized accurately, however large or small its components, because
/// they are scaled by the largest magnitude before squaring.
[[nodiscard]] inline std::optional<Vec2> try_normalize(const Vec2& v) noexcept {
    if (!is_finite(v)) {
        return std::nullopt;
    }
    const double largest = detail::max_abs_component(v);
    if (largest == 0.0) {
        return std::nullopt;
    }
    // Divide rather than multiply by 1 / largest: that reciprocal overflows for subnormal input.
    const Vec2 scaled = v / largest;
    return scaled / std::sqrt(length_squared(scaled));
}

/// Returns whether @p v has unit length within @p tolerance, measured on the squared length:
/// `|‖v‖² − 1| ≤ tolerance`. False for vectors with NaN or infinite components.
[[nodiscard]] inline bool is_normalized(
    const Vec2& v, double tolerance = unit_length_tolerance
) noexcept {
    return std::abs(length_squared(v) - 1.0) <= tolerance;
}

/// Linear interpolation from @p a (at t = 0) to @p b (at t = 1). Values of @p t outside [0, 1]
/// extrapolate. Exact at both ends; uses std::lerp for each component.
[[nodiscard]] constexpr Vec2 lerp(const Vec2& a, const Vec2& b, double t) noexcept {
    return Vec2{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t)};
}

/// Reflects @p v off a line with unit normal @p normal: `v − 2 (v · normal) normal`. The result
/// has the same length as @p v. @p normal must have unit length.
[[nodiscard]] constexpr Vec2 reflect(const Vec2& v, const Vec2& normal) noexcept {
    return v - normal * (2.0 * dot(v, normal));
}

/// Projects @p v onto the line through the origin along @p onto, which need not be a unit
/// vector. Projecting onto the zero vector, which spans no line, yields the zero vector.
[[nodiscard]] constexpr Vec2 project(const Vec2& v, const Vec2& onto) noexcept {
    const double onto_length_squared = length_squared(onto);
    if (onto_length_squared == 0.0) {
        return Vec2{};
    }
    return onto * (dot(v, onto) / onto_length_squared);
}

/// Returns whether @p a and @p b are equal within a tolerance, with the same rule as the Vec3
/// overload: every component of `a − b` must be within
/// `max(absolute_tolerance, relative_tolerance × s)`, where `s` is the largest component
/// magnitude of either vector. Vectors with NaN or infinite components are approximately equal
/// only when they are exactly equal.
[[nodiscard]] inline bool approx_equal(
    const Vec2& a,
    const Vec2& b,
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
    return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
}

} // namespace projectile
