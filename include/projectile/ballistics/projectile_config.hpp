#pragma once

/// @file
/// @brief The parameters of a projectile.

#include "projectile/core/constants.hpp"
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
///
/// By default a projectile feels gravity only. Setting a drag coefficient and a reference area
/// adds air resistance, which also makes it drift with the wind; see docs/ballistics.md.
struct ProjectileConfig {
    /// Mass in kg; finite and greater than zero. Gravity alone accelerates every mass equally;
    /// the mass matters to drag and to impact energy.
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

    /// Drag coefficient `C_d` (dimensionless); finite and zero or greater. Zero, the default,
    /// turns air resistance off. Typical values: about 0.47 for a sphere, 0.2 to 0.4 for bullets.
    double drag_coefficient = 0.0;

    /// Reference area `A` in m² that drag acts on, usually the frontal cross-section; finite and
    /// zero or greater, and greater than zero when `drag_coefficient` is. For a sphere of radius
    /// `r`, `π · r²`.
    double reference_area = 0.0;

    /// Density `ρ` of the air in kg/m³; finite and zero or greater. Sea level by default.
    double air_density = standard_air_density;

    /// Velocity of the air in m/s. Drag acts on the projectile's velocity relative to the air,
    /// so wind matters only when there is drag.
    Vec3 wind;

    /// Exact member-wise comparison.
    constexpr bool operator==(const ProjectileConfig&) const = default;
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
    if (!std::isfinite(config.drag_coefficient) || config.drag_coefficient < 0.0) {
        return Error::invalid_drag_coefficient;
    }
    if (!std::isfinite(config.reference_area) || config.reference_area < 0.0 ||
        (config.drag_coefficient > 0.0 && config.reference_area == 0.0)) {
        return Error::invalid_reference_area;
    }
    if (!std::isfinite(config.air_density) || config.air_density < 0.0) {
        return Error::invalid_air_density;
    }
    if (!is_finite(config.wind)) {
        return Error::invalid_wind;
    }
    return std::nullopt;
}

} // namespace projectile
