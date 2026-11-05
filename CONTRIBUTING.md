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

## Repository layout

| Path | Contents |
|---|---|
| `include/projectile/<module>/` | Public headers, one directory per module (`core`, `math`, `collision`, ...). |
| `src/<module>/` | Library implementation files. |
| `tests/unit/<module>/` | Catch2 unit tests, mirroring the header layout. |
| `tests/support/` | Shared test helpers: value printers, tolerance matchers, the seeded random generator. |
| `tests/package/` | Downstream projects that test installation and embedding. |
| `docs/` | Technical documentation: the [mathematical conventions](docs/mathematics.md) and the [collision query conventions](docs/collision.md). |
| `cmake/` | CMake modules: compiler options, version parsing, the package template. |
| `.github/workflows/` | CI, sanitizer and release workflows. |

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
  negative values.
- Do not use exceptions as control flow in collision queries.

### Performance

- No heap allocations in per-frame update loops.
- Prefer `std::span` for non-owning ranges.
- Avoid virtual dispatch and `std::shared_ptr` in hot paths.
- No SIMD or threading before benchmarks justify it.

## Tests

- Write tests with Catch2 v3 in `tests/unit/<module>/test_<name>.cpp`, and add new files to
  `tests/CMakeLists.txt`.
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
- Keep tolerances valid when the compiler fuses multiply-adds (Clang does on ARM64): compare
  exactly only where the arithmetic is exact.
- Compare points inside a segment within a tolerance. They come from `std::lerp`, whose rounding
  differs between standard libraries; only the endpoints are exact.
- Derive a property test's expected outcome from the actual geometry, not from how the input was
  generated. A random point meant to lie outside a shape can still land inside it.
- Test code has its own `tests/.clang-tidy`. It inherits the project configuration and disables
  only a check that misfires on Catch2's macros.

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
- CI must pass: build matrix, sanitizers, clang-tidy and clang-format.
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
