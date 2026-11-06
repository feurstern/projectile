# Real-time simulation

The simulation module (`include/projectile/simulation/`) advances many projectiles together at a
fixed time step. It integrates their motion, sweeps them through a collision world so that fast
projectiles cannot pass through thin objects, and reports the end of each flight. It builds on
the ballistics module ([ballistics.md](ballistics.md)) and the collision module
([collision.md](collision.md)).

```cpp
#include <projectile/projectile.hpp>

using namespace projectile;

// A world to fly through: here, the ground plane y = 0, made of material 1.
PrimitiveWorld world;
if (world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1)) {
    // invalid shape
}

Simulation simulation; // 120 Hz, semi-implicit Euler
simulation.set_collision_world(&world);
simulation.set_event_callback([](const ProjectileEvent& event) {
    if (event.kind == ProjectileEventKind::hit) {
        spawn_impact_effect(event.hit->point, event.hit->normal, event.hit->material_id);
    }
});

ProjectileConfig bullet;
bullet.radius = 0.005;
bullet.gravity = {0.0, -standard_gravity, 0.0};
bullet.lifetime = 3.0;
if (const Result<ProjectileState> state = create_projectile(bullet, muzzle, aim * 800.0)) {
    const Result<ProjectileId> id = simulation.add_projectile(*state, bullet);
}

// Every frame:
simulation.update(frame_delta);
const double alpha = simulation.interpolation_alpha();
for (const SimulatedProjectile& projectile : simulation.projectiles()) {
    draw_bullet(interpolated_position(projectile, alpha));
}
```

[examples/04_swept_collision](../examples/04_swept_collision/main.cpp) is a complete program.

## Settings

`Simulation simulation;` uses the defaults. For other settings, fill a `SimulationConfig` and
create the simulation with `Simulation::create(config)`, which returns a `Result`: the
simulation, or the `Error` that makes the settings invalid.

| Setting | Default | Meaning |
|---|---|---|
| `time_step` | 1/120 s | Duration of every step. Finite and greater than zero. |
| `max_substeps` | 8 | Most steps one `update()` runs. At least 1. |
| `max_frame_delta` | 0.25 s | Most simulated time that can be owed. Finite and no shorter than `time_step`. |
| `integrator` | `semi_implicit_euler` | The integrator that advances projectiles; see [ballistics.md](ballistics.md). |

## The fixed time step

A frame takes whatever time it takes, but a simulation is stable and reproducible only if
every step has the same length. `update(frame_delta)` therefore adds the frame's duration to an
accumulator and runs as many whole steps as it holds:

```cpp
accumulator += frame_delta;
while (accumulator >= time_step) {
    step();
    accumulator -= time_step;
}
```

Time short of a whole step carries over to the next frame. Three limits keep unusual frames from
destabilizing the game:

| Frame delta | What happens |
|---|---|
| NaN, negative or zero | Adds no time. |
| Longer than `max_frame_delta`, for example after a breakpoint or a loading hitch | Cut to `max_frame_delta`. The rest is dropped. |
| Calls for more than `max_substeps` steps | `max_substeps` steps run now. The rest carry over and run in later updates, even if those frames add no time. |
| Leaves more than `max_frame_delta` owed after the steps | The excess is dropped. A machine that cannot keep up slows the simulation down instead of spending ever longer on each frame (the "spiral of death"). |

`update()` returns a `FrameSteps`: the steps it ran, and the time it dropped. Dropped time is
never simulated, so the game runs slower than real time while it is nonzero.

- **Run your own loop** with `step()`, which runs exactly one step and ignores the accumulator.
- **Run other systems at the same rate** with `FixedTimestep`, the accumulator on its own:

  ```cpp
  FixedTimestep timestep; // or FixedTimestep::create(config)
  const FrameSteps frame = timestep.advance(frame_delta);
  for (std::uint32_t i = 0; i < frame.steps; ++i) {
      ai_tick(timestep.time_step());
  }
  ```

## Projectiles

`add_projectile(state, config)` adds a projectile and returns its `ProjectileId`, or the `Error`
that makes the state or configuration invalid. Create the state with `create_projectile()` (see
[ballistics.md](ballistics.md)). The projectile moves from the next step on.

A projectile then flies until one of these happens:

| Event | Cause | The projectile |
|---|---|---|
| `ProjectileEventKind::hit` | Its sphere touches the collision world | Stops at the contact and leaves |
| `ProjectileEventKind::expired` | Its lifetime runs out | Stops where it is and leaves |
| none | You call `remove_projectile(id)` or `clear()` | Leaves at once |

A projectile added with an inactive state expires at the next step without moving.

**Ids are stable.** An id stays valid until its projectile leaves. After that it is never
valid again: a later projectile never gets the same id, although the memory is reused. (Strictly,
ids repeat once the same storage has been reused 2³² times.) `ProjectileId{}` identifies no
projectile, and `id.value` can be used to hash or print ids.

**Reading projectiles.** `find(id)` returns the projectile or `nullptr`, and `projectiles()`
returns all of them as a span of `SimulatedProjectile`:

| Field | Meaning |
|---|---|
| `id` | Its id |
| `config` | Its `ProjectileConfig` |
| `state` | Its `ProjectileState` after the last step |
| `previous_position` | Its position before the last step, for drawing between steps |

The order of `projectiles()` is deterministic but unspecified: removing a projectile moves the
last one into its place. Pointers from `find()` and the span from `projectiles()` are valid until
the next call that adds, removes or steps projectiles.

## Collisions

Set a world with `set_collision_world(&world)`: any `ICollisionWorld`, such as the library's
`PrimitiveWorld` or your own (see [collision.md](collision.md)). The simulation does not own the
world. Pass `nullptr` to fly without collisions.

Each step advances every projectile with the integrator, then sweeps a sphere of its
`config.radius` along the straight line from where it was to where it is now. At the first
contact:

- the projectile stops, inactive, with its center on that line at the contact and the velocity it
  had there;
- the event's `time` is the time of impact, between the start and the end of the step;
- the event's `hit` is the `HitResult`, whose `point` lies on the surface that was hit.

**Fast projectiles cannot pass through thin objects.** At 120 Hz, a bullet at 1,000 m/s covers
8.3 m per step. Testing only where it ends each step would miss anything thinner than that. The
sweep covers the whole path, so a 1 cm plate stops bullets at any speed. The integration tests
check this for planes, spheres, boxes and triangles at 1,000 and 10,000 m/s.

**The path between steps is a straight line.** Under gravity the true path curves slightly within
a step; the largest gap between the two is `g·dt²/8`, 0.09 mm at 120 Hz.

**Predictions match.** `predict_trajectory()` with the simulation's `time_step`, `integrator` and
world takes the same steps and sweeps, so a preview ends exactly where the simulated projectile
hits: the same contact, position and velocity, bit for bit.

In this version every hit ends the flight. Material responses such as ricochet and penetration
are planned for Milestone 5.

## Events

`set_event_callback(callback)` installs a `std::function<void(const ProjectileEvent&)>` that
receives every event:

| Field | Meaning |
|---|---|
| `kind` | `hit` or `expired` |
| `id` | The projectile's id, no longer valid |
| `time` | Simulation time of the event, in s |
| `state` | The final state, inactive; for a hit, at the contact |
| `config` | The projectile's configuration, for example to compute impact energy from its mass |
| `hit` | The contact, for hits |

Events arrive after the step that produced them has finished, in a deterministic order. By then
the projectiles they report have left the simulation. Inside the callback you may:

- read the simulation, and add or remove projectiles (projectiles added move from the next step
  on);
- replace or clear the callback; the new one receives events from the next step on.

Calls to `update()` and `step()` from inside the callback do nothing. The callback must not
destroy, move or assign the simulation. If it throws, the exception leaves `update()` or
`step()`, and the remaining events of that step are not reported; the simulation stays usable.

## Drawing between steps

When the frame rate differs from the simulation rate, drawing projectiles at their latest state
makes them stutter: some frames show two steps of progress, others none. Draw them between their
last two positions instead:

```cpp
const double alpha = simulation.interpolation_alpha(); // 0 to 1
for (const SimulatedProjectile& projectile : simulation.projectiles()) {
    draw_bullet(interpolated_position(projectile, alpha));
}
```

`interpolation_alpha()` is how far the time carried over reaches into the next step. Drawing
then lags the simulation by less than one step, 8 ms at 120 Hz. A projectile added since the last
step is drawn where it was added.

## Determinism

The same projectiles, settings, world and sequence of calls give bit-identical results in the
same build. The result depends only on the number of steps, not on the frame times that
delivered them. The integration tests check both, comparing runs at 60 Hz, at 144 Hz and with
random frame times.

Other builds may differ in the last bits of their results:

- **Fused multiply-add.** Compilers may contract `a * b + c` into a single fused instruction,
  which rounds once instead of twice. GCC does so by default (`-ffp-contract=fast`), and Clang
  within single expressions (`-ffp-contract=on`), whenever the target CPU has FMA instructions:
  always on ARM64, and on x86-64 with flags such as `-march=native`. MSVC contracts only with
  `/fp:contract`.
- **Standard libraries** implement some functions, such as `std::lerp`, with different
  rounding.
- **`-ffast-math` and `/fp:fast` are unsupported.** They let the compiler reorder arithmetic and
  assume NaN never occurs.

Projectile does not impose `-ffp-contract=off`. Much of it is inline code compiled with your
project's flags, and the differences between standard libraries would remain anyway. For
lockstep networking, where peers must agree bit for bit, run the same build on every peer, or
compile everything with `-ffp-contract=off` and the same compiler, standard library and CPU
architecture.

## Memory

The simulation keeps the memory of projectiles that left and reuses it. Once it has held its
largest number of projectiles, adding, stepping and removing projectiles allocates nothing.
`reserve(count)` allocates that memory up front. The integration tests count heap allocations
over 20 seconds of continuous firing: there are none.

A `PrimitiveWorld` allocates only when shapes are added. Events are delivered without allocating;
the callback itself is stored once, by `set_event_callback()`.

## Threads

A `Simulation` may be used by one thread at a time. Its steps query the collision world on the
calling thread. Simulations on different threads may share a collision world that answers queries
concurrently, such as a `PrimitiveWorld` to which no shapes are added meanwhile.

## Limitations

- **Every hit ends the flight.** Ricochet and penetration come with Milestone 5.
- **Projectiles do not collide with each other.**
- **The world is static during a step.** The sweep does not account for a target that moves
  within the step. Move targets between updates; very fast targets may then be missed.
- **`PrimitiveWorld` tests every shape.** It suits small scenes; for large ones, implement
  `ICollisionWorld` on top of a spatial index or an engine's physics.
