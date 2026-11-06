// 03_hitscan: fires instant shots (hitscan) into a small firing range and prints what each one
// hits.
//
// Shows how to build a collision world from primitives with PrimitiveWorld, give each surface a
// material, query it with raycast(), and use the hit's normal to compute a ricochet direction.

#include <projectile/projectile.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

using namespace projectile;

namespace {

// Materials of the range; hits report them.
constexpr std::uint32_t ground = 1;
constexpr std::uint32_t target = 2;
constexpr std::uint32_t crate = 3;
constexpr std::uint32_t sign = 4;

std::string_view name_of(std::uint32_t material) {
    switch (material) {
    case ground:
        return "ground";
    case target:
        return "target";
    case crate:
        return "crate";
    case sign:
        return "sign";
    default:
        return "unknown";
    }
}

/// Adds @p shape to @p world, reporting a shape that the world rejects.
template <typename Shape>
bool add(PrimitiveWorld& world, const Shape& shape, std::uint32_t material) {
    if (const std::optional<Error> error = world.add(shape, material)) {
        const std::string_view reason = describe(*error);
        std::fprintf(
            stderr, "invalid shape: %.*s\n", static_cast<int>(reason.size()), reason.data()
        );
        return false;
    }
    return true;
}

} // namespace

int main() {
    // A range along +x with +y up: ground, two round targets, a crate and a slanted sign.
    PrimitiveWorld range;
    const bool built =
        add(range, Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, ground) &&
        add(range, Sphere{.center = {20.0, 1.5, 0.0}, .radius = 0.5}, target) &&
        add(range, Sphere{.center = {35.0, 1.5, 3.0}, .radius = 0.5}, target) &&
        add(range, AABB{.min = {12.0, 0.0, -3.0}, .max = {13.0, 1.0, -2.0}}, crate) &&
        add(range,
            Triangle{.a = {25.0, 0.0, -4.0}, .b = {25.0, 0.0, -1.0}, .c = {27.0, 3.0, -2.5}},
            sign);
    if (!built) {
        return 1;
    }

    // Shots from a muzzle 1.5 m above the ground, aimed at points in the range.
    const Vec3 muzzle{0.0, 1.5, 0.0};
    const std::array aims{
        Vec3{20.0, 1.5, 0.0},  // the near target, dead center
        Vec3{35.0, 1.3, 3.0},  // the far target, a little low
        Vec3{12.5, 0.5, -2.5}, // the crate
        Vec3{26.0, 1.0, -2.5}, // the sign
        Vec3{10.0, 0.0, 6.0},  // the ground
        Vec3{50.0, 4.0, 0.0},  // over everything
    };
    constexpr double max_range = 100.0; // m

    std::printf(
        "shot  hits     distance (m)  point (m)                normal              ricochet\n"
    );
    int shot = 0;
    for (const Vec3& aim : aims) {
        ++shot;
        const std::optional<Ray> ray = make_ray(muzzle, aim - muzzle); // normalizes the direction
        if (!ray) {
            continue;
        }
        const std::optional<HitResult> hit = raycast(range, *ray, max_range);
        if (!hit) {
            std::printf("%4d  nothing within %.0f m\n", shot, max_range);
            continue;
        }
        // The normal faces the shooter, so reflecting off it gives the ricochet direction.
        const Vec3 ricochet = reflect(ray->direction, hit->normal);
        const std::string_view material = name_of(hit->material_id);
        std::printf(
            "%4d  %-7.*s  %12.3f  (%6.2f, %5.2f, %5.2f)  (%5.2f, %5.2f, %5.2f)  (%5.2f, %5.2f, "
            "%5.2f)\n",
            shot,
            static_cast<int>(material.size()),
            material.data(),
            hit->distance,
            hit->point.x,
            hit->point.y,
            hit->point.z,
            hit->normal.x,
            hit->normal.y,
            hit->normal.z,
            ricochet.x,
            ricochet.y,
            ricochet.z
        );
    }
}
