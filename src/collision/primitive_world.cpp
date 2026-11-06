#include "projectile/collision/primitive_world.hpp"

#include "projectile/collision/intersection.hpp"
#include "projectile/collision/sweep.hpp"
#include "projectile/math/segment.hpp"

#include <optional>
#include <vector>

namespace projectile {

namespace {

/// Adds @p shape to @p bodies if it is valid.
template <typename Body, typename Shape>
std::optional<Error> add_valid(
    std::vector<Body>& bodies, const Shape& shape, std::uint32_t material
) {
    if (!is_valid(shape)) {
        return Error::invalid_shape;
    }
    bodies.push_back(Body{.shape = shape, .material_id = material});
    return std::nullopt;
}

/// Replaces @p nearest by @p hit, made of @p material, if @p hit is closer.
void keep_nearest(
    std::optional<HitResult>& nearest, std::optional<HitResult> hit, std::uint32_t material
) noexcept {
    if (hit && (!nearest || hit->distance < nearest->distance)) {
        hit->material_id = material;
        nearest = hit;
    }
}

} // namespace

std::optional<Error> PrimitiveWorld::add(const Plane& plane, std::uint32_t material_id) {
    return add_valid(planes_, plane, material_id);
}

std::optional<Error> PrimitiveWorld::add(const Sphere& sphere, std::uint32_t material_id) {
    return add_valid(spheres_, sphere, material_id);
}

std::optional<Error> PrimitiveWorld::add(const AABB& box, std::uint32_t material_id) {
    return add_valid(boxes_, box, material_id);
}

std::optional<Error> PrimitiveWorld::add(const Triangle& triangle, std::uint32_t material_id) {
    return add_valid(triangles_, triangle, material_id);
}

void PrimitiveWorld::clear() noexcept {
    planes_.clear();
    spheres_.clear();
    boxes_.clear();
    triangles_.clear();
}

std::size_t PrimitiveWorld::size() const noexcept {
    return planes_.size() + spheres_.size() + boxes_.size() + triangles_.size();
}

std::optional<HitResult> PrimitiveWorld::first_ray_hit(
    const Ray& ray, double max_distance
) const noexcept {
    // Each hit shortens the range searched for the next shapes. That changes no result: the
    // distance a query computes does not depend on the range.
    std::optional<HitResult> nearest;
    const auto search = [&](const auto& bodies) {
        for (const auto& body : bodies) {
            const double range = nearest ? nearest->distance : max_distance;
            keep_nearest(nearest, intersect(ray, body.shape, range), body.material_id);
        }
    };
    search(planes_);
    search(spheres_);
    search(boxes_);
    search(triangles_);
    return nearest;
}

std::optional<HitResult> PrimitiveWorld::first_sweep_hit(
    const Vec3& start, const Vec3& end, double radius
) const noexcept {
    const Segment path{.start = start, .end = end};
    std::optional<HitResult> nearest;
    const auto search = [&](const auto& bodies) {
        for (const auto& body : bodies) {
            // Qualified: inside the class, the name would find the member sweep_sphere().
            keep_nearest(
                nearest, projectile::sweep_sphere(path, radius, body.shape), body.material_id
            );
        }
    };
    search(planes_);
    search(spheres_);
    search(boxes_);
    search(triangles_);
    return nearest;
}

} // namespace projectile
