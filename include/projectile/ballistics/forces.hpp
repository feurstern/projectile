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

/// The forces Projectile applies to a projectile, as an acceleration field: uniform gravity.
struct ProjectileForces {
    Vec3 gravity; ///< Gravitational acceleration in m/s².

    /// The acceleration at @p position and @p velocity: gravity, the same everywhere.
    [[nodiscard]] constexpr Vec3
    operator()(const Vec3& /*position*/, const Vec3& /*velocity*/) const noexcept {
        return gravity;
    }
};

/// The forces acting on projectiles configured by @p config.
[[nodiscard]] constexpr ProjectileForces forces_of(const ProjectileConfig& config) noexcept {
    return ProjectileForces{.gravity = config.gravity};
}

} // namespace projectile
