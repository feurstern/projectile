// Benchmarks of collision queries: rays cast into a world of 38 shapes (the 10,000 and 100,000
// raycast queries of the specification), the single-shape intersection tests behind them, and
// the swept-sphere queries that the simulation runs for every projectile at every step.

#include <projectile/projectile.hpp>

#include "suite.hpp"
#include "workloads.hpp"
#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace projectile::bench {
namespace {

/// The arena's room and obstacles, or an error reported through @p state.
std::optional<PrimitiveWorld> make_arena(benchmark::State& state) {
    PrimitiveWorld world;
    if (!add_room(world) || !add_obstacles(world)) {
        state.SkipWithError("the arena has an invalid shape");
        return std::nullopt;
    }
    return world;
}

/// Casts state.range(0) rays into the closed arena per iteration. Each ray tests all 38 shapes
/// and reports the nearest hit.
void raycast_arena(benchmark::State& state) {
    const std::optional<PrimitiveWorld> world = make_arena(state);
    if (!world) {
        return;
    }
    const std::vector<Ray> rays = arena_rays(static_cast<std::size_t>(state.range(0)), 1);
    std::size_t hits = 0;
    for ([[maybe_unused]] auto iteration : state) {
        hits = 0;
        for (const Ray& ray : rays) {
            std::optional<HitResult> hit = raycast(*world, ray, arena_ray_length);
            hits += hit ? 1U : 0U;
            benchmark::DoNotOptimize(hit);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    // Every ray hits the closed room: a check that the benchmark measured what it claims to.
    state.counters["hit_fraction"] = static_cast<double>(hits) / static_cast<double>(rays.size());
}

/// Tests 1,024 rays per iteration against one shape centred on the origin, about half of them
/// hits.
template <typename Shape>
void intersect_ray(benchmark::State& state, const Shape& shape) {
    const std::vector<Ray> rays = rays_at_origin(1'024, 2);
    std::size_t hits = 0;
    for ([[maybe_unused]] auto iteration : state) {
        hits = 0;
        for (const Ray& ray : rays) {
            std::optional<HitResult> hit = intersect(ray, shape);
            hits += hit ? 1U : 0U;
            benchmark::DoNotOptimize(hit);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rays.size()));
    state.counters["hit_fraction"] = static_cast<double>(hits) / static_cast<double>(rays.size());
}

/// Sweeps state.range(0) spheres of the bullet's radius along paths in the arena per iteration.
void sweep_arena(benchmark::State& state) {
    const std::optional<PrimitiveWorld> world = make_arena(state);
    if (!world) {
        return;
    }
    const std::vector<SweepPath> paths = arena_sweeps(static_cast<std::size_t>(state.range(0)), 3);
    const double radius = bullet(false).radius;
    std::size_t hits = 0;
    for ([[maybe_unused]] auto iteration : state) {
        hits = 0;
        for (const SweepPath& path : paths) {
            std::optional<HitResult> hit = sweep_sphere(*world, path.start, path.end, radius);
            hits += hit ? 1U : 0U;
            benchmark::DoNotOptimize(hit);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.counters["hit_fraction"] = static_cast<double>(hits) / static_cast<double>(paths.size());
}

} // namespace

void register_raycast_benchmarks() {
    benchmark::RegisterBenchmark("raycast/arena", raycast_arena)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Unit(benchmark::kMillisecond);

    static constexpr Plane plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    static constexpr Sphere sphere{.center = {0.0, 0.0, 0.0}, .radius = 1.0};
    static constexpr AABB box{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
    static constexpr Triangle triangle{
        .a = {-1.0, 0.0, -1.0},
        .b = {1.0, 0.0, -1.0},
        .c = {0.0, 0.0, 1.0},
    };
    benchmark::RegisterBenchmark("intersect/ray-plane", [](benchmark::State& state) {
        intersect_ray(state, plane);
    })->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("intersect/ray-sphere", [](benchmark::State& state) {
        intersect_ray(state, sphere);
    })->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("intersect/ray-box", [](benchmark::State& state) {
        intersect_ray(state, box);
    })->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("intersect/ray-triangle", [](benchmark::State& state) {
        intersect_ray(state, triangle);
    })->Unit(benchmark::kMicrosecond);

    benchmark::RegisterBenchmark("sweep/arena", sweep_arena)
        ->Arg(10'000)
        ->Unit(benchmark::kMillisecond);
}

} // namespace projectile::bench
