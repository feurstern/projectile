// Benchmarks of the real-time simulation: one fixed step of 1,000, 10,000 and 100,000
// projectiles (the specification's projectile updates), in free flight, under air drag, and
// bouncing around the closed arena, where every step sweeps each projectile past 38 shapes and
// resolves its impacts.

#include <projectile/projectile.hpp>

#include "suite.hpp"
#include "workloads.hpp"
#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

namespace projectile::bench {
namespace {

/// What the projectiles of a simulation benchmark fly through.
enum class Scene : std::uint8_t {
    free_flight, ///< Nothing: gravity only, no collision world.
    drag,        ///< Nothing, but with air drag and wind.
    arena,       ///< The closed arena, whose elastic walls and obstacles bounce them forever.
};

/// Runs one Simulation::step() of state.range(0) projectiles per iteration. No projectile ever
/// stops or expires, so every step does the same amount of work.
void simulation_step(benchmark::State& state, Scene scene) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const ProjectileConfig config = bullet(scene == Scene::drag);

    PrimitiveWorld arena;
    MaterialTable materials;
    const StandardInteraction interaction;
    if (!add_room(arena) || !add_obstacles(arena) || materials.add(elastic_material())) {
        state.SkipWithError("invalid arena");
        return;
    }

    Simulation simulation;
    if (scene == Scene::arena) {
        simulation.set_collision_world(&arena);
        simulation.set_materials(&materials);
        simulation.set_interaction(&interaction);
    }
    simulation.reserve(count);
    for (const ProjectileState& spawn : arena_spawns(count, 4, config)) {
        if (!simulation.add_projectile(spawn, config)) {
            state.SkipWithError("invalid projectile");
            return;
        }
    }

    for ([[maybe_unused]] auto iteration : state) {
        simulation.step();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    // All of them still fly: a check that every step simulated the full count.
    state.counters["active"] = static_cast<double>(simulation.projectiles().size());
}

} // namespace

void register_simulation_benchmarks() {
    const auto register_scene = [](const char* name, Scene scene) {
        benchmark::RegisterBenchmark(
            name, [scene](benchmark::State& state) { simulation_step(state, scene); }
        )
            ->Arg(1'000)
            ->Arg(10'000)
            ->Arg(100'000)
            ->Unit(benchmark::kMicrosecond);
    };
    register_scene("simulation/free", Scene::free_flight);
    register_scene("simulation/drag", Scene::drag);
    register_scene("simulation/arena", Scene::arena);
}

} // namespace projectile::bench
