#pragma once

/// @file
/// @brief Material interactions: what happens when a projectile strikes a surface.
///
/// An interaction model decides whether a projectile ricochets, penetrates or stops, given an
/// Impact. Implement IMaterialInteraction to replace the standard model, StandardInteraction.
/// See docs/materials.md for the model and its assumptions.

#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/core/export.hpp"
#include "projectile/core/result.hpp"
#include "projectile/materials/material.hpp"
#include "projectile/math/vec3.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

namespace projectile {

/// What happened to a projectile at an impact.
enum class InteractionOutcome : std::uint8_t {
    stopped,    ///< It stopped at the surface.
    ricocheted, ///< It bounced off the surface and flies on.
    penetrated, ///< It passed through the object and flies on beyond it.
};

/// Everything an interaction model knows about an impact.
struct Impact {
    /// The projectile at the contact, moving at the velocity it struck with.
    ProjectileState state;

    /// The projectile's parameters. Its mass gives its kinetic energy.
    ProjectileConfig config;

    /// The contact. `hit.normal` faces the projectile, and `hit.material_id` identifies the
    /// material.
    HitResult hit;

    /// The material struck.
    Material material;

    /// How far the projectile's path runs through the struck object, in m. Zero for a surface
    /// without thickness, such as a plane or a single triangle. See docs/materials.md for how the
    /// simulation measures it.
    double thickness = 0.0;
};

/// What an interaction model decided.
struct InteractionResult {
    /// What happened.
    InteractionOutcome outcome = InteractionOutcome::stopped;

    /// The projectile's velocity after the impact, in m/s; zero when it stopped. After a
    /// penetration, the velocity with which it leaves the object.
    Vec3 outgoing_velocity;

    /// The projectile's kinetic energy after the impact, in J; zero when it stopped.
    double energy_remaining = 0.0;

    /// Exact member-wise comparison.
    constexpr bool operator==(const InteractionResult&) const = default;
};

/// A material interaction model: decides what happens when a projectile strikes a surface.
///
/// Implement this interface to replace StandardInteraction, for example with different rules
/// per material or with effects of your own. Implementations must not throw, and should not let
/// a projectile gain energy unless the game wants that.
class IMaterialInteraction {
public:
    virtual ~IMaterialInteraction() = default;

    /// Decides what happens at @p impact.
    [[nodiscard]] virtual InteractionResult resolve(const Impact& impact) const = 0;

protected:
    IMaterialInteraction() = default;
    IMaterialInteraction(const IMaterialInteraction&) = default;
    IMaterialInteraction(IMaterialInteraction&&) = default;
    IMaterialInteraction& operator=(const IMaterialInteraction&) = default;
    IMaterialInteraction& operator=(IMaterialInteraction&&) = default;
};

/// The decision of the standard interaction model, for projectiles that stop when slower than
/// @p stopping_speed m/s after an impact:
///
/// 1. **Ricochet** if the projectile strikes at an angle to the surface of at most
///    `material.ricochet_angle` (see impact_angle()). It leaves with ricochet_velocity().
/// 2. Otherwise, **penetrate** if its kinetic energy exceeds penetration_energy() for the
///    impact's thickness. It flies on in the same direction, with the energy left over.
/// 3. Otherwise, **stop**.
///
/// A projectile that would ricochet or penetrate more slowly than @p stopping_speed stops
/// instead. The outgoing speed never exceeds the speed at impact, and the remaining energy never
/// exceeds the energy at impact. This is the decision of StandardInteraction; call it from a
/// model of your own to reuse it, for example for some materials only.
[[nodiscard]] PROJECTILE_API InteractionResult
resolve_standard(const Impact& impact, double stopping_speed) noexcept;

/// The standard material interaction model: see resolve_standard().
class StandardInteraction final : public IMaterialInteraction {
public:
    /// The default stopping speed, in m/s.
    static constexpr double default_stopping_speed = 0.1;

    /// A model with the default stopping speed.
    StandardInteraction() noexcept = default;

    /// A model in which projectiles slower than @p stopping_speed m/s after an impact stop, or
    /// Error::invalid_stopping_speed if @p stopping_speed is negative or not finite.
    [[nodiscard]] static Result<StandardInteraction> create(double stopping_speed) noexcept {
        if (!std::isfinite(stopping_speed) || stopping_speed < 0.0) {
            return Error::invalid_stopping_speed;
        }
        return StandardInteraction(stopping_speed);
    }

    /// Speed, in m/s, below which projectiles stop after an impact.
    [[nodiscard]] double stopping_speed() const noexcept { return stopping_speed_; }

    /// Decides what happens at @p impact; see resolve_standard().
    [[nodiscard]] InteractionResult resolve(const Impact& impact) const override {
        return resolve_standard(impact, stopping_speed_);
    }

private:
    explicit StandardInteraction(double stopping_speed) noexcept
        : stopping_speed_(stopping_speed) {}

    double stopping_speed_ = default_stopping_speed;
};

} // namespace projectile
