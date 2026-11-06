#pragma once

/// @file
/// @brief Triangles.

#include "projectile/math/segment.hpp"
#include "projectile/math/vec3.hpp"

#include <array>
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

/// The point of @p triangle closest to @p point: a vertex, a point on an edge, or a point inside
/// the triangle. Triangles with zero area are treated as the segments between their vertices.
[[nodiscard]] constexpr Vec3 closest_point(const Triangle& triangle, const Vec3& point) noexcept {
    // Ericson, Real-Time Collision Detection, section 5.1.5: find the Voronoi region of the
    // triangle that contains the point, testing the vertices first, then the edges.
    const Vec3 ab = triangle.b - triangle.a;
    const Vec3 ac = triangle.c - triangle.a;
    const Vec3 ap = point - triangle.a;
    const double d1 = dot(ab, ap);
    const double d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        return triangle.a;
    }
    const Vec3 bp = point - triangle.b;
    const double d3 = dot(ab, bp);
    const double d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) {
        return triangle.b;
    }
    const Vec3 cp = point - triangle.c;
    const double d5 = dot(ab, cp);
    const double d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) {
        return triangle.c;
    }
    // Barycentric weights, each scaled by the triangle's doubled area squared.
    const double vc = d1 * d4 - d3 * d2;
    const double vb = d5 * d2 - d1 * d6;
    const double va = d3 * d6 - d5 * d4;
    if (!(va + vb + vc > 0.0)) {
        // Zero area: the closest point lies on one of the edges.
        const std::array<Vec3, 3> candidates{
            closest_point(Segment{.start = triangle.a, .end = triangle.b}, point),
            closest_point(Segment{.start = triangle.b, .end = triangle.c}, point),
            closest_point(Segment{.start = triangle.c, .end = triangle.a}, point),
        };
        Vec3 nearest = candidates.front();
        for (const Vec3& candidate : candidates) {
            if (distance_squared(candidate, point) < distance_squared(nearest, point)) {
                nearest = candidate;
            }
        }
        return nearest;
    }
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        return closest_point(Segment{.start = triangle.a, .end = triangle.b}, point);
    }
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        return closest_point(Segment{.start = triangle.a, .end = triangle.c}, point);
    }
    if (va <= 0.0 && d4 - d3 >= 0.0 && d5 - d6 >= 0.0) {
        return closest_point(Segment{.start = triangle.b, .end = triangle.c}, point);
    }
    const double scale = va + vb + vc;
    return triangle.a + ab * (vb / scale) + ac * (vc / scale);
}

} // namespace projectile
