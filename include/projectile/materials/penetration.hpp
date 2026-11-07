#pragma once

/// @file
/// @brief Penetration: the energy a projectile needs to pass through an object.

#include "projectile/materials/material.hpp"

namespace projectile {

/// The kinetic energy, in J, that a projectile needs to pass through @p thickness metres of
/// @p material:
///
///     penetration_cost + resistance · thickness
///
/// A projectile with more energy passes through and keeps the difference; one with less stops.
/// Infinite for an impenetrable material. @p thickness is zero for surfaces without thickness,
/// such as a single triangle.
[[nodiscard]] constexpr double penetration_energy(
    const Material& material, double thickness
) noexcept {
    return material.penetration_cost + material.resistance * thickness;
}

} // namespace projectile
