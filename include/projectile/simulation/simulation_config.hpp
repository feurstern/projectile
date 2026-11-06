#pragma once

/// @file
/// @brief Settings of a real-time simulation: its fixed time step, the limits that keep long
/// frames from destabilizing it, and its integrator.

#include "projectile/ballistics/integrator.hpp"
#include "projectile/core/result.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

namespace projectile {

/// The default simulation rate: 120 steps per second.
inline constexpr double default_simulation_rate = 120.0;

/// Settings of a Simulation, and of a FixedTimestep (which uses the timing settings only).
struct SimulationConfig {
    /// Duration of one fixed step, in s; finite and greater than zero. Every step has exactly
    /// this duration, whatever the frame rate.
    double time_step = 1.0 / default_simulation_rate;

    /// Most steps one update may run; at least 1. When more are due, for example after a slow
    /// frame, the rest carry over to later updates, so the cost of a single update stays
    /// bounded.
    std::uint32_t max_substeps = 8;

    /// Most simulated time that can be owed, in s; finite and no shorter than `time_step`.
    /// Longer frame deltas, such as after a pause in a debugger, are cut to this, and so is time
    /// carried over that the substep limit cannot work off. Time cut off is never simulated: a
    /// game that falls this far behind slows down instead of freezing to catch up.
    double max_frame_delta = 0.25;

    /// The integrator that advances projectiles.
    IntegratorKind integrator = IntegratorKind::semi_implicit_euler;
};

/// Checks @p config and returns the first problem found, or `std::nullopt` if it is valid.
[[nodiscard]] inline std::optional<Error> validate(const SimulationConfig& config) noexcept {
    if (!std::isfinite(config.time_step) || !(config.time_step > 0.0)) {
        return Error::invalid_time_step;
    }
    if (config.max_substeps == 0) {
        return Error::invalid_max_substeps;
    }
    if (!std::isfinite(config.max_frame_delta) || !(config.max_frame_delta >= config.time_step)) {
        return Error::invalid_max_frame_delta;
    }
    return std::nullopt;
}

} // namespace projectile
