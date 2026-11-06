#pragma once

/// @file
/// @brief Numerical integrators that advance projectiles through time.
///
/// See docs/ballistics.md for their accuracy and stability.

#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/math/vec3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace projectile {

/// @cond PROJECTILE_DETAIL
namespace detail {

/// The rules every integrator follows: an inactive projectile does not move, a time step that
/// is not finite or not greater than zero is ignored, and a step never outlasts the remaining
/// lifetime. Calls @p integrate with the duration to advance, then expires the projectile if its
/// lifetime has run out.
template <typename Integrate>
void advance(ProjectileState& state, double dt, const Integrate& integrate) {
    if (!state.active || !(dt > 0.0) || !std::isfinite(dt)) {
        return;
    }
    const double duration = (std::min)(dt, state.remaining_lifetime);
    integrate(duration);
    state.remaining_lifetime -= duration;
    if (!(state.remaining_lifetime > 0.0)) {
        state.remaining_lifetime = 0.0;
        state.active = false;
    }
}

template <typename Field>
inline constexpr bool nothrow_field =
    std::is_nothrow_invocable_v<const Field&, const Vec3&, const Vec3&>;

} // namespace detail
/// @endcond

/// Semi-implicit (symplectic) Euler integration:
///
///     v[n+1] = v[n] + a(x[n], v[n]) · dt
///     x[n+1] = x[n] + v[n+1] · dt
///
/// First-order accurate, with one force evaluation per step. Under gravity alone the velocity is
/// exact, and after a time `t` the position is off the exact solution by exactly
/// `½ · gravity · t · dt`, a displacement along gravity. Being symplectic, it keeps the energy of
/// oscillating systems bounded instead of letting it drift. The default integrator.
struct SemiImplicitEuler {
    /// Advances @p state by @p dt seconds under the acceleration field @p field.
    template <AccelerationField Field>
    void step(
        ProjectileState& state, double dt, const Field& field
    ) const noexcept(detail::nothrow_field<Field>) {
        detail::advance(state, dt, [&](double h) {
            const Vec3 acceleration = field(state.position, state.velocity);
            state.velocity += acceleration * h;
            state.position += state.velocity * h;
            state.acceleration = field(state.position, state.velocity);
        });
    }

    /// Advances @p state by @p dt seconds under the forces of @p config.
    void step(ProjectileState& state, const ProjectileConfig& config, double dt) const noexcept {
        step(state, dt, forces_of(config));
    }
};

/// Classical fourth-order Runge-Kutta integration.
///
/// Fourth-order accurate, with four force evaluations per step. Exact, up to rounding, under
/// constant acceleration such as gravity alone, and far more accurate than SemiImplicitEuler
/// when forces depend on velocity or position.
struct RungeKutta4 {
    /// Advances @p state by @p dt seconds under the acceleration field @p field.
    template <AccelerationField Field>
    void step(
        ProjectileState& state, double dt, const Field& field
    ) const noexcept(detail::nothrow_field<Field>) {
        detail::advance(state, dt, [&](double h) {
            const double half = 0.5 * h;
            const Vec3 x1 = state.position;
            const Vec3 v1 = state.velocity;
            const Vec3 a1 = field(x1, v1);
            const Vec3 x2 = x1 + v1 * half;
            const Vec3 v2 = v1 + a1 * half;
            const Vec3 a2 = field(x2, v2);
            const Vec3 x3 = x1 + v2 * half;
            const Vec3 v3 = v1 + a2 * half;
            const Vec3 a3 = field(x3, v3);
            const Vec3 x4 = x1 + v3 * h;
            const Vec3 v4 = v1 + a3 * h;
            const Vec3 a4 = field(x4, v4);
            const double sixth = h / 6.0;
            state.position = x1 + (v1 + 2.0 * v2 + 2.0 * v3 + v4) * sixth;
            state.velocity = v1 + (a1 + 2.0 * a2 + 2.0 * a3 + a4) * sixth;
            state.acceleration = field(state.position, state.velocity);
        });
    }

    /// Advances @p state by @p dt seconds under the forces of @p config.
    void step(ProjectileState& state, const ProjectileConfig& config, double dt) const noexcept {
        step(state, dt, forces_of(config));
    }
};

/// Selects an integrator at run time, for configuration structs.
enum class IntegratorKind : std::uint8_t {
    semi_implicit_euler, ///< SemiImplicitEuler: fast, first order. The default.
    rk4,                 ///< RungeKutta4: four times the work per step, fourth order.
};

/// Advances @p state by @p dt seconds under the forces of @p config, with the integrator
/// selected by @p kind.
inline void step(
    IntegratorKind kind, ProjectileState& state, const ProjectileConfig& config, double dt
) noexcept {
    switch (kind) {
    case IntegratorKind::semi_implicit_euler:
        SemiImplicitEuler{}.step(state, config, dt);
        return;
    case IntegratorKind::rk4:
        RungeKutta4{}.step(state, config, dt);
        return;
    }
}

} // namespace projectile
