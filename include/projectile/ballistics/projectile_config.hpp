#pragma once

/// @file
/// @brief The parameters of a projectile.

#include "projectile/core/result.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>
#include <limits>
#include <optional>

namespace projectile {

/// The parameters of a projectile, which do not change as it flies (SI units throughout).
///
/// A projectile is described by two parts. ProjectileConfig holds what it is and the world it
/// moves in; ProjectileState holds where it is and how it moves, and changes every step. Many
/// projectiles can share one configuration.
struct ProjectileConfig {
    /// Mass in kg; finite and greater than zero. Gravity alone accelerates every mass equally;
    /// the mass matters to features that depend on it, such as drag and impact energy.
    double mass = 1.0;

    /// Radius in m; zero or greater. Collision queries sweep a sphere of this radius, and a
    /// radius of zero makes the projectile a point.
    double radius = 0.0;

    /// Velocity in m/s given to projectiles by create_projectile() unless it is passed one.
    Vec3 initial_velocity;

    /// Gravitational acceleration in m/s². There is no default "up": set the vector for your
    /// world, for example `{0.0, -standard_gravity, 0.0}` for +y up. Zero means no gravity.
    Vec3 gravity;

    /// How long the projectile exists, in s; greater than zero. Infinite by default.
    double lifetime = std::numeric_limits<double>::infinity();
};

/// Checks @p config and returns the first problem found, or `std::nullopt` if it is valid.
[[nodiscard]] inline std::optional<Error> validate(const ProjectileConfig& config) noexcept {
    if (!std::isfinite(config.mass) || !(config.mass > 0.0)) {
        return Error::invalid_mass;
    }
    if (!std::isfinite(config.radius) || config.radius < 0.0) {
        return Error::invalid_radius;
    }
    if (!is_finite(config.initial_velocity)) {
        return Error::invalid_velocity;
    }
    if (!is_finite(config.gravity)) {
        return Error::invalid_gravity;
    }
    if (!(config.lifetime > 0.0)) { // rejects NaN; infinity is allowed
        return Error::invalid_lifetime;
    }
    return std::nullopt;
}

} // namespace projectile
