#include "projectile/materials/interaction.hpp"

#include "projectile/ballistics/energy.hpp"
#include "projectile/materials/penetration.hpp"
#include "projectile/materials/ricochet.hpp"

#include <optional>

namespace projectile {

namespace {

/// The result of a projectile flying on at @p velocity after an impact, or of it stopping if
/// that is slower than @p stopping_speed.
[[nodiscard]] InteractionResult fly_on(
    InteractionOutcome outcome, const Vec3& velocity, double mass, double stopping_speed
) noexcept {
    const double speed = length(velocity);
    if (!(speed > 0.0) || speed < stopping_speed) {
        return InteractionResult{};
    }
    return InteractionResult{
        .outcome = outcome,
        .outgoing_velocity = velocity,
        .energy_remaining = kinetic_energy(mass, velocity),
    };
}

} // namespace

InteractionResult resolve_standard(const Impact& impact, double stopping_speed) noexcept {
    const Material& material = impact.material;
    const Vec3& velocity = impact.state.velocity;
    const double mass = impact.config.mass;

    if (material.ricochet_angle > 0.0 &&
        impact_angle(velocity, impact.hit.normal) <= material.ricochet_angle) {
        // A ricochet too slow to go on stops the projectile; it does not penetrate instead.
        return fly_on(
            InteractionOutcome::ricocheted,
            ricochet_velocity(velocity, impact.hit.normal, material.restitution, material.friction),
            mass,
            stopping_speed
        );
    }

    const double energy = kinetic_energy(mass, velocity);
    const double cost = penetration_energy(material, impact.thickness);
    const std::optional<Vec3> direction = try_normalize(velocity);
    if (!(energy > cost) || !direction) {
        return InteractionResult{};
    }
    // Passing through for free leaves the velocity exactly as it was.
    const Vec3 outgoing =
        cost == 0.0 ? velocity : *direction * speed_for_energy(mass, energy - cost);
    return fly_on(InteractionOutcome::penetrated, outgoing, mass, stopping_speed);
}

} // namespace projectile
