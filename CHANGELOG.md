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
- CMake presets: `debug`, `release`, `release-tests`, `sanitize` and `dev`.
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
- Simulation module (`projectile/simulation/`):
  - `Simulation`: many projectiles advanced at a fixed time step, with stable `ProjectileId`s,
    swept collisions against an `ICollisionWorld`, lifetimes, hit and expiry events through an
    optional callback, and interpolation data for drawing between steps. Results are
    reproducible and independent of the frame rate, and a running simulation stops allocating
    once it reaches its largest size.
  - `SimulationConfig`: the time step (120 Hz by default), the most steps per update, the most
    time owed, and the integrator.
  - `FixedTimestep`: the accumulator that turns frame times into fixed steps, usable on its own.
- Core: the `Error` codes and `Result<T>` type used to reject invalid configuration, and the
  `standard_gravity` constant.
- Examples `01_basic_projectile`, `02_ballistic_drop`, `03_hitscan` and `04_swept_collision`,
  built by default (`PROJECTILE_BUILD_EXAMPLES`) and run as tests.
- Documentation of the numerical conventions in `docs/mathematics.md`, of the collision query
  conventions in `docs/collision.md`, of the integrators' accuracy and stability in
  `docs/ballistics.md`, and of the simulation in `docs/simulation.md`.
- Test suite built on Catch2 v3:
  - unit tests;
  - integration tests of complete simulation workflows, including tunneling, reproducibility and
    heap-allocation counts;
  - a build-time check that every public header compiles on its own;
  - package tests that consume the installed and the embedded library from a downstream
    project;
  - property tests that check mathematical properties on seeded random inputs.
- clang-format and clang-tidy configuration.
- GitHub Actions workflows: CI (GCC, Clang, MSVC and AppleClang; minimum compiler and CMake
  versions; formatting; static analysis), sanitizers (ASan, LSan, UBSan) and releases.
