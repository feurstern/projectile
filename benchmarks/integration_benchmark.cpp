// Benchmarks of numerical integration: single steps of each integrator, with and without air
// drag, and trajectory predictions of 60, 600 and 6,000 samples (the specification's
// "trajectory prediction for different sample counts").

#include <projectile/projectile.hpp>

#include "suite.hpp"
#include "workloads.hpp"
#include <benchmark/benchmark.h>

#include <cstdint>
#include <optional>

namespace projectile::bench {
namespace {

/// The time step of the default simulation rate, 120 Hz.
constexpr double step_time = 1.0 / default_simulation_rate;

/// Advances one bullet by one step per iteration. Under drag, its speed approaches terminal
/// velocity; without drag, it falls ever faster, which costs the same per step.
template <typename Integrator>
void integrate(benchmark::State& state, bool with_drag) {
    const ProjectileConfig config = bullet(with_drag);
    const Result<ProjectileState> start =
        create_projectile(config, Vec3{0.0, 1.5, 0.0}, Vec3{400.0, 20.0, 0.0});
    if (!start) {
        state.SkipWithError("invalid projectile");
        return;
    }
    ProjectileState projectile = *start;
    const Integrator integrator;
    for ([[maybe_unused]] auto iteration : state) {
        integrator.step(projectile, config, step_time);
        benchmark::DoNotOptimize(projectile);
    }
    state.SetItemsProcessed(state.iterations());
}

/// Predicts a trajectory of state.range(0) samples per iteration, at 60 samples per second and
/// 120 steps per second, reusing one Trajectory so that only the prediction is measured.
///
/// With a world, the shot flies over the arena's obstacles (without the room around them) and
/// never comes down on one, so each prediction runs its full length and sweeps every step past
/// all 32 obstacles.
void predict(benchmark::State& state, bool with_world) {
    const ProjectileConfig config = bullet(false);
    const Result<ProjectileState> start =
        create_projectile(config, Vec3{0.0, 15.0, 0.0}, Vec3{30.0, 10.0, 0.0});
    PrimitiveWorld obstacles;
    if (!start || !add_obstacles(obstacles)) {
        state.SkipWithError("invalid projectile or obstacle");
        return;
    }

    TrajectoryConfig trajectory_config;
    trajectory_config.sample_interval = 1.0 / 60.0;
    trajectory_config.time_step = step_time;
    trajectory_config.max_duration =
        static_cast<double>(state.range(0)) * trajectory_config.sample_interval;

    Trajectory trajectory;
    for ([[maybe_unused]] auto iteration : state) {
        const std::optional<Error> error =
            with_world
                ? predict_trajectory(*start, config, trajectory_config, obstacles, trajectory)
                : predict_trajectory(*start, config, trajectory_config, trajectory);
        if (error || trajectory.end != TrajectoryEnd::max_duration) {
            state.SkipWithError("the prediction did not run its full length");
            return;
        }
        benchmark::DoNotOptimize(trajectory);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.counters["samples"] = static_cast<double>(trajectory.samples.size());
}

} // namespace

void register_integration_benchmarks() {
    benchmark::RegisterBenchmark("integrate/euler/gravity", [](benchmark::State& state) {
        integrate<SemiImplicitEuler>(state, false);
    });
    benchmark::RegisterBenchmark("integrate/euler/drag", [](benchmark::State& state) {
        integrate<SemiImplicitEuler>(state, true);
    });
    benchmark::RegisterBenchmark("integrate/rk4/gravity", [](benchmark::State& state) {
        integrate<RungeKutta4>(state, false);
    });
    benchmark::RegisterBenchmark("integrate/rk4/drag", [](benchmark::State& state) {
        integrate<RungeKutta4>(state, true);
    });

    benchmark::RegisterBenchmark(
        "trajectory/free", [](benchmark::State& state) { predict(state, false); }
    )
        ->Arg(60)
        ->Arg(600)
        ->Arg(6'000)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark(
        "trajectory/world", [](benchmark::State& state) { predict(state, true); }
    )
        ->Arg(60)
        ->Arg(600)
        ->Arg(6'000)
        ->Unit(benchmark::kMicrosecond);
}

} // namespace projectile::bench
