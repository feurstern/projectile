#pragma once

/// @file
/// @brief The result of a collision query.

#include "projectile/math/vec3.hpp"

#include <cstdint>

namespace projectile {

/// Where and how a query (a ray, a segment or a swept sphere) first touched a surface.
///
/// Queries return `std::optional<HitResult>`; an empty optional means that nothing was hit.
/// Every query and shape follows the same conventions, described in docs/collision.md.
struct HitResult {
    /// Distance from the start of the query to the contact, in metres. For a swept sphere, the
    /// distance its center travels.
    double distance = 0.0;

    /// Contact point on the surface that was hit.
    Vec3 point;

    /// Unit surface normal at the contact, facing the side the query came from, so that
    /// `dot(normal, direction of travel) ≤ 0`.
    Vec3 normal;

    /// Material of the surface that was hit. Collision worlds set it; queries against a single
    /// shape leave it 0.
    std::uint32_t material_id = 0;

    /// Whether the front side of the surface was hit: the outside of a sphere or box, or the side
    /// a plane's or triangle's normal points to. False when a plane or triangle is hit from behind,
    /// and when a query that started inside a sphere or box reaches its surface from within.
    bool front_face = true;

    /// Exact member-wise comparison.
    constexpr bool operator==(const HitResult&) const = default;
};

} // namespace projectile
