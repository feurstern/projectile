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
- Collision module (`projectile/collision/`):
  - `HitResult`: distance, contact point, a normal facing the query, material id, and whether a
    front or back face was hit.
  - `intersect()` for rays and segments against planes, spheres, boxes and triangles, with
    optional back-face culling (`Culling`) and the same boundary-contact and degenerate-case
    rules for every shape.
  - `ICollisionWorld`, the interface to an application's collision geometry, with the validated
    entry points `raycast()` and `sweep_sphere()`.
- Documentation of the numerical conventions in `docs/mathematics.md` and of the collision query
  conventions in `docs/collision.md`.
- Test suite built on Catch2 v3:
  - unit tests;
  - a build-time check that every public header compiles on its own;
  - package tests that consume the installed and the embedded library from a downstream
    project;
  - property tests that check mathematical properties on seeded random inputs.
- clang-format and clang-tidy configuration.
- GitHub Actions workflows: CI (GCC, Clang, MSVC and AppleClang; minimum compiler and CMake
  versions; formatting; static analysis), sanitizers (ASan, LSan, UBSan) and releases.
