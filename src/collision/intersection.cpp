#include "projectile/collision/intersection.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace projectile {

namespace {

// Every test works on the parametric line `origin + s · direction` for s in [0, max_parameter].
// Rays use their unit direction, so s is the distance; segments use `end - start` and s in [0, 1].

/// A contact along the line, before it becomes a HitResult.
struct LineHit {
    double parameter = 0.0; ///< s at the contact.
    Vec3 normal;            ///< Unit normal facing against the direction of travel.
    bool front_face = true; ///< Whether the front side was hit.
};

/// Whether @p parameter lies in [0, max_parameter]; false for NaN.
[[nodiscard]] bool in_range(double parameter, double max_parameter) noexcept {
    return parameter >= 0.0 && parameter <= max_parameter;
}

// --- Plane ----------------------------------------------------------------------------------

std::optional<LineHit> line_plane(
    const Vec3& origin,
    const Vec3& direction,
    double max_parameter,
    const Plane& plane,
    Culling culling
) noexcept {
    const double approach = dot(plane.normal, direction);
    if (approach == 0.0) {
        return std::nullopt; // parallel, including lines that lie in the plane
    }
    const bool front_face = approach < 0.0; // travelling against the normal: coming from the front
    if (!front_face && culling == Culling::back_faces) {
        return std::nullopt;
    }
    const double parameter = -signed_distance(plane, origin) / approach;
    if (!in_range(parameter, max_parameter)) {
        return std::nullopt;
    }
    return LineHit{
        .parameter = parameter,
        .normal = front_face ? plane.normal : -plane.normal,
        .front_face = front_face,
    };
}

// --- Sphere ---------------------------------------------------------------------------------

std::optional<LineHit> line_sphere(
    const Vec3& origin,
    const Vec3& direction,
    double max_parameter,
    const Sphere& sphere,
    Culling culling
) noexcept {
    // Solve |offset + s · direction|² = r², i.e. a s² + 2 b s + c = 0.
    const double a = length_squared(direction);
    if (a == 0.0) {
        return std::nullopt; // a zero-length segment does not move
    }
    const Vec3 offset = origin - sphere.center;
    const double b = dot(offset, direction);
    const double radius_squared = sphere.radius * sphere.radius;
    const double c = length_squared(offset) - radius_squared;

    const bool inside = c < 0.0; // strictly inside: a start on the surface counts as outside
    if (!inside && b >= 0.0) {
        return std::nullopt; // outside, and not moving towards the center
    }
    if (inside && culling == Culling::back_faces) {
        return std::nullopt; // the only contact from inside is a back face
    }

    // The discriminant b² − a c, computed from the distance between the center and the line.
    // Evaluating b² − a c directly cancels catastrophically when the sphere is small compared
    // with its distance from the origin.
    const Vec3 perpendicular = offset - direction * (b / a);
    double discriminant = a * (radius_squared - length_squared(perpendicular));
    if (discriminant < 0.0) {
        if (!inside) {
            return std::nullopt; // the line passes the sphere
        }
        discriminant = 0.0; // rounding, for a start just inside the surface
    }
    const double root = std::sqrt(discriminant);

    // Roots (−b ∓ root) / a, rearranged so that nearly equal values are never subtracted.
    double parameter = 0.0;
    if (!inside) {
        parameter = c / (root - b); // entry; b < 0
    } else if (b <= 0.0) {
        parameter = (root - b) / a; // exit
    } else {
        parameter = c / (-b - root); // exit
    }
    if (!in_range(parameter, max_parameter)) {
        return std::nullopt;
    }

    // Outward normal at the contact. A sphere of radius zero has no surface direction (nor can a
    // query start inside it), so its normal faces the query.
    std::optional<Vec3> outward;
    if (sphere.radius > 0.0) {
        outward = try_normalize(offset + direction * parameter);
    }
    if (!outward) {
        return LineHit{
            .parameter = parameter, .normal = -direction / std::sqrt(a), .front_face = !inside
        };
    }
    return LineHit{
        .parameter = parameter,
        .normal = inside ? -*outward : *outward,
        .front_face = !inside,
    };
}

// --- Axis-aligned box -----------------------------------------------------------------------

/// Running state of the slab test: the part of the line inside every slab clipped so far.
struct SlabClip {
    double enter = -std::numeric_limits<double>::infinity();
    double exit = std::numeric_limits<double>::infinity();
    Vec3 enter_normal;             ///< Outward normal of the face where the line enters.
    Vec3 exit_normal;              ///< Outward normal of the face where the line leaves.
    bool on_parallel_face = false; ///< The line lies in the plane of a face it is parallel to.

    /// Clips against the slab `low ≤ x ≤ high` of one axis, where @p axis is its unit vector.
    /// Returns false if the line misses the slab entirely.
    bool clip(double origin, double direction, double low, double high, const Vec3& axis) noexcept {
        if (direction == 0.0) {
            if (origin < low || origin > high) {
                return false; // parallel to the slab and outside it
            }
            on_parallel_face = on_parallel_face || origin == low || origin == high;
            return true;
        }
        // Divide rather than multiply by a reciprocal, which overflows for tiny components.
        double near = (low - origin) / direction;
        double far = (high - origin) / direction;
        Vec3 near_normal = -axis;
        Vec3 far_normal = axis;
        if (direction < 0.0) {
            std::swap(near, far);
            std::swap(near_normal, far_normal);
        }
        if (near > enter) {
            enter = near;
            enter_normal = near_normal;
        }
        if (far < exit) {
            exit = far;
            exit_normal = far_normal;
        }
        return true;
    }
};

std::optional<LineHit> line_aabb(
    const Vec3& origin,
    const Vec3& direction,
    double max_parameter,
    const AABB& box,
    Culling culling
) noexcept {
    SlabClip slabs;
    if (!slabs.clip(origin.x, direction.x, box.min.x, box.max.x, Vec3{1.0, 0.0, 0.0}) ||
        !slabs.clip(origin.y, direction.y, box.min.y, box.max.y, Vec3{0.0, 1.0, 0.0}) ||
        !slabs.clip(origin.z, direction.z, box.min.z, box.max.z, Vec3{0.0, 0.0, 1.0}) ||
        slabs.enter > slabs.exit) {
        return std::nullopt; // the line misses the box
    }

    if (slabs.enter > 0.0) {
        // Reaches the box from outside; grazing an edge or face counts.
        if (!in_range(slabs.enter, max_parameter)) {
            return std::nullopt;
        }
        return LineHit{.parameter = slabs.enter, .normal = slabs.enter_normal, .front_face = true};
    }
    if (!(slabs.exit > 0.0) || slabs.on_parallel_face) {
        // The box lies behind the start, or the start is on the surface and the line moves
        // outwards or slides along a face: no contact.
        return std::nullopt;
    }
    if (slabs.enter == 0.0) {
        // Starts on the surface and moves into the box.
        if (!in_range(0.0, max_parameter)) {
            return std::nullopt;
        }
        return LineHit{.parameter = 0.0, .normal = slabs.enter_normal, .front_face = true};
    }
    // Starts strictly inside: the first contact is where the line leaves the box.
    if (culling == Culling::back_faces || !in_range(slabs.exit, max_parameter)) {
        return std::nullopt;
    }
    return LineHit{.parameter = slabs.exit, .normal = -slabs.exit_normal, .front_face = false};
}

// --- Triangle -------------------------------------------------------------------------------

std::optional<LineHit> line_triangle(
    const Vec3& origin,
    const Vec3& direction,
    double max_parameter,
    const Triangle& triangle,
    Culling culling
) noexcept {
    // Möller–Trumbore: solve origin + s · direction = a + u · edge1 + v · edge2.
    const Vec3 edge1 = triangle.b - triangle.a;
    const Vec3 edge2 = triangle.c - triangle.a;
    const Vec3 p = cross(direction, edge2);
    // determinant = −dot(direction, cross(edge1, edge2)): positive when travelling against the
    // front normal, zero when parallel to the plane or for a zero-area triangle.
    const double determinant = dot(edge1, p);
    if (determinant == 0.0) {
        return std::nullopt;
    }
    const bool front_face = determinant > 0.0;
    if (!front_face && culling == Culling::back_faces) {
        return std::nullopt;
    }

    // Barycentric coordinates of the crossing point. Edges and vertices are inclusive:
    // u, v ≥ 0 and u + v ≤ 1. The comparisons are written to reject NaN.
    const Vec3 to_origin = origin - triangle.a;
    const double u = dot(to_origin, p) / determinant;
    if (!in_range(u, 1.0)) {
        return std::nullopt;
    }
    const Vec3 q = cross(to_origin, edge1);
    const double v = dot(direction, q) / determinant;
    const bool within_triangle = v >= 0.0 && u + v <= 1.0;
    if (!within_triangle) {
        return std::nullopt;
    }
    const double parameter = dot(edge2, q) / determinant;
    if (!in_range(parameter, max_parameter)) {
        return std::nullopt;
    }

    const std::optional<Vec3> normal = face_normal(triangle);
    if (!normal) {
        return std::nullopt; // zero area; the determinant was nonzero only through rounding
    }
    return LineHit{
        .parameter = parameter,
        .normal = front_face ? *normal : -*normal,
        .front_face = front_face,
    };
}

// --- Conversion to HitResult ----------------------------------------------------------------

std::optional<HitResult> ray_hit(const Ray& ray, const std::optional<LineHit>& hit) noexcept {
    if (!hit) {
        return std::nullopt;
    }
    return HitResult{
        .distance = hit->parameter + 0.0, // turns a distance of -0 into +0
        .point = point_at(ray, hit->parameter),
        .normal = hit->normal + Vec3{}, // turns components of -0, from negated normals, into +0
        .material_id = 0,
        .front_face = hit->front_face,
    };
}

std::optional<HitResult> segment_hit(
    const Segment& segment, const std::optional<LineHit>& hit
) noexcept {
    if (!hit) {
        return std::nullopt;
    }
    return HitResult{
        .distance = hit->parameter * length(segment) + 0.0,        // turns a distance of -0 into +0
        .point = lerp(segment.start, segment.end, hit->parameter), // exact at both endpoints
        .normal = hit->normal + Vec3{},                            // turns components of -0 into +0
        .material_id = 0,
        .front_face = hit->front_face,
    };
}

} // namespace

// --- Ray queries ----------------------------------------------------------------------------

std::optional<HitResult> intersect(
    const Ray& ray, const Plane& plane, double max_distance, Culling culling
) noexcept {
    return ray_hit(ray, line_plane(ray.origin, ray.direction, max_distance, plane, culling));
}

std::optional<HitResult> intersect(
    const Ray& ray, const Sphere& sphere, double max_distance, Culling culling
) noexcept {
    return ray_hit(ray, line_sphere(ray.origin, ray.direction, max_distance, sphere, culling));
}

std::optional<HitResult> intersect(
    const Ray& ray, const AABB& box, double max_distance, Culling culling
) noexcept {
    return ray_hit(ray, line_aabb(ray.origin, ray.direction, max_distance, box, culling));
}

std::optional<HitResult> intersect(
    const Ray& ray, const Triangle& triangle, double max_distance, Culling culling
) noexcept {
    return ray_hit(ray, line_triangle(ray.origin, ray.direction, max_distance, triangle, culling));
}

// --- Segment queries ------------------------------------------------------------------------

std::optional<HitResult> intersect(
    const Segment& segment, const Plane& plane, Culling culling
) noexcept {
    // Interpolate the endpoints' signed distances rather than intersecting a line, so that an
    // endpoint on the plane yields a fraction of exactly 0 or 1.
    const double start_distance = signed_distance(plane, segment.start);
    const double end_distance = signed_distance(plane, segment.end);
    if (start_distance == end_distance) {
        return std::nullopt; // parallel to the plane, or zero length
    }
    const bool front_face = end_distance < start_distance; // moving against the normal
    if (!front_face && culling == Culling::back_faces) {
        return std::nullopt;
    }
    const double fraction = start_distance / (start_distance - end_distance);
    if (!in_range(fraction, 1.0)) {
        return std::nullopt;
    }
    return segment_hit(
        segment,
        LineHit{
            .parameter = fraction,
            .normal = front_face ? plane.normal : -plane.normal,
            .front_face = front_face,
        }
    );
}

std::optional<HitResult> intersect(
    const Segment& segment, const Sphere& sphere, Culling culling
) noexcept {
    return segment_hit(
        segment, line_sphere(segment.start, segment.end - segment.start, 1.0, sphere, culling)
    );
}

std::optional<HitResult> intersect(
    const Segment& segment, const AABB& box, Culling culling
) noexcept {
    return segment_hit(
        segment, line_aabb(segment.start, segment.end - segment.start, 1.0, box, culling)
    );
}

std::optional<HitResult> intersect(
    const Segment& segment, const Triangle& triangle, Culling culling
) noexcept {
    return segment_hit(
        segment, line_triangle(segment.start, segment.end - segment.start, 1.0, triangle, culling)
    );
}

} // namespace projectile
