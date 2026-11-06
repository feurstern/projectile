#pragma once

/// @file
/// @brief Line segments between two points.

#include "projectile/math/vec3.hpp"

#include <algorithm>

namespace projectile {

/// The straight line segment from @ref start to @ref end: the points
///
///     P(f) = start + f · (end − start),  0 ≤ f ≤ 1
///
/// A segment is the path a point covers during one simulation step. A segment whose endpoints
/// coincide does not move; collision queries never report hits for it.
struct Segment {
    Vec3 start; ///< Start point.
    Vec3 end;   ///< End point.

    /// Exact member-wise comparison.
    constexpr bool operator==(const Segment&) const = default;
};

/// Length of @p segment: the distance between its endpoints.
[[nodiscard]] inline double length(const Segment& segment) noexcept {
    return distance(segment.start, segment.end);
}

/// Returns whether both endpoints of @p segment are finite.
[[nodiscard]] inline bool is_valid(const Segment& segment) noexcept {
    return is_finite(segment.start) && is_finite(segment.end);
}

/// The point of @p segment closest to @p point. For a segment whose endpoints coincide, that is
/// its start.
[[nodiscard]] constexpr Vec3 closest_point(const Segment& segment, const Vec3& point) noexcept {
    const Vec3 direction = segment.end - segment.start;
    const double direction_squared = length_squared(direction);
    if (!(direction_squared > 0.0)) {
        return segment.start;
    }
    const double fraction =
        std::clamp(dot(point - segment.start, direction) / direction_squared, 0.0, 1.0);
    return lerp(segment.start, segment.end, fraction); // exact at both endpoints
}

} // namespace projectile
