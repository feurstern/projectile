#pragma once

/// @file
/// @brief Scalar math helpers and the default tolerances for floating-point comparisons.
///
/// See docs/mathematics.md for the numerical conventions used throughout Projectile.

#include <algorithm>
#include <cmath>
#include <numbers>

namespace projectile {

/// Default absolute tolerance of approx_equal(). Differences up to this size count as equal
/// whatever the magnitude of the values; in SI units it is a picometre, or a picometre per
/// second, and so on.
inline constexpr double default_absolute_tolerance = 1e-12;

/// Default relative tolerance of approx_equal(): roughly 4500 units in the last place of a
/// double, enough to absorb the rounding of a few dozen arithmetic operations.
inline constexpr double default_relative_tolerance = 1e-9;

/// Default tolerance of is_normalized(), applied to the squared length: |‖v‖² − 1| ≤ 1e-6.
/// Directions normalized in single precision, as many engines store them, pass this check.
inline constexpr double unit_length_tolerance = 1e-6;

/// Returns whether @p a and @p b are equal within a tolerance, that is, whether
///
///     |a − b| ≤ max(absolute_tolerance, relative_tolerance × max(|a|, |b|))
///
/// The absolute tolerance handles values near zero, the relative one large values. NaN is never
/// approximately equal to anything, and an infinity only to the same infinity. Both tolerances
/// are expected to be non-negative.
[[nodiscard]] inline bool approx_equal(
    double a,
    double b,
    double absolute_tolerance = default_absolute_tolerance,
    double relative_tolerance = default_relative_tolerance
) noexcept {
    if (a == b) {
        return true; // also covers equal infinities
    }
    if (!std::isfinite(a) || !std::isfinite(b)) {
        return false;
    }
    const double scale = (std::max)(std::abs(a), std::abs(b));
    return std::abs(a - b) <= (std::max)(absolute_tolerance, relative_tolerance * scale);
}

/// Converts an angle from degrees to radians. Projectile's API takes angles in radians.
[[nodiscard]] constexpr double degrees_to_radians(double degrees) noexcept {
    return degrees * (std::numbers::pi / 180.0);
}

/// Converts an angle from radians to degrees.
[[nodiscard]] constexpr double radians_to_degrees(double radians) noexcept {
    return radians * (180.0 / std::numbers::pi);
}

} // namespace projectile
