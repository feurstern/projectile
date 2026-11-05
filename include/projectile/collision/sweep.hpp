#pragma once

/// @file
/// @brief Swept-sphere queries against a collision world.

#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>
#include <optional>

namespace projectile {

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
