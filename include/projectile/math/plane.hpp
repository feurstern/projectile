#pragma once

/// @file
/// @brief Infinite planes in normal-distance form.

#include "projectile/math/vec3.hpp"

#include <cmath>
#include <optional>

namespace projectile {

/// The infinite plane of all points `p` with `dot(normal, p) == distance`.
///
/// @ref normal has unit length and points to the plane's front side. @ref distance is the signed
/// distance from the coordinate origin to the plane, measured along @ref normal. For example,
/// `Plane{.normal = {0.0, 1.0, 0.0}, .distance = 2.0}` is the plane `y = 2` facing +y. Build
/// planes with make_plane() or plane_through_points(), which normalize the normal.
struct Plane {
    Vec3 normal;           ///< Unit normal, pointing to the front side.
    double distance = 0.0; ///< Signed distance from the origin to the plane along @ref normal.

    /// Exact member-wise comparison.
    constexpr bool operator==(const Plane&) const = default;
};

/// Creates the plane through @p point with the given @p normal, normalizing the normal.
///
/// Returns `std::nullopt` if @p normal has no direction (it is the zero vector or not finite),
/// if @p point is not finite, or if the plane's distance from the origin overflows.
[[nodiscard]] inline std::optional<Plane> make_plane(
    const Vec3& normal, const Vec3& point
) noexcept {
    if (!is_finite(point)) {
        return std::nullopt;
    }
    const std::optional<Vec3> unit_normal = try_normalize(normal);
    if (!unit_normal) {
        return std::nullopt;
    }
    const double distance = dot(*unit_normal, point);
    if (!std::isfinite(distance)) {
        return std::nullopt;
    }
    return Plane{.normal = *unit_normal, .distance = distance};
}

/// Creates the plane through three points. The normal follows the right-hand rule: it points
/// towards the side from which @p a, @p b, @p c appear counter-clockwise.
///
/// Returns `std::nullopt` when the points span no plane, because they are collinear or
/// coincide, or when a point is not finite. Nearly collinear points give a less precise normal.
[[nodiscard]] inline std::optional<Plane> plane_through_points(
    const Vec3& a, const Vec3& b, const Vec3& c
) noexcept {
    if (!is_finite(a) || !is_finite(b) || !is_finite(c)) {
        return std::nullopt;
    }
    return make_plane(cross(b - a, c - a), a);
}

/// Signed distance from @p plane to @p point: positive in front of the plane (the side the
/// normal points to), negative behind it, zero on it.
[[nodiscard]] constexpr double signed_distance(const Plane& plane, const Vec3& point) noexcept {
    return dot(plane.normal, point) - plane.distance;
}

/// The point on @p plane closest to @p point: its orthogonal projection onto the plane.
[[nodiscard]] constexpr Vec3 closest_point(const Plane& plane, const Vec3& point) noexcept {
    return point - plane.normal * signed_distance(plane, point);
}

/// Returns whether @p plane satisfies the Plane invariants: a unit-length normal (within
/// unit_length_tolerance) and a finite distance.
[[nodiscard]] inline bool is_valid(const Plane& plane) noexcept {
    return is_normalized(plane.normal) && std::isfinite(plane.distance);
}

} // namespace projectile
