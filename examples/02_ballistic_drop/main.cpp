// 02_ballistic_drop: fires a shot horizontally and tabulates how far it drops downrange.
//
// Predicts the trajectory with both integrators at a coarse 60 Hz time step and compares them
// with the exact solution. Under gravity alone, semi-implicit Euler's path is displaced along
// gravity by ½ · g · t · dt, while fourth-order Runge-Kutta matches the exact path to rounding.

#include <projectile/projectile.hpp>

#include <cstddef>
#include <cstdio>
#include <string_view>

using namespace projectile;

namespace {

void report(Error error) {
    const std::string_view reason = describe(error);
    std::fprintf(stderr, "error: %.*s\n", static_cast<int>(reason.size()), reason.data());
}

} // namespace

int main() {
    ProjectileConfig config;
    config.initial_velocity = {300.0, 0.0, 0.0};    // m/s, horizontally along +x
    config.gravity = {0.0, -standard_gravity, 0.0}; // m/s², +y is up

    const Result<ProjectileState> shot = create_projectile(config, {0.0, 1.5, 0.0});
    if (!shot) {
        report(shot.error());
        return 1;
    }

    TrajectoryConfig euler;
    euler.sample_interval = 0.1; // s
    euler.max_duration = 0.5;    // s
    euler.time_step = 1.0 / 60.0;
    euler.integrator = IntegratorKind::semi_implicit_euler;

    TrajectoryConfig runge_kutta = euler;
    runge_kutta.integrator = IntegratorKind::rk4;

    const Result<Trajectory> by_euler = predict_trajectory(*shot, config, euler);
    const Result<Trajectory> by_runge_kutta = predict_trajectory(*shot, config, runge_kutta);
    if (!by_euler || !by_runge_kutta) {
        report(by_euler ? by_runge_kutta.error() : by_euler.error());
        return 1;
    }

    std::printf("  time (s)  range (m)  drop (m)  Euler error (mm)  RK4 error (mm)\n");
    for (std::size_t i = 0; i < by_euler->samples.size(); ++i) {
        const TrajectorySample& euler_sample = by_euler->samples.at(i);
        const TrajectorySample& rk4_sample = by_runge_kutta->samples.at(i);
        const Vec3 exact =
            ballistic_position(shot->position, shot->velocity, config.gravity, euler_sample.time);
        std::printf(
            "%10.2f %10.1f %9.4f %17.2f %15.6f\n",
            euler_sample.time,
            exact.x,
            shot->position.y - exact.y,
            1000.0 * distance(euler_sample.position, exact),
            1000.0 * distance(rk4_sample.position, exact)
        );
    }
    return 0;
}
