#pragma once

/// @file
/// @brief Kinetic energy of projectiles, the currency of material interactions.

#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>

namespace projectile {

/// Kinetic energy `½ m |v|²`, in J, of a body of @p mass kg moving at @p velocity m/s.
[[nodiscard]] constexpr double kinetic_energy(double mass, const Vec3& velocity) noexcept {
    return 0.5 * mass * length_squared(velocity);
}

/// Kinetic energy, in J, of the projectile in @p state configured by @p config.
[[nodiscard]] constexpr double kinetic_energy(
    const ProjectileState& state, const ProjectileConfig& config
) noexcept {
    return kinetic_energy(config.mass, state.velocity);
}

/// The speed `√(2E / m)`, in m/s, at which a body of @p mass kg has @p energy J of kinetic
/// energy; zero for an energy of zero or less. @p mass must be greater than zero.
[[nodiscard]] inline double speed_for_energy(double mass, double energy) noexcept {
    return energy > 0.0 ? std::sqrt(2.0 * energy / mass) : 0.0;
}

} // namespace projectile
