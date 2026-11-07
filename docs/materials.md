# Material interactions

The materials module (`include/projectile/materials/`) decides what happens when a projectile
strikes a surface: whether it ricochets, passes through, or stops, and how much energy that costs
it. It works with the simulation ([simulation.md](simulation.md)), which finds the contacts, and
with the collision world ([collision.md](collision.md)), which reports which material each surface
is made of.

> **These are gameplay models.** Materials hold tuning parameters chosen for how a game should
> feel. They are not physical constants, and Projectile makes no claim that its ricochets or
> penetrations match real projectiles and real materials.

```cpp
#include <projectile/projectile.hpp>

using namespace projectile;

constexpr std::uint32_t wood = 2;

// The materials: a wooden board that bullets can shoot through.
MaterialTable materials;
Material board;
board.id = wood;
board.penetration_cost = 50.0;               // J to break in
board.resistance = 2'000.0;                  // J per metre of wood
board.ricochet_angle = degrees_to_radians(10.0);
board.restitution = 0.3;
if (materials.add(board)) {
    // invalid material
}

// The world's surfaces carry material ids.
PrimitiveWorld world;
if (world.add(AABB{.min = {15.0, 0.0, -1.0}, .max = {15.02, 2.0, 1.0}}, wood)) {
    // invalid shape
}

// The standard model decides each impact.
const StandardInteraction interaction;
Simulation simulation;
simulation.set_collision_world(&world);
simulation.set_materials(&materials);
simulation.set_interaction(&interaction);
simulation.set_event_callback([](const ProjectileEvent& event) {
    if (event.kind == ProjectileEventKind::penetration) {
        const double absorbed =
            kinetic_energy(event.state, event.config) - event.interaction->energy_remaining;
        spawn_splinters(event.hit->point, absorbed);
    }
});
```

[examples/05_material_interaction](../examples/05_material_interaction/main.cpp) is a complete
program.

## Materials

A `Material` describes how a surface responds:

| Field | Unit | Range | Default | Meaning |
|---|---|---|---|---|
| `id` | | any | 0 | The id that collision worlds report as `HitResult::material_id`. |
| `resistance` | J/m | finite, ≥ 0 | 0 | Energy lost per metre travelled through the material. |
| `restitution` | | 0 to 1 | 0 | Fraction of the speed into the surface that a ricochet keeps. |
| `friction` | | 0 to 1 | 0 | Fraction of the speed along the surface that a ricochet loses. |
| `penetration_cost` | J | ≥ 0, may be infinite | infinite | Energy needed to break into the material, whatever its thickness. |
| `ricochet_angle` | rad | 0 to π/2 | 0 | Shots this close to the surface or closer ricochet; 0 never, π/2 always. |

A default-constructed `Material` stops every projectile: it is impenetrable and never makes them
ricochet. `validate(material)` reports an invalid property, such as `Error::invalid_restitution`.

**The material table.** A `MaterialTable` maps ids to materials. `add()` adds or replaces a
material, and `material_for(id)` looks one up. Ids without a material get the table's fallback
material, a default `Material` unless you change it with `set_fallback()`. Without a table, the
simulation treats every surface as a default `Material`.

## The standard model

`StandardInteraction` decides each impact in three steps. Let `v` be the projectile's velocity
when it strikes, `m` its mass, and `n` the unit surface normal, which faces the projectile.

**1. Ricochet** if the projectile strikes at a shallow enough angle. The angle between its path
and the surface is

    θ = atan2(−v·n, |v_t|),    where v_n = (v·n) n and v_t = v − v_n

0 for a grazing shot and π/2 for a head-on one (`impact_angle()`). If `θ ≤ ricochet_angle`, the
projectile leaves with

    v' = (1 − friction) · v_t − restitution · v_n

(`ricochet_velocity()`). The part of the velocity into the surface reverses, scaled by the
restitution, and the part along the surface shrinks by the friction. With
`friction = 1 − restitution`, this is the scaled reflection `V' = e R`, where
`R = v − 2 (v·n) n` and `e` is the restitution.

**2. Penetrate** otherwise, if the projectile has the energy. Its kinetic energy is
`E = ½ m |v|²`, and passing through costs

    E_cost = penetration_cost + resistance · thickness

(`penetration_energy()`), where `thickness` is how far the projectile's path runs through the
object (see below). If `E > E_cost`, the projectile flies on in the same direction with the energy
left over, `E' = E − E_cost`, which is a speed of `√(2 E' / m)`.

**3. Stop** otherwise.

A projectile that would ricochet or penetrate more slowly than the model's *stopping speed*
(0.1 m/s by default; `StandardInteraction::create(speed)` sets another) stops instead. That ends
bouncing that would otherwise go on with ever smaller bounces.

`resolve_standard(impact, stopping_speed)` makes the same decision as a free function, so a model
of your own can use it for some impacts.

### Guarantees

For valid materials, the standard model keeps every impact within these bounds, which the tests
check on random impacts:

- The outgoing speed never exceeds the impact speed, and the remaining energy never exceeds the
  impact energy.
- `energy_remaining` is the kinetic energy of `outgoing_velocity`.
- A projectile that flies on is at least as fast as the stopping speed; a stopped one has zero
  velocity and energy.
- A ricochet never sends the projectile into the surface; a penetration keeps its direction.
- A material that costs nothing to penetrate leaves the velocity exactly as it was.

## Energy

`kinetic_energy(mass, velocity)` and `kinetic_energy(state, config)` give `½ m |v|²` in joules,
and `speed_for_energy(mass, energy)` inverts it. Every impact event carries the energy before (from
`event.state` and `event.config`) and after (`event.interaction->energy_remaining`). Their
difference is the energy the impact absorbed, a natural basis for damage.

## Impacts in the simulation

At each contact, the simulation:

1. **Measures the thickness** of the object struck, along the path. It casts a ray from the
   contact point in the direction of travel. If the first surface that ray meets is a back face
   of the same material, the ray has left the object there, and the distance is the thickness.
   Otherwise the object has no thickness along the path, as for a plane or a single triangle,
   and the thickness is 0.
2. **Asks the interaction model** with an `Impact`: the projectile's state at the contact, its
   configuration, the `HitResult`, the `Material` and the thickness.
3. **Moves the projectile on.** After a ricochet, it continues from 1 µm off the surface, along the
   normal. After a penetration, it moves through the object along its path until it no longer
   overlaps it, then continues from 1 µm beyond. The passage takes no time. If the projectile
   reaches another object before it is through, it stops 1 µm short of that object and strikes it
   next.
4. **Flies on for the rest of the step**, and may strike again. After
   `SimulationConfig::max_impacts_per_step` impacts (4 by default) it waits where it is until the
   next step, so that a projectile trapped between surfaces cannot stall the simulation.

A model that returns a velocity of zero, or one that is not finite, stops the projectile. Events
report each impact: `ricochet` and `penetration` while the projectile flies on, and `hit` when it
stops. `event.interaction` holds the model's decision.

Closed solids have thickness: boxes, spheres, and closed triangle meshes whose normals point
outward. Two touching objects of the same material are measured as one.

## Writing your own model

Implement `IMaterialInteraction` to replace the standard model. Its one function receives the
`Impact` and returns an `InteractionResult`. This model follows the standard rules, except that
surfaces of material 9 bounce every projectile back at full speed:

```cpp
class Trampolines final : public projectile::IMaterialInteraction {
public:
    projectile::InteractionResult resolve(const projectile::Impact& impact) const override {
        using namespace projectile;
        if (impact.hit.material_id != 9) {
            return resolve_standard(impact, StandardInteraction::default_stopping_speed);
        }
        const Vec3 bounced = reflect(impact.state.velocity, impact.hit.normal);
        return InteractionResult{
            .outcome = InteractionOutcome::ricocheted,
            .outgoing_velocity = bounced,
            .energy_remaining = kinetic_energy(impact.config.mass, bounced),
        };
    }
};
```

Implementations must not throw. Nothing stops a model from adding energy, by returning a velocity
faster than the incoming one, so do that only when the game wants it.

## Mathematical assumptions and limitations

- **Projectiles are spheres without rotation.** Spin, tumbling, deformation and fragmentation are
  not modelled, and neither are projectile shapes.
- **Impacts are instantaneous.** A ricochet changes the velocity at once, and a penetration
  passes through the object in no time, so a thick object does not delay the projectile.
- **Penetration goes straight through.** The direction does not change, and the cost grows
  linearly with thickness, for a material that is the same throughout. The thickness is measured
  along a ray from the contact point, not across the projectile's whole cross-section.
- **The ricochet threshold depends on the angle only**, not on speed or energy.
- **Surfaces do not move and do not react.** Objects receive no momentum, and impacts do not
  change the world.
- **Thickness relies on back faces.** An object that is not closed, or whose triangles face
  inward, has no thickness along the path, and its penetration costs only `penetration_cost`.
- **Previews stop at the first contact.** `predict_trajectory()` does not apply interactions.
