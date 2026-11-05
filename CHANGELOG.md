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
- Test suite built on Catch2 v3:
  - unit tests;
  - a build-time check that every public header compiles on its own;
  - package tests that consume the installed and the embedded library from a downstream
    project.
- clang-format and clang-tidy configuration.
- GitHub Actions workflows: CI (GCC, Clang, MSVC and AppleClang; minimum compiler and CMake
  versions; formatting; static analysis), sanitizers (ASan, LSan, UBSan) and releases.
