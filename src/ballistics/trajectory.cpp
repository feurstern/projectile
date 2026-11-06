#include "projectile/ballistics/trajectory.hpp"

#include "ballistics/step_contact.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace projectile {

namespace {

/// Sample times closer than this fraction of the sample interval count as the same moment, so
/// that rounding never produces two samples for one instant.
constexpr double same_time_fraction = 1e-9;

/// Reserve memory up front only for predictions of up to this many samples.
constexpr double max_reserved_samples = 1e6;

/// Builds a predicted trajectory into a Trajectory, one integration step at a time.
class Predictor {
public:
    Predictor(
        const ProjectileConfig& config,
        const TrajectoryConfig& settings,
        const ICollisionWorld* world,
        Trajectory& out
    ) noexcept
        : config_(config),
          settings_(settings),
          world_(world),
          out_(&out),
          tolerance_(same_time_fraction * settings.sample_interval) {}

    /// Predicts the trajectory of @p initial, which must be valid.
    void run(const ProjectileState& initial) {
        start(initial);
        if (!initial.active) {
            out_->end = TrajectoryEnd::lifetime;
            return;
        }
        ProjectileState current = initial;
        double time = 0.0;
        while (settings_.max_duration - time > tolerance_) {
            const double requested = (std::min)(settings_.time_step, settings_.max_duration - time);
            // The integrators never step past the remaining lifetime.
            const double duration = (std::min)(requested, current.remaining_lifetime);
            ProjectileState next = current;
            step(settings_.integrator, next, config_, requested);
            if (stopped_by_hit(current, next, time, duration)) {
                return;
            }
            sample_step(current, next, time, duration);
            current = next;
            time += duration;
            if (!current.active) {
                out_->end = TrajectoryEnd::lifetime;
                break;
            }
        }
        append({.time = time, .position = current.position, .velocity = current.velocity});
    }

private:
    /// Resets the output and records the initial sample.
    void start(const ProjectileState& initial) {
        out_->samples.clear();
        out_->end = TrajectoryEnd::max_duration;
        out_->hit.reset();
        const double expected_samples = settings_.max_duration / settings_.sample_interval + 2.0;
        if (expected_samples <= max_reserved_samples) {
            out_->samples.reserve(static_cast<std::size_t>(expected_samples));
        }
        out_->samples.push_back(
            {.time = 0.0, .position = initial.position, .velocity = initial.velocity}
        );
    }

    /// Time at which the next regular sample is due.
    [[nodiscard]] double due_time() const noexcept {
        return static_cast<double>(next_sample_) * settings_.sample_interval;
    }

    /// Records the regular samples due during the step of @p duration from @p from, at @p time,
    /// to @p to.
    void sample_step(
        const ProjectileState& from, const ProjectileState& to, double time, double duration
    ) {
        const double end_time = time + duration;
        for (; due_time() <= end_time + tolerance_; ++next_sample_) {
            if (end_time - due_time() <= tolerance_) {
                // Due at the end of the step: use the step's own state, unrounded.
                out_->samples.push_back(
                    {.time = end_time, .position = to.position, .velocity = to.velocity}
                );
            } else {
                out_->samples.push_back(
                    detail::interpolate_step(
                        from, to, time, duration, (due_time() - time) / duration
                    )
                );
            }
        }
    }

    /// If the step of @p duration from @p from, at @p time, to @p to hits the collision world,
    /// ends the trajectory at the contact and returns true.
    [[nodiscard]] bool stopped_by_hit(
        const ProjectileState& from, const ProjectileState& to, double time, double duration
    ) {
        if (world_ == nullptr || !(duration > 0.0)) {
            return false;
        }
        const std::optional<detail::StepContact> contact =
            detail::sweep_step(*world_, from, to, config_.radius);
        if (!contact) {
            return false;
        }
        const double hit_time = time + contact->fraction * duration;
        for (; due_time() < hit_time - tolerance_; ++next_sample_) {
            out_->samples.push_back(
                detail::interpolate_step(from, to, time, duration, (due_time() - time) / duration)
            );
        }
        append(detail::contact_sample(from, to, time, duration, contact->fraction));
        out_->end = TrajectoryEnd::hit;
        out_->hit = contact->hit;
        return true;
    }

    /// Appends @p sample, or replaces the last sample if it is at practically the same time.
    void append(const TrajectorySample& sample) {
        if (!out_->samples.empty() && sample.time - out_->samples.back().time <= tolerance_) {
            out_->samples.back() = sample;
        } else {
            out_->samples.push_back(sample);
        }
    }

    ProjectileConfig config_;
    TrajectoryConfig settings_;
    const ICollisionWorld* world_;
    Trajectory* out_;
    double tolerance_;
    std::size_t next_sample_ = 1; // the next regular sample is due at next_sample_ * interval
};

std::optional<Error> predict(
    const ProjectileState& initial,
    const ProjectileConfig& config,
    const TrajectoryConfig& settings,
    const ICollisionWorld* world,
    Trajectory& out
) {
    if (const std::optional<Error> error = validate(config)) {
        return error;
    }
    if (const std::optional<Error> error = validate(settings)) {
        return error;
    }
    if (const std::optional<Error> error = validate(initial)) {
        return error;
    }
    Predictor(config, settings, world, out).run(initial);
    return std::nullopt;
}

} // namespace

Result<Trajectory> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config
) {
    Trajectory trajectory;
    if (const std::optional<Error> error =
            predict(state, config, trajectory_config, nullptr, trajectory)) {
        return *error;
    }
    return trajectory;
}

Result<Trajectory> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config,
    const ICollisionWorld& world
) {
    Trajectory trajectory;
    if (const std::optional<Error> error =
            predict(state, config, trajectory_config, &world, trajectory)) {
        return *error;
    }
    return trajectory;
}

std::optional<Error> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config,
    Trajectory& out
) {
    return predict(state, config, trajectory_config, nullptr, out);
}

std::optional<Error> predict_trajectory(
    const ProjectileState& state,
    const ProjectileConfig& config,
    const TrajectoryConfig& trajectory_config,
    const ICollisionWorld& world,
    Trajectory& out
) {
    return predict(state, config, trajectory_config, &world, out);
}

} // namespace projectile
