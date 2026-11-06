#pragma once

/// @file
/// @brief The changing state of a projectile, and how to create one.

#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/core/result.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>
#include <limits>
#include <optional>

namespace projectile {

/// Where a projectile is and how it moves (SI units). The integrators update it every step.
///
/// Its parameters, such as mass, radius and gravity, live in the ProjectileConfig it was
/// created from.
struct ProjectileState {
    Vec3 position; ///< Position of the center, in m.
    Vec3 velocity; ///< Velocity, in m/s.

    /// Acceleration at the current position and velocity, in m/s². The integrators keep it up to
    /// date for display and diagnostics, but do not read it: they evaluate the forces anew.
    Vec3 acceleration;

    /// Time left before the projectile expires, in s; infinite for projectiles that never do.
    double remaining_lifetime = std::numeric_limits<double>::infinity();

    /// False once the projectile has expired. Integrators do not move inactive projectiles.
    bool active = true;

    /// Exact member-wise comparison.
    constexpr bool operator==(const ProjectileState&) const = default;
};

/// Checks @p state and returns the first problem found, or `std::nullopt` if it is valid: a
/// finite position and velocity, and a remaining lifetime that is zero or greater (infinity is
/// allowed).
[[nodiscard]] inline std::optional<Error> validate(const ProjectileState& state) noexcept {
    if (!is_finite(state.position)) {
        return Error::invalid_position;
    }
    if (!is_finite(state.velocity)) {
        return Error::invalid_velocity;
    }
    if (!(state.remaining_lifetime >= 0.0)) { // rejects NaN; infinity is allowed
        return Error::invalid_lifetime;
    }
    return std::nullopt;
}

/// Creates an active projectile at @p position moving at @p velocity, with the full lifetime of
/// @p config. Returns the first problem found instead if @p config is invalid, or @p position or
/// @p velocity is not finite.
[[nodiscard]] inline Result<ProjectileState> create_projectile(
    const ProjectileConfig& config, const Vec3& position, const Vec3& velocity
) noexcept {
    if (const std::optional<Error> error = validate(config)) {
        return *error;
    }
    if (!is_finite(position)) {
        return Error::invalid_position;
    }
    if (!is_finite(velocity)) {
        return Error::invalid_velocity;
    }
    return ProjectileState{
        .position = position,
        .velocity = velocity,
        .acceleration = forces_of(config)(position, velocity),
        .remaining_lifetime = config.lifetime,
        .active = true,
    };
}

/// Creates an active projectile at @p position moving at `config.initial_velocity`. See the
/// overload above.
[[nodiscard]] inline Result<ProjectileState> create_projectile(
    const ProjectileConfig& config, const Vec3& position
) noexcept {
    return create_projectile(config, position, config.initial_velocity);
}

} // namespace projectile
