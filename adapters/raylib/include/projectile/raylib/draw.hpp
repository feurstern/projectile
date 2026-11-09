#pragma once

/// @file
/// @brief Debug drawing of Projectile's shapes, queries, trajectories and projectiles with
/// raylib.
///
/// Every function draws in 3D: call them between BeginMode3D() and EndMode3D(). Lengths are in
/// the scene's units, metres by Projectile's convention.

#include <projectile/ballistics/trajectory.hpp>
#include <projectile/collision/hit_result.hpp>
#include <projectile/math/aabb.hpp>
#include <projectile/math/plane.hpp>
#include <projectile/math/ray.hpp>
#include <projectile/math/sphere.hpp>
#include <projectile/math/triangle.hpp>
#include <projectile/math/vec3.hpp>
#include <projectile/raylib/conversions.hpp>
#include <projectile/simulation/simulation.hpp>

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

namespace projectile::raylib {

/// Draws @p sphere as a solid ball.
inline void draw_sphere(const Sphere& sphere, Color color) {
    DrawSphere(to_vector3(sphere.center), static_cast<float>(sphere.radius), color);
}

/// Draws @p box as a solid box.
inline void draw_box(const AABB& box, Color color) {
    DrawCubeV(to_vector3(center(box)), to_vector3(2.0 * half_extents(box)), color);
}

/// Draws the edges of @p box.
inline void draw_box_wires(const AABB& box, Color color) {
    DrawBoundingBox(to_bounding_box(box), color);
}

/// Draws @p triangle, visible from both sides: raylib culls faces seen from behind.
inline void draw_triangle(const Triangle& triangle, Color color) {
    const Vector3 a = to_vector3(triangle.a);
    const Vector3 b = to_vector3(triangle.b);
    const Vector3 c = to_vector3(triangle.c);
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, b, color);
}

/// Draws a square piece of the infinite @p plane, `2 · half_size` wide, centred on the point of
/// the plane nearest to @p around.
inline void draw_plane(const Plane& plane, double half_size, Color color, const Vec3& around = {}) {
    // Two directions along the plane: the normal crossed with the axis it is least aligned
    // with, and the normal crossed with that.
    const Vec3& normal = plane.normal;
    const Vec3 axis = std::abs(normal.x) < 0.9 ? Vec3{1.0, 0.0, 0.0} : Vec3{0.0, 1.0, 0.0};
    const std::optional<Vec3> along = try_normalize(cross(normal, axis));
    if (!along) {
        return;
    }
    const Vec3 middle = closest_point(plane, around);
    const Vec3 u = half_size * *along;
    const Vec3 v = half_size * cross(normal, *along);
    draw_triangle(Triangle{middle - u - v, middle + u - v, middle + u + v}, color);
    draw_triangle(Triangle{middle - u - v, middle + u + v, middle - u + v}, color);
}

/// Draws an arrow from @p from to @p to: a shaft of @p radius, and a cone for the head.
inline void draw_arrow(const Vec3& from, const Vec3& to, double radius, Color color) {
    const Vec3 shaft = to - from;
    const double shaft_length = length(shaft);
    if (!(shaft_length > 0.0) || !(radius > 0.0)) {
        return;
    }
    const double head_length = std::min(0.35 * shaft_length, 6.0 * radius);
    const Vec3 neck = to - (head_length / shaft_length) * shaft;
    const auto shaft_radius = static_cast<float>(radius);
    DrawCylinderEx(to_vector3(from), to_vector3(neck), shaft_radius, shaft_radius, 8, color);
    DrawCylinderEx(to_vector3(neck), to_vector3(to), 2.5F * shaft_radius, 0.0F, 12, color);
}

/// Draws @p ray as a line from its origin to @p distance along it.
inline void draw_ray(const Ray& ray, double distance, Color color) {
    DrawLine3D(to_vector3(ray.origin), to_vector3(point_at(ray, distance)), color);
}

/// Draws the contact of @p hit: a ball at the point, and an arrow along the normal,
/// @p normal_length long. The ball's radius and the arrow's thickness scale with it.
inline void draw_hit(
    const HitResult& hit, double normal_length, Color point_color, Color normal_color
) {
    DrawSphere(to_vector3(hit.point), static_cast<float>(0.08 * normal_length), point_color);
    draw_arrow(
        hit.point, hit.point + normal_length * hit.normal, 0.025 * normal_length, normal_color
    );
}

namespace detail {

/// Draws one segment of a path: a line, or a tube of @p thickness.
inline void draw_segment(const Vec3& start, const Vec3& end, double thickness, Color color) {
    if (thickness > 0.0) {
        const auto radius = static_cast<float>(0.5 * thickness);
        DrawCylinderEx(to_vector3(start), to_vector3(end), radius, radius, 6, color);
    } else {
        DrawLine3D(to_vector3(start), to_vector3(end), color);
    }
}

} // namespace detail

/// Draws a path through @p points: lines, or with a @p thickness greater than zero, tubes that
/// stay visible from afar.
inline void draw_path(std::span<const Vec3> points, Color color, double thickness = 0.0) {
    const Vec3* previous = nullptr;
    for (const Vec3& point : points) {
        if (previous != nullptr) {
            detail::draw_segment(*previous, point, thickness, color);
        }
        previous = &point;
    }
}

/// Draws a path through the positions of @p samples, such as the samples of a Trajectory from
/// predict_trajectory(). See draw_path().
inline void draw_trajectory(
    std::span<const TrajectorySample> samples, Color color, double thickness = 0.0
) {
    const Vec3* previous = nullptr;
    for (const TrajectorySample& sample : samples) {
        if (previous != nullptr) {
            detail::draw_segment(*previous, sample.position, thickness, color);
        }
        previous = &sample.position;
    }
}

/// Draws every projectile of @p simulation as a ball where interpolated_position() puts it, so
/// that motion stays smooth between fixed steps. Each ball has the projectile's radius or
/// @p min_radius, whichever is larger: bullets are too small to see from a distance.
inline void draw_projectiles(const Simulation& simulation, Color color, double min_radius = 0.0) {
    const double alpha = simulation.interpolation_alpha();
    for (const SimulatedProjectile& projectile : simulation.projectiles()) {
        const double radius = std::max(projectile.config.radius, min_radius);
        DrawSphere(
            to_vector3(interpolated_position(projectile, alpha)), static_cast<float>(radius), color
        );
    }
}

} // namespace projectile::raylib
