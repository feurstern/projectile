#pragma once

/// @file
/// @brief Ricochet: the angle at which a projectile strikes a surface, and how it bounces off.

#include "projectile/math/vec3.hpp"

#include <algorithm>
#include <cmath>

namespace projectile {

/// The angle between @p velocity and a surface with the unit normal @p normal, in radians: 0 when
/// the velocity grazes the surface, π/2 when it strikes it head-on.
///
/// @p normal faces the side the projectile comes from, as HitResult::normal does. A velocity of
/// zero, or one that moves away from the surface, gives 0.
[[nodiscard]] inline double impact_angle(const Vec3& velocity, const Vec3& normal) noexcept {
    const double into_surface = -dot(velocity, normal);
    const Vec3 along_surface = velocity + normal * into_surface;
    // atan2 stays accurate at every angle, unlike asin near π/2 or acos near 0.
    return std::atan2((std::max)(into_surface, 0.0), length(along_surface));
}

/// The velocity of a projectile after it ricochets off a surface with the unit normal @p normal:
///
///     v' = (1 − friction) · v_t − restitution · v_n
///
/// where `v_n = (v · n) n` is the part of the velocity @p velocity into the surface, and
/// `v_t = v − v_n` the part along it. The part into the surface reverses, scaled by
/// @p restitution; the part along it shrinks by @p friction.
///
/// With `friction = 1 − restitution`, this is `restitution · reflect(v, n)`, the reflection
/// `V' = e R` scaled by the restitution `e`. For @p restitution and @p friction in [0, 1], the
/// speed never increases.
[[nodiscard]] constexpr Vec3 ricochet_velocity(
    const Vec3& velocity, const Vec3& normal, double restitution, double friction
) noexcept {
    const Vec3 into_surface = normal * dot(velocity, normal);
    const Vec3 along_surface = velocity - into_surface;
    return along_surface * (1.0 - friction) - into_surface * restitution;
}

} // namespace projectile
