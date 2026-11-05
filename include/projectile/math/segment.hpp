#pragma once

/// @file
/// @brief Line segments between two points.

#include "projectile/math/vec3.hpp"

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

} // namespace projectile
