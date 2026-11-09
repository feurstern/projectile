// raylib demo: material interaction. Bullets meet glass, wood, a slanted steel plate and
// concrete, and ricochet, pass through or stop, as the standard interaction model decides.
//
// Each target is in its own lane, in front of an earth berm that stops whatever reaches it.
// Trails change colour at each impact: yellow in flight, orange after a ricochet, cyan after a
// penetration. Each impact is marked with the energy before and after it, flying bullets show
// the energy they have left, and the selected material's properties can be changed while the
// demo runs. Materials are gameplay settings, not measurements.
//
// The scripted run (--frames N, see common/demo.hpp) shoots each lane, then raises the wood's
// penetration cost and shoots it again. It checks that bullets ricocheted, penetrated and
// stopped, and that the last one stopped in the wood it went through before.

#include <projectile/projectile.hpp>
#include <projectile/raylib.hpp>

#include "common/demo.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace adapter = projectile::raylib;
using projectile::AABB;
using projectile::ProjectileEventKind;
using projectile::Vec3;

// Material ids.
constexpr std::uint32_t dirt = 1;
constexpr std::uint32_t glass = 2;
constexpr std::uint32_t wood = 3;
constexpr std::uint32_t steel = 4;
constexpr std::uint32_t concrete = 5;

constexpr double infinite_cost = std::numeric_limits<double>::infinity();

/// A lane: its target's material, where it runs, and how to draw it.
struct Lane {
    std::string_view name;
    std::uint32_t material;
    double z; ///< The lane's centre line.
    Color color;
};

constexpr std::array<Lane, 4> lanes{{
    {.name = "glass", .material = glass, .z = -6.0, .color = {150, 210, 255, 110}},
    {.name = "wood", .material = wood, .z = -2.0, .color = {176, 128, 82, 255}},
    {.name = "steel", .material = steel, .z = 2.0, .color = {150, 170, 190, 255}},
    {.name = "concrete", .material = concrete, .z = 6.0, .color = {130, 130, 136, 255}},
}};

std::string_view name_of(std::uint32_t material) {
    for (const Lane& lane : lanes) {
        if (lane.material == material) {
            return lane.name;
        }
    }
    return "the berm";
}

/// The targets: a 2 cm glass pane, a 5 cm board, a steel plate sloping up at 15 degrees, and a
/// concrete block. The earth berm behind them stops everything.
constexpr AABB glass_pane{.min = {12.0, 0.0, -7.5}, .max = {12.02, 3.0, -4.5}};
constexpr AABB wood_board{.min = {20.0, 0.0, -3.5}, .max = {20.05, 3.0, -0.5}};
constexpr AABB concrete_block{.min = {28.0, 0.0, 4.5}, .max = {29.0, 3.0, 7.5}};
constexpr AABB berm{.min = {40.0, 0.0, -9.0}, .max = {41.5, 5.0, 9.0}};
const double plate_rise = 8.0 * std::tan(projectile::degrees_to_radians(15.0));
const std::array<Vec3, 4> steel_plate{{
    {26.0, 0.0, 0.5},
    {26.0, 0.0, 3.5},
    {34.0, plate_rise, 3.5},
    {34.0, plate_rise, 0.5},
}};

/// The bullets: 10 g, from a gun 1.5 m above the ground.
constexpr double bullet_mass = 0.01;
constexpr double gun_height = 1.5;

/// Penetration costs that the , and . keys step through, in J.
constexpr std::array<double, 9> cost_steps{
    0.0, 10.0, 25.0, 50.0, 100.0, 200.0, 400.0, 800.0, infinite_cost
};

projectile::MaterialTable range_materials() {
    projectile::Material ground;
    ground.id = dirt;
    ground.ricochet_angle = projectile::degrees_to_radians(5.0);
    ground.restitution = 0.1;
    ground.friction = 0.6;

    projectile::Material pane;
    pane.id = glass;
    pane.penetration_cost = 10.0;
    pane.resistance = 1'000.0; // a 2 cm pane costs 30 J in all
    pane.ricochet_angle = projectile::degrees_to_radians(5.0);
    pane.restitution = 0.2;
    pane.friction = 0.3;

    projectile::Material board;
    board.id = wood;
    board.penetration_cost = 50.0;
    board.resistance = 2'000.0; // a 5 cm board costs 150 J in all
    board.ricochet_angle = projectile::degrees_to_radians(10.0);
    board.restitution = 0.3;
    board.friction = 0.4;

    projectile::Material plate; // impenetrable, as a default Material is
    plate.id = steel;
    plate.ricochet_angle = projectile::degrees_to_radians(25.0);
    plate.restitution = 0.6;
    plate.friction = 0.2;

    projectile::Material block;
    block.id = concrete;
    block.ricochet_angle = projectile::degrees_to_radians(15.0);
    block.restitution = 0.3;
    block.friction = 0.5;

    projectile::MaterialTable table;
    for (const projectile::Material& material : {ground, pane, board, plate, block}) {
        static_cast<void>(table.add(material));
    }
    return table;
}

/// What the keyboard, or the script, asks for in one frame.
struct Commands {
    bool fire = false;
    bool clear = false;
    std::optional<std::size_t> lane;
    double elevation_rate = 0.0;     ///< degrees per s
    double speed_rate = 0.0;         ///< m/s per s
    int ricochet_steps = 0;          ///< 5 degrees each
    int restitution_steps = 0;       ///< 0.1 each
    int cost_steps = 0;              ///< along cost_steps
    std::optional<double> elevation; ///< degrees (scripts only)
    std::optional<double> speed;     ///< m/s (scripts only)
};

int key_steps(int down, int up) {
    return (IsKeyPressed(up) ? 1 : 0) - (IsKeyPressed(down) ? 1 : 0);
}

Commands read_input() {
    Commands commands;
    commands.fire = IsKeyPressed(KEY_SPACE);
    commands.clear = IsKeyPressed(KEY_C);
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        if (IsKeyPressed(KEY_ONE + static_cast<int>(i))) {
            commands.lane = i;
        }
    }
    commands.elevation_rate = (IsKeyDown(KEY_UP) ? 4.0 : 0.0) - (IsKeyDown(KEY_DOWN) ? 4.0 : 0.0);
    commands.speed_rate =
        (IsKeyDown(KEY_RIGHT) ? 100.0 : 0.0) - (IsKeyDown(KEY_LEFT) ? 100.0 : 0.0);
    commands.ricochet_steps = key_steps(KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET);
    commands.restitution_steps = key_steps(KEY_MINUS, KEY_EQUAL);
    commands.cost_steps = key_steps(KEY_COMMA, KEY_PERIOD);
    return commands;
}

/// The scripted sequence, 150 frames at 25 per second: one shot down each lane, then the wood
/// made three steps harder to penetrate and shot again.
Commands scripted(int frame) {
    Commands commands;
    switch (frame) {
    case 0:
        commands.clear = true;
        commands.lane = 0;
        commands.elevation = 0.0;
        commands.speed = 300.0;
        break;
    case 28:
        commands.lane = 1;
        break;
    case 56:
        commands.lane = 2;
        commands.elevation = -3.0;
        break;
    case 84:
        commands.lane = 3;
        commands.elevation = 0.0;
        break;
    case 106:
        commands.lane = 1;
        break;
    case 108:
    case 110:
    case 112:
        commands.cost_steps = 1;
        break;
    case 8:
    case 36:
    case 64:
    case 92:
    case 118:
        commands.fire = true;
        break;
    default:
        break;
    }
    return commands;
}

/// A part of a bullet's trail between two impacts, in one colour.
struct TrailPart {
    Color color;
    std::vector<Vec3> points;
};

struct Bullet {
    projectile::ProjectileId id;
    std::vector<TrailPart> trail;
};

struct Impact {
    Vec3 point;
    ProjectileEventKind kind = ProjectileEventKind::hit;
    std::uint32_t material = 0;
    double energy_before = 0.0; ///< J
    double energy_after = 0.0;  ///< J
};

constexpr Color flight_color{255, 214, 90, 255};
constexpr Color ricochet_color{255, 140, 60, 255};
constexpr Color penetration_color{90, 220, 255, 255};
constexpr Color stop_color{255, 80, 80, 255};

Color color_of(ProjectileEventKind kind) {
    switch (kind) {
    case ProjectileEventKind::ricochet:
        return ricochet_color;
    case ProjectileEventKind::penetration:
        return penetration_color;
    case ProjectileEventKind::hit:
    case ProjectileEventKind::expired:
        break;
    }
    return stop_color;
}

std::string describe(const Impact& impact) {
    const std::string material(name_of(impact.material));
    const std::string before = demo::fixed(impact.energy_before, 0);
    switch (impact.kind) {
    case ProjectileEventKind::ricochet:
        return material + ": ricocheted, " + before + " -> " + demo::fixed(impact.energy_after, 0) +
               " J";
    case ProjectileEventKind::penetration:
        return material + ": went through, " + before + " -> " +
               demo::fixed(impact.energy_after, 0) + " J";
    case ProjectileEventKind::hit:
    case ProjectileEventKind::expired:
        break;
    }
    return material + ": stopped, " + before + " J absorbed";
}

class MaterialRange {
public:
    MaterialRange() : materials_(range_materials()) {
        const projectile::Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
        bool invalid = static_cast<bool>(world_.add(ground, dirt));
        invalid = static_cast<bool>(world_.add(glass_pane, glass)) || invalid;
        invalid = static_cast<bool>(world_.add(wood_board, wood)) || invalid;
        invalid = static_cast<bool>(world_.add(concrete_block, concrete)) || invalid;
        invalid = static_cast<bool>(world_.add(berm, dirt)) || invalid;
        const auto& p = steel_plate;
        invalid =
            static_cast<bool>(world_.add(projectile::Triangle{p.at(0), p.at(1), p.at(2)}, steel)) ||
            invalid;
        invalid =
            static_cast<bool>(world_.add(projectile::Triangle{p.at(0), p.at(2), p.at(3)}, steel)) ||
            invalid;
        // A bullet slower than 20 m/s after an impact is spent.
        const projectile::Result<projectile::StandardInteraction> interaction =
            projectile::StandardInteraction::create(20.0);
        valid_ = !invalid && interaction.has_value() && materials_.size() == 5;
        if (interaction) {
            interaction_ = *interaction;
        }
        simulation_.set_collision_world(&world_);
        simulation_.set_materials(&materials_);
        simulation_.set_interaction(&interaction_);
        simulation_.set_event_callback([this](const projectile::ProjectileEvent& event) {
            on_event(event);
        });
    }

    // The simulation refers to this object's members.
    MaterialRange(const MaterialRange&) = delete;
    MaterialRange(MaterialRange&&) = delete;
    MaterialRange& operator=(const MaterialRange&) = delete;
    MaterialRange& operator=(MaterialRange&&) = delete;
    ~MaterialRange() = default;

    void apply(const Commands& commands, double dt) {
        if (commands.clear) {
            simulation_.clear();
            bullets_.clear();
            impacts_.clear();
            materials_ = range_materials();
        }
        lane_ = commands.lane.value_or(lane_);
        elevation_ = std::clamp(
            commands.elevation.value_or(elevation_ + commands.elevation_rate * dt), -10.0, 10.0
        );
        speed_ =
            std::clamp(commands.speed.value_or(speed_ + commands.speed_rate * dt), 50.0, 600.0);
        edit_material(commands);
        if (commands.fire) {
            fire();
        }
    }

    void advance(double dt) {
        simulation_.update(dt);
        const double alpha = simulation_.interpolation_alpha();
        for (const projectile::SimulatedProjectile& projectile : simulation_.projectiles()) {
            if (Bullet* bullet = find(projectile.id)) {
                bullet->trail.back().points.push_back(
                    projectile::interpolated_position(projectile, alpha)
                );
            }
        }
    }

    void draw_scene(const Camera3D& camera) const {
        BeginMode3D(camera);
        adapter::draw_plane(
            projectile::Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0},
            45.0,
            demo::ground_color,
            Vec3{20.0, 0.0, 0.0}
        );
        for (const double z : {-8.0, -4.0, 0.0, 4.0, 8.0}) {
            DrawLine3D(
                {-2.0F, 0.01F, static_cast<float>(z)},
                {40.0F, 0.01F, static_cast<float>(z)},
                demo::grid_color
            );
        }
        draw_targets();
        draw_gun();
        for (const Bullet& bullet : bullets_) {
            for (const TrailPart& part : bullet.trail) {
                adapter::draw_path(part.points, part.color, 0.07);
            }
        }
        for (const Impact& impact : impacts_) {
            DrawSphere(adapter::to_vector3(impact.point), 0.18F, color_of(impact.kind));
        }
        adapter::draw_projectiles(simulation_, RAYWHITE, 0.15);
        EndMode3D();

        for (const Lane& lane : lanes) {
            demo::draw_label(
                camera, Vec3{0.0, 0.0, lane.z}, std::string(lane.name), demo::dim_text_color
            );
        }
        draw_energy_labels(camera);
    }

    void draw_panels(const demo::Options& options, bool autoplay) const {
        demo::Panel gun(18);
        gun.add("Material interaction", demo::accent_color);
        gun.add(
            "Gun: elevation " + demo::fixed(elevation_, 1) + " deg, " + demo::fixed(speed_, 0) +
            " m/s, " +
            demo::fixed(projectile::kinetic_energy(bullet_mass, Vec3{speed_, 0.0, 0.0}), 0) + " J"
        );
        for (std::size_t i = 0; i < lanes.size(); ++i) {
            const projectile::Material& material = materials_.material_for(lanes.at(i).material);
            gun.add(
                std::string(i == lane_ ? "> " : "  ") + std::to_string(i + 1) + " " +
                    properties_of(material),
                i == lane_ ? demo::accent_color : demo::text_color
            );
        }
        gun.draw(12, 12);

        demo::Panel log(18);
        log.add("Impacts", demo::dim_text_color);
        const std::size_t first = impacts_.size() > 6 ? impacts_.size() - 6 : 0;
        for (std::size_t i = first; i < impacts_.size(); ++i) {
            log.add(describe(impacts_.at(i)), color_of(impacts_.at(i).kind));
        }
        log.draw(-12, 12);

        demo::Panel controls(16);
        controls.add(
            "Space fire   Up/Down elevation   Left/Right speed   1-4 lane and material   C clear",
            demo::dim_text_color
        );
        controls.add(
            "Selected material:  [ ] ricochet angle   - = restitution   , . penetration cost",
            demo::dim_text_color
        );
        controls.add(demo::autoplay_hint(options, autoplay), demo::dim_text_color);
        controls.draw(12, -12);
    }

    /// Checks the scripted run: bullets ricocheted, penetrated and stopped, and the last one,
    /// fired after the wood was made harder to penetrate, stopped in the wood.
    [[nodiscard]] int check() const {
        const bool last_stopped_in_wood = !impacts_.empty() &&
                                          impacts_.back().kind == ProjectileEventKind::hit &&
                                          impacts_.back().material == wood;
        const bool passed =
            valid_ && ricochets_ > 0 && penetrations_ > 0 && stops_ > 0 && last_stopped_in_wood;
        return demo::report(
            passed,
            "material_interaction: " + std::to_string(ricochets_) + " ricochets, " +
                std::to_string(penetrations_) + " penetrations, " + std::to_string(stops_) +
                " stops; " +
                (last_stopped_in_wood ? "the last bullet stopped in the stiffened wood"
                                      : "the last bullet did not stop in the wood")
        );
    }

private:
    /// A material's properties in one line.
    static std::string properties_of(const projectile::Material& material) {
        const std::string cost = std::isinf(material.penetration_cost)
                                     ? std::string("impenetrable")
                                     : "cost " + demo::fixed(material.penetration_cost, 0) +
                                           " J + " + demo::fixed(material.resistance, 0) + " J/m";
        return std::string(name_of(material.id)) + ": " + cost + ", ricochet <= " +
               demo::fixed(projectile::radians_to_degrees(material.ricochet_angle), 0) +
               " deg, restitution " + demo::fixed(material.restitution, 1);
    }

    void edit_material(const Commands& commands) {
        projectile::Material material = materials_.material_for(lanes.at(lane_).material);
        const double step = projectile::degrees_to_radians(5.0);
        material.ricochet_angle = std::clamp(
            material.ricochet_angle + commands.ricochet_steps * step, 0.0, std::numbers::pi / 2.0
        );
        material.restitution =
            std::clamp(material.restitution + 0.1 * commands.restitution_steps, 0.0, 1.0);
        if (commands.cost_steps != 0) {
            const std::ptrdiff_t current =
                std::ranges::lower_bound(cost_steps, material.penetration_cost) -
                cost_steps.begin();
            const auto index = std::clamp<std::ptrdiff_t>(
                current + commands.cost_steps, 0, static_cast<std::ptrdiff_t>(cost_steps.size()) - 1
            );
            material.penetration_cost = cost_steps.at(static_cast<std::size_t>(index));
        }
        static_cast<void>(materials_.add(material));
    }

    void fire() {
        const double elevation = projectile::degrees_to_radians(elevation_);
        projectile::ProjectileConfig config;
        config.mass = bullet_mass;
        config.radius = 0.005;
        config.gravity = {0.0, -9.81, 0.0};
        config.lifetime = 3.0;
        const Vec3 muzzle{0.6, gun_height, lanes.at(lane_).z};
        const Vec3 velocity{speed_ * std::cos(elevation), speed_ * std::sin(elevation), 0.0};
        const projectile::Result<projectile::ProjectileState> start =
            projectile::create_projectile(config, muzzle, velocity);
        if (!start) {
            return;
        }
        if (const projectile::Result<projectile::ProjectileId> id =
                simulation_.add_projectile(*start, config)) {
            bullets_.push_back(
                Bullet{.id = *id, .trail = {TrailPart{.color = flight_color, .points = {muzzle}}}}
            );
            if (bullets_.size() > 12) {
                bullets_.pop_front();
            }
        }
    }

    void on_event(const projectile::ProjectileEvent& event) {
        Bullet* bullet = find(event.id);
        if (event.kind == ProjectileEventKind::expired || bullet == nullptr || !event.hit) {
            return;
        }
        const Impact impact{
            .point = event.state.position,
            .kind = event.kind,
            .material = event.hit->material_id,
            .energy_before = projectile::kinetic_energy(event.state, event.config),
            .energy_after = event.interaction ? event.interaction->energy_remaining : 0.0,
        };
        impacts_.push_back(impact);
        if (impacts_.size() > 40) {
            impacts_.pop_front();
        }
        bullet->trail.back().points.push_back(impact.point);
        switch (event.kind) {
        case ProjectileEventKind::ricochet:
            ++ricochets_;
            bullet->trail.push_back(TrailPart{.color = ricochet_color, .points = {impact.point}});
            break;
        case ProjectileEventKind::penetration:
            ++penetrations_;
            bullet->trail.push_back(
                TrailPart{.color = penetration_color, .points = {impact.point}}
            );
            break;
        case ProjectileEventKind::hit:
        case ProjectileEventKind::expired:
            ++stops_;
            break;
        }
    }

    [[nodiscard]] Bullet* find(projectile::ProjectileId id) {
        const auto found = std::ranges::find(bullets_, id, &Bullet::id);
        return found == bullets_.end() ? nullptr : &*found;
    }

    static void draw_targets() {
        adapter::draw_box(glass_pane, lanes.at(0).color);
        adapter::draw_box_wires(glass_pane, Fade(SKYBLUE, 0.8F));
        adapter::draw_box(wood_board, lanes.at(1).color);
        const auto& p = steel_plate;
        adapter::draw_triangle(projectile::Triangle{p.at(0), p.at(1), p.at(2)}, lanes.at(2).color);
        adapter::draw_triangle(projectile::Triangle{p.at(0), p.at(2), p.at(3)}, lanes.at(2).color);
        adapter::draw_box(concrete_block, lanes.at(3).color);
        adapter::draw_box_wires(concrete_block, Fade(BLACK, 0.4F));
        adapter::draw_box(berm, Color{96, 78, 58, 255});
    }

    void draw_gun() const {
        const double elevation = projectile::degrees_to_radians(elevation_);
        const Vec3 breech{-0.4, gun_height, lanes.at(lane_).z};
        const Vec3 muzzle = breech + Vec3{std::cos(elevation), std::sin(elevation), 0.0};
        DrawCylinderEx(
            adapter::to_vector3(breech), adapter::to_vector3(muzzle), 0.09F, 0.06F, 10, DARKGRAY
        );
        DrawCube(adapter::to_vector3(breech + Vec3{0.0, -0.75, 0.0}), 0.2F, 1.5F, 0.2F, GRAY);
    }

    void draw_energy_labels(const Camera3D& camera) const {
        const double alpha = simulation_.interpolation_alpha();
        for (const projectile::SimulatedProjectile& projectile : simulation_.projectiles()) {
            const double energy = projectile::kinetic_energy(projectile.state, projectile.config);
            demo::draw_label(
                camera,
                projectile::interpolated_position(projectile, alpha),
                demo::fixed(energy, 0) + " J",
                RAYWHITE
            );
        }
        const std::size_t first = impacts_.size() > 5 ? impacts_.size() - 5 : 0;
        for (std::size_t i = first; i < impacts_.size(); ++i) {
            const Impact& impact = impacts_.at(i);
            // Where impacts coincide, label only the latest.
            const auto later = impacts_.begin() + static_cast<std::ptrdiff_t>(i) + 1;
            if (std::any_of(later, impacts_.end(), [&](const Impact& other) {
                    return projectile::distance(other.point, impact.point) < 0.5;
                })) {
                continue;
            }
            const std::string text = impact.kind == ProjectileEventKind::hit
                                         ? demo::fixed(impact.energy_before, 0) + " J absorbed"
                                         : demo::fixed(impact.energy_before, 0) + " -> " +
                                               demo::fixed(impact.energy_after, 0) + " J";
            demo::draw_label(
                camera, impact.point + Vec3{0.0, 0.4, 0.0}, text, color_of(impact.kind), 16
            );
        }
    }

    projectile::PrimitiveWorld world_;
    projectile::MaterialTable materials_;
    projectile::StandardInteraction interaction_;
    projectile::Simulation simulation_;
    std::deque<Bullet> bullets_;
    std::deque<Impact> impacts_;
    std::size_t lane_ = 0;
    double elevation_ = 0.0; ///< degrees
    double speed_ = 300.0;   ///< m/s
    int ricochets_ = 0;
    int penetrations_ = 0;
    int stops_ = 0;
    bool valid_ = true;
};

} // namespace

int main(int argc, char** argv) {
    const std::optional<demo::Options> options = demo::parse_options(argc, argv);
    if (!options) {
        return 2;
    }
    if (!demo::open_window(*options, "Projectile demo: material interaction")) {
        return demo::exit_no_window;
    }

    MaterialRange scene;
    demo::OrbitCamera camera{
        .target = {18.0, 1.0, 1.5}, .distance = 37.0, .yaw = -0.8, .pitch = 0.5
    };
    bool autoplay = options->scripted();
    int script_frame = 0;
    for (int frame = 0; !demo::finished(*options, frame); ++frame) {
        const double dt = demo::frame_time(*options);
        if (!options->scripted()) {
            if (IsKeyPressed(KEY_TAB)) {
                autoplay = !autoplay;
                script_frame = 0;
            }
            camera.update_from_mouse();
        }
        // Interactive autoplay repeats the script; a scripted run plays it once.
        const int step = options->scripted() ? script_frame : script_frame % 150;
        scene.apply(autoplay ? scripted(step) : read_input(), dt);
        scene.advance(dt);
        ++script_frame;

        BeginDrawing();
        ClearBackground(demo::background);
        scene.draw_scene(camera.camera());
        scene.draw_panels(*options, autoplay);
        demo::record_frame(*options, frame);
        EndDrawing();
    }
    CloseWindow();
    return options->scripted() ? scene.check() : 0;
}
