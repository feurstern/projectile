#pragma once

/// @file
/// @brief Physical constants.

namespace projectile {

/// Standard acceleration of gravity at the Earth's surface, in m/s² (exact by definition).
///
/// Projectile assumes no "up" axis, so gravity is always a vector you configure. Build it from
/// this constant for your world's up axis, for example `{0.0, -standard_gravity, 0.0}` when +y is
/// up.
inline constexpr double standard_gravity = 9.80665;

/// Density of air at sea level and 15 °C in the International Standard Atmosphere, in kg/m³:
/// the default air density for drag.
inline constexpr double standard_air_density = 1.225;

} // namespace projectile
