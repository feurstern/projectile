#pragma once

// Internal to the library, not installed: how one integration step meets the collision world.
// Trajectory prediction and Simulation both use these functions, so that a prediction made with
// a simulation's settings stops exactly where the simulated projectile does.

#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/ballistics/trajectory.hpp"
#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/collision/sweep.hpp"
#include "projectile/math/vec3.hpp"

#include <algorithm>
#include <optional>

namespace projectile::detail {

/// The sample at fraction @p s of a step of duration @p duration (greater than zero) that starts
/// at @p start_time in state @p from and ends in state @p to.
///
/// It lies on the cubic Hermite curve through both states' positions and velocities, which is
/// exact for motion under constant acceleration.
[[nodiscard]] inline TrajectorySample interpolate_step(
    const ProjectileState& from,
    const ProjectileState& to,
    double start_time,
    double duration,
    double s
) noexcept {
    const double s2 = s * s;
    const double s3 = s2 * s;
    // Hermite basis: the weight of `to` (from's weight is 1 minus it), the weights of the two
    // velocities, and the derivatives of all three with respect to s.
    const double weight_to = 3.0 * s2 - 2.0 * s3;
    const double weight_from_velocity = s3 - 2.0 * s2 + s;
    const double weight_to_velocity = s3 - s2;
    const double slope_to = 6.0 * s - 6.0 * s2;
    const double slope_from_velocity = 3.0 * s2 - 4.0 * s + 1.0;
    const double slope_to_velocity = 3.0 * s2 - 2.0 * s;

    const Vec3 chord = to.position - from.position;
    return TrajectorySample{
        .time = start_time + s * duration,
        .position =
            from.position + chord * weight_to +
            (from.velocity * weight_from_velocity + to.velocity * weight_to_velocity) * duration,
        .velocity = chord * (slope_to / duration) + from.velocity * slope_from_velocity +
                    to.velocity * slope_to_velocity,
    };
}

/// A contact found during a step.
struct StepContact {
    HitResult hit;         ///< The contact.
    double fraction = 0.0; ///< Fraction of the step, in distance and in time, before the contact.
};

/// Sweeps a projectile of @p radius along the straight line from its position in @p from to its
/// position in @p to through @p world, and returns the first contact.
[[nodiscard]] inline std::optional<StepContact> sweep_step(
    const ICollisionWorld& world,
    const ProjectileState& from,
    const ProjectileState& to,
    double radius
) {
    const std::optional<HitResult> hit = sweep_sphere(world, from.position, to.position, radius);
    if (!hit) {
        return std::nullopt;
    }
    const double length = distance(from.position, to.position);
    const double fraction = length > 0.0 ? std::clamp(hit->distance / length, 0.0, 1.0) : 0.0;
    return StepContact{.hit = *hit, .fraction = fraction};
}

/// The sample at the contact found at fraction @p fraction of the step from @p from to @p to.
/// Its position lies on the straight line the query swept, where the contact is; its velocity
/// follows the step's motion.
[[nodiscard]] inline TrajectorySample contact_sample(
    const ProjectileState& from,
    const ProjectileState& to,
    double start_time,
    double duration,
    double fraction
) noexcept {
    TrajectorySample sample = interpolate_step(from, to, start_time, duration, fraction);
    sample.position = lerp(from.position, to.position, fraction);
    return sample;
}

/// The state of a projectile configured by @p config that stopped at the contact found at
/// fraction @p fraction of the step of @p duration from @p from to @p to: inactive, at the
/// contact sample.
[[nodiscard]] inline ProjectileState contact_state(
    const ProjectileState& from,
    const ProjectileState& to,
    double duration,
    double fraction,
    const ProjectileConfig& config
) noexcept {
    const TrajectorySample sample = contact_sample(from, to, 0.0, duration, fraction);
    return ProjectileState{
        .position = sample.position,
        .velocity = sample.velocity,
        .acceleration = forces_of(config)(sample.position, sample.velocity),
        .remaining_lifetime = (std::max)(from.remaining_lifetime - fraction * duration, 0.0),
        .active = false,
    };
}

} // namespace projectile::detail
