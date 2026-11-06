#pragma once

/// @file
/// @brief Axis-aligned bounding boxes.

#include "projectile/math/vec3.hpp"

#include <optional>

namespace projectile {

/// An axis-aligned bounding box: all points between the corners @ref min and @ref max, inclusive.
///
/// A valid box has finite corners with `min` no greater than `max` on every axis. Boxes that are
/// flat on one or more axes (`min == max`) are valid. Build boxes from arbitrary corners with
/// make_aabb(), which orders them.
struct AABB {
    Vec3 min; ///< Corner with the smallest coordinates.
    Vec3 max; ///< Corner with the largest coordinates.

    /// Exact member-wise comparison.
    constexpr bool operator==(const AABB&) const = default;
};

/// Creates the box spanned by two opposite corners given in any order.
///
/// To build a box from a center and half extents `h`, pass `center - h` and `center + h`.
/// Returns `std::nullopt` if a corner is not finite.
[[nodiscard]] inline std::optional<AABB> make_aabb(
    const Vec3& corner_a, const Vec3& corner_b
) noexcept {
    if (!is_finite(corner_a) || !is_finite(corner_b)) {
        return std::nullopt;
    }
    return AABB{
        .min = detail::component_min(corner_a, corner_b),
        .max = detail::component_max(corner_a, corner_b),
    };
}

/// Returns whether @p box satisfies the AABB invariants: finite corners with `min ≤ max` on every
/// axis.
[[nodiscard]] inline bool is_valid(const AABB& box) noexcept {
    return is_finite(box.min) && is_finite(box.max) && box.min.x <= box.max.x &&
           box.min.y <= box.max.y && box.min.z <= box.max.z;
}

/// Center of @p box. Computed from halved corners, so it cannot overflow even for boxes that
/// span most of the double range.
[[nodiscard]] constexpr Vec3 center(const AABB& box) noexcept {
    return box.min * 0.5 + box.max * 0.5;
}

/// Half the size of @p box on each axis: the distance from its center to its faces. Computed
/// from halved corners, so it cannot overflow.
[[nodiscard]] constexpr Vec3 half_extents(const AABB& box) noexcept {
    return box.max * 0.5 - box.min * 0.5;
}

/// Returns whether @p point lies inside @p box or on its boundary.
[[nodiscard]] constexpr bool contains(const AABB& box, const Vec3& point) noexcept {
    return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
           point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

/// The point of @p box closest to @p point: @p point itself if the box contains it, otherwise
/// the nearest point on the box's surface. Computed by clamping each coordinate, so it is exact.
[[nodiscard]] constexpr Vec3 closest_point(const AABB& box, const Vec3& point) noexcept {
    return detail::component_max(box.min, detail::component_min(point, box.max));
}

} // namespace projectile
