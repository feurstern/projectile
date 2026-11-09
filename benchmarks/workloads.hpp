#pragma once

/// @file
/// @brief The workloads that Projectile's benchmarks measure.
///
/// Every input comes from SplitMix64 with a fixed seed, so every compiler and standard library
/// measures the same work. (The distributions of `<random>` are implementation-defined, so they
/// would not.) docs/benchmarks.md describes each workload.

#include <projectile/projectile.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

namespace projectile::bench {

/// SplitMix64, from Steele, Lea and Flood, "Fast splittable pseudorandom number generators"
/// (2014): a small generator whose output is fully specified.
class SplitMix64 {
public:
    explicit constexpr SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}

    /// The next 64 random bits.
    constexpr std::uint64_t next() noexcept {
        state_ += 0x9E37'79B9'7F4A'7C15U;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30U)) * 0xBF58'476D'1CE4'E5B9U;
        z = (z ^ (z >> 27U)) * 0x94D0'49BB'1331'11EBU;
        return z ^ (z >> 31U);
    }

    /// Uniform in [0, 1): the top 53 bits as a fraction.
    constexpr double uniform() noexcept { return static_cast<double>(next() >> 11U) * 0x1.0p-53; }

    /// Uniform in [@p low, @p high).
    constexpr double uniform(double low, double high) noexcept {
        return low + (high - low) * uniform();
    }

    /// Uniform in the box spanned by @p low and @p high.
    constexpr Vec3 uniform(const Vec3& low, const Vec3& high) noexcept {
        // One draw per statement: the order in which function arguments are evaluated is
        // unspecified, so draws inside one call would differ between compilers.
        const double x = uniform(low.x, high.x);
        const double y = uniform(low.y, high.y);
        const double z = uniform(low.z, high.z);
        return {x, y, z};
    }

    /// A direction uniformly distributed over the unit sphere. Rejection sampling from the cube
    /// needs only arithmetic and a square root, which are exact to the last bit on every platform.
    Vec3 direction() noexcept {
        for (;;) {
            const Vec3 candidate = uniform(Vec3{-1.0, -1.0, -1.0}, Vec3{1.0, 1.0, 1.0});
            const double squared = length_squared(candidate);
            if (squared > 1e-6 && squared <= 1.0) {
                return candidate / std::sqrt(squared);
            }
        }
    }

private:
    std::uint64_t state_;
};

// ---------------------------------------------------------------------------------------------
// The arena
// ---------------------------------------------------------------------------------------------

/// Material id of every surface in the arena.
inline constexpr std::uint32_t arena_material = 1;

/// The arena is a room of 40 m × 20 m × 40 m: x and z from -20 to 20 m, y from 0 to 20 m.
inline constexpr double arena_half_width = 20.0;
inline constexpr double arena_height = 20.0;

/// The obstacles stand on the floor and stay below 8 m. Projectiles and rays start in the open
/// space above them.
inline constexpr Vec3 open_space_min{-18.0, 10.0, -18.0};
inline constexpr Vec3 open_space_max{18.0, 18.0, 18.0};

/// Rays longer than the arena's diagonal (about 60 m) cannot miss its walls.
inline constexpr double arena_ray_length = 100.0;

/// Adds the arena's 32 obstacles to @p world: 8 boxes standing on the floor, 8 spheres, and
/// 16 triangles at random orientations. Returns false if a shape was invalid, which would make
/// the workload differ from the documented one.
[[nodiscard]] inline bool add_obstacles(PrimitiveWorld& world) {
    SplitMix64 random(20'261'109);
    bool valid = true;
    for (int i = 0; i < 8; ++i) {
        const double x = random.uniform(-17.0, 17.0);
        const double z = random.uniform(-17.0, 17.0);
        const double half_width = random.uniform(0.5, 2.0);
        const double height = random.uniform(1.0, 8.0);
        const AABB box{
            .min = {x - half_width, 0.0, z - half_width},
            .max = {x + half_width, height, z + half_width},
        };
        valid = !world.add(box, arena_material) && valid;
    }
    for (int i = 0; i < 8; ++i) {
        const double x = random.uniform(-17.0, 17.0);
        const double z = random.uniform(-17.0, 17.0);
        const double radius = random.uniform(0.5, 2.0);
        const double y = radius + random.uniform(0.0, 8.0 - 2.0 * radius);
        valid = !world.add(Sphere{.center = {x, y, z}, .radius = radius}, arena_material) && valid;
    }
    for (int i = 0; i < 16; ++i) {
        const Vec3 center = random.uniform(Vec3{-17.0, 2.5, -17.0}, Vec3{17.0, 5.5, 17.0});
        const Vec3 a = center + 2.0 * random.direction();
        const Vec3 b = center + 2.0 * random.direction();
        const Vec3 c = center + 2.0 * random.direction();
        valid = !world.add(Triangle{a, b, c}, arena_material) && valid;
    }
    return valid;
}

/// Adds the arena's room to @p world: six planes facing inward, so that nothing inside can
/// leave.
[[nodiscard]] inline bool add_room(PrimitiveWorld& world) {
    const double w = arena_half_width;
    bool valid = true;
    valid = !world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, arena_material) && valid;
    valid =
        !world.add(Plane{.normal = {0.0, -1.0, 0.0}, .distance = -arena_height}, arena_material) &&
        valid;
    valid = !world.add(Plane{.normal = {1.0, 0.0, 0.0}, .distance = -w}, arena_material) && valid;
    valid = !world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -w}, arena_material) && valid;
    valid = !world.add(Plane{.normal = {0.0, 0.0, 1.0}, .distance = -w}, arena_material) && valid;
    valid = !world.add(Plane{.normal = {0.0, 0.0, -1.0}, .distance = -w}, arena_material) && valid;
    return valid;
}

/// @p count rays from random points of the open space, in uniformly random directions. In the
/// closed arena, every one of them hits something.
inline std::vector<Ray> arena_rays(std::size_t count, std::uint64_t seed) {
    SplitMix64 random(seed);
    std::vector<Ray> rays;
    rays.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 origin = random.uniform(open_space_min, open_space_max);
        rays.push_back(Ray{.origin = origin, .direction = random.direction()});
    }
    return rays;
}

/// @p count rays toward the unit shapes of the single-shape benchmarks: from 5 m away, aimed at
/// random points within 1.5 m of the origin, so that some hit and some miss.
inline std::vector<Ray> rays_at_origin(std::size_t count, std::uint64_t seed) {
    SplitMix64 random(seed);
    std::vector<Ray> rays;
    rays.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 origin = 5.0 * random.direction();
        const Vec3 target = random.uniform(Vec3{-1.5, -1.5, -1.5}, Vec3{1.5, 1.5, 1.5});
        // The target is at most 2.6 m from the origin and the start 5 m, so the direction is
        // never zero.
        if (const std::optional<Ray> ray = make_ray(origin, target - origin)) {
            rays.push_back(*ray);
        }
    }
    return rays;
}

/// The path of one swept-sphere query.
struct SweepPath {
    Vec3 start;
    Vec3 end;
};

/// @p count paths from random points of the open space, 0.5 to 8 m long in random directions:
/// the steps of projectiles at up to about 1,000 m/s at 120 Hz. Most of them hit nothing, so
/// most queries test every shape, as most steps of a simulation do.
inline std::vector<SweepPath> arena_sweeps(std::size_t count, std::uint64_t seed) {
    SplitMix64 random(seed);
    std::vector<SweepPath> paths;
    paths.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 start = random.uniform(open_space_min, open_space_max);
        const Vec3 direction = random.direction();
        const double length = random.uniform(0.5, 8.0);
        paths.push_back(SweepPath{.start = start, .end = start + length * direction});
    }
    return paths;
}

// ---------------------------------------------------------------------------------------------
// Projectiles
// ---------------------------------------------------------------------------------------------

/// A pistol bullet: 8 g, 9 mm in diameter, under Earth's gravity. With drag, it also feels air
/// resistance and a light crosswind.
inline ProjectileConfig bullet(bool with_drag) {
    ProjectileConfig config;
    config.mass = 0.008;
    config.radius = 0.0045;
    config.gravity = {0.0, -9.81, 0.0};
    if (with_drag) {
        config.drag_coefficient = 0.3;
        config.reference_area = std::numbers::pi * config.radius * config.radius;
        config.wind = {3.0, 0.0, 1.0};
    }
    return config;
}

/// @p count projectiles at random points of the open space, flying in random directions at 20 to
/// 400 m/s.
inline std::vector<ProjectileState> arena_spawns(
    std::size_t count, std::uint64_t seed, const ProjectileConfig& config
) {
    SplitMix64 random(seed);
    std::vector<ProjectileState> spawns;
    spawns.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 position = random.uniform(open_space_min, open_space_max);
        const Vec3 direction = random.direction();
        const double speed = random.uniform(20.0, 400.0);
        const Result<ProjectileState> state =
            create_projectile(config, position, speed * direction);
        if (state) {
            spawns.push_back(*state);
        }
    }
    return spawns;
}

/// A material that bounces every projectile back at full speed, so that projectiles in the
/// closed arena fly and collide forever: a steady workload for as long as a benchmark runs.
inline Material elastic_material() {
    Material material;
    material.id = arena_material;
    material.restitution = 1.0;
    material.friction = 0.0;
    material.ricochet_angle = std::numbers::pi / 2.0; // every impact ricochets
    return material;
}

} // namespace projectile::bench
