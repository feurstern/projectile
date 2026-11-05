#pragma once

/// @file
/// @brief Spheres.

#include "projectile/math/vec3.hpp"

#include <cmath>

namespace projectile {

/// A solid sphere: all points within @ref radius of @ref center.
///
/// A radius of zero is valid and describes a single point; a negative radius is invalid.
struct Sphere {
    Vec3 center;         ///< Center point.
    double radius = 0.0; ///< Radius; zero or positive.

    /// Exact member-wise comparison.
    constexpr bool operator==(const Sphere&) const = default;
};

/// Returns whether @p sphere satisfies the Sphere invariants: a finite center and a finite,
/// non-negative radius.
[[nodiscard]] inline bool is_valid(const Sphere& sphere) noexcept {
    return is_finite(sphere.center) && std::isfinite(sphere.radius) && sphere.radius >= 0.0;
}

/// Returns whether @p point lies inside @p sphere or on its surface. The comparison uses squared
/// distances, so a point computed to lie exactly on the surface may fall either side by rounding.
[[nodiscard]] constexpr bool contains(const Sphere& sphere, const Vec3& point) noexcept {
    return distance_squared(sphere.center, point) <= sphere.radius * sphere.radius;
}

} // namespace projectile
