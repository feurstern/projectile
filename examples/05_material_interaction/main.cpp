// 05_material_interaction: fires bullets into a range of wood, steel, concrete and dirt, and
// prints what each impact does: ricochet, penetration or stop, with the energy it costs.
//
// Shows how to define materials, put them in a MaterialTable, give the collision world's
// surfaces their ids, and let a StandardInteraction decide each impact. The bullets also feel
// air drag and a light crosswind. The material values are gameplay settings, not measurements.

#include <projectile/projectile.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <optional>
#include <string_view>

using namespace projectile;

namespace {

constexpr std::uint32_t dirt = 1;
constexpr std::uint32_t wood = 2;
constexpr std::uint32_t steel = 3;
constexpr std::uint32_t concrete = 4;

std::string_view name_of(std::uint32_t material) {
    switch (material) {
    case dirt:
        return "dirt";
    case wood:
        return "wood";
    case steel:
        return "steel";
    case concrete:
        return "concrete";
    default:
        return "?";
    }
}

/// The range's materials.
MaterialTable range_materials() {
    Material ground;
    ground.id = dirt;
    ground.ricochet_angle = degrees_to_radians(8.0); // only very shallow shots skip off it
    ground.restitution = 0.2;
    ground.friction = 0.5;

    Material board;
    board.id = wood;
    board.penetration_cost = 50.0; // J to break in
    board.resistance = 2'000.0;    // J per metre: a 2 cm board costs 90 J in all
    board.ricochet_angle = degrees_to_radians(10.0);
    board.restitution = 0.3;
    board.friction = 0.4;

    Material plate;
    plate.id = steel;
    plate.penetration_cost = 5'000.0;
    plate.resistance = 5e6;
    plate.ricochet_angle = degrees_to_radians(25.0);
    plate.restitution = 0.6;
    plate.friction = 0.2;

    Material wall;
    wall.id = concrete; // impenetrable, as a default Material is
    wall.ricochet_angle = degrees_to_radians(15.0);
    wall.restitution = 0.3;
    wall.friction = 0.5;

    MaterialTable table;
    for (const Material& material : {ground, board, plate, wall}) {
        if (table.add(material)) {
            std::fprintf(stderr, "invalid material %u\n", material.id);
        }
    }
    return table;
}

struct Shot {
    std::string_view name;
    Vec3 origin;
    Vec3 velocity;
};

std::string_view verb_for(ProjectileEventKind kind) {
    switch (kind) {
    case ProjectileEventKind::ricochet:
        return "ricocheted off";
    case ProjectileEventKind::penetration:
        return "went through";
    case ProjectileEventKind::hit:
        return "stopped in";
    case ProjectileEventKind::expired:
        return "expired";
    }
    return "?";
}

/// Prints what happened to @p shot: its speed and energy before and after the impact.
void print_event(const ProjectileEvent& event, std::string_view shot) {
    const double speed = length(event.state.velocity);
    std::printf("%-18.*s  t = %.4f s  ", static_cast<int>(shot.size()), shot.data(), event.time);
    if (event.kind == ProjectileEventKind::expired) {
        std::printf("expired at %.1f m/s\n", speed);
        return;
    }
    const std::string_view material = event.hit ? name_of(event.hit->material_id) : "?";
    const std::string_view verb = verb_for(event.kind);
    const double speed_after =
        event.interaction ? length(event.interaction->outgoing_velocity) : 0.0;
    const double energy_after = event.interaction ? event.interaction->energy_remaining : 0.0;
    std::printf(
        "%-14.*s %-8.*s %6.1f -> %6.1f m/s  %7.1f -> %7.1f J\n",
        static_cast<int>(verb.size()),
        verb.data(),
        static_cast<int>(material.size()),
        material.data(),
        speed,
        speed_after,
        kinetic_energy(event.state, event.config),
        energy_after
    );
}

} // namespace

int main() {
    // The range, with +y up: dirt ground, a 2 cm wooden board at 15 m, a steel plate at 30 m,
    // and a concrete wall at 40 m.
    PrimitiveWorld range;
    if (range.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, dirt) ||
        range.add(AABB{.min = {15.0, 0.0, -1.0}, .max = {15.02, 2.0, 1.0}}, wood) ||
        range.add(AABB{.min = {30.0, 0.0, 2.0}, .max = {30.05, 3.0, 30.0}}, steel) ||
        range.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -40.0}, concrete)) {
        std::fprintf(stderr, "invalid shape\n");
        return 1;
    }
    const MaterialTable materials = range_materials();
    // A bullet slower than 20 m/s after an impact is spent: it stops there.
    const Result<StandardInteraction> interaction = StandardInteraction::create(20.0);
    if (!interaction) {
        return 1;
    }

    Simulation simulation;
    simulation.set_collision_world(&range);
    simulation.set_materials(&materials);
    simulation.set_interaction(&*interaction);

    // An 8 g bullet with drag, in a 2 m/s crosswind.
    ProjectileConfig bullet;
    bullet.mass = 0.008;
    bullet.radius = 0.0045;
    bullet.gravity = {0.0, -standard_gravity, 0.0};
    bullet.drag_coefficient = 0.3;
    bullet.reference_area = std::numbers::pi * bullet.radius * bullet.radius;
    bullet.wind = {0.0, 0.0, 2.0};
    bullet.lifetime = 2.0;

    const double angle = degrees_to_radians(15.0);
    const std::array shots{
        Shot{"through the board", {0.0, 1.0, 0.0}, {800.0, 0.0, 0.0}},
        Shot{"slow, at the board", {0.0, 1.0, 0.5}, {100.0, 1.0, 0.0}},
        Shot{"steel, head-on", {0.0, 1.5, 10.0}, {800.0, 0.0, 0.0}},
        Shot{
            "steel, glancing", {25.0, 1.5, 3.0}, Vec3{std::sin(angle), 0.0, std::cos(angle)} * 800.0
        },
        Shot{"skipping off dirt", {0.0, 0.5, -5.0}, Vec3{1.0, -0.05, 0.0} * 300.0},
    };
    std::array<ProjectileId, shots.size()> ids{};
    for (std::size_t i = 0; i < shots.size(); ++i) {
        const Result<ProjectileState> state =
            create_projectile(bullet, shots.at(i).origin, shots.at(i).velocity);
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

    simulation.set_event_callback([&ids, &shots](const ProjectileEvent& event) {
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids.at(i) == event.id) {
                print_event(event, shots.at(i).name);
            }
        }
    });

    while (!simulation.projectiles().empty()) {
        simulation.update(1.0 / 60.0);
    }
}
