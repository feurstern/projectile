#pragma once

/// @file
/// @brief The interface through which Projectile queries a game's collision geometry.

#include "projectile/collision/hit_result.hpp"
#include "projectile/math/ray.hpp"
#include "projectile/math/vec3.hpp"

#include <optional>

namespace projectile {

/// Collision geometry that Projectile can query, implemented by the application.
///
/// Projectile never owns a game's geometry. Implement this interface on top of whatever holds
/// it, such as a list of shapes, a bounding volume hierarchy or an engine's physics scene, and
/// query it through the free functions raycast() and sweep_sphere(). Those validate their
/// arguments first, so implementations may assume valid input.
///
/// Implementations must follow the query conventions of docs/collision.md, which the
/// intersect() functions also follow:
/// - report the first contact along the query, with `material_id` set;
/// - never report contacts outside the queried range;
/// - not throw.
///
/// Queries are `const`. An implementation that is not modified during a query can be queried
/// from several threads at once.
class ICollisionWorld {
public:
    virtual ~ICollisionWorld() = default;

    /// Returns the first contact along @p ray at a distance in `[0, max_distance]`, or
    /// `std::nullopt`. Called with `is_valid(ray)` and `max_distance ≥ 0`, possibly infinite.
    [[nodiscard]] virtual std::optional<HitResult> raycast(
        const Ray& ray, double max_distance
    ) const = 0;

    /// Returns the first contact of a sphere of @p radius whose center moves in a straight line
    /// from @p start to @p end, or `std::nullopt`.
    ///
    /// The result is that of a segment query of the center against every shape enlarged by
    /// @p radius, expressed for the original shape:
    /// - `distance` is how far the center travels before the contact;
    /// - `point` is the contact point on the surface of the shape;
    /// - `normal` is the shape's unit surface normal there, facing against the motion;
    /// - `front_face` is false if the sphere already overlaps the shape at @p start; the contact
    ///   is then where it stops overlapping.
    ///
    /// Called with finite endpoints and a finite `radius ≥ 0`. A radius of zero makes this a
    /// segment query.
    [[nodiscard]] virtual std::optional<HitResult> sweep_sphere(
        const Vec3& start, const Vec3& end, double radius
    ) const = 0;

protected:
    ICollisionWorld() = default;
    ICollisionWorld(const ICollisionWorld&) = default;
    ICollisionWorld(ICollisionWorld&&) = default;
    ICollisionWorld& operator=(const ICollisionWorld&) = default;
    ICollisionWorld& operator=(ICollisionWorld&&) = default;
};

} // namespace projectile
