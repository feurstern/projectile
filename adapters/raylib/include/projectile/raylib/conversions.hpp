#pragma once

/// @file
/// @brief Conversions between Projectile's types and raylib's.
///
/// Projectile computes in double precision and raylib draws in single precision. Converting to
/// raylib rounds each component to the nearest `float`, which keeps about seven significant
/// digits: 0.1 mm at 1 km from the origin. Both libraries use right-handed coordinates, and
/// neither fixes an up axis (raylib's cameras usually look along +y as up), so the axes carry
/// over unchanged.
///
/// raylib declares its types in the global namespace, among them a `Ray` and a `Material`
/// unrelated to Projectile's. Inside `namespace projectile`, `Ray` means projectile::Ray; this
/// file writes raylib's as `::Ray`.

#include <projectile/math/aabb.hpp>
#include <projectile/math/ray.hpp>
#include <projectile/math/vec3.hpp>

#include <raylib.h>

#include <optional>

namespace projectile::raylib {

/// @p v in raylib's single precision.
[[nodiscard]] constexpr Vector3 to_vector3(const Vec3& v) noexcept {
    return Vector3{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

/// @p v in Projectile's double precision. Exact: every float is a double.
[[nodiscard]] constexpr Vec3 to_vec3(const Vector3& v) noexcept {
    return Vec3{static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

/// @p ray as a raylib ray, for raylib's collision functions or for drawing.
[[nodiscard]] constexpr ::Ray to_raylib_ray(const Ray& ray) noexcept {
    return ::Ray{.position = to_vector3(ray.origin), .direction = to_vector3(ray.direction)};
}

/// @p ray as a Projectile ray, with its direction normalized; for example the ray through the
/// mouse cursor that GetScreenToWorldRay() returns. `std::nullopt` if the direction is zero or
/// a component is not finite. See make_ray().
[[nodiscard]] inline std::optional<Ray> to_ray(const ::Ray& ray) noexcept {
    return make_ray(to_vec3(ray.position), to_vec3(ray.direction));
}

/// @p box as a raylib bounding box.
[[nodiscard]] constexpr BoundingBox to_bounding_box(const AABB& box) noexcept {
    return BoundingBox{.min = to_vector3(box.min), .max = to_vector3(box.max)};
}

/// @p box as a Projectile box, whichever order its corners come in; `std::nullopt` if a corner
/// is not finite. See make_aabb().
[[nodiscard]] inline std::optional<AABB> to_aabb(const BoundingBox& box) noexcept {
    return make_aabb(to_vec3(box.min), to_vec3(box.max));
}

} // namespace projectile::raylib
