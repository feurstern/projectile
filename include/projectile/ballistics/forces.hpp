#pragma once

/// @file
/// @brief The forces acting on projectiles, expressed as acceleration fields.

#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/math/vec3.hpp"

#include <type_traits>

namespace projectile {

/// An acceleration field: anything callable as `field(position, velocity)` that returns the
/// acceleration, in m/s², of a body at that position moving at that velocity.
///
/// The integrators advance projectiles under any acceleration field. Pass your own to model
/// forces Projectile does not provide, such as homing or buoyancy.
template <typename Field>
concept AccelerationField = std::is_invocable_r_v<Vec3, const Field&, const Vec3&, const Vec3&>;

/// The forces Projectile applies to a projectile, as an acceleration field: uniform gravity, and
/// quadratic air drag against the velocity relative to the wind,
///
///     a = gravity − drag_factor · |v − wind| · (v − wind)
///
/// where `drag_factor = ½ ρ C_d A / m` (see drag_factor()). Without drag this is gravity alone,
/// computed exactly as such.
struct ProjectileForces {
    Vec3 gravity;             ///< Gravitational acceleration in m/s².
    double drag_factor = 0.0; ///< `½ ρ C_d A / m`, in 1/m. Zero means no drag.
    Vec3 wind;                ///< Velocity of the air in m/s.

    /// The acceleration of a projectile at @p position moving at @p velocity.
    [[nodiscard]] Vec3 operator()(const Vec3& /*position*/, const Vec3& velocity) const noexcept {
        if (drag_factor == 0.0) {
            return gravity;
        }
        const Vec3 relative = velocity - wind;
        return gravity - relative * (drag_factor * length(relative));
    }
};

/// The drag factor `½ ρ C_d A / m` of projectiles configured by @p config, in 1/m: the drag
/// acceleration per squared unit of speed relative to the air. Zero without drag.
[[nodiscard]] constexpr double drag_factor(const ProjectileConfig& config) noexcept {
    return 0.5 * config.air_density * config.drag_coefficient * config.reference_area / config.mass;
}

/// The forces acting on projectiles configured by @p config.
[[nodiscard]] constexpr ProjectileForces forces_of(const ProjectileConfig& config) noexcept {
    return ProjectileForces{
        .gravity = config.gravity,
        .drag_factor = drag_factor(config),
        .wind = config.wind,
    };
}

} // namespace projectile
