#pragma once

/// @file
/// @brief Ray queries against a collision world: hitscan.

#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/math/ray.hpp"

#include <optional>

namespace projectile {

/// Casts @p ray into @p world and returns the first contact within @p max_distance, or
/// `std::nullopt`.
///
/// This is the validated entry point for hitscan queries. It returns `std::nullopt` without
/// querying the world when @p ray is invalid (see is_valid()) or @p max_distance is negative or
/// NaN. An infinite @p max_distance is allowed.
[[nodiscard]] inline std::optional<HitResult> raycast(
    const ICollisionWorld& world, const Ray& ray, double max_distance
) {
    if (!is_valid(ray) || !(max_distance >= 0.0)) {
        return std::nullopt;
    }
    return world.raycast(ray, max_distance);
}

} // namespace projectile
