#pragma once

/// @file
/// @brief A ready-made collision world made of geometric primitives.

#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/core/export.hpp"
#include "projectile/core/result.hpp"
#include "projectile/math/aabb.hpp"
#include "projectile/math/plane.hpp"
#include "projectile/math/ray.hpp"
#include "projectile/math/sphere.hpp"
#include "projectile/math/triangle.hpp"
#include "projectile/math/vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace projectile {

/// A collision world made of planes, spheres, boxes and triangles, each with a material.
///
/// Ray queries use intersect(), and swept-sphere queries use sweep_sphere(), so they follow the
/// conventions of docs/collision.md. Planes and triangles are two-sided. When shapes are hit at
/// exactly the same distance, the query reports the one that comes first in the order planes,
/// spheres, boxes, triangles, and within each kind the one added first.
///
/// Every query tests every shape, so its cost grows with the number of shapes. PrimitiveWorld
/// suits small scenes, tests and examples. For large scenes, implement ICollisionWorld on top of
/// a spatial index or an engine's physics instead.
///
/// Queries do not modify the world, so several threads may query it at once, provided none adds
/// or removes shapes meanwhile.
class PrimitiveWorld final : public ICollisionWorld {
public:
    /// @name Adding shapes
    /// Each function adds a shape made of material @p material_id, which hits on it report. An
    /// invalid shape (see the is_valid() functions) is not added; the function returns
    /// Error::invalid_shape instead.
    /// @{
    [[nodiscard]] PROJECTILE_API std::optional<Error> add(
        const Plane& plane, std::uint32_t material_id = 0
    );
    [[nodiscard]] PROJECTILE_API std::optional<Error> add(
        const Sphere& sphere, std::uint32_t material_id = 0
    );
    [[nodiscard]] PROJECTILE_API std::optional<Error> add(
        const AABB& box, std::uint32_t material_id = 0
    );
    [[nodiscard]] PROJECTILE_API std::optional<Error> add(
        const Triangle& triangle, std::uint32_t material_id = 0
    );
    /// @}

    /// Removes every shape.
    PROJECTILE_API void clear() noexcept;

    /// Number of shapes in the world.
    [[nodiscard]] PROJECTILE_API std::size_t size() const noexcept;

    /// Whether the world has no shapes.
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    /// The first contact along @p ray within @p max_distance, with its material. See
    /// ICollisionWorld::raycast(); query through projectile::raycast(), which validates.
    [[nodiscard]] std::optional<HitResult> raycast(
        const Ray& ray, double max_distance
    ) const override {
        return first_ray_hit(ray, max_distance);
    }

    /// The first contact of a sphere of @p radius moving from @p start to @p end, with its
    /// material. See ICollisionWorld::sweep_sphere(); query through projectile::sweep_sphere(),
    /// which validates.
    [[nodiscard]] std::optional<HitResult> sweep_sphere(
        const Vec3& start, const Vec3& end, double radius
    ) const override {
        return first_sweep_hit(start, end, radius);
    }

private:
    template <typename Shape>
    struct Body {
        Shape shape;
        std::uint32_t material_id = 0;
    };

    // The overrides above are inline, so that the class needs no exported virtual table.
    [[nodiscard]] PROJECTILE_API std::optional<HitResult> first_ray_hit(
        const Ray& ray, double max_distance
    ) const noexcept;
    [[nodiscard]] PROJECTILE_API std::optional<HitResult> first_sweep_hit(
        const Vec3& start, const Vec3& end, double radius
    ) const noexcept;

    std::vector<Body<Plane>> planes_;
    std::vector<Body<Sphere>> spheres_;
    std::vector<Body<AABB>> boxes_;
    std::vector<Body<Triangle>> triangles_;
};

} // namespace projectile
