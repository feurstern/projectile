#pragma once

/// @file
/// @brief Triangles.

#include "projectile/math/vec3.hpp"

#include <optional>

namespace projectile {

/// A triangle with the vertices @ref a, @ref b and @ref c.
///
/// The order of the vertices defines the front side by the right-hand rule: seen from the front,
/// `a → b → c` runs counter-clockwise, and face_normal() points to the front. Collision queries
/// use this to tell front faces from back faces.
///
/// A triangle whose vertices are collinear or coincide has zero area. It is valid input but has
/// no surface, so collision queries never hit it.
struct Triangle {
    Vec3 a; ///< First vertex.
    Vec3 b; ///< Second vertex.
    Vec3 c; ///< Third vertex.

    /// Exact member-wise comparison.
    constexpr bool operator==(const Triangle&) const = default;
};

/// Unit normal of the front side of @p triangle, following the right-hand rule, or
/// `std::nullopt` if the triangle has zero area.
[[nodiscard]] inline std::optional<Vec3> face_normal(const Triangle& triangle) noexcept {
    return try_normalize(cross(triangle.b - triangle.a, triangle.c - triangle.a));
}

/// Returns whether every vertex of @p triangle is finite. Triangles with zero area are valid.
[[nodiscard]] inline bool is_valid(const Triangle& triangle) noexcept {
    return is_finite(triangle.a) && is_finite(triangle.b) && is_finite(triangle.c);
}

} // namespace projectile
