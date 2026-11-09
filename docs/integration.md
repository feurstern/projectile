# Engine integration

Projectile does not own your scene. It computes where projectiles go and what they hit, and
leaves drawing, effects and game rules to your engine. This guide covers the places where the
two meet: units and axes, the frame loop, collision geometry, materials and events. It ends with
the raylib adapter, a complete integration that the demos in
[adapters/raylib](../adapters/raylib/README.md) use.

## Units and axes

Projectile works in SI units: metres, seconds, kilograms, joules. It uses right-handed
coordinates but fixes no up axis: gravity is a vector in each `ProjectileConfig`. See
[mathematics.md](mathematics.md#coordinates-and-units).

Convert at the boundary, where positions and directions pass between your engine and Projectile:

| Engine | Its conventions | To Projectile | Gravity |
|---|---|---|---|
| raylib, Godot | right-handed, y up, metres | unchanged | `{0, -9.81, 0}` |
| Unity | left-handed, y up, metres | negate z: `(x, y, -z)` | `{0, -9.81, 0}` |
| Unreal Engine | left-handed, z up, centimetres | negate y and scale: `(x, -y, z) / 100` | `{0, 0, -9.81}` |

Apply the same conversion to directions and normals, and its inverse to results. Negating an
axis mirrors the scene, which also reverses the winding of every triangle. Projectile treats as
the front of a triangle the side from which `a → b → c` appears counter-clockwise (see
[collision.md](collision.md#conventions)). Whether you must swap two vertices of each triangle
depends on the winding your engine uses for front faces. To check, convert one triangle: its
`face_normal()` should point the way the engine's surface normal points after conversion.

Projectile computes in `double`. Engines usually store `float`, which is precise to about
0.1 mm at 1 km from the origin. Keep simulation state in Projectile, and convert to `float`
only to draw.

## The frame loop

A `Simulation` steps at a fixed rate, 120 Hz by default, however irregular the frames are. Call
`update()` once per frame with the frame's duration, and draw each projectile between its last
two steps:

```cpp
simulation.update(frame_delta); // runs the steps now due
const double alpha = simulation.interpolation_alpha();
for (const SimulatedProjectile& projectile : simulation.projectiles()) {
    draw_bullet(interpolated_position(projectile, alpha));
}
```

If your engine already ticks at a fixed rate, call `step()` from its tick instead, with a
`SimulationConfig::time_step` equal to the tick. [simulation.md](simulation.md) covers the
timestep, its limits and what interpolation costs.

## Collision geometry

The simulation sweeps projectiles through an `ICollisionWorld`: anything that answers ray and
swept-sphere queries.

- **Small or static scenes** can use `PrimitiveWorld`, filled with planes, spheres, boxes and
  triangles. It tests every shape for every query, so its cost grows with the number of shapes;
  [benchmarks.md](benchmarks.md) measures it.
- **Engine scenes** are better served by implementing `ICollisionWorld` over the engine's own
  physics queries or spatial index, so that Projectile sees the same geometry as everything
  else. [collision.md](collision.md#your-own-world) shows a complete implementation and the
  contract it must keep: the nearest hit, a normal that faces the query, and a material id.

## Materials and events

Collision worlds report a material id with each hit. Map your engine's physical materials (or
surface tags) to ids, and describe each id with a `Material` in a `MaterialTable`. The
simulation's interaction model then decides whether a projectile ricochets, penetrates or stops
([materials.md](materials.md)).

Events tell your game what happened. Install a callback with `set_event_callback()`; the
simulation calls it after each step, outside the step's work:

```cpp
simulation.set_event_callback([&](const ProjectileEvent& event) {
    if (!event.hit) {
        return; // the projectile expired
    }
    const double absorbed = kinetic_energy(event.state, event.config) -
                            (event.interaction ? event.interaction->energy_remaining : 0.0);
    effects.spawn_impact(event.hit->point, event.hit->normal, event.hit->material_id);
    damage.apply(event.hit->point, absorbed);
});
```

The energy an impact absorbed is a natural basis for damage. The callback may add and remove
projectiles; [simulation.md](simulation.md#events) lists what else it may do.

## Threads

A simulation belongs to one thread at a time, and queries its collision world on that thread.
Several simulations may share a world that answers queries concurrently. See
[simulation.md](simulation.md#threads).

## The raylib adapter

The raylib adapter is Projectile's reference integration: a few header-only functions that
connect Projectile to [raylib](https://www.raylib.com/), version 5.5 or newer. It lives in
[adapters/raylib](../adapters/raylib/README.md), apart from the core library, which never
depends on it. Link the CMake target `Projectile::Raylib` and include
`<projectile/raylib.hpp>`; the adapter's README explains how to build it.

raylib uses right-handed coordinates with y up by convention, so positions convert unchanged.
raylib defines its own global `Ray` and `Material` types, so qualify Projectile's as
`projectile::Ray` and `projectile::Material` rather than writing `using namespace projectile;`.

### Conversions

In `namespace projectile::raylib`, from `<projectile/raylib/conversions.hpp>`:

| Function | Converts |
|---|---|
| `to_vector3(Vec3)` | to raylib's `Vector3`, rounding to `float` |
| `to_vec3(Vector3)` | to `Vec3`, exactly |
| `to_ray(::Ray)` | a raylib ray, such as the one through the mouse cursor from `GetScreenToWorldRay()`, to a `projectile::Ray` with a unit direction; `std::nullopt` if it has no direction |
| `to_raylib_ray(Ray)` | a `projectile::Ray` to raylib's `Ray` |
| `to_bounding_box(AABB)`, `to_aabb(BoundingBox)` | boxes, both ways; `to_aabb` accepts corners in either order |

### Drawing

From `<projectile/raylib/draw.hpp>`, for use between `BeginMode3D()` and `EndMode3D()`:

| Function | Draws |
|---|---|
| `draw_sphere`, `draw_box`, `draw_box_wires`, `draw_triangle` | a shape; triangles from both sides |
| `draw_plane(plane, half_size, color, around)` | a square piece of an infinite plane, centred near a point |
| `draw_ray(ray, distance, color)` | a ray, up to a distance |
| `draw_hit(hit, normal_length, point_color, normal_color)` | a contact point and its normal |
| `draw_arrow(from, to, radius, color)` | an arrow, for example a velocity |
| `draw_path(points, color, thickness)`, `draw_trajectory(samples, color, thickness)` | a path, such as a predicted `Trajectory`: lines, or tubes when `thickness` is above zero |
| `draw_projectiles(simulation, color, min_radius)` | every simulated projectile, at its interpolated position |

### A complete program

Left-click to throw balls at a crate:

```cpp
#include <projectile/projectile.hpp>
#include <projectile/raylib.hpp>

#include <raylib.h>

#include <optional>

int main() {
    namespace adapter = projectile::raylib;

    projectile::PrimitiveWorld world;
    const projectile::Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    const projectile::AABB crate{.min = {8.0, 0.0, -1.0}, .max = {10.0, 2.0, 1.0}};
    if (world.add(ground) || world.add(crate)) {
        return 1;
    }

    projectile::Simulation simulation;
    simulation.set_collision_world(&world);

    projectile::ProjectileConfig ball;
    ball.radius = 0.1;
    ball.gravity = {0.0, -projectile::standard_gravity, 0.0};
    ball.lifetime = 5.0;

    InitWindow(1280, 720, "Projectile and raylib");
    SetTargetFPS(60);
    const Camera3D camera{
        .position = {-6.0F, 6.0F, 14.0F},
        .target = {6.0F, 1.0F, 0.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = 45.0F,
        .projection = CAMERA_PERSPECTIVE,
    };

    while (!WindowShouldClose()) {
        // Throw a ball along the ray through the mouse cursor.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const ::Ray mouse = GetScreenToWorldRay(GetMousePosition(), camera);
            if (const std::optional<projectile::Ray> aim = adapter::to_ray(mouse)) {
                const projectile::Result<projectile::ProjectileState> thrown =
                    projectile::create_projectile(ball, aim->origin, 25.0 * aim->direction);
                if (thrown) {
                    static_cast<void>(simulation.add_projectile(*thrown, ball));
                }
            }
        }
        simulation.update(static_cast<double>(GetFrameTime()));

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        adapter::draw_plane(ground, 20.0, LIGHTGRAY);
        adapter::draw_box(crate, BROWN);
        adapter::draw_projectiles(simulation, RED);
        EndMode3D();
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
```

The four demos in [adapters/raylib/examples](../adapters/raylib/examples/) build on the same
pattern: trajectory prediction, hitscan queries, material interactions, and thousands of
projectiles at once.
