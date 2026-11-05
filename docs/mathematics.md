# Mathematical conventions

This document defines the conventions behind Projectile's math module (`include/projectile/math/`):
coordinates and units, the floating-point model, how values are compared, and how degenerate input
is handled. Later modules (collision, ballistics, materials) build on these rules.

## Coordinates and units

- **Right-handed coordinates.** `cross({1, 0, 0}, {0, 1, 0}) == {0, 0, 1}`.
- **No fixed up axis.** Gravity is a vector you configure: `{0, -9.81, 0}` for a Y-up world,
  `{0, 0, -9.81}` for Z-up.
- **SI units throughout:**

  | Quantity | Unit |
  |---|---|
  | Position, distance | metre (m) |
  | Velocity | m/s |
  | Acceleration | m/s² |
  | Mass | kilogram (kg) |
  | Time | second (s) |
  | Force | newton (N) |
  | Energy | joule (J) |
  | Density | kg/m³ |

- **Angles in radians.** `degrees_to_radians()` and `radians_to_degrees()` convert.

## Floating-point model

- **All computation uses `double`** (IEEE-754 binary64, about 16 significant digits).
- **NaN and infinity have defined meanings.** Projectile relies on them to detect invalid input,
  so `-ffast-math`, `/fp:fast` and similar flags are unsupported: they let the compiler assume
  such values never occur.
- **Dividing a vector by zero is not guarded.** `v / 0.0` follows IEEE-754 and produces infinities
  or NaN instead of silently returning a made-up value. Use `try_normalize()` to obtain a direction
  safely.
- **The range of `length()` is limited.** It squares the components, so for components larger
  than about 1e154 it overflows to infinity. Physical quantities in SI units are many orders of
  magnitude smaller. `try_normalize()` has no such limit.
- **Last-bit differences are possible across platforms.** Some compilers fuse a multiplication
  and an addition into one instruction (FMA), with one rounding instead of two. Clang does this by
  default on ARM64. Results can therefore differ in the last bits between platforms. Results are
  reproducible on the same platform with the same compiler settings.

## Comparing floating-point values

`operator==` compares exactly. Computed values carry rounding errors, so compare them with
`approx_equal()`.

For scalars, `approx_equal(a, b, absolute_tolerance, relative_tolerance)` is true when

    |a − b| ≤ max(absolute_tolerance, relative_tolerance × max(|a|, |b|))

| Default | Value | Rationale |
|---|---|---|
| `default_absolute_tolerance` | 1e-12 | Handles values near zero: a picometre, or a picometre per second. |
| `default_relative_tolerance` | 1e-9 | About 4500 units in the last place: absorbs the rounding of a few dozen operations. |

For `Vec2` and `Vec3`, every component of `a − b` must be within the same bound. The relative part
is measured against the largest component of either vector, not component by component. As a
result, rounding noise in a small component of a large vector is tolerated:
`{1e6, 1e-6, 0}` is approximately equal to `{1e6, 2e-6, 0}`. Any difference that matters relative
to the vector as a whole is not.

NaN is never approximately equal to anything. Values with infinite components are approximately
equal only when they are exactly equal.

## Vector operations

| Function | Definition | Notes |
|---|---|---|
| `dot(a, b)` | `a.x·b.x + a.y·b.y + a.z·b.z` | |
| `cross(a, b)` | `(a.y·b.z − a.z·b.y, a.z·b.x − a.x·b.z, a.x·b.y − a.y·b.x)` | Right-handed; length `‖a‖‖b‖ sin θ`. For `Vec2` it returns the scalar `a.x·b.y − a.y·b.x`, which is positive when `b` is counter-clockwise from `a`. |
| `length(v)`, `length_squared(v)` | `√(v·v)`, `v·v` | Prefer the squared form for comparisons. |
| `distance(a, b)`, `distance_squared(a, b)` | `length(b − a)`, `length_squared(b − a)` | |
| `lerp(a, b, t)` | `a + t (b − a)` | Exact at `t = 0` and `t = 1`; extrapolates outside [0, 1]. |
| `reflect(v, n)` | `v − 2 (v·n) n` | `n` must be a unit vector; which way it faces does not matter. Preserves length. |
| `project(v, u)` | `(v·u / u·u) u` | `u` need not be a unit vector. Projecting onto the zero vector gives the zero vector. |

## Normalization

`try_normalize(v)` returns the unit vector `v / ‖v‖` as a `std::optional`. It is empty exactly
when `v` has no direction: when `v` is the zero vector (including negative zeros), or when a
component is NaN or infinite.

Every other vector is normalized accurately, from subnormal components up to the largest finite
double. A naive `v / length(v)` would fail at both ends:

- Squaring 1e200 overflows to infinity, so the result becomes 0.
- Squaring 1e-200 underflows to zero, so the result becomes NaN.

`try_normalize` avoids both by dividing by the largest component magnitude before squaring:

    s = v / max(|v.x|, |v.y|, |v.z|)        every component of s now lies in [-1, 1]
    v̂ = s / √(s·s)

The result has unit length to within a few units in the last place. The tests check this for
vectors scaled anywhere between 2⁻¹⁰⁰⁰ and 2¹⁰⁰⁰, as well as for subnormal input.

`is_normalized(v, tolerance)` checks `|‖v‖² − 1| ≤ tolerance`. The default tolerance,
`unit_length_tolerance = 1e-6`, accepts directions normalized in single precision, which many
engines use. Converted to `double`, those are off by about 1e-7.

## Geometric primitives

The primitives are plain aggregates, so you can initialize them directly:
`Ray{.origin = {0, 1, 0}, .direction = {0, 0, -1}}`. Each one has invariants:

| Type | Representation | Invariants |
|---|---|---|
| `Ray` | `origin`, `direction`; the points `P(t) = origin + t · direction` for `t ≥ 0` | Finite origin; unit-length direction, so `t` is a distance |
| `Plane` | `normal`, `distance`; the points `p` with `dot(normal, p) == distance` | Unit-length normal, pointing to the front side; finite distance |
| `Sphere` | `center`, `radius` | Finite center; finite, non-negative radius. Radius 0 is a point |
| `AABB` | corners `min`, `max` | Finite corners with `min ≤ max` on every axis. Flat boxes (`min == max` on an axis) are valid |
| `Segment` | `start`, `end`; the points `start + f · (end − start)` for `0 ≤ f ≤ 1` | Finite endpoints. Zero length is valid but never hits anything |
| `Triangle` | vertices `a`, `b`, `c`; counter-clockwise when seen from the front | Finite vertices. Zero area is valid but never hits anything; `face_normal()` gives the front normal |

Because they are aggregates, nothing stops a primitive from breaking its invariants. There are
two safeguards:

- **Builders validate.** `make_ray()`, `make_plane()`, `plane_through_points()` and `make_aabb()`
  normalize directions, order corners, and return `std::nullopt` for input that cannot form the
  primitive (a zero direction, collinear points, a non-finite value).
- **`is_valid()` checks the invariants.** Functions that accept primitives from outside the
  library check them with `is_valid()` at the API boundary.

Further conventions:

- **Planes:** `signed_distance(plane, p) = dot(normal, p) − distance`. It is positive in front of
  the plane (the side the normal points to), negative behind it, and zero on it.
  `plane_through_points(a, b, c)` orients the normal by the right-hand rule: it points towards the
  side from which `a → b → c` appear counter-clockwise.
- **Containment is inclusive.** `contains()` counts points on the surface of a sphere or the
  boundary of a box as inside. A point computed to lie exactly on a surface may land on either
  side by rounding.
- **No overflow in box geometry.** `center()` and `half_extents()` halve the corners before
  combining them, so they cannot overflow even for boxes spanning most of the double range.

## Error handling

- **Expected degenerate cases are not errors.** A zero vector or collinear points produce an empty
  `std::optional`, not an exception.
- **Every math function is `noexcept`.**
- **Some preconditions are documented but not checked.** For example, `reflect()` needs a unit
  normal. Checking it on every call would cost every user in hot loops; check untrusted data once
  with `is_valid()` or `is_normalized()` instead.
- **Public headers survive `<windows.h>`.** They call `(std::min)` and `(std::max)`, so that
  header's `min`/`max` macros cannot break them.

## How the math is tested

- **Exact comparisons where the arithmetic is exact.** With small integers and powers of two, the
  tests compare results exactly, many of them at compile time with `STATIC_REQUIRE`.
- **Tolerances elsewhere.** They are chosen so that the tests also pass when the compiler fuses
  multiply-adds.
- **Property tests.** Algebraic properties are checked on 10,000 random inputs each: cross products
  are perpendicular and anticommutative, reflection preserves length, projections leave a
  perpendicular remainder, and normalization is accurate at every scale.
- **Reproducible randomness.** The inputs come from a generator seeded with Catch2's
  `--rng-seed`. CTest pins the seed, so every run checks the same inputs on every platform.
