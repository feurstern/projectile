#pragma once

/// @file
/// @brief Trajectory prediction: where a projectile will go, sampled over time.

#include "projectile/ballistics/integrator.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/core/export.hpp"
#include "projectile/core/result.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace projectile {

/// Exact position after @p time seconds of a body starting at @p position with @p velocity under
/// the constant @p acceleration:
///
///     position + velocity · t + ½ · acceleration · t²
///
/// Under gravity alone this is the true path, against which numerical results can be compared.
[[nodiscard]] constexpr Vec3 ballistic_position(
    const Vec3& position, const Vec3& velocity, const Vec3& acceleration, double time
) noexcept {
    return position + velocity * time + acceleration * (0.5 * time * time);
}

/// Exact velocity after @p time seconds of a body starting with @p velocity under the constant
/// @p acceleration: `velocity + acceleration · t`.
[[nodiscard]] constexpr Vec3 ballistic_velocity(
    const Vec3& velocity, const Vec3& acceleration, double time
) noexcept {
    return velocity + acceleration * time;
}

/// How to predict a trajectory.
///
/// To make a prediction match a simulation exactly, use the simulation's time step and
/// integrator: the prediction then repeats the simulation's steps.
struct TrajectoryConfig {
    /// Time between samples, in s; finite and greater than zero.
    double sample_interval = 1.0 / 60.0;

    /// How far ahead to predict, in s; finite and zero or greater.
    double max_duration = 5.0;

    /// Integration time step, in s; finite and greater than zero.
    double time_step = 1.0 / 120.0;

    /// The integrator to step with.
    IntegratorKind integrator = IntegratorKind::semi_implicit_euler;
};

/// Checks @p config and returns the first problem found, or `std::nullopt` if it is valid.
[[nodiscard]] inline std::optional<Error> validate(const TrajectoryConfig& config) noexcept {
    if (!std::isfinite(config.sample_interval) || !(config.sample_interval > 0.0)) {
        return Error::invalid_sample_interval;
    }
    if (!std::isfinite(config.max_duration) || config.max_duration < 0.0) {
        return Error::invalid_duration;
    }
    if (!std::isfinite(config.time_step) || !(config.time_step > 0.0)) {
        return Error::invalid_time_step;
    }
    return std::nullopt;
}

/// The projectile's center and velocity at one moment of a predicted trajectory.
struct TrajectorySample {
    double time = 0.0; ///< Time since the start of the prediction, in s.
    Vec3 position;     ///< Position of the projectile's center, in m.
    Vec3 velocity;     ///< Velocity, in m/s.

    /// Exact member-wise comparison.
    constexpr bool operator==(const TrajectorySample&) const = default;
};

/// Why a prediction ended.
enum class TrajectoryEnd : std::uint8_t {
    max_duration, ///< The prediction covered TrajectoryConfig::max_duration.
    lifetime,     ///< The projectile expired, or was inactive from the start.
    hit,          ///< The projectile hit something in the collision world.
};

/// A predicted trajectory.
struct Trajectory {
    /// Samples in order of time: the start, one every sample interval, and a final sample where
    /// the prediction ended. No two samples share a time.
    std::vector<TrajectorySample> samples;

    /// Why the prediction ended.
    TrajectoryEnd end = TrajectoryEnd::max_duration;

    /// The first contact, when `end` is TrajectoryEnd::hit. The final sample is then the
    /// projectile's center at the contact.
    std::optional<HitResult> hit;
};

/// Predicts where the projectile in @p state will go under the forces of @p config, stepping as
/// described by @p trajectory_config.
///
/// The steps are those a simulation with the same time step and integrator would take. Samples
/// between steps are interpolated along the cubic curve through the neighbouring steps' positions
/// and velocities, which is exact under gravity alone. Returns the first problem found instead
/// if a configuration or @p state is invalid.
[[nodiscard]] PROJECTILE_API Result<Trajectory> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config
);

/// Like the overload above, but stops at the first contact with @p world. Each step sweeps a
/// sphere of `config.radius` along the straight line from one step's position to the next, as
/// a simulation would.
[[nodiscard]] PROJECTILE_API Result<Trajectory> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config,
    const ICollisionWorld& world
);

/// Like predict_trajectory() without a world, but writes into @p out, reusing the memory of its
/// samples. Updating a preview every frame this way allocates nothing once the buffer is large
/// enough. Returns the first problem found, leaving @p out unchanged, or `std::nullopt`.
[[nodiscard]] PROJECTILE_API std::optional<Error> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config,
    Trajectory& out
);

/// Like predict_trajectory() with a world, but writes into @p out, reusing the memory of its
/// samples. Returns the first problem found, leaving @p out unchanged, or `std::nullopt`.
[[nodiscard]] PROJECTILE_API std::optional<Error> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config,
    const ICollisionWorld& world,
    Trajectory& out
);

} // namespace projectile
