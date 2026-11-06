#include "projectile/ballistics/trajectory.hpp"

#include "projectile/collision/sweep.hpp"

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

/// The sample at fraction @p s of a step of duration @p duration (greater than zero) that starts
/// at @p start_time in state @p from and ends in state @p to.
///
/// It lies on the cubic Hermite curve through both states' positions and velocities, which is
/// exact for motion under constant acceleration.
[[nodiscard]] TrajectorySample interpolate(
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
                    interpolate(from, to, time, duration, (due_time() - time) / duration)
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
        const std::optional<HitResult> hit =
            sweep_sphere(*world_, from.position, to.position, config_.radius);
        if (!hit) {
            return false;
        }
        // The collision query saw the straight line between the steps; the contact lies at this
        // fraction of it.
        const double length = distance(from.position, to.position);
        const double fraction = length > 0.0 ? std::clamp(hit->distance / length, 0.0, 1.0) : 0.0;
        const double hit_time = time + fraction * duration;
        for (; due_time() < hit_time - tolerance_; ++next_sample_) {
            out_->samples.push_back(
                interpolate(from, to, time, duration, (due_time() - time) / duration)
            );
        }
        TrajectorySample contact = interpolate(from, to, time, duration, fraction);
        contact.position = lerp(from.position, to.position, fraction);
        append(contact);
        out_->end = TrajectoryEnd::hit;
        out_->hit = hit;
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
