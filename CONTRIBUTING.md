# Contributing to Projectile

Thank you for helping. This guide covers the development setup, the conventions the code
follows, and what "done" means for a change.

## Development setup

You need:

- A C++20 compiler: GCC 12+, Clang 16+, MSVC 2022, or AppleClang 15+.
- CMake 3.21+ to use the presets. The project itself needs only 3.20.
- Ninja (recommended).
- clang-format and clang-tidy **22.1.8**, the versions CI uses. If your distribution ships
  other versions, install these with `pip install clang-format==22.1.8 clang-tidy==22.1.8`.

The everyday loop uses the `dev` preset: a Debug build with tests and warnings as errors.

```sh
export CMAKE_GENERATOR=Ninja
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Before opening a pull request, also run the checks that CI runs:

```sh
# Sanitizers (GCC or Clang; Linux or macOS)
cmake --preset sanitize && cmake --build --preset sanitize && ctest --preset sanitize

# Shared-library build, in its own directory
cmake --preset release-tests -B build/shared -DBUILD_SHARED_LIBS=ON -DPROJECTILE_WARNINGS_AS_ERRORS=ON
cmake --build build/shared && ctest --test-dir build/shared --output-on-failure

# Static analysis: clang-tidy runs while the dev build compiles
cmake --preset dev -DPROJECTILE_ENABLE_CLANG_TIDY=ON && cmake --build --preset dev

# Formatting
git ls-files -z -- '*.hpp' '*.cpp' | xargs -0 -r clang-format --dry-run --Werror
```

To fix formatting, run the last command with `-i` in place of `--dry-run --Werror`.

The benchmarks and the raylib demos are optional, and download Google Benchmark and raylib if they
are not installed. Build them when you change them, or code they use:

```sh
cmake --preset bench && cmake --build --preset bench    # see docs/benchmarks.md
cmake --preset demos && cmake --build --preset demos && ctest --preset demos
```

Without a display, the demo tests are skipped; `xvfb-run -a ctest --preset demos` runs them, as CI
does.

## Repository layout

| Path | Contents |
|---|---|
| `include/projectile/<module>/` | Public headers, one directory per module (`core`, `math`, `collision`, ...). |
| `src/<module>/` | Library implementation files, and headers internal to the library, which are not installed. |
| `tests/unit/<module>/` | Catch2 unit tests, mirroring the header layout. |
| `tests/integration/` | Tests of complete workflows across modules, in their own test program. |
| `tests/support/` | Shared test helpers: value printers, tolerance matchers, the seeded random generator, an allocation counter. |
| `tests/package/` | Downstream projects that test installation and embedding. |
| `examples/` | Example programs, one directory each, run as tests. |
| `adapters/raylib/` | The optional raylib adapter (`include/`), its demos (`examples/`), their tests, and the script that records the README's GIFs (`tools/`). Not part of the core library. |
| `benchmarks/` | The benchmarks, their workloads, and `report.py`, which summarizes results. |
| `docs/` | Technical documentation: the [mathematical conventions](docs/mathematics.md), the [collision query conventions](docs/collision.md), the [ballistics](docs/ballistics.md), the [simulation](docs/simulation.md), the [material interactions](docs/materials.md), [engine integration](docs/integration.md) and the [benchmarks](docs/benchmarks.md). `docs/media/` holds the README's GIFs. |
| `cmake/` | CMake modules: compiler options, version parsing, the package template. |
| `.github/workflows/` | CI, sanitizer, release, and demo-and-benchmark workflows. |

## Coding conventions

The naming rules below are enforced by clang-tidy (`readability-identifier-naming`).

| Element | Style | Example |
|---|---|---|
| Namespaces, functions, variables, parameters, data members | `snake_case` | `predict_trajectory`, `max_distance` |
| Classes, structs, enums, template parameters | `PascalCase` | `HitResult`, `SemiImplicitEuler` |
| Enumerators | `snake_case` | `IntegratorKind::semi_implicit_euler` |
| Private and protected data members | `snake_case_` | `accumulator_` |
| Macros | `PROJECTILE_` + `SCREAMING_SNAKE_CASE` | `PROJECTILE_API` |
| Compile-time constants | `snake_case`, or `SCREAMING_SNAKE_CASE` where clearer | `default_frequency` |
| Files | `snake_case` | `collision_world.hpp`, `test_raycast.cpp` |

### Headers

- Use `#pragma once`.
- Include what you use.
- Inside the library, include project headers with quotes and the full path:
  `"projectile/core/version.hpp"`. Tests, examples and users write `<projectile/...>`.
- Every public header must compile on its own. The build checks this.
- Never put `using namespace` in a header.
- Public API lives in `namespace projectile`. Keep implementation details in `src/`, or in
  `namespace projectile::detail` when templates require them in a header.
- Code shared by several library source files goes in an internal header under `src/<module>/`,
  included as `"<module>/<name>.hpp"`. Internal headers are not installed.
- Mark functions defined in `src/` with `PROJECTILE_API` so that shared builds export them.
  Inline, `constexpr` and template code needs no marker.
- Call `(std::min)(a, b)` and `(std::max)(a, b)` in public headers, with the parentheses, so that
  the `min`/`max` macros of `<windows.h>` cannot break them.

### Initialization

- Initialize vectors positionally: `Vec3{x, y, z}`.
- Use designated initializers for structs whose members differ in meaning:
  `Ray{.origin = muzzle, .direction = aim}`.
- clang-tidy cannot tell these cases apart, so this rule is enforced in review.

### API documentation

Document every public declaration with a Doxygen comment (`///`, `@param`, `@return`). State
units, preconditions, and what happens on invalid input.

### Units and numerics

The full conventions are in [docs/mathematics.md](docs/mathematics.md). In short:

- SI units (meters, seconds, kilograms, newtons) and radians.
- `double` throughout.
- There is no fixed "up" axis: gravity is a configurable vector.
- Compare computed floating-point values with `approx_equal()` or a tolerance. Exact comparison
  is only for results that are exact by construction, such as arithmetic on small integers or
  powers of two.

### Error handling

- An expected geometric miss is not an error; return `std::optional`.
- Reject invalid configuration explicitly at the public API boundary, for example non-finite or
  negative values. Functions that take a configuration return a `Result<T>` or a
  `std::optional<Error>`; add an `Error` value, with a description, for each new kind of problem.
- Do not use exceptions as control flow in collision queries.

### Performance

- No heap allocations in per-frame update loops.
- Prefer `std::span` for non-owning ranges.
- Avoid virtual dispatch and `std::shared_ptr` in hot paths.
- No SIMD or threading before benchmarks justify it.
- Measure changes that affect performance: run the benchmarks before and after on the same
  machine, and compare the runs with `benchmarks/report.py` ([docs/benchmarks.md](docs/benchmarks.md)).

## Tests

- Write tests with Catch2 v3 in `tests/unit/<module>/test_<name>.cpp`, and add new files to
  `tests/CMakeLists.txt`. Tests of whole workflows across modules go in `tests/integration/`.
- Tag test cases as `[module][topic]`. CTest labels separate `unit` from `package` tests.
- Test behavior, not construction. Compare numerical results against analytical solutions,
  with tolerances that suit the algorithm (`Catch::Matchers::WithinAbs` / `WithinRel`).
- Cover edge cases: zero vectors, parallel rays, degenerate geometry, boundary contact and
  non-finite input.
- Use the helpers in `tests/support/`:
  - `math_matchers.hpp` prints math types with all 17 digits.
  - `CHECK_THAT(actual, is_near(expected))` compares vectors within a tolerance.
  - `require_value()` unwraps a `std::optional`, failing the test if it is empty.
- Tag property tests (properties checked on many random inputs) `[property]`. Draw the inputs from
  `projectile::test::Random` (`support/random.hpp`), which uses Catch2's seed:
  - CTest pins the seed with `--rng-seed=20261105`, so failures are reproducible.
  - To explore other inputs, run the test binary with `--rng-seed random`.
  - Skip inputs that are ill-conditioned for the property under test, and explain why in a
    comment.
  - Make at most one draw per function call. The order in which a call's arguments are evaluated
    is unspecified, so `f(random.vec3(...), random.vec3(...))` draws in a different order on
    different compilers. Draw into named variables first; braced initializers such as
    `Vec3{random.uniform(...), ...}` are evaluated left to right.
- Keep tolerances valid when the compiler fuses multiply-adds (Clang does on ARM64): compare
  exactly only where the arithmetic is exact.
- Compare points inside a segment within a tolerance. They come from `std::lerp`, whose rounding
  differs between standard libraries; only the endpoints are exact.
- Derive a property test's expected outcome from the actual geometry, not from how the input was
  generated. A random point meant to lie outside a shape can still land inside it.
- Test code has its own `tests/.clang-tidy`. It inherits the project configuration and disables
  only a check that misfires on Catch2's macros. Likewise, `examples/.clang-tidy` allows `printf`
  and exceptions escaping `main` in the example programs, and `adapters/raylib/examples/.clang-tidy`
  allows exceptions escaping `main` in the demos.
- To prove that code does not allocate, count allocations with `test::allocation_count()`
  (`support/allocation_counter.hpp`) around it, in the integration tests, which replace the
  global `operator new`. Keep Catch2 assertions out of the counted code: they allocate.
- Check geometry against an independent computation. The sweep tests compare every contact with
  the distance to the shape, and box sweeps with sweeps against the box's triangles; this found a
  bug that hand-picked cases missed.
- Measure an integrator's order of convergence on a problem that shows it. For example,
  semi-implicit Euler is only first order, yet its error on a circular orbit sampled after whole
  periods shrinks at second order.

## Demos and benchmarks

- The demos use only Projectile's public API and the adapter. CI enforces this by building them
  on their own against the installed library.
- Each demo's scripted run checks that the demo shows what it should (`check()` in each demo).
  Update the check when you change the script.
- When a demo looks different, record the GIFs again with
  `python3 adapters/raylib/tools/record_demos.py build/demos --xvfb`, and keep each under about
  2 MB.
- Benchmarks draw their inputs from `workloads.hpp`, never from `<random>` distributions, so that
  every platform measures the same work. When you add one, describe it in
  [docs/benchmarks.md](docs/benchmarks.md).

## Definition of done

A change is complete only when:

- it compiles without warnings;
- the relevant tests exist and pass, and existing tests still pass;
- the public API it adds or changes is documented;
- it adds no dependency to the core library;
- the README or technical documentation is updated, and `CHANGELOG.md` lists user-visible
  changes under `[Unreleased]`;
- it matches the architecture: the core stays engine-independent, and adapters stay optional.

Compiling is not the same as done.

## Pull requests

- Keep each pull request to one logical change, and explain what it does and why.
- CI must pass: build matrix, sanitizers, clang-tidy, clang-format, and the demo and benchmark
  workflow.
- Explain any change to an existing public API in the pull request description.

## Releasing (maintainers)

1. Set the new version in `include/projectile/core/version.hpp`. This is the only place it
   lives; CMake reads it from there.
2. In `CHANGELOG.md`, move the `[Unreleased]` entries under a new `## [X.Y.Z] - YYYY-MM-DD`
   heading.
3. Commit, then tag and push: `git tag -a vX.Y.Z -m "Projectile X.Y.Z"` and
   `git push origin vX.Y.Z`.
4. The Release workflow then:
   - runs the full CI;
   - checks that the tag matches `version.hpp` and that `CHANGELOG.md` has the section;
   - builds `.tar.gz` and `.zip` source archives with a `SHA256SUMS` file;
   - publishes the GitHub release, with the changelog section as its notes.

   Tags with a suffix, such as `vX.Y.Z-rc1`, are published as pre-releases.

Running the Release workflow manually from the Actions tab performs a dry run: it builds the
archives but publishes nothing.
