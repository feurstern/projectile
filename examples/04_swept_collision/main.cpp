// 04_swept_collision: fires projectiles at a 1 cm thick plate, at speeds up to 2,000 m/s, in a
// real-time simulation driven by uneven frame times.
//
// At 120 steps per second, a bullet at 2,000 m/s covers almost 17 m per step: checking only where
// it ends each step would let it pass straight through the plate. Projectile sweeps each step's
// path instead, so every shot stops at the plate's surface. The example also shows the game-loop
// side: update() with whatever time each frame took, event callbacks, lifetimes, and the
// interpolation factor for drawing between steps.

#include <projectile/projectile.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

using namespace projectile;

namespace {

constexpr std::uint32_t steel = 7;

/// Prints the event that ended a projectile's flight.
void print_event(const ProjectileEvent& event, double speed) {
    if (event.kind == ProjectileEventKind::expired) {
        std::printf(
            "%7.0f m/s  expired at t = %.4f s, %.1f m downrange\n",
            speed,
            event.time,
            event.state.position.x
        );
        return;
    }
    if (!event.hit) {
        return;
    }
    const HitResult& hit = *event.hit;
    std::printf(
        "%7.0f m/s  hit %s at t = %.6f s, x = %.4f m (center %.4f m), drop %.2f mm\n",
        speed,
        hit.material_id == steel ? "the plate" : "the ground",
        event.time,
        hit.point.x,
        event.state.position.x,
        (1.5 - event.state.position.y) * 1000.0
    );
}

} // namespace

int main() {
    // A 1 cm steel plate 50 m away, 2 m square, standing on the ground (+y is up).
    PrimitiveWorld world;
    if (world.add(AABB{.min = {50.0, 0.0, -1.0}, .max = {50.01, 2.0, 1.0}}, steel) ||
        world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1)) {
        std::fprintf(stderr, "invalid world\n");
        return 1;
    }

    Simulation simulation; // 120 Hz, semi-implicit Euler, at most 8 steps per update
    simulation.set_collision_world(&world);

    // Shots fired horizontally from 1.5 m, one per speed. Each projectile is a 5 mm sphere that
    // lives for half a second.
    const std::array speeds{150.0, 400.0, 900.0, 2'000.0, 60.0}; // m/s
    ProjectileConfig bullet;
    bullet.mass = 0.01;
    bullet.radius = 0.005;
    bullet.gravity = {0.0, -standard_gravity, 0.0};
    bullet.lifetime = 0.5;

    std::array<ProjectileId, speeds.size()> ids{};
    for (std::size_t i = 0; i < speeds.size(); ++i) {
        const Result<ProjectileState> state =
            create_projectile(bullet, Vec3{0.0, 1.5, 0.0}, Vec3{speeds.at(i), 0.0, 0.0});
        const Result<ProjectileId> id =
            state ? simulation.add_projectile(*state, bullet) : Result<ProjectileId>(state.error());
        if (!id) {
            const std::string_view reason = describe(id.error());
            std::fprintf(
                stderr, "cannot fire: %.*s\n", static_cast<int>(reason.size()), reason.data()
            );
            return 1;
        }
        ids.at(i) = *id;
    }

    // Report each flight's end with the speed it was fired at.
    simulation.set_event_callback([&ids, &speeds](const ProjectileEvent& event) {
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids.at(i) == event.id) {
                print_event(event, speeds.at(i));
            }
        }
    });

    // A game loop whose frames take uneven times, including one long hitch. The simulation
    // still advances in fixed 1/120 s steps.
    const std::array frame_times{1.0 / 60.0, 1.0 / 144.0, 1.0 / 30.0, 0.1, 1.0 / 60.0, 1.0 / 90.0};
    std::uint64_t frames = 0;
    while (!simulation.projectiles().empty()) {
        const double frame_delta = frame_times.at(frames % frame_times.size());
        simulation.update(frame_delta);
        ++frames;
        // A renderer would now draw each projectile at
        // interpolated_position(projectile, simulation.interpolation_alpha()).
    }
    std::printf(
        "\n%llu frames, %llu fixed steps (%.4f s simulated)\n",
        static_cast<unsigned long long>(frames),
        static_cast<unsigned long long>(simulation.step_count()),
        simulation.time()
    );
}
