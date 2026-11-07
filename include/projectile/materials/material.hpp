#pragma once

/// @file
/// @brief Materials: the gameplay properties of surfaces that projectiles strike.

#include "projectile/core/export.hpp"
#include "projectile/core/result.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace projectile {

/// How a surface responds to projectiles: whether they ricochet off it, pass through it, or stop
/// in it, and how much energy that costs them.
///
/// These are gameplay parameters, tuned for the feel of a game. They are not physical constants
/// of real materials, and Projectile makes no claim that they predict real ricochets or
/// penetration. docs/materials.md describes the model that uses them.
///
/// A default-constructed material stops every projectile: it is impenetrable and never makes
/// projectiles ricochet.
struct Material {
    /// Identifies the material. Collision worlds report it as HitResult::material_id.
    std::uint32_t id = 0;

    /// Energy a projectile loses per metre it travels through the material, in J/m; finite and
    /// zero or greater. Thicker objects of the material cost more to penetrate.
    double resistance = 0.0;

    /// Fraction of the speed into the surface that a ricochet keeps, from 0 to 1. At 1 the
    /// projectile leaves the surface as fast as it struck it; at 0 it slides along the surface.
    double restitution = 0.0;

    /// Fraction of the speed along the surface that a ricochet loses, from 0 to 1.
    double friction = 0.0;

    /// Energy a projectile needs to break into the material, in J, whatever its thickness; zero
    /// or greater. Infinite, the default, makes the material impenetrable.
    double penetration_cost = std::numeric_limits<double>::infinity();

    /// Largest angle between a projectile's path and the surface, in radians from 0 to π/2, at
    /// which projectiles ricochet. Shallower impacts ricochet; steeper ones try to penetrate. Zero,
    /// the default, never ricochets; π/2 always does.
    double ricochet_angle = 0.0;

    /// Exact member-wise comparison.
    constexpr bool operator==(const Material&) const = default;
};

/// @cond PROJECTILE_DETAIL
namespace detail {

/// Whether `low ≤ value ≤ high`; false for NaN.
[[nodiscard]] constexpr bool within(double value, double low, double high) noexcept {
    return value >= low && value <= high;
}

} // namespace detail
/// @endcond

/// Checks @p material and returns the first problem found, or `std::nullopt` if it is valid.
[[nodiscard]] inline std::optional<Error> validate(const Material& material) noexcept {
    if (!std::isfinite(material.resistance) || material.resistance < 0.0) {
        return Error::invalid_resistance;
    }
    if (!detail::within(material.restitution, 0.0, 1.0)) {
        return Error::invalid_restitution;
    }
    if (!detail::within(material.friction, 0.0, 1.0)) {
        return Error::invalid_friction;
    }
    if (!(material.penetration_cost >= 0.0)) { // rejects NaN; infinity is allowed
        return Error::invalid_penetration_cost;
    }
    if (!detail::within(material.ricochet_angle, 0.0, std::numbers::pi / 2.0)) {
        return Error::invalid_ricochet_angle;
    }
    return std::nullopt;
}

/// The materials of a game, looked up by id.
///
/// Collision worlds report the material of each surface as an id (HitResult::material_id); a
/// MaterialTable turns that id into a Material. Ids without a material get the fallback
/// material, which by default stops every projectile.
///
/// Lookups do not modify the table, so several threads may look up materials at once, provided
/// none modifies the table meanwhile.
class MaterialTable {
public:
    /// Adds @p material, replacing the material with the same id if there is one. Returns the
    /// first problem found in @p material instead, changing nothing.
    [[nodiscard]] PROJECTILE_API std::optional<Error> add(const Material& material);

    /// Removes the material with @p id. Returns false if there is none.
    PROJECTILE_API bool remove(std::uint32_t id) noexcept;

    /// Removes every material. The fallback material stays.
    void clear() noexcept { materials_.clear(); }

    /// The material with @p id, or `nullptr` if there is none.
    [[nodiscard]] PROJECTILE_API const Material* find(std::uint32_t id) const noexcept;

    /// The material that surfaces of material @p id are made of: the one with that id, or the
    /// fallback material.
    [[nodiscard]] const Material& material_for(std::uint32_t id) const noexcept {
        const Material* const material = find(id);
        return material != nullptr ? *material : fallback_;
    }

    /// Sets the material used for ids without a material of their own. Returns the first problem
    /// found in @p material instead, changing nothing.
    [[nodiscard]] std::optional<Error> set_fallback(const Material& material) noexcept {
        if (const std::optional<Error> error = validate(material)) {
            return error;
        }
        fallback_ = material;
        return std::nullopt;
    }

    /// The material used for ids without a material of their own.
    [[nodiscard]] const Material& fallback() const noexcept { return fallback_; }

    /// Number of materials, not counting the fallback.
    [[nodiscard]] std::size_t size() const noexcept { return materials_.size(); }

    /// Whether the table has no materials, apart from the fallback.
    [[nodiscard]] bool empty() const noexcept { return materials_.empty(); }

private:
    std::vector<Material> materials_; ///< Sorted by id.
    Material fallback_;
};

} // namespace projectile
