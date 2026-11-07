# Ballistics

The ballistics module (`include/projectile/ballistics/`) moves projectiles through time. It
describes projectiles, the forces on them (gravity, and optionally air drag and wind), the
numerical integrators that advance them, kinetic energy, and trajectory prediction. All quantities
are in SI units (see [mathematics.md](mathematics.md)).

## Describing a projectile

A projectile has two parts:

| Part | Holds | Changes |
|---|---|---|
| `ProjectileConfig` | Mass, radius, initial velocity, gravity, lifetime, drag and wind | Never; many projectiles can share one |
| `ProjectileState` | Position, velocity, acceleration, remaining lifetime, active flag | Every step |

```cpp
ProjectileConfig config;
config.mass = 0.01;                             // kg
config.radius = 0.005;                          // m; 0 makes a point projectile
config.initial_velocity = {0.0, 30.0, 0.0};     // m/s
config.gravity = {0.0, -standard_gravity, 0.0}; // m/s², +y up in this world
config.lifetime = 10.0;                         // s; infinite by default

Result<ProjectileState> state = create_projectile(config, {0.0, 10.0, 0.0});
if (!state) {
    std::printf("%s\n", describe(state.error()).data()); // for example "mass must be finite..."
}
```

- **Gravity has no default direction.** Projectile assumes no up axis, so `gravity` defaults to
  zero. Build it from `standard_gravity` (9.80665 m/s²) for your world's up axis.
- **Mass does not change the motion under gravity alone.** It is still validated, because
  features such as drag and impact energy depend on it.

**Invalid input is rejected explicitly.** `create_projectile()` returns a `Result`: either the
state, or the `Error` that prevented it, such as `Error::invalid_mass`. `validate(config)` and
`validate(state)` report the first problem as a `std::optional<Error>`, and `describe(error)`
turns an error into a sentence.

## Forces

The integrators advance a projectile under an **acceleration field**: any callable
`Vec3 field(const Vec3& position, const Vec3& velocity)` that returns the acceleration in m/s².
The `AccelerationField` concept checks this signature.

- **`forces_of(config)`** gives the forces Projectile applies, as a `ProjectileForces` field:
  uniform gravity, and air drag when the configuration has it (see below).
- **Your own field** models forces Projectile does not provide. Pass it to an integrator's
  `step(state, dt, field)`:

  ```cpp
  // Gravity plus linear air resistance, a = g - k v.
  const auto field = [g = config.gravity](const Vec3& /*position*/, const Vec3& velocity) {
      return g - velocity * 0.1;
  };
  ProjectileState projectile = *state;
  RungeKutta4{}.step(projectile, 1.0 / 120.0, field);
  ```

## Drag and wind

By default a projectile feels gravity only, which suits most games. For air resistance, set a drag
coefficient and a reference area:

```cpp
ProjectileConfig bullet;
bullet.mass = 0.008;                                                     // kg
bullet.radius = 0.0045;                                                  // m
bullet.gravity = {0.0, -standard_gravity, 0.0};
bullet.drag_coefficient = 0.3;                                           // C_d
bullet.reference_area = std::numbers::pi * bullet.radius * bullet.radius; // A, in m²
bullet.wind = {0.0, 0.0, 2.0};                                           // m/s, a crosswind
// bullet.air_density defaults to 1.225 kg/m³, sea level
```

Drag is the quadratic drag force of a body moving fast through air, against the velocity
relative to the air:

    F_d = −½ ρ C_d A |v − w| (v − w)

where `ρ` is `air_density`, `C_d` is `drag_coefficient`, `A` is `reference_area` and `w` is
`wind`. The acceleration is then

    a = (m g + F_d) / m = g − k |v − w| (v − w),    k = ½ ρ C_d A / m

`drag_factor(config)` gives `k`, in 1/m. For the 8 g bullet above it is 1.5 · 10⁻³ per metre: at
800 m/s, drag decelerates it at about 940 m/s².

| Setting | Default | Notes |
|---|---|---|
| `drag_coefficient` | 0 (no drag) | About 0.47 for a sphere, 0.2 to 0.4 for bullets. |
| `reference_area` | 0 | Required, greater than zero, whenever `drag_coefficient` is: usually the frontal cross-section, `π r²` for a sphere. |
| `air_density` | 1.225 kg/m³ | `standard_air_density`, sea level at 15 °C. Zero is a vacuum. |
| `wind` | zero | Matters only with drag. |

**What drag does:**

- **It slows projectiles.** Without gravity, `dv/dt = −k v²` gives `v(t) = v₀ / (1 + k v₀ t)`.
- **Falling projectiles reach a terminal velocity** `v_t = √(g / k)`, at which drag balances
  gravity. Falling from rest, `v(t) = v_t tanh(g t / v_t)`.
- **Wind carries projectiles along.** Drag acts on the velocity relative to the air, so a
  projectile at rest in a wind accelerates until it moves with the air.
- **It only takes energy away.** Without gravity, every step lowers the kinetic energy relative
  to the air, and a projectile falling from rest never exceeds its terminal velocity.

The tests check each of these against the exact solutions above, with both integrators.

**Accuracy.** Drag depends on the velocity, so neither integrator is exact. The table measures
one second of flight against `k = 0.5` per metre, from 10 m/s:

| Steps per second | `SemiImplicitEuler` position error | `RungeKutta4` position error |
|---|---|---|
| 25 | 4.0 · 10⁻¹ m | 4.9 · 10⁻⁵ m |
| 50 | 1.9 · 10⁻¹ m | 2.6 · 10⁻⁶ m |
| 100 | 9.6 · 10⁻² m | 1.5 · 10⁻⁷ m |
| 200 | 4.8 · 10⁻² m | 8.7 · 10⁻⁹ m |

Halving the step halves Euler's error and divides RK4's by 16; the tests check both orders.

**Stability.** Drag is strongest when the projectile moves fast through the air. Ignoring gravity,
one `SemiImplicitEuler` step multiplies the speed relative to the air by `1 − k |v − w| · dt`, so
the product `k |v − w| · dt` must stay below 1, or drag reverses the projectile within a step.
Keep it well below 1 with either integrator. For bullets at 120 Hz it is around 0.01; very light,
draggy objects at high speed need smaller steps.

**Assumptions.** The model treats the projectile as a point with a constant drag coefficient: it
ignores how `C_d` changes with speed (notably near the speed of sound), spin and lift (the Magnus
effect), and tumbling. The air density and the wind are uniform and constant for each projectile,
so changing them affects only projectiles created with the new values. For other forces, pass
your own acceleration field to the integrators.

## Integrators

Both integrators are stateless structs with the same interface. `step(state, config, dt)` uses
the configured forces, and `step(state, dt, field)` uses any acceleration field. To choose one at
run time, for example from a configuration file, use `step(IntegratorKind, state, config, dt)`.

| Integrator | Update | Force evaluations per step | Order |
|---|---|---|---|
| `SemiImplicitEuler` (default) | `v += a(x, v)·dt`, then `x += v·dt` with the new `v` | 1 | 1 |
| `RungeKutta4` | Classical fourth-order Runge-Kutta on position and velocity | 4 | 4 |

After each step, both evaluate the field once more, to refresh `ProjectileState::acceleration`
for display; neither reads that field as input. `SemiImplicitEuler` is the default because it is
cheap and stable; benchmarks (Milestone 6) will show whether RK4's accuracy is worth its cost
in a full simulation.

## Numerical error

**Under gravity alone**, both results are known in closed form. After `n` steps of `dt`, at time
`t = n·dt`:

- `SemiImplicitEuler` has exactly the right velocity, `v0 + g·t`.
- Its position is `x0 + v0·t + ½·g·t² + ½·g·t·dt`: off the exact path by `½·g·t·dt`, a
  displacement along gravity that grows linearly in time and step size.
- `RungeKutta4` is exact up to rounding.

Measured after 2 s, starting at 25.5 m/s:

| Rate | `SemiImplicitEuler` position error | `RungeKutta4` position error |
|---|---|---|
| 30 Hz | 0.327 m | 1e-14 m |
| 60 Hz | 0.163 m | 3e-14 m |
| 120 Hz | 0.082 m | 1e-13 m |
| 240 Hz | 0.041 m | 1e-13 m |

**With velocity-dependent forces**, such as drag, neither integrator is exact. The table measures
linear drag (`a = −v`, in 1/s) acting for 1 s on a projectile starting at 10 m/s. Doubling the
rate halves Euler's error and divides RK4's by 16:

| Rate | `SemiImplicitEuler` position error | `RungeKutta4` position error |
|---|---|---|
| 30 Hz | 1.5e-1 m | 3.9e-8 m |
| 60 Hz | 7.5e-2 m | 2.4e-9 m |
| 120 Hz | 3.7e-2 m | 1.5e-10 m |
| 240 Hz | 1.9e-2 m | 9.3e-12 m |

The test suite checks all of this: Euler's error under gravity against its closed form, RK4's
exactness, and orders of convergence of 1 and 4 measured under linear drag.

**Choosing an integrator:**

- Games usually prefer `SemiImplicitEuler` at the simulation rate. The visible error is a few
  centimetres per second of flight, and it is identical in a trajectory preview that uses the
  same settings.
- Use `RungeKutta4` when accuracy matters, as in long-range ballistics or analysis, or when
  strong velocity-dependent forces would require tiny steps with Euler.

## Stability

- **Under gravity alone**, both integrators are stable at any time step: errors never grow
  faster than described above.
- **Forces that make the motion oscillate or decay quickly**, like a spring of angular frequency
  ω or drag with rate `k`, limit the time step of every explicit integrator:

  | Integrator | Oscillation, `ω·dt` below | Decay, `k·dt` below |
  |---|---|---|
  | `SemiImplicitEuler` | 2 | 2 (and below 1 to avoid sign flips) |
  | `RungeKutta4` | 2√2 ≈ 2.83 | ≈ 2.79 |

  Beyond these limits the numbers grow without bound. The tests check both sides of the
  oscillation limits.
- **Energy behaves differently.** `SemiImplicitEuler` is symplectic: the energy of an oscillation
  wobbles but never drifts (at most 5% at `ω·dt = 0.1`, over 160 periods). `RungeKutta4` slowly
  loses energy instead (0.014% over the same run). Over very long runs, Euler's motion stays
  bounded, while RK4 damps it.

**Supported configurations** are finite inputs of physical magnitude, and time steps well inside
the limits above. Gravity alone has no step limit; rates of 60 to 240 Hz are typical.

## Lifetime

Each step reduces `remaining_lifetime` by the time it advances, and a step never outlasts it.
When the lifetime runs out within a step, the projectile moves only for the time it had left,
then becomes inactive with a remaining lifetime of 0.

The integrators leave a state untouched when:
- the projectile is inactive;
- the time step is zero, negative, NaN or infinite.

## Trajectory prediction

`predict_trajectory(state, config, trajectory_config)` predicts where a projectile will go:

```cpp
TrajectoryConfig settings;
settings.sample_interval = 1.0 / 60.0; // one sample per frame
settings.max_duration = 5.0;
settings.time_step = 1.0 / 120.0;      // the simulation's step...
settings.integrator = IntegratorKind::semi_implicit_euler; // ...and integrator

Result<Trajectory> trajectory = predict_trajectory(*state, config, settings);
for (const TrajectorySample& sample : trajectory->samples) {
    draw_point(sample.position); // sample.time, sample.velocity are there too
}
```

- **It repeats a simulation's steps.** With the `time_step` and `integrator` of the
  `Simulation` (see [simulation.md](simulation.md)), a preview shows exactly where the projectile
  will be, error included, and with the simulation's collision world it ends exactly where the
  projectile will hit. Use `ballistic_position()` and `ballistic_velocity()` for the exact
  physical path under constant acceleration.
- **Samples fall every `sample_interval`**, from time 0 to where the prediction ended, which is
  always the last sample. No two samples share a time. Samples between steps lie on the cubic
  curve through the neighbouring steps' positions and velocities, which is exact under gravity
  alone, and accurate to third order with drag.
- **The prediction ends** at `max_duration`, when the projectile expires, or, if you pass a
  collision world, at the first contact. `Trajectory::end` says which.
- **Collisions.** Like the simulation, each step sweeps a sphere of `config.radius` along the
  straight line between consecutive steps. At a contact:
  - `Trajectory::hit` holds the HitResult, whose `point` is on the surface that was hit;
  - the final sample is the projectile's center at that moment.
- **Previews that update every frame should not allocate.** The overloads taking a
  `Trajectory&` refill it and reuse its memory.
- **Invalid input is reported.** An invalid configuration or state yields the corresponding
  `Error` instead of a trajectory, and the output buffer is left untouched.

Predictions stop at the first contact: they do not apply material interactions (see
[materials.md](materials.md)).

Predicting `max_duration / time_step` steps costs as much as simulating them. Samples take
memory in proportion to `max_duration / sample_interval`.
