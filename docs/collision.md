# Collision queries

The collision module (`include/projectile/collision/`) answers one question: where does a ray, a
segment or a moving sphere first touch a surface? It has three layers:

- `intersect(query, shape)` tests a ray or a segment against a single plane, sphere,
  axis-aligned box or triangle.
- `sweep_sphere(path, radius, shape)` tests a moving sphere against a single shape.
- `ICollisionWorld` is the interface between Projectile and your game's geometry. You query it
  through `raycast()` and `sweep_sphere()`. `PrimitiveWorld` is a ready-made world built from the
  shapes above.

All queries share the conventions below, so a hit means the same thing whichever shape or query
produced it.

## The result

Queries return `std::optional<HitResult>`. An empty optional means nothing was hit; a miss is a
normal outcome, not an error.

| Field | Meaning |
|---|---|
| `distance` | How far the query travelled before the contact, in metres: from the ray's origin or the segment's start. For a swept sphere, how far its center travelled. |
| `point` | The contact point on the surface. |
| `normal` | The unit surface normal at the contact, **facing the side the query came from**: `dot(normal, direction) ≤ 0`. |
| `material_id` | The material of the surface. Worlds set it; `intersect()` leaves it 0. |
| `front_face` | Whether the front side of the surface was hit (see below). |

## Conventions

**First contact.** A query reports the smallest distance at which it touches a surface within its
range. For a ray that range is `[0, max_distance]`, and `max_distance` may be infinite; for a
segment it is the whole segment. Both ends of the range are inclusive.

**Normals face the query.** Because the normal always faces the side the query came from, the
same code works whichever side is hit:

- `reflect(velocity, hit.normal)` gives a ricochet;
- `hit.point + hit.normal * skin` steps back off the surface;
- `hit.point - hit.normal * skin` steps through it.

**Front and back faces.** Each shape has a front side:

| Shape | Front side |
|---|---|
| Plane | The side its `normal` points to |
| Triangle | The side from which `a → b → c` appears counter-clockwise (the side `face_normal()` points to) |
| Sphere, box | The outside |

`front_face` is false when a plane or triangle is hit from behind, and when a query that started
inside a sphere or box reaches its surface from within, which is where it leaves.

**Culling.** Passing `Culling::back_faces` ignores back faces. Planes and triangles become
one-sided, and a query that starts inside a sphere or box reports nothing for it.

## Surfaces, solids and boundary contact

Planes and triangles are **surfaces**: they have no inside. Spheres and boxes are **solids**.
This difference decides what happens when a query starts on or inside a shape:

| Situation | Plane or triangle | Sphere or box |
|---|---|---|
| Reaches the shape from outside | Hit (front or back face, by direction) | Hit where it enters (front face) |
| Grazes the shape, touching it at a single point or along an edge | Not possible; see parallel queries | Hit where it touches (front face) |
| Starts strictly inside | Not applicable | Hit where it leaves (back face) |
| Starts on the surface, moving into the shape | Hit at distance 0 | Hit at distance 0 (front face) |
| Starts on the surface, moving away or sliding along it | Hit at distance 0 when moving away; no hit when sliding along it | No hit |
| Runs parallel to the surface | No hit, even when lying in it | Not applicable |

A query that starts on a solid's surface and moves away is leaving the solid, so it touches
nothing. A surface has no inside, so a query that starts on it cannot tell which side it came
from. Reporting a contact at distance 0 is the safe choice, because the alternative would let a
projectile pass through a wall unnoticed. To continue past a contact, step back from the surface
by a small skin first (`hit.point + hit.normal * skin`), or use `Culling::back_faces` for
one-sided surfaces.

## Degenerate input

- **Zero-length segments** never hit: they do not move. Use `contains()` to test points.
- **Zero-area triangles** never hit: triangles whose vertices are collinear or coincide have no
  surface.
- **Spheres of radius 0** are points. Only a query passing exactly through the center hits them,
  and the normal then faces the query.
- **Flat boxes**, which have zero thickness on an axis, are hit where a query crosses them, like a
  rectangle.
- **Edges and corners.** Triangle edges and vertices belong to the triangle. At a box's edge or
  corner, the normal is that of one of the faces meeting there, preferring x over y over z.

## Swept spheres

A projectile with a radius is a sphere, and between two simulation steps its center moves along a
segment. `sweep_sphere(path, radius, shape)` finds where such a sphere first touches a shape:

```cpp
const Segment path{.start = {-5.0, 0.0, 0.0}, .end = {5.0, 0.0, 0.0}};
const AABB crate{.min = {-1.0, -1.0, -1.0}, .max = {1.0, 1.0, 1.0}};
if (const std::optional<HitResult> hit = sweep_sphere(path, 0.5, crate)) {
    // hit->distance == 3.5: the center stops half a metre before the face at x = -1.
    // hit->point == {-1, 0, 0}, on the crate; hit->normal == {-1, 0, 0}.
}
```

The sphere touches the shape exactly when its center comes within `radius` of it, so a sweep is
a segment query of the center against the shape enlarged by the radius. For a sphere, that is a
larger sphere. For a plane, it is a slab. For a box or a triangle it has rounded edges and
corners, so a sphere sweeping past a corner touches it later than it would touch a box simply
grown by the radius. The result describes the original shape:

| Field | Meaning |
|---|---|
| `distance` | How far the center travels before the contact. |
| `point` | The contact point on the shape's surface. |
| `normal` | The unit normal at the contact, facing the side the sphere came from. At the edges and corners of boxes and triangles, it points from the contact to the sphere's center. |
| `front_face` | As for segment queries: false for planes and triangles reached from behind, and for a shape the sphere already overlaps at the start. A contact on a triangle's rim counts as a front face unless the sphere's center is behind the triangle's plane. |

How a sweep starts decides its result, for every shape:

| At the start, the sphere | Moving into the shape | Moving away, or sliding along its surface |
|---|---|---|
| Is clear of the shape | Hit where it first touches; grazing the shape counts | No hit |
| Touches the shape | Hit at distance 0 | No hit |
| Overlaps the shape | Hit where it stops overlapping, a back face | Same |

Planes and triangles, which are surfaces, behave like thin solids here: a sphere overlapping them
is reported where it clears them on the far side. `Culling::back_faces` ignores those contacts,
and contacts from behind.

A radius of 0 gives exactly the segment query `intersect(path, shape)`, which keeps the
segment's own rule for starting on a surface (a hit at distance 0). Paths of zero length and
triangles of zero area never hit.

## Validation

`intersect()` and `sweep_sphere(path, radius, shape)` do not validate their arguments, so that
they stay cheap in inner loops. Their results are meaningful only for valid input:

- a valid ray (`is_valid(ray)`), or a segment with finite endpoints;
- valid shapes (`is_valid(shape)`);
- `max_distance ≥ 0`, and a finite `radius ≥ 0`.

Validate data from outside your program once, with the `is_valid()` functions.

The world entry points `raycast(world, ray, max_distance)` and
`sweep_sphere(world, start, end, radius)` do validate. They return `std::nullopt` without
querying the world when the ray is invalid, `max_distance` is negative or NaN, an endpoint is not
finite, or the radius is negative, NaN or infinite. World implementations can therefore assume
valid input.

## Collision worlds

A collision world holds the geometry that rays and projectiles can hit: the ground, walls,
targets. Projectile includes one world, `PrimitiveWorld`, and works with any world you implement.

### PrimitiveWorld

`PrimitiveWorld` holds planes, spheres, boxes and triangles, each made of a material that hits on
it report:

```cpp
PrimitiveWorld world;
if (world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, ground) ||
    world.add(AABB{.min = {10.0, 0.0, -1.0}, .max = {10.01, 2.0, 1.0}}, steel)) {
    // a shape was invalid: add() returned Error::invalid_shape and added nothing
}

const std::optional<HitResult> hit = raycast(world, ray, 1000.0); // hit->material_id
```

- Planes and triangles are two-sided.
- When two shapes are hit at exactly the same distance, the one that comes first wins: planes,
  then spheres, boxes and triangles, and within each kind the one added first.
- Every query tests every shape, so its cost grows with the number of shapes. `PrimitiveWorld`
  suits small scenes, tests and examples. For large scenes, implement `ICollisionWorld` on top
  of a spatial index or your engine's physics, as described next.
- Several threads may query a `PrimitiveWorld` at once, as long as no shapes are added
  meanwhile.

[examples/03_hitscan](../examples/03_hitscan/main.cpp) fires rays into one.

### Your own world

Projectile does not own your game's geometry. To run queries against it, implement
`ICollisionWorld`:

```cpp
class ICollisionWorld {
public:
    virtual std::optional<HitResult> raycast(const Ray& ray, double max_distance) const = 0;
    virtual std::optional<HitResult> sweep_sphere(const Vec3& start, const Vec3& end,
                                                  double radius) const = 0;
};
```

- **`raycast`** returns the first contact along the ray within `max_distance`, following the
  conventions above, with `material_id` set.
- **`sweep_sphere`** returns the first contact of a sphere of `radius` whose center moves from
  `start` to `end`. It is equivalent to a segment query of the center against every shape
  enlarged by `radius`, expressed for the original shape:
  - `distance` is how far the center travels;
  - `point` lies on the shape's surface;
  - `normal` is the shape's normal there, facing against the motion;
  - `front_face` is false when the sphere overlaps the shape at `start`; the contact is then
    where it stops overlapping.

  A radius of 0 makes it a segment query.

Queries are `const` and must not throw. A world that is not modified during queries can be
queried from several threads at once.

You can map these two functions onto an engine's ray and sphere casts, or build them on the
`intersect()` functions. Here is a complete world made of spheres, the one used in
`tests/unit/collision/test_collision_world.cpp`:

```cpp
#include <projectile/projectile.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class SphereWorld final : public projectile::ICollisionWorld {
public:
    struct Body {
        projectile::Sphere sphere;
        std::uint32_t material_id = 0;
    };

    explicit SphereWorld(std::vector<Body> bodies) : bodies_(std::move(bodies)) {}

    std::optional<projectile::HitResult>
    raycast(const projectile::Ray& ray, double max_distance) const override {
        std::optional<projectile::HitResult> nearest;
        for (const Body& body : bodies_) {
            auto hit = projectile::intersect(ray, body.sphere, max_distance);
            if (hit && (!nearest || hit->distance < nearest->distance)) {
                hit->material_id = body.material_id;
                nearest = hit;
            }
        }
        return nearest;
    }

    std::optional<projectile::HitResult> sweep_sphere(
        const projectile::Vec3& start, const projectile::Vec3& end, double radius
    ) const override {
        // A sphere of radius r touches a sphere of radius R when their centers are R + r apart,
        // so sweeping it is a segment query of its center against the enlarged sphere.
        std::optional<projectile::HitResult> nearest;
        for (const Body& body : bodies_) {
            const projectile::Sphere enlarged{body.sphere.center, body.sphere.radius + radius};
            auto hit = projectile::intersect(projectile::Segment{start, end}, enlarged);
            if (hit && (!nearest || hit->distance < nearest->distance)) {
                // The query found the center at the contact; move to the body's surface.
                const projectile::Vec3 outward = hit->front_face ? hit->normal : -hit->normal;
                hit->point -= outward * radius;
                hit->material_id = body.material_id;
                nearest = hit;
            }
        }
        return nearest;
    }

private:
    std::vector<Body> bodies_;
};

// Hitscan through the validated entry point:
//   const SphereWorld world({{projectile::Sphere{{5.0, 0.0, 0.0}, 1.0}, 2}});
//   auto hit = projectile::raycast(world, ray, 1000.0);
```

A world of other shapes can answer `sweep_sphere` with the library's sweeps:
`projectile::sweep_sphere(Segment{.start = start, .end = end}, radius, shape)`. (Inside a class
derived from `ICollisionWorld`, qualify the name: unqualified, it finds the member function.)

## Algorithms

Rays and segments share one implementation per shape. Each one works on the line
`origin + s · direction`:

- **Rays** use their unit direction, so `s` is the distance.
- **Segments** use `end − start` with `s ∈ [0, 1]`. The distance is `s × length`, and the contact
  point comes from `lerp(start, end, s)`, which is exact at both endpoints.

| Shape | Method | Numerical notes |
|---|---|---|
| Plane | `s = −signed_distance / dot(normal, direction)` | Segments interpolate the signed distances of their endpoints instead, so an endpoint lying on the plane is hit exactly there. |
| Sphere | Roots of `a s² + 2 b s + c = 0` | The discriminant comes from the distance between the center and the line, which stays accurate far from the origin; `b² − a c` cancels catastrophically there. The roots are rearranged so that nearly equal values are never subtracted. |
| Box | Slab method: clip the line against the three pairs of faces | Divides by the direction components instead of multiplying by their reciprocals, which overflow for tiny components. Slabs parallel to the line are handled exactly. |
| Triangle | Möller–Trumbore | The sign of the determinant tells the front face from the back face. Barycentric bounds are inclusive and reject NaN. |

Swept spheres find the interval of the path's line inside the enlarged shape, then apply the rules
above:

| Shape | Enlarged shape | Method |
|---|---|---|
| Plane | Slab | Interpolates the endpoints' signed distances, like the segment query. |
| Sphere | Sphere of the summed radii | The segment query, with the contact moved onto the original sphere. |
| Box | Box with rounded edges and corners | The union of three boxes each grown along one axis, twelve cylinders around the edges and eight spheres at the corners. Each piece is convex and they overlap, so the line's interval is the hull of theirs. |
| Triangle | Triangle thickened by the radius, with rounded edges and corners | The union of a prism over the triangle, three cylinders and three spheres. |

Whether a sphere starts overlapping, touching or clear of a box or triangle is decided from the
distance to its closest point (`closest_point()`), and the normal at the contact points from that
closest point to the center. The tests check every sweep against the geometry directly, and
compare box sweeps with sweeps against the twelve triangles of the box's surface: two
independent computations of the same contact.

**Known limitation.** The triangle test is not *watertight*. A ray passing exactly through an edge
shared by two triangles can, through rounding, miss both. This is rare in practice; a watertight
algorithm (Woop, Benthin and Wald, 2013) would remove it.
