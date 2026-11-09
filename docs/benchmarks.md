# Benchmarks

Projectile's benchmarks measure what games spend time on: ray queries against a world,
swept-sphere queries, integration steps, trajectory predictions, and fixed steps of simulations
with thousands of projectiles. They use [Google Benchmark](https://github.com/google/benchmark)
and live in [benchmarks/](../benchmarks/), apart from the library, which never depends on them.

This page explains what each benchmark measures, how to run them so that results can be
reproduced, and what they measured on a reference machine.

## What is measured

| Benchmark | Work per iteration | Measures |
|---|---|---|
| `raycast/arena/10000`, `/100000` | 10,000 or 100,000 rays cast into the arena, each reporting its nearest hit | World raycasts, through `ICollisionWorld` and `PrimitiveWorld` |
| `intersect/ray-plane`, `-sphere`, `-box`, `-triangle` | 1,024 rays against one shape, about half of them hits | The intersection tests behind every query |
| `sweep/arena/10000` | 10,000 spheres of a bullet's radius swept 0.5 to 8 m through the arena | The swept-sphere query the simulation runs for each projectile at each step |
| `integrate/euler/…`, `integrate/rk4/…` | One step of one bullet, under gravity alone or with drag and wind | The cost of each integrator, and of air drag |
| `trajectory/free/60`, `/600`, `/6000` | One `predict_trajectory()` of 60, 600 or 6,000 samples (1, 10 or 100 s) | Trajectory prediction for different sample counts |
| `trajectory/world/…` | The same, sweeping every step past the arena's 32 obstacles | Prediction with collision detection |
| `simulation/free/1000`, `/10000`, `/100000` | One `Simulation::step()` of 1,000, 10,000 or 100,000 bullets in free flight | Projectile updates: integration and bookkeeping |
| `simulation/drag/…` | The same, with air drag and wind | Projectile updates under drag |
| `simulation/arena/…` | The same, inside the closed arena, where every step sweeps each bullet past 38 shapes and resolves its ricochets | Projectile updates with collisions and impacts |

Each benchmark reports the time per iteration and, as `items_per_second`, its throughput: rays,
queries, steps, samples or projectile updates per second. Some also report a check that they
measured what they claim. Every ray in the closed arena must hit something (`hit_fraction` = 1),
and every simulated bullet must still be flying at the end (`active`).

### The workloads

All inputs are drawn from SplitMix64 with fixed seeds ([benchmarks/workloads.hpp](../benchmarks/workloads.hpp)),
because the distributions of `<random>` are implementation-defined. Every compiler and standard
library therefore measures the same work, bit for bit wherever floating-point arithmetic is the
same (see [simulation.md](simulation.md#determinism)).

- **The arena** is a closed room, 40 m × 20 m × 40 m, of six inward-facing planes, holding 32
  obstacles at fixed random places below 8 m: 8 boxes standing on the floor, 8 spheres and 16
  triangles at random orientations. Rays, sweeps and projectiles start in the open space between
  10 and 18 m.
- **The bullets** have a mass of 8 g and a diameter of 9 mm. They fly at 20 to 400 m/s in random
  directions, under Earth's gravity, and with drag, also feel a crosswind of 3 m/s.
- **In the arena**, every surface is perfectly elastic and always deflects (restitution 1,
  friction 0, ricochet angle 90°). Bullets keep bouncing for as long as a benchmark runs, so every
  step does the same amount of work.

`PrimitiveWorld` tests every shape for every query, so its cost grows linearly with the number of
shapes. Games with large scenes should implement `ICollisionWorld` over a spatial index (see
[collision.md](collision.md#collision-worlds)); the `intersect/…` benchmarks show the cost per
shape that such an index saves.

## Running the benchmarks

Build them with the `bench` preset: a Release build of the library and the benchmarks only,
with the compiler's default optimization flags (`-O3 -DNDEBUG` for GCC and Clang) and no
`-march=native`, so that results compare across machines of the same architecture. Google
Benchmark is downloaded if it is not installed.

```sh
cmake --preset bench
cmake --build --preset bench
```

Then run them on a quiet machine:

```sh
taskset -c 2 build/bench/bin/projectile_benchmarks \
    --benchmark_repetitions=10 --benchmark_min_time=0.5s \
    --benchmark_enable_random_interleaving=true \
    --benchmark_report_aggregates_only=true \
    --benchmark_out=run.json --benchmark_out_format=json
```

- **Repetitions.** Each benchmark runs 10 times for at least 0.5 s. The results are the median of
  the repetitions, with their coefficient of variation (CV: standard deviation over mean), which
  shows how stable each measurement was.
- **Random interleaving** runs the repetitions of all benchmarks in random order, so that a slow
  drift of the machine, such as warming up, spreads over every benchmark instead of biasing the
  last ones.
- **One core.** `taskset -c 2` pins the run to one CPU (Linux). On CPUs with performance and
  efficiency cores, choose a performance core; `lscpu --extended` or
  `/sys/devices/cpu_core/cpus` lists them. The benchmarks are single-threaded.
- **Address randomization.** On Linux, the program runs itself again with address-space
  randomization disabled, as Google Benchmark's own main function does, which removes one source
  of run-to-run noise.
- **A quiet machine.** Close other programs, plug laptops in, and if you can, set the CPU's
  frequency governor to `performance` (`sudo cpupower frequency-set -g performance`). Note it if
  you cannot.

Every result records how and where it was made, in the JSON's `context`. Google Benchmark adds
the CPU count, caches, nominal clock and whether frequency scaling was on. `projectile_benchmarks`
adds the Projectile version, the compiler and its version, the build type, the C++ flags and the
CPU model.

### Reports and comparisons

[benchmarks/report.py](../benchmarks/report.py) (Python 3, standard library only) turns a run into
Markdown tables. Given two runs, it also compares them, benchmark by benchmark:

```sh
python3 benchmarks/report.py run.json             # environment and results
python3 benchmarks/report.py run.json other.json  # and the difference between the runs
```

To check that results reproduce, make two runs and compare them. To measure a change, run before
and after it on the same machine, with the same build settings.

## Results

Measured on 2026-11-09 with the procedure above, by two runs one after the other.

- **CPU:** Intel Core 7 240H, 6 performance and 4 efficiency cores (16 threads), up to 5.2 GHz.
  The runs were pinned to a performance core. Caches as Google Benchmark reports them: L1 data
  48 KiB (×8), L2 1,280 KiB (×8), L3 24 MiB.
- **Build:** GCC 16.2.1, `-O3 -DNDEBUG`, Release, Projectile as a static library, Google
  Benchmark 1.10.0. Linux 7.2.
- **Conditions:** a laptop on AC power, in a desktop session with a web browser open. The CPU ran
  with the `powersave` governor of intel_pstate (energy preference `balance_performance`) and
  turbo enabled; changing them needs root. Address randomization was off.

| Benchmark | Median time | Throughput | Per item | CV |
|---|--:|--:|--:|--:|
| `raycast/arena/10000` | 4.67 ms | 2.17 M/s | 462 ns | 1.1% |
| `raycast/arena/100000` | 46.7 ms | 2.17 M/s | 462 ns | 2.1% |
| `intersect/ray-plane` | 4.11 µs | 252 M/s | 3.97 ns | 0.6% |
| `intersect/ray-sphere` | 12.4 µs | 83.4 M/s | 12 ns | 2.3% |
| `intersect/ray-box` | 7.16 µs | 144 M/s | 6.95 ns | 1.8% |
| `intersect/ray-triangle` | 6.32 µs | 164 M/s | 6.11 ns | 1.7% |
| `sweep/arena/10000` | 21.4 ms | 471 k/s | 2.12 µs | 1.7% |
| `integrate/euler/gravity` | 3.58 ns | 283 M/s | 3.54 ns | 1.0% |
| `integrate/euler/drag` | 16 ns | 63.2 M/s | 15.8 ns | 1.3% |
| `integrate/rk4/gravity` | 5.91 ns | 171 M/s | 5.86 ns | 2.4% |
| `integrate/rk4/drag` | 57.9 ns | 17.5 M/s | 57.2 ns | 2.5% |
| `trajectory/free/60` | 1.83 µs | 33.1 M/s | 30.2 ns | 1.2% |
| `trajectory/free/600` | 18.2 µs | 33.4 M/s | 29.9 ns | 1.1% |
| `trajectory/free/6000` | 187 µs | 32.4 M/s | 30.9 ns | 0.9% |
| `trajectory/world/60` | 217 µs | 280 k/s | 3.57 µs | 1.1% |
| `trajectory/world/600` | 2.16 ms | 281 k/s | 3.56 µs | 1.5% |
| `trajectory/world/6000` | 21.5 ms | 282 k/s | 3.54 µs | 2.3% |
| `simulation/free/1000` | 6.3 µs | 160 M/s | 6.23 ns | 2.4% |
| `simulation/free/10000` | 72 µs | 140 M/s | 7.13 ns | 1.3% |
| `simulation/free/100000` | 873 µs | 116 M/s | 8.63 ns | 2.9% |
| `simulation/drag/1000` | 9.84 µs | 103 M/s | 9.69 ns | 0.7% |
| `simulation/drag/10000` | 98.2 µs | 103 M/s | 9.7 ns | 0.6% |
| `simulation/drag/100000` | 1.12 ms | 90.5 M/s | 11 ns | 5.1% |
| `simulation/arena/1000` | 2.47 ms | 411 k/s | 2.43 µs | 0.4% |
| `simulation/arena/10000` | 24.5 ms | 414 k/s | 2.42 µs | 0.5% |
| `simulation/arena/100000` | 231 ms | 438 k/s | 2.28 µs | 1.7% |

**Reproducibility.** The two runs differed by 1.3% at the median, and every benchmark was about
that much faster in the second run, as the machine settled. The largest differences were in the
free-flight and drag simulations of 100,000 projectiles, at 6.8% and 5.7%. Their 20 MB of
projectile state no longer fits the per-core caches, and shares the L3 cache with the rest of the
machine. Within a run, the coefficient of variation stayed below 2.5%, except for those two (2.9%
and 5.1%).

**What the numbers show:**

- **Raycasts scale linearly:** 2.17 million rays per second at both 10,000 and 100,000 queries,
  about 460 ns per ray against 38 shapes, or 12 ns per shape.
- **Single intersection tests take 4 to 12 ns:** planes are cheapest, then triangles and boxes;
  spheres take a square root.
- **A swept sphere costs about 4.6 raycasts:** 2.1 µs against the same 38 shapes. A sphere
  swept past a box meets its faces, edges and corners: 23 pieces.
- **Integrators:** semi-implicit Euler takes 3.5 ns per step and RK4 5.9 ns under gravity alone,
  a constant field that the compiler evaluates once. With drag, a velocity-dependent field
  with a square root, Euler takes 16 ns and RK4, which evaluates it four times, 57 ns.
- **Trajectory prediction** costs about 30 ns per sample (two steps each) in free flight.
  Sweeping each step past 32 obstacles raises that to 3.6 µs: collision queries dominate.
- **Simulation:** free flight costs 6 to 9 ns per projectile update, drag adds about 3.5 ns, and
  the arena's collisions and impacts bring it to 2.4 µs, almost all of it in queries against
  38 shapes. One core at 120 Hz has 8.3 ms per step. That is enough for over a million projectiles
  in free flight, but only about 3,400 in the arena. Scenes larger than a few dozen shapes need a
  collision world with a spatial index.

## Limitations

- **Single-threaded.** Projectile has no threading and no SIMD yet. As the specification asks,
  these baselines come first; optimizations will be measured against them.
- **Brute-force worlds.** `PrimitiveWorld` tests every shape. Its numbers show what a scene of 38
  shapes costs, not what a game with a spatial index would see.
- **One machine.** Absolute numbers depend on the CPU, the compiler and the build flags. Compare
  runs made on the same machine with the same settings; across machines, compare ratios between
  benchmarks rather than times.
