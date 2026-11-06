#pragma once

/// @file
/// @brief The accumulator that turns frames of varying length into fixed simulation steps.

#include "projectile/core/result.hpp"
#include "projectile/simulation/simulation_config.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace projectile {

/// What one frame's time amounts to: the fixed steps now due, and any time given up.
struct FrameSteps {
    /// Fixed steps to run now; at most SimulationConfig::max_substeps.
    std::uint32_t steps = 0;

    /// Time, in s, cut off by SimulationConfig::max_frame_delta: it will never be simulated.
    /// Zero unless the simulation fell that far behind. Infinite for an infinite frame delta.
    double dropped_time = 0.0;

    /// Exact member-wise comparison.
    constexpr bool operator==(const FrameSteps&) const = default;
};

/// A fixed-timestep accumulator.
///
/// Frames vary in length, but a simulation is stable and reproducible only with a fixed time
/// step. A FixedTimestep collects the time of each frame and says how many fixed steps are due:
///
///     const FrameSteps frame = timestep.advance(frame_delta);
///     for (std::uint32_t i = 0; i < frame.steps; ++i) {
///         step(timestep.time_step());
///     }
///
/// Time short of a whole step carries over to the next frame. interpolation_alpha() tells how far
/// it reaches into the next step, for drawing between the last two simulation states.
///
/// The limits of SimulationConfig keep long frames from destabilizing the simulation:
/// - Frame deltas that are NaN, negative or zero add no time. Frame deltas above
///   `max_frame_delta` are cut to it.
/// - At most `max_substeps` steps are due per frame. Time still owed after them carries over to
///   later frames, up to `max_frame_delta` in total. Anything beyond is dropped, so that a
///   machine that cannot keep up slows the simulation down instead of spending ever longer on
///   each frame.
///
/// Simulation keeps a FixedTimestep. Use one directly to run other systems at a fixed rate.
class FixedTimestep {
public:
    /// An accumulator with the default settings of SimulationConfig: 120 Hz, at most 8 steps
    /// per frame, and at most 0.25 s owed.
    FixedTimestep() noexcept = default;

    /// Creates an accumulator with the timing settings of @p config (`time_step`,
    /// `max_substeps` and `max_frame_delta`), or returns the first problem found in @p config.
    [[nodiscard]] static Result<FixedTimestep> create(const SimulationConfig& config) noexcept {
        if (const std::optional<Error> error = validate(config)) {
            return *error;
        }
        return FixedTimestep(config);
    }

    /// Adds @p frame_delta seconds of frame time and returns the steps now due.
    [[nodiscard]] FrameSteps advance(double frame_delta) noexcept {
        FrameSteps frame;
        double added = frame_delta > 0.0 ? frame_delta : 0.0; // also rejects NaN
        if (added > max_frame_delta_) {
            frame.dropped_time = added - max_frame_delta_;
            added = max_frame_delta_;
        }
        pending_ += added;
        while (pending_ >= time_step_ && frame.steps < max_substeps_) {
            pending_ -= time_step_;
            ++frame.steps;
        }
        if (pending_ > max_frame_delta_) {
            frame.dropped_time += pending_ - max_frame_delta_;
            pending_ = max_frame_delta_;
        }
        return frame;
    }

    /// How far the time carried over reaches into the next step, from 0 to 1: the weight of the
    /// newest state when interpolating between the last two for drawing. 1 while more than a
    /// whole step is owed.
    [[nodiscard]] double interpolation_alpha() const noexcept {
        return (std::min)(pending_ / time_step_, 1.0);
    }

    /// Time accumulated but not yet simulated, in s.
    [[nodiscard]] double pending_time() const noexcept { return pending_; }

    /// Duration of one step, in s.
    [[nodiscard]] double time_step() const noexcept { return time_step_; }

    /// Most steps due per frame.
    [[nodiscard]] std::uint32_t max_substeps() const noexcept { return max_substeps_; }

    /// Most time that can be owed, in s.
    [[nodiscard]] double max_frame_delta() const noexcept { return max_frame_delta_; }

    /// Discards the time carried over, for example after loading a level.
    void reset() noexcept { pending_ = 0.0; }

private:
    explicit FixedTimestep(const SimulationConfig& config) noexcept
        : time_step_(config.time_step),
          max_substeps_(config.max_substeps),
          max_frame_delta_(config.max_frame_delta) {}

    double time_step_ = SimulationConfig{}.time_step;
    std::uint32_t max_substeps_ = SimulationConfig{}.max_substeps;
    double max_frame_delta_ = SimulationConfig{}.max_frame_delta;
    double pending_ = 0.0;
};

} // namespace projectile
