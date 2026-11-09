#pragma once

/// @file
/// @brief Registration of Projectile's benchmarks with Google Benchmark.
///
/// Each file registers its benchmarks from a function, which main() calls, instead of through
/// Google Benchmark's static-registration macros. The names group the benchmarks by what they
/// measure; see docs/benchmarks.md.

namespace projectile::bench {

/// Registers `raycast/…`, `intersect/…` and `sweep/…` (raycast_benchmark.cpp).
void register_raycast_benchmarks();

/// Registers `integrate/…` and `trajectory/…` (integration_benchmark.cpp).
void register_integration_benchmarks();

/// Registers `simulation/…` (simulation_benchmark.cpp).
void register_simulation_benchmarks();

} // namespace projectile::bench
