# Projectile

Projectile is an engine-independent C++20 library for real-time projectile simulation: hitscan
and ray queries, ballistic motion under gravity, drag and wind, trajectory prediction, swept
collision, and configurable material interactions such as ricochet and penetration. It depends
only on the C++ standard library. It plugs into your engine through small interfaces instead
of owning your scene.

> **Status: pre-alpha, Milestone 4 of 7.** In place so far:
> - the build, packaging, test and CI foundation;
> - the math module: vectors, rays, segments, planes, spheres, boxes and triangles;
> - the collision module: ray, segment and swept-sphere queries, the collision-world interface,
>   and a ready-made world built from primitives;
> - the ballistics module: projectiles under gravity, semi-implicit Euler and RK4 integration,
>   and trajectory prediction;
> - the simulation module: many projectiles at a fixed time step, with swept collisions,
>   lifetimes and events.
>
> The features marked as planned below are **not implemented yet**.

## Feature roadmap

| Feature | Release | Status |
|---|---|---|
| Vector and mathematical utilities | v0.1 | Done (Milestone 1) |
| Ray and geometric intersection | v0.1 | Done (Milestone 2) |
| Hitscan queries | v0.1 | Done (Milestone 2) |
| Projectile integration (semi-implicit Euler, RK4) | v0.1 | Done (Milestone 3) |
| Gravity and trajectory prediction | v0.1 | Done (Milestone 3) |
| Fixed-timestep simulation | v0.1 | Done (Milestone 4) |
| Swept sphere collision | v0.2 | Done (Milestone 4) |
| Wind and aerodynamic drag | v0.2 | Planned (Milestone 5) |
| Material interaction interface | v0.2 | Planned (Milestone 5) |
| Ricochet and penetration | v0.3 | Planned (Milestone 5) |
| raylib visualization adapter and demos | v0.3 | Planned (Milestone 6) |
| Benchmark suite | v0.3 | Planned (Milestone 6) |
| Single-header distribution | v1.0 | Optional |

Material interactions are configurable gameplay approximations. They are not validated
real-world penetration models.

## Architecture

The module layering is shown below. Arrows point from a module to the modules it uses. Every
module except `materials` exists today; the adapter is planned.

```mermaid
flowchart TD
    game["Your game or engine"] --> simulation
    raylib["adapters/raylib<br/>(optional, separate target)"] --> simulation
    world["Your collision world<br/>(implements ICollisionWorld)"] -.-> collision

    subgraph lib["Projectile core library (C++20, standard library only)"]
        simulation["simulation<br/>fixed timestep, projectile manager"]
        materials["materials<br/>interaction, ricochet, penetration"]
        ballistics["ballistics<br/>state, forces, integrators, trajectory"]
        collision["collision<br/>raycast, intersection, sweep"]
        math["math<br/>Vec2/Vec3, Ray, Plane, AABB, Sphere"]
        core["core<br/>version, export macro"]

        simulation --> materials
        simulation --> ballistics
        simulation --> collision
        materials --> ballistics
        materials --> collision
        ballistics --> math
        collision --> math
        math --> core
    end
```

## Requirements

- **Compiler with C++20:** GCC 12+, Clang 16+, MSVC 2022 (19.30+), or AppleClang 15+.
- **CMake 3.20+.** The presets in `CMakePresets.json` need CMake 3.21+.
- **Ninja** is recommended but not required. Any CMake generator works.

The library never downloads anything. Only the test suite has a dependency, Catch2 v3. It uses an
installed Catch2 3 if one is found; otherwise it downloads the pinned v3.16.1 release, which is
verified by SHA-256.

CI builds and tests these configurations:

| Platform | Compiler | Generator | Library |
|---|---|---|---|
| Ubuntu 24.04 | GCC 12, GCC 14, Clang 16 | Ninja | static |
| Ubuntu 24.04 | Clang 18 | Ninja | shared |
| Ubuntu 24.04 | GCC 12 with CMake 3.20 (no presets) | Unix Makefiles | static |
| Windows Server 2022 | MSVC 2022 | Visual Studio 17 2022 | static and shared |
| macOS 15 | AppleClang | Unix Makefiles | static |

There are also AddressSanitizer + UndefinedBehaviorSanitizer runs with GCC 14 and Clang 18, a
clang-tidy run and a clang-format check.

## Building

```sh
export CMAKE_GENERATOR=Ninja   # optional; omit it to use CMake's default generator

cmake --preset dev             # configure: Debug, tests, examples, warnings as errors
cmake --build --preset dev     # build
ctest --preset dev             # run the tests
```

| Preset | Build type | Tests and examples | Notes |
|---|---|---|---|
| `debug` | Debug | yes | |
| `release` | Release | no | the library only |
| `release-tests` | Release | yes | what CI runs |
| `sanitize` | Debug | yes | ASan, LSan and UBSan; GCC or Clang; not on Windows |
| `dev` | Debug | yes | warnings as errors |

Each preset builds into `build/<preset>`. Pass `-B <dir>` when configuring to use another
directory, for example to keep GCC and Clang builds side by side.

Without presets, CMake 3.20 is enough:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Options

| Option | Default | Description |
|---|---|---|
| `PROJECTILE_BUILD_TESTS` | ON at top level | Build the test suite. This fetches Catch2 3 if it is not installed. |
| `PROJECTILE_BUILD_EXAMPLES` | ON at top level | Build the example programs in `examples/`. |
| `PROJECTILE_INSTALL` | ON at top level | Generate install rules and the `find_package` package. |
| `PROJECTILE_WARNINGS_AS_ERRORS` | OFF | Treat warnings in Projectile's own targets as errors. |
| `PROJECTILE_ENABLE_CLANG_TIDY` | OFF | Run clang-tidy on Projectile's sources while building (Ninja and Makefile generators). |
| `BUILD_SHARED_LIBS` | OFF | Build a shared library instead of a static one. |

"Top level" means Projectile is the project being built. When another project embeds it through
`add_subdirectory` or `FetchContent`, the tests and install rules are off unless that project
turns them on. Projectile's warning flags never propagate to consumers.

## Using Projectile in your project

### Installed package

```sh
cmake --preset release
cmake --build --preset release
cmake --install build/release --prefix /path/to/prefix
```

```cmake
find_package(Projectile 0.1 CONFIG REQUIRED)   # configure with -DCMAKE_PREFIX_PATH=/path/to/prefix
target_link_libraries(my_game PRIVATE Projectile::Projectile)
```

Before 1.0, minor releases may break compatibility, so `find_package(Projectile 0.1)` accepts only
0.1.x releases.

### FetchContent or add_subdirectory

```cmake
include(FetchContent)
FetchContent_Declare(
    Projectile
    GIT_REPOSITORY <repository-url>
    GIT_TAG        <release-tag>
)
FetchContent_MakeAvailable(Projectile)

target_link_libraries(my_game PRIVATE Projectile::Projectile)
```

`add_subdirectory(path/to/projectile)` works the same way. Linking `Projectile::Projectile`
provides the include paths and the C++20 requirement.

### Shared library on Windows

Windows finds DLLs next to the executable or on `PATH`. When Projectile is built as a shared
library, copy it next to your program:

```cmake
add_custom_command(TARGET my_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Projectile::Projectile> $<TARGET_FILE_DIR:my_game>)
```

### Troubleshooting: `CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS-NOTFOUND`

Linking `Projectile::Projectile` makes your target C++20. With CMake 3.28 or newer, CMake then
scans C++20 sources for module imports. With Clang, that scan needs `clang-scan-deps`, which
some distributions package separately (on Ubuntu, `clang-tools-NN`). If your project does not
use C++20 modules, turn the scan off instead:

```cmake
set_target_properties(my_game PROPERTIES CXX_SCAN_FOR_MODULES OFF)
# or, for every target in a directory: set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
```

### Quick start

Describe a projectile, create it, and advance it at a fixed time step:

```cpp
#include <projectile/projectile.hpp>

#include <cstdio>

using namespace projectile;

int main() {
    ProjectileConfig config;
    config.mass = 0.01;                             // kg
    config.radius = 0.005;                          // m
    config.initial_velocity = {0.0, 30.0, 0.0};     // m/s
    config.gravity = {0.0, -standard_gravity, 0.0}; // no built-in "up": +y is up here

    const Result<ProjectileState> created = create_projectile(config, {0.0, 10.0, 0.0});
    if (!created) {
        return 1; // describe(created.error()) says what is wrong
    }
    ProjectileState state = *created;

    // Advance one second at 120 Hz.
    const SemiImplicitEuler integrator;
    for (int i = 0; i < 120; ++i) {
        integrator.step(state, config, 1.0 / 120.0);
    }
    std::printf("after 1 s: height %.2f m, vertical speed %.2f m/s\n", state.position.y, state.velocity.y);

    // Preview the next 5 s, stepping exactly as the loop above does.
    TrajectoryConfig preview;
    preview.sample_interval = 0.5;
    preview.max_duration = 5.0;
    if (const Result<Trajectory> trajectory = predict_trajectory(state, config, preview)) {
        for (const TrajectorySample& sample : trajectory->samples) {
            std::printf("t = %.1f s: height %.2f m\n", sample.time, sample.position.y);
        }
    }
}
```

See [docs/ballistics.md](docs/ballistics.md) for the integrators' accuracy and stability, custom
forces, and trajectory prediction against a collision world.

### Real-time simulation

A `Simulation` advances many projectiles at a fixed time step, sweeps them through a collision
world, and reports each flight's end:

```cpp
#include <projectile/projectile.hpp>

#include <cstdio>

using namespace projectile;

int main() {
    // A world to hit: the ground, and a 1 cm steel plate 50 m away (+y is up).
    PrimitiveWorld world;
    if (world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1) ||
        world.add(AABB{.min = {50.0, 0.0, -1.0}, .max = {50.01, 2.0, 1.0}}, 2)) {
        return 1; // a shape was invalid
    }

    Simulation simulation; // fixed steps at 120 Hz
    simulation.set_collision_world(&world);
    simulation.set_event_callback([](const ProjectileEvent& event) {
        if (event.hit) {
            std::printf("hit material %u at x = %.3f m after %.4f s\n",
                        event.hit->material_id, event.hit->point.x, event.time);
        }
    });

    ProjectileConfig bullet;
    bullet.radius = 0.005;
    bullet.gravity = {0.0, -standard_gravity, 0.0};
    for (const double speed : {300.0, 900.0}) {
        const Result<ProjectileState> state =
            create_projectile(bullet, {0.0, 1.5, 0.0}, {speed, 0.0, 0.0});
        if (!state || !simulation.add_projectile(*state, bullet)) {
            return 1;
        }
    }

    // The game loop: one update per frame, with the time the frame took.
    while (!simulation.projectiles().empty()) {
        simulation.update(1.0 / 60.0);
        // Draw each projectile at interpolated_position(projectile, simulation.interpolation_alpha()).
    }
}
```

At 900 m/s a bullet covers 7.5 m per step, yet it stops at the plate's surface: every step sweeps
the projectile's whole path. See [docs/simulation.md](docs/simulation.md) for the fixed time step
and its limits, projectile ids and events, drawing between steps, determinism and memory.

### Hitscan

Rays and segments can be tested against the geometric primitives:

```cpp
#include <projectile/projectile.hpp>

#include <cstdio>
#include <optional>

using namespace projectile;

int main() {
    // A shot from a muzzle 1.5 m above the ground. make_ray() normalizes the direction, and
    // returns std::nullopt if there is none (zero length, NaN or infinity).
    const std::optional<Ray> shot = make_ray(Vec3{0.0, 1.5, 0.0}, Vec3{40.0, -1.0, 3.0});
    if (!shot) {
        return 1;
    }

    // Hitscan against a target within 100 m.
    const Sphere target{.center = {20.0, 1.0, 1.5}, .radius = 0.5};
    if (const std::optional<HitResult> hit = intersect(*shot, target, 100.0)) {
        std::printf("target hit after %.2f m\n", hit->distance);
        // The normal faces the shooter, so reflecting off it gives the ricochet direction.
        const Vec3 ricochet = reflect(shot->direction, hit->normal);
        std::printf("ricochet: {%.2f, %.2f, %.2f}\n", ricochet.x, ricochet.y, ricochet.z);
    }

    // Where would the shot reach the ground plane y = 0?
    const Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    if (const std::optional<HitResult> hit = intersect(*shot, ground)) {
        std::printf("ground reached after %.1f m\n", hit->distance);
    }
}
```

All quantities are in SI units, and angles are in radians. See
[docs/mathematics.md](docs/mathematics.md) for the mathematical conventions, and
[docs/collision.md](docs/collision.md) for the query conventions: what a hit reports, front and
back faces, and boundary and degenerate cases. To query your game's own geometry, implement
`ICollisionWorld` and call `raycast(world, ray, max_distance)`; `docs/collision.md` has a complete
example. For a handful of shapes, the ready-made `PrimitiveWorld` does the job; see
[examples/03_hitscan](examples/03_hitscan/main.cpp).

### Checking the library version

To detect headers and a library from different releases, compare the version your program was
compiled against with the version of the library it linked:

```cpp
#include <projectile/projectile.hpp>

#include <cstdio>

int main() {
    // PROJECTILE_VERSION_STRING is the version of the headers you compiled against;
    // projectile::version() and version_string() describe the library you linked.
    const projectile::Version headers{
        .major = PROJECTILE_VERSION_MAJOR,
        .minor = PROJECTILE_VERSION_MINOR,
        .patch = PROJECTILE_VERSION_PATCH,
    };
    if (projectile::version() != headers) {
        std::fprintf(stderr, "Projectile headers and library do not match\n");
        return 1;
    }

    const auto linked = projectile::version_string();
    std::printf("Projectile %.*s\n", static_cast<int>(linked.size()), linked.data());
}
```

## Examples

The programs in [examples/](examples/) are built with the project:

| Example | Shows |
|---|---|
| [01_basic_projectile](examples/01_basic_projectile/main.cpp) | Creating a projectile and stepping it until it lands, printing its flight |
| [02_ballistic_drop](examples/02_ballistic_drop/main.cpp) | Predicting a horizontal shot with both integrators, compared with the exact drop |
| [03_hitscan](examples/03_hitscan/main.cpp) | Building a world of primitives with materials, firing rays into it, and computing ricochet directions |
| [04_swept_collision](examples/04_swept_collision/main.cpp) | A simulation driven by uneven frame times, stopping bullets of up to 2,000 m/s at a 1 cm plate |

Run them from the build directory, for example `build/dev/bin/02_ballistic_drop`.

## Tests

`ctest --preset dev` runs the whole suite. Tests carry labels:

- `unit`: Catch2 unit tests. These include property tests that check mathematical properties on
  10,000 random inputs each. CTest pins the random seed, so every run checks the same inputs; to
  explore other inputs, run `build/dev/bin/projectile_unit_tests "[property]" --rng-seed random`.
- `integration`: complete simulation workflows. Hundreds of projectiles fly through a room, fast
  projectiles meet thin shapes of every kind, repeated runs are compared bit for bit, and heap
  allocations are counted.
- `package`: installs the library into a staging prefix, then configures, builds and runs a
  small downstream project with `find_package(Projectile)` and with `add_subdirectory`. These
  take a few seconds; skip them with `ctest --preset dev -LE package`.
- `example`: runs each example program, which must complete successfully.

During the build, every public header is also compiled on its own, which proves that each
header is self-contained.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow, coding conventions, the
definition of done and the release process.

## License

Projectile is released under the [MIT License](LICENSE).
