# Changelog

All notable changes to Projectile are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Before 1.0, minor releases
may contain breaking changes.

## [Unreleased]

### Added

- CMake build for the core library, as a static or a shared library (`BUILD_SHARED_LIBS`), with
  install rules and a CMake package: `find_package(Projectile)` provides the
  `Projectile::Projectile` target. Embedding through `add_subdirectory` or FetchContent is also
  supported.
- CMake presets: `debug`, `release`, `release-tests`, `sanitize` and `dev`, plus `bench` for
  the benchmarks and `demos` for the raylib demos.
- Version API: `projectile::version()`, `projectile::version_string()`, the
  `projectile::Version` type and the `PROJECTILE_VERSION*` macros. Use them to detect a mismatch
  between headers and library.
- Math module (`projectile/math/`):
  - `Vec2` and `Vec3`, with arithmetic, `dot`, `cross`, `length`, `distance`, `lerp`, `reflect`,
    `project`, `approx_equal` and `is_finite`.
  - `try_normalize`, which returns `std::nullopt` for zero or non-finite vectors and is accurate
    at every scale from subnormal values up to the largest finite double.
  - Geometric primitives `Ray`, `Plane`, `Sphere` and `AABB`, with validating builders
    (`make_ray`, `make_plane`, `plane_through_points`, `make_aabb`) and `is_valid` checks.
  - Scalar helpers: `approx_equal` and degree/radian conversion.
  - `Segment` and `Triangle` primitives, with `face_normal` for triangles.
  - `closest_point` for boxes, segments and triangles, alongside the one for planes.
- Collision module (`projectile/collision/`):
  - `HitResult`: distance, contact point, a normal facing the query, material id, and whether a
    front or back face was hit.
  - `intersect()` for rays and segments against planes, spheres, boxes and triangles, with
    optional back-face culling (`Culling`) and the same boundary-contact and degenerate-case
    rules for every shape.
  - `ICollisionWorld`, the interface to an application's collision geometry, with the validated
    entry points `raycast()` and `sweep_sphere()`.
  - `sweep_sphere(path, radius, shape)`: swept-sphere queries against planes, spheres, boxes and
    triangles, exact for their rounded edges and corners, so that fast projectiles cannot pass
    through thin shapes.
  - `PrimitiveWorld`, a ready-made collision world of planes, spheres, boxes and triangles with
    materials.
- Ballistics module (`projectile/ballistics/`):
  - `ProjectileConfig` (mass, radius, initial velocity, gravity, lifetime) and `ProjectileState`
    (position, velocity, acceleration, remaining lifetime, active), created with
    `create_projectile`.
  - `SemiImplicitEuler` (the default) and `RungeKutta4` integrators, selectable at run time with
    `IntegratorKind`. Each step stops at the end of the projectile's lifetime, and accepts any
    custom `AccelerationField`.
  - `predict_trajectory`: samples a projectile's future path with the same steps a simulation
    takes, optionally stopping at the first hit in a collision world, and can reuse its output
    buffer. Also `ballistic_position` and `ballistic_velocity` for the exact motion under
    constant acceleration.
  - Quadratic air drag and wind: `ProjectileConfig::drag_coefficient`, `reference_area`,
    `air_density` (sea level by default) and `wind`, applied by `forces_of` and so by the
    integrators, predictions and simulations. Off by default. Also `drag_factor`.
  - `kinetic_energy` and `speed_for_energy`.
- Materials module (`projectile/materials/`):
  - `Material` (resistance, restitution, friction, penetration cost, ricochet angle) and
    `MaterialTable`, which looks materials up by the ids collision worlds report.
  - `IMaterialInteraction`, the replaceable interface that decides each impact, and
    `StandardInteraction`: ricochet at shallow angles, penetration when the projectile's energy
    covers the cost for the object's thickness, a stop otherwise. Its rules are also available
    as `resolve_standard`.
  - `impact_angle`, `ricochet_velocity` and `penetration_energy`.
- Simulation module (`projectile/simulation/`):
  - `Simulation`: many projectiles advanced at a fixed time step, with stable `ProjectileId`s,
    swept collisions against an `ICollisionWorld`, lifetimes, hit and expiry events through an
    optional callback, and interpolation data for drawing between steps. Results are
    reproducible and independent of the frame rate, and a running simulation stops allocating
    once it reaches its largest size.
  - `SimulationConfig`: the time step (120 Hz by default), the most steps per update, the most
    time owed, the integrator, and the most impacts per projectile per step.
  - Impacts decided by an interaction model with a material table (`set_interaction`,
    `set_materials`): projectiles that ricochet or penetrate fly on for the rest of the step, and
    `ricochet` and `penetration` events report the outgoing velocity and the remaining energy.
  - `FixedTimestep`: the accumulator that turns frame times into fixed steps, usable on its own.
- Core: the `Error` codes and `Result<T>` type used to reject invalid configuration, and the
  `standard_gravity` and `standard_air_density` constants.
- Examples `01_basic_projectile`, `02_ballistic_drop`, `03_hitscan`, `04_swept_collision` and
  `05_material_interaction`, built by default (`PROJECTILE_BUILD_EXAMPLES`) and run as tests.
- raylib adapter (`adapters/raylib/`, `PROJECTILE_BUILD_RAYLIB`): the header-only
  `Projectile::Raylib` target, with conversions between Projectile's and raylib's types and debug
  drawing of shapes, rays, hits, trajectories and simulated projectiles. It needs raylib 5.5 or
  newer, and downloads raylib 5.5 if none is installed. It builds with Projectile, or on its own
  against an installed Projectile. The core library never depends on it.
- Four raylib demos: `ballistic_drop`, `hitscan_visualizer`, `material_interaction` and
  `multiple_projectiles`. Each can play a deterministic scripted sequence (`--frames`, `--record`,
  `--hidden`) and check what it showed; CTest runs these as tests. `tools/record_demos.py`
  records them as the README's GIFs.
- Benchmarks (`benchmarks/`, `PROJECTILE_BUILD_BENCHMARKS`): `projectile_benchmarks`, built on
  Google Benchmark, measures raycasts, intersection tests, sphere sweeps, integrator steps,
  trajectory predictions and simulation steps of 1,000 to 100,000 projectiles, on workloads that
  are identical on every platform. Each result records the compiler, flags, build type and CPU,
  and `benchmarks/report.py` summarizes and compares runs.
- Documentation of the numerical conventions in `docs/mathematics.md`, of the collision query
  conventions in `docs/collision.md`, of the integrators' accuracy and stability in
  `docs/ballistics.md`, of the simulation in `docs/simulation.md`, of material interactions
  and their assumptions in `docs/materials.md`, of engine integration and the raylib adapter in
  `docs/integration.md`, and of the benchmark methodology and results in `docs/benchmarks.md`.
  The README shows recordings of the demos.
- Test suite built on Catch2 v3:
  - unit tests;
  - integration tests of complete simulation workflows, including tunneling, reproducibility and
    heap-allocation counts;
  - a build-time check that every public header compiles on its own;
  - package tests that consume the installed and the embedded library from a downstream
    project;
  - property tests that check mathematical properties on seeded random inputs;
  - a check that the installed library passes no other library on to its consumers.
- clang-format and clang-tidy configuration.
- GitHub Actions workflows: CI (GCC, Clang, MSVC and AppleClang; minimum compiler and CMake
  versions; formatting; static analysis), sanitizers (ASan, LSan, UBSan), releases, and the
  demos and benchmarks (Linux with clang-tidy and the demos under Xvfb, Windows, macOS).
