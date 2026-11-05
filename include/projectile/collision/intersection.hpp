#pragma once

/// @file
/// @brief Intersection tests between rays or segments and the geometric primitives.
///
/// Every function returns the first contact along the query as a HitResult, or `std::nullopt`
/// when there is none. All of them share the conventions described in docs/collision.md:
///
/// - `distance` is measured from the start of the query along its direction;
/// - `normal` is a unit vector facing the side the query came from;
/// - `front_face` tells which side of the surface was hit.
///
/// Planes and triangles are two-sided surfaces; a query that starts on one hits it at distance
/// 0. Spheres and boxes are solids; a query that starts on their surface hits them only when it
/// moves inwards, and a query that starts strictly inside reports where it leaves (a back face).
/// Queries parallel to a plane or triangle, zero-length segments and zero-area triangles never
/// hit.
///
/// These functions do not validate their input, so that they stay cheap in inner loops. Their
/// results are meaningful only for valid input: a valid ray (`is_valid(ray)`), finite segment
/// endpoints, valid shapes (`is_valid(shape)`), and `max_distance ≥ 0`. Validate data from
/// outside the program once, with the is_valid() functions.

#include "projectile/collision/hit_result.hpp"
#include "projectile/core/export.hpp"
#include "projectile/math/aabb.hpp"
#include "projectile/math/plane.hpp"
#include "projectile/math/ray.hpp"
#include "projectile/math/segment.hpp"
#include "projectile/math/sphere.hpp"
#include "projectile/math/triangle.hpp"

#include <cstdint>
#include <limits>
#include <optional>

namespace projectile {

/// Which sides of a surface a query can hit.
enum class Culling : std::uint8_t {
    /// Report hits on either side of a surface (the default).
    none,
    /// Ignore back faces: planes and triangles hit from behind, and the surface of a sphere or
    /// box that a query reaches from inside.
    back_faces,
};

/// @name Ray queries
/// Return the first contact along @p ray at a distance in `[0, max_distance]`; @p max_distance
/// may be infinite.
/// @{

/// First contact of @p ray with @p plane. A ray parallel to the plane never hits it, even when
/// it lies in the plane.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Ray& ray,
    const Plane& plane,
    double max_distance = std::numeric_limits<double>::infinity(),
    Culling culling = Culling::none
) noexcept;

/// First contact of @p ray with @p sphere: where it enters the sphere, or, if the ray starts
/// inside, where it leaves it (a back face).
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Ray& ray,
    const Sphere& sphere,
    double max_distance = std::numeric_limits<double>::infinity(),
    Culling culling = Culling::none
) noexcept;

/// First contact of @p ray with @p box: where it enters the box, or, if the ray starts inside,
/// where it leaves it (a back face). At an edge or corner, the normal is that of one of the
/// faces meeting there.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Ray& ray,
    const AABB& box,
    double max_distance = std::numeric_limits<double>::infinity(),
    Culling culling = Culling::none
) noexcept;

/// First contact of @p ray with @p triangle. Edges and vertices belong to the triangle. A ray
/// parallel to the triangle's plane never hits it.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Ray& ray,
    const Triangle& triangle,
    double max_distance = std::numeric_limits<double>::infinity(),
    Culling culling = Culling::none
) noexcept;

/// @}

/// @name Segment queries
/// Return the first contact along @p segment, from its start to its end (inclusive), with
/// `distance` measured from the start. A segment query against a shape gives the same result as
/// a ray from the segment's start, towards its end, limited to the segment's length.
/// @{

/// First contact of @p segment with @p plane. An endpoint that lies on the plane is hit exactly
/// at that endpoint.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Segment& segment, const Plane& plane, Culling culling = Culling::none
) noexcept;

/// First contact of @p segment with @p sphere.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Segment& segment, const Sphere& sphere, Culling culling = Culling::none
) noexcept;

/// First contact of @p segment with @p box.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Segment& segment, const AABB& box, Culling culling = Culling::none
) noexcept;

/// First contact of @p segment with @p triangle.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> intersect(
    const Segment& segment, const Triangle& triangle, Culling culling = Culling::none
) noexcept;

/// @}

} // namespace projectile
