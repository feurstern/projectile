#include "projectile/collision/sweep.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace projectile {

namespace {

// A sphere of radius r touches a shape when its center comes within r of the shape: when the
// center enters the shape enlarged by r. The sweeps below find where the line
// `start + t · direction`, with direction = end − start, crosses the enlarged shape; the path
// itself is t in [0, 1].
//
// Enlarged boxes and triangles are convex, so the line crosses each one in a single interval of
// t. Each enlarged shape is the union of a few simple convex pieces (a box grown along one axis,
// a cylinder around an edge, a sphere around a corner), and because those pieces overlap and
// their union is convex, the interval of the whole shape is the hull of the pieces' intervals.

constexpr double infinity = std::numeric_limits<double>::infinity();

/// The values of t for which the line lies inside a convex region: `[enter, exit]`, which is
/// empty when `enter > exit`.
struct Interval {
    double enter = infinity;
    double exit = -infinity;

    /// The whole line.
    [[nodiscard]] static constexpr Interval all() noexcept {
        return {.enter = -infinity, .exit = infinity};
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return !(enter <= exit); }

    /// The part of this interval that also lies in @p other.
    [[nodiscard]] constexpr Interval clip(const Interval& other) const noexcept {
        return {.enter = (std::max)(enter, other.enter), .exit = (std::min)(exit, other.exit)};
    }

    /// Extends this interval to cover @p other as well. An empty interval may hold any bounds
    /// with `enter > exit`, so it is replaced rather than extended.
    constexpr void cover(const Interval& other) noexcept {
        if (other.empty()) {
            return;
        }
        if (empty()) {
            *this = other;
            return;
        }
        enter = (std::min)(enter, other.enter);
        exit = (std::max)(exit, other.exit);
    }
};

/// The part of the line where `low ≤ origin + t · direction ≤ high`, along one coordinate.
[[nodiscard]] Interval slab(double origin, double direction, double low, double high) noexcept {
    if (direction == 0.0) {
        return origin >= low && origin <= high ? Interval::all() : Interval{};
    }
    // Divide rather than multiply by a reciprocal, which overflows for tiny components.
    const double to_low = (low - origin) / direction;
    const double to_high = (high - origin) / direction;
    return direction > 0.0 ? Interval{.enter = to_low, .exit = to_high}
                           : Interval{.enter = to_high, .exit = to_low};
}

/// The part of the line `offset + t · direction` within @p radius of the coordinate origin; pass
/// the line relative to the center of the ball.
[[nodiscard]] Interval ball(const Vec3& offset, const Vec3& direction, double radius) noexcept {
    const double radius_squared = radius * radius;
    const double a = length_squared(direction);
    if (a == 0.0) {
        return length_squared(offset) <= radius_squared ? Interval::all() : Interval{};
    }
    // As in intersect(): the discriminant comes from the distance between the center and the
    // line, which stays accurate far from the origin, and the roots are rearranged so that
    // nearly equal values are never subtracted.
    const double b = dot(offset, direction);
    const Vec3 perpendicular = offset - direction * (b / a);
    const double discriminant = a * (radius_squared - length_squared(perpendicular));
    if (discriminant < 0.0) {
        return {};
    }
    const double root = std::sqrt(discriminant);
    const double q = b > 0.0 ? -(b + root) : root - b;
    if (q == 0.0) {
        return {.enter = 0.0, .exit = 0.0}; // touches the ball at t = 0 only
    }
    const double first = q / a;
    const double second = (length_squared(offset) - radius_squared) / q;
    return {.enter = (std::min)(first, second), .exit = (std::max)(first, second)};
}

/// The part of the line within @p radius of the segment from @p from to @p to, not counting the
/// balls around its ends: a cylinder. Empty for a segment of zero length, whose ball covers it.
[[nodiscard]] Interval cylinder(
    const Vec3& origin, const Vec3& direction, const Vec3& from, const Vec3& to, double radius
) noexcept {
    const Vec3 edge = to - from;
    const double edge_length = length(edge);
    if (!(edge_length > 0.0)) {
        return {};
    }
    const Vec3 axis = edge / edge_length; // exact for the axis-aligned edges of boxes
    const Vec3 offset = origin - from;
    const double axial_offset = dot(offset, axis);
    const double axial_direction = dot(direction, axis);
    const Interval along = slab(axial_offset, axial_direction, 0.0, edge_length);
    if (along.empty()) {
        return {};
    }
    return along.clip(
        ball(offset - axis * axial_offset, direction - axis * axial_direction, radius)
    );
}

/// Corner @p index of @p box, numbered by bits: bit 0 selects the maximum x, bit 1 the maximum y,
/// bit 2 the maximum z.
[[nodiscard]] constexpr Vec3 corner(const AABB& box, unsigned index) noexcept {
    return Vec3{
        (index & 1U) != 0U ? box.max.x : box.min.x,
        (index & 2U) != 0U ? box.max.y : box.min.y,
        (index & 4U) != 0U ? box.max.z : box.min.z,
    };
}

/// The part of the line within @p radius of @p box: a box with rounded edges and corners.
[[nodiscard]] Interval rounded_box(
    const Vec3& origin, const Vec3& direction, const AABB& box, double radius
) noexcept {
    const Interval x = slab(origin.x, direction.x, box.min.x, box.max.x);
    const Interval y = slab(origin.y, direction.y, box.min.y, box.max.y);
    const Interval z = slab(origin.z, direction.z, box.min.z, box.max.z);
    const Interval grown_x = slab(origin.x, direction.x, box.min.x - radius, box.max.x + radius);
    const Interval grown_y = slab(origin.y, direction.y, box.min.y - radius, box.max.y + radius);
    const Interval grown_z = slab(origin.z, direction.z, box.min.z - radius, box.max.z + radius);
    // The box grown on every axis contains the rounded box: lines that miss it miss everything.
    if (grown_x.clip(grown_y).clip(grown_z).empty()) {
        return {};
    }

    // The faces: the box grown along one axis at a time.
    Interval inside = grown_x.clip(y).clip(z);
    inside.cover(x.clip(grown_y).clip(z));
    inside.cover(x.clip(y).clip(grown_z));
    // The edges: each joins two corners whose numbers differ in a single bit.
    for (unsigned from = 0U; from < 8U; ++from) {
        for (unsigned bit = 1U; bit < 8U; bit <<= 1U) {
            if ((from & bit) == 0U) {
                inside.cover(
                    cylinder(origin, direction, corner(box, from), corner(box, from | bit), radius)
                );
            }
        }
    }
    // The corners.
    for (unsigned index = 0U; index < 8U; ++index) {
        inside.cover(ball(origin - corner(box, index), direction, radius));
    }
    return inside;
}

/// The part of the line on the inner side of the plane through @p from and @p to perpendicular to
/// a triangle with the unit normal @p normal, for a triangle edge that runs from @p from to @p to.
[[nodiscard]] Interval inside_edge(
    const Vec3& origin, const Vec3& direction, const Vec3& from, const Vec3& to, const Vec3& normal
) noexcept {
    // For the counter-clockwise vertex order around the normal, this points into the triangle.
    const Vec3 inward = cross(normal, to - from);
    return slab(dot(inward, origin - from), dot(inward, direction), 0.0, infinity);
}

/// The part of the line within @p radius of @p triangle, whose unit normal is @p normal: a
/// triangle thickened by the radius on both sides, with rounded edges and corners.
[[nodiscard]] Interval rounded_triangle(
    const Vec3& origin,
    const Vec3& direction,
    const Triangle& triangle,
    const Vec3& normal,
    double radius
) noexcept {
    // The face: a prism over the triangle, reaching the radius to either side of it.
    Interval inside =
        slab(dot(normal, origin - triangle.a), dot(normal, direction), -radius, radius)
            .clip(inside_edge(origin, direction, triangle.a, triangle.b, normal))
            .clip(inside_edge(origin, direction, triangle.b, triangle.c, normal))
            .clip(inside_edge(origin, direction, triangle.c, triangle.a, normal));
    // The edges and the corners.
    inside.cover(cylinder(origin, direction, triangle.a, triangle.b, radius));
    inside.cover(cylinder(origin, direction, triangle.b, triangle.c, radius));
    inside.cover(cylinder(origin, direction, triangle.c, triangle.a, radius));
    inside.cover(ball(origin - triangle.a, direction, radius));
    inside.cover(ball(origin - triangle.b, direction, radius));
    inside.cover(ball(origin - triangle.c, direction, radius));
    return inside;
}

/// The first contact of a sphere of @p radius moving along @p path with a shape, given the part
/// @p inside of the path's line that lies within @p radius of the shape.
///
/// Follows the rules for solids in docs/collision.md:
/// - from outside, the contact is where the sphere first touches the shape;
/// - a sphere touching the shape at the start hits it at distance 0 if it moves into it, and not
///   at all if it moves away or slides along it;
/// - a sphere overlapping the shape at the start hits it where it stops overlapping, a back face.
///
/// @p closest returns the point of the shape closest to a given point. @p front tells whether a
/// contact reached from outside, with the given outward normal, is on the shape's front side.
template <typename Closest, typename Front>
[[nodiscard]] std::optional<HitResult> first_contact(
    const Segment& path,
    double radius,
    const Interval& inside,
    const Closest& closest,
    const Front& front,
    Culling culling
) noexcept {
    if (inside.empty() || !(inside.exit > 0.0)) {
        return std::nullopt; // the line misses the shape, or the shape lies behind the start
    }
    const Vec3 direction = path.end - path.start;
    double parameter = inside.enter;
    bool entering = true;
    if (!(inside.enter > 0.0)) {
        // The center starts within the radius of the shape, or at exactly that distance. Decide
        // which from the distance itself: the interval cannot tell touching and sliding along a
        // face from overlapping.
        const Vec3 offset = path.start - closest(path.start);
        if (length_squared(offset) < radius * radius) {
            parameter = inside.exit;
            entering = false;
        } else if (dot(direction, offset) < 0.0) {
            parameter = 0.0;
        } else {
            return std::nullopt;
        }
    }
    if (!(parameter <= 1.0)) {
        return std::nullopt; // the contact lies beyond the end of the path
    }

    const Vec3 center = lerp(path.start, path.end, parameter);
    const Vec3 point = closest(center);
    std::optional<Vec3> outward = try_normalize(center - point);
    if (!outward) {
        outward = try_normalize(-direction); // only when rounding puts the center on the shape
    }
    if (!outward) {
        return std::nullopt;
    }
    const bool front_face = entering && front(*outward);
    if (!front_face && culling == Culling::back_faces) {
        return std::nullopt;
    }
    return HitResult{
        .distance = parameter * length(path) + 0.0, // turns a distance of -0 into +0
        .point = point,
        .normal = (entering ? *outward : -*outward) + Vec3{}, // turns components of -0 into +0
        .material_id = 0,
        .front_face = front_face,
    };
}

} // namespace

std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const Plane& plane, Culling culling
) noexcept {
    if (radius == 0.0) {
        return intersect(path, plane, culling);
    }
    // The enlarged plane is the slab |signed distance| ≤ radius. Interpolate the endpoints'
    // signed distances, as the segment query does, so that an endpoint at the contact distance
    // yields a fraction of exactly 1.
    const double start_distance = signed_distance(plane, path.start);
    const double end_distance = signed_distance(plane, path.end);
    if (start_distance == end_distance) {
        return std::nullopt; // parallel to the plane, or zero length: never reaches or leaves it
    }
    const bool overlapping = std::abs(start_distance) < radius;
    const bool rising = end_distance > start_distance;
    // The side of the plane the contact is on: where a sphere comes from, or where an
    // overlapping sphere goes.
    const bool front_side = overlapping ? rising : start_distance > 0.0;
    if (!overlapping && rising == front_side) {
        return std::nullopt; // moving away from the plane
    }
    const double contact_distance = front_side ? radius : -radius;
    const double fraction = (start_distance - contact_distance) / (start_distance - end_distance);
    if (fraction < 0.0 || fraction > 1.0) {
        return std::nullopt;
    }
    const bool front_face = !overlapping && front_side;
    if (!front_face && culling == Culling::back_faces) {
        return std::nullopt;
    }
    const Vec3 outward = front_side ? plane.normal : -plane.normal;
    return HitResult{
        .distance = fraction * length(path) + 0.0, // turns a distance of -0 into +0
        .point = closest_point(plane, lerp(path.start, path.end, fraction)),
        .normal = (overlapping ? -outward : outward) + Vec3{}, // turns components of -0 into +0
        .material_id = 0,
        .front_face = front_face,
    };
}

std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const Sphere& sphere, Culling culling
) noexcept {
    if (radius == 0.0) {
        return intersect(path, sphere, culling);
    }
    std::optional<HitResult> hit =
        intersect(path, Sphere{.center = sphere.center, .radius = sphere.radius + radius}, culling);
    if (hit) {
        // The query found the center on the enlarged sphere. The contact lies on the original
        // sphere, in the same direction from its center.
        const Vec3 outward = hit->front_face ? hit->normal : -hit->normal;
        hit->point = sphere.center + outward * sphere.radius;
    }
    return hit;
}

std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const AABB& box, Culling culling
) noexcept {
    if (radius == 0.0) {
        return intersect(path, box, culling);
    }
    if (path.start == path.end) {
        return std::nullopt;
    }
    return first_contact(
        path,
        radius,
        rounded_box(path.start, path.end - path.start, box, radius),
        [&box](const Vec3& point) { return closest_point(box, point); },
        [](const Vec3& /*outward*/) { return true; }, // a box is a solid: its outside is in front
        culling
    );
}

std::optional<HitResult> sweep_sphere(
    const Segment& path, double radius, const Triangle& triangle, Culling culling
) noexcept {
    if (radius == 0.0) {
        return intersect(path, triangle, culling);
    }
    const std::optional<Vec3> normal = face_normal(triangle);
    if (!normal || path.start == path.end) {
        return std::nullopt; // zero area, or a path that does not move
    }
    const Vec3 front_normal = *normal;
    return first_contact(
        path,
        radius,
        rounded_triangle(path.start, path.end - path.start, triangle, front_normal, radius),
        [&triangle](const Vec3& point) { return closest_point(triangle, point); },
        // Front unless the sphere's center is behind the triangle's plane.
        [&front_normal](const Vec3& outward) { return dot(outward, front_normal) >= 0.0; },
        culling
    );
}

} // namespace projectile
