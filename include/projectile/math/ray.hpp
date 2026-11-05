#pragma once

/// @file
/// @brief Rays: a start point and a unit direction.

#include "projectile/math/vec3.hpp"

#include <optional>

namespace projectile {

/// A ray from @ref origin along @ref direction, describing the points
///
///     P(t) = origin + t · direction,  t ≥ 0
///
/// By convention @ref direction has unit length, so that `t` is the distance from the origin.
/// Build rays from untrusted data with make_ray(), which normalizes the direction, and check
/// rays received from elsewhere with is_valid().
struct Ray {
    Vec3 origin;    ///< Start point.
    Vec3 direction; ///< Direction of travel; unit length by convention.

    /// Exact member-wise comparison.
    constexpr bool operator==(const Ray&) const = default;
};

/// Returns the point at parameter @p t along @p ray: `origin + t · direction`. For a ray with a
/// unit direction, @p t is the distance from the origin.
[[nodiscard]] constexpr Vec3 point_at(const Ray& ray, double t) noexcept {
    return ray.origin + ray.direction * t;
}

/// Creates a ray from @p origin along @p direction, normalizing the direction.
///
/// Returns `std::nullopt` if @p origin is not finite or @p direction has no direction (it is the
/// zero vector or not finite).
[[nodiscard]] inline std::optional<Ray> make_ray(
    const Vec3& origin, const Vec3& direction
) noexcept {
    if (!is_finite(origin)) {
        return std::nullopt;
    }
    const std::optional<Vec3> unit_direction = try_normalize(direction);
    if (!unit_direction) {
        return std::nullopt;
    }
    return Ray{.origin = origin, .direction = *unit_direction};
}

/// Returns whether @p ray satisfies the Ray invariants: a finite origin and a unit-length
/// direction (within unit_length_tolerance).
[[nodiscard]] inline bool is_valid(const Ray& ray) noexcept {
    return is_finite(ray.origin) && is_normalized(ray.direction);
}

} // namespace projectile
