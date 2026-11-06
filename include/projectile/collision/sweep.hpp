#pragma once

/// @file
/// @brief Swept-sphere queries: the first contact of a moving sphere, against single shapes or a
/// collision world.
///
/// A sphere that moves in a straight line touches a shape exactly when its center comes within
/// its radius of the shape. A sweep is therefore a segment query of the center against the shape
/// enlarged by the radius, and the queries below compute exactly that: for a box the enlarged
/// shape has rounded edges and corners, for a triangle rounded edges. Checking the whole path,
/// not just where the sphere ends up, means that fast projectiles cannot pass through thin
/// objects between two steps.

#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/collision/intersection.hpp"
#include "projectile/core/export.hpp"
#include "projectile/math/aabb.hpp"
#include "projectile/math/plane.hpp"
#include "projectile/math/segment.hpp"
#include "projectile/math/sphere.hpp"
#include "projectile/math/triangle.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>
#include <optional>

namespace projectile {

/// @name Swept-sphere queries against single shapes
///
/// Return the first contact of a sphere of @p radius whose center moves along @p path, from its
/// start to its end:
///
/// - `distance` is how far the center travels before the contact;
/// - `point` is the contact point on the surface of the shape;
/// - `normal` is the shape's unit surface normal there, facing the side the sphere came from;
/// - `front_face` follows intersect(): it is false for planes and triangles reached from behind,
///   and for every shape that the sphere already overlaps at the start of the path. The contact
///   is then where the sphere stops overlapping the shape.
///
/// A sphere that starts touching a shape hits it at distance 0 when it moves into it, and not at
/// all when it moves away or slides along its surface. A path of zero length never hits, and
/// neither does a triangle of zero area. A radius of zero gives exactly the result of the
/// segment query `intersect(path, shape, culling)`, which has the rules of docs/collision.md for
/// starting on a surface.
///
/// Like intersect(), these functions do not validate their input: they expect finite path
/// endpoints, a finite `radius ≥ 0` and a valid shape (see the is_valid() functions).
/// @{

/// First contact of a sphere moving along @p path with @p plane.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const Plane& plane, Culling culling = Culling::none
) noexcept;

/// First contact of a sphere moving along @p path with @p sphere.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const Sphere& sphere, Culling culling = Culling::none
) noexcept;

/// First contact of a sphere moving along @p path with @p box. At the box's edges and corners,
/// the normal points from the box to the sphere's center.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const AABB& box, Culling culling = Culling::none
) noexcept;

/// First contact of a sphere moving along @p path with @p triangle. At the triangle's edges and
/// vertices, the normal points from the triangle to the sphere's center; such a contact counts
/// as a front face unless the sphere's center is behind the triangle's plane.
[[nodiscard]] PROJECTILE_API std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const Triangle& triangle, Culling culling = Culling::none
) noexcept;

/// @}

/// Sweeps a sphere of @p radius with its center moving from @p start to @p end through
/// @p world, and returns the first contact or `std::nullopt`. See
/// ICollisionWorld::sweep_sphere() for the meaning of the result.
///
/// This is the validated entry point for swept queries. It returns `std::nullopt` without
/// querying the world when an endpoint is not finite, or @p radius is negative, NaN or infinite.
[[nodiscard]] inline std::optional<HitResult> sweep_sphere(
    const ICollisionWorld& world, const Vec3& start, const Vec3& end, double radius
) {
    if (!is_finite(start) || !is_finite(end) || !std::isfinite(radius) || radius < 0.0) {
        return std::nullopt;
    }
    return world.sweep_sphere(start, end, radius);
}

} // namespace projectile
