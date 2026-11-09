// raylib demo: ballistic drop. Balls fired horizontally from a tower fall under gravity.
//
// Each shot leaves a trail. The path of the next shot is predicted with predict_trajectory()
// and drawn as a dashed line before it is fired, and a ring marks where the prediction says it
// will land. The panel shows the latest shot's position and velocity. Gravity can be switched
// between Earth, the Moon and Mars, or turned off, and air drag turned on and off.
//
// The scripted run (--frames N, see common/demo.hpp) fires four shots under different gravity
// and checks that each one landed where its prediction said, after the predicted time.

#include <projectile/projectile.hpp>
#include <projectile/raylib.hpp>

#include "common/demo.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace adapter = projectile::raylib;
using projectile::Vec3;

/// The tower is 9.6 m tall; the gun on top fires along +x from 10 m.
constexpr projectile::AABB tower{.min = {-1.0, 0.0, -1.0}, .max = {1.0, 9.6, 1.0}};
constexpr Vec3 muzzle{0.0, 10.0, 0.0};
constexpr Vec3 gravity_arrow_base{-1.6, 10.0, 1.8};

/// The balls: 1 kg, 10 cm in radius, and, with drag on, the drag coefficient of a sphere.
constexpr double ball_mass = 1.0;
constexpr double ball_radius = 0.1;
constexpr double sphere_drag_coefficient = 0.47;

struct Planet {
    std::string_view name;
    double gravity; ///< m/s^2
};

constexpr std::array<Planet, 4> planets{{
    {.name = "Earth", .gravity = 9.81},
    {.name = "Moon", .gravity = 1.62},
    {.name = "Mars", .gravity = 3.71},
    {.name = "zero-g", .gravity = 0.0},
}};

/// Colours of successive shots' trails.
constexpr std::array<Color, 4> trail_colors{{
    {255, 196, 64, 255},
    {102, 204, 255, 255},
    {255, 110, 110, 255},
    {140, 230, 140, 255},
}};

/// What the keyboard, or the script, asks for in one frame.
struct Commands {
    bool fire = false;
    bool clear = false;
    bool toggle_drag = false;
    double speed_rate = 0.0;           ///< Change of the muzzle speed, in m/s per s.
    double gravity_rate = 0.0;         ///< Change of gravity, in m/s^2 per s.
    std::optional<std::size_t> planet; ///< Switch to this planet's gravity.
    std::optional<double> speed;       ///< Set the muzzle speed (scripts only).
    std::optional<bool> drag;          ///< Turn drag on or off (scripts only).
};

Commands read_input() {
    Commands commands;
    commands.fire = IsKeyPressed(KEY_SPACE);
    commands.clear = IsKeyPressed(KEY_C);
    commands.toggle_drag = IsKeyPressed(KEY_D);
    commands.speed_rate = (IsKeyDown(KEY_UP) ? 10.0 : 0.0) - (IsKeyDown(KEY_DOWN) ? 10.0 : 0.0);
    commands.gravity_rate = (IsKeyDown(KEY_RIGHT) ? 4.0 : 0.0) - (IsKeyDown(KEY_LEFT) ? 4.0 : 0.0);
    for (std::size_t i = 0; i < planets.size(); ++i) {
        if (IsKeyPressed(KEY_ONE + static_cast<int>(i))) {
            commands.planet = i;
        }
    }
    return commands;
}

/// The scripted sequence, 150 frames at 25 per second: shots on Earth, the Moon and Mars, then a
/// faster one on Earth with air drag. A ball falls 10 m in 1.4 s on Earth, 2.3 s on Mars and
/// 3.5 s on the Moon, whatever its speed.
Commands scripted(int frame) {
    Commands commands;
    switch (frame) {
    case 0:
        commands.clear = true;
        commands.planet = 0;
        commands.speed = 20.0;
        commands.drag = false;
        break;
    case 20:
        commands.planet = 1;
        commands.speed = 12.0;
        break;
    case 50:
        commands.planet = 2;
        commands.speed = 20.0;
        break;
    case 80:
        commands.planet = 0;
        commands.speed = 30.0;
        commands.drag = true;
        break;
    case 5:
    case 25:
    case 55:
    case 85:
        commands.fire = true;
        break;
    default:
        break;
    }
    return commands;
}

/// One shot and what became of it.
struct Shot {
    projectile::ProjectileId id;
    std::string_view planet;
    double launch_time = 0.0;      ///< Simulation time at which it was fired, in s.
    std::vector<Vec3> trail;       ///< Its positions, one per frame.
    std::optional<Vec3> predicted; ///< Where predict_trajectory() said it would land.
    double predicted_flight = 0.0; ///< How long the prediction said it would fly, in s.
    std::optional<Vec3> landed;    ///< Where it landed.
    double flight = 0.0;           ///< How long it flew, in s.
};

class BallisticDrop {
public:
    BallisticDrop() {
        const projectile::Plane ground{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
        if (world_.add(ground) || world_.add(tower)) {
            valid_ = false;
        }
        simulation_.set_collision_world(&world_);
        simulation_.set_event_callback([this](const projectile::ProjectileEvent& event) {
            on_event(event);
        });
    }

    // The event callback refers to this object.
    BallisticDrop(const BallisticDrop&) = delete;
    BallisticDrop(BallisticDrop&&) = delete;
    BallisticDrop& operator=(const BallisticDrop&) = delete;
    BallisticDrop& operator=(BallisticDrop&&) = delete;
    ~BallisticDrop() = default;

    [[nodiscard]] bool valid() const { return valid_; }

    /// Carries out @p commands, which cover @p dt seconds.
    void apply(const Commands& commands, double dt) {
        if (commands.clear) {
            simulation_.clear();
            shots_.clear();
        }
        if (commands.planet) {
            planet_ = *commands.planet;
            gravity_ = planets.at(planet_).gravity;
        }
        speed_ = std::clamp(commands.speed.value_or(speed_ + commands.speed_rate * dt), 2.0, 60.0);
        if (commands.gravity_rate != 0.0) {
            gravity_ = std::clamp(gravity_ + commands.gravity_rate * dt, 0.0, 30.0);
        }
        drag_ = commands.drag.value_or(commands.toggle_drag ? !drag_ : drag_);
        preview_ = predict(next_shot());
        if (commands.fire) {
            fire();
        }
    }

    /// Advances the simulation by @p dt seconds and extends the trails.
    void advance(double dt) {
        simulation_.update(dt);
        const double alpha = simulation_.interpolation_alpha();
        for (const projectile::SimulatedProjectile& projectile : simulation_.projectiles()) {
            if (Shot* shot = find(projectile.id)) {
                shot->trail.push_back(projectile::interpolated_position(projectile, alpha));
            }
        }
    }

    void draw_scene(const Camera3D& camera) const {
        BeginMode3D(camera);
        adapter::draw_plane(ground_plane(), 110.0, demo::ground_color, Vec3{40.0, 0.0, 0.0});
        for (int x = 0; x <= 100; x += 10) {
            const auto line_x = static_cast<float>(x);
            DrawLine3D({line_x, 0.01F, -8.0F}, {line_x, 0.01F, 8.0F}, demo::grid_color);
        }
        adapter::draw_box(tower, Color{96, 104, 120, 255});
        adapter::draw_box_wires(tower, Color{140, 150, 170, 255});
        DrawCylinderEx({-0.6F, 10.0F, 0.0F}, {0.5F, 10.0F, 0.0F}, 0.18F, 0.15F, 12, DARKGRAY);
        // Gravity, drawn 0.4 m long per m/s^2, in front of the tower.
        adapter::draw_arrow(
            gravity_arrow_base, gravity_arrow_base + Vec3{0.0, -0.4 * gravity_, 0.0}, 0.08, RED
        );
        draw_preview();
        draw_shots();
        adapter::draw_projectiles(simulation_, RAYWHITE, 0.35);
        EndMode3D();

        for (int x = 10; x <= 90; x += 10) {
            demo::draw_label(
                camera,
                Vec3{static_cast<double>(x), 0.0, 0.0},
                std::to_string(x) + " m",
                demo::dim_text_color
            );
        }
        demo::draw_label(
            camera, gravity_arrow_base + Vec3{0.0, -0.2 * gravity_, 0.0}, "g", RED, 22
        );
    }

    void draw_panels(const demo::Options& options, bool autoplay) const {
        demo::Panel status;
        status.add("Ballistic drop", demo::accent_color);
        status.add(
            "Gravity: " + demo::fixed(gravity_, 2) + " m/s^2 (" + std::string(planet_name()) + ")"
        );
        status.add("Muzzle speed: " + demo::fixed(speed_, 1) + " m/s");
        status.add(std::string("Air drag: ") + (drag_ ? "on (sphere, Cd 0.47)" : "off"));
        add_latest_shot(status);
        status.draw(-12, 12);

        demo::Panel controls(18);
        controls.add(
            "Space fire   Up/Down muzzle speed   Left/Right gravity", demo::dim_text_color
        );
        controls.add("1-4 Earth/Moon/Mars/zero-g   D air drag   C clear", demo::dim_text_color);
        controls.add("Right-drag orbit   Wheel zoom", demo::dim_text_color);
        controls.add(demo::autoplay_hint(options, autoplay), demo::dim_text_color);
        controls.draw(12, -12);
    }

    /// Checks the scripted run: every shot landed where, and when, its prediction said, and
    /// the lower the gravity, the longer the flight.
    [[nodiscard]] int check() const {
        bool passed = valid_ && shots_.size() == 4;
        std::string summary = "ballistic_drop: " + std::to_string(shots_.size()) + " shots";
        for (const Shot& shot : shots_) {
            summary += "\n  " + std::string(shot.planet) + ": ";
            if (!shot.landed || !shot.predicted) {
                passed = false;
                summary += "did not land";
                continue;
            }
            const double miss = projectile::distance(*shot.landed, *shot.predicted);
            passed = passed && miss < 1e-6 && std::abs(shot.flight - shot.predicted_flight) < 1e-6;
            summary += "landed at x = " + demo::fixed(shot.landed->x, 3) + " m after " +
                       demo::fixed(shot.flight, 3) + " s, " + demo::fixed(miss, 9) +
                       " m from the prediction";
        }
        // Shots 1 to 3 fell from the same height on Earth, the Moon and Mars.
        passed = passed && shots_.at(1).flight > shots_.at(2).flight &&
                 shots_.at(2).flight > shots_.at(0).flight;
        return demo::report(passed, summary);
    }

private:
    [[nodiscard]] static projectile::Plane ground_plane() {
        return projectile::Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
    }

    /// The configuration of the next shot.
    [[nodiscard]] projectile::ProjectileConfig next_shot() const {
        projectile::ProjectileConfig shot;
        shot.mass = ball_mass;
        shot.radius = ball_radius;
        shot.gravity = {0.0, -gravity_, 0.0};
        shot.initial_velocity = {speed_, 0.0, 0.0};
        shot.lifetime = 20.0; // shots in zero-g fly on until they expire
        if (drag_) {
            shot.drag_coefficient = sphere_drag_coefficient;
            shot.reference_area = std::numbers::pi * ball_radius * ball_radius;
        }
        return shot;
    }

    /// The predicted flight of a shot with @p shot, stepped exactly as the simulation steps.
    [[nodiscard]] projectile::Trajectory predict(const projectile::ProjectileConfig& shot) const {
        projectile::TrajectoryConfig trajectory_config;
        trajectory_config.sample_interval = 1.0 / 30.0;
        trajectory_config.max_duration = shot.lifetime;
        trajectory_config.time_step = simulation_.config().time_step;
        trajectory_config.integrator = simulation_.config().integrator;
        const projectile::Result<projectile::ProjectileState> start =
            projectile::create_projectile(shot, muzzle);
        if (!start) {
            return {};
        }
        projectile::Result<projectile::Trajectory> trajectory =
            projectile::predict_trajectory(*start, shot, trajectory_config, world_);
        return trajectory ? std::move(*trajectory) : projectile::Trajectory{};
    }

    void fire() {
        const projectile::ProjectileConfig shot_config = next_shot();
        const projectile::Result<projectile::ProjectileState> start =
            projectile::create_projectile(shot_config, muzzle);
        if (!start) {
            return;
        }
        const projectile::Result<projectile::ProjectileId> id =
            simulation_.add_projectile(*start, shot_config);
        if (!id) {
            return;
        }
        Shot shot;
        shot.id = *id;
        shot.planet = planet_name();
        shot.launch_time = simulation_.time();
        shot.trail.push_back(muzzle);
        if (preview_.end == projectile::TrajectoryEnd::hit && preview_.hit) {
            shot.predicted = preview_.hit->point;
            shot.predicted_flight = preview_.samples.back().time;
        }
        shots_.push_back(std::move(shot));
    }

    void on_event(const projectile::ProjectileEvent& event) {
        Shot* shot = find(event.id);
        if (shot == nullptr || event.kind != projectile::ProjectileEventKind::hit || !event.hit) {
            return;
        }
        shot->landed = event.hit->point;
        shot->flight = event.time - shot->launch_time;
        shot->trail.push_back(event.state.position);
    }

    [[nodiscard]] Shot* find(projectile::ProjectileId id) {
        const auto found =
            std::ranges::find_if(shots_, [id](const Shot& shot) { return shot.id == id; });
        return found == shots_.end() ? nullptr : &*found;
    }

    [[nodiscard]] std::string_view planet_name() const {
        return gravity_ == planets.at(planet_).gravity ? planets.at(planet_).name : "custom";
    }

    void draw_preview() const {
        // Dashed: every other segment of the prediction.
        const std::span<const projectile::TrajectorySample> samples = preview_.samples;
        for (std::size_t i = 0; i + 1 < samples.size(); i += 2) {
            adapter::draw_trajectory(samples.subspan(i, 2), demo::accent_color, 0.08);
        }
        if (preview_.hit) {
            // A disc on the ground where the prediction lands.
            DrawCylinder(
                adapter::to_vector3(preview_.hit->point),
                0.9F,
                0.9F,
                0.03F,
                24,
                Fade(demo::accent_color, 0.5F)
            );
        }
    }

    void draw_shots() const {
        std::size_t index = 0;
        for (const Shot& shot : shots_) {
            const Color color = trail_colors.at(index++ % trail_colors.size());
            adapter::draw_path(shot.trail, color, 0.18);
            if (shot.landed) {
                DrawSphere(adapter::to_vector3(*shot.landed), 0.3F, color);
            }
        }
        if (const std::optional<projectile::SimulatedProjectile> latest = latest_flying()) {
            const Vec3 position =
                projectile::interpolated_position(*latest, simulation_.interpolation_alpha());
            adapter::draw_arrow(position, position + 0.15 * latest->state.velocity, 0.06, SKYBLUE);
        }
    }

    /// The most recent shot still in the air, if any.
    [[nodiscard]] std::optional<projectile::SimulatedProjectile> latest_flying() const {
        const std::span<const projectile::SimulatedProjectile> flying = simulation_.projectiles();
        for (const Shot& shot : std::views::reverse(shots_)) {
            const auto found =
                std::ranges::find(flying, shot.id, &projectile::SimulatedProjectile::id);
            if (found != flying.end()) {
                return *found;
            }
        }
        return std::nullopt;
    }

    void add_latest_shot(demo::Panel& panel) const {
        if (const std::optional<projectile::SimulatedProjectile> latest = latest_flying()) {
            const projectile::ProjectileState& state = latest->state;
            panel.add("Latest shot, in flight:", demo::dim_text_color);
            panel.add("  position " + demo::format(state.position, 2) + " m");
            panel.add("  velocity " + demo::format(state.velocity, 2) + " m/s");
            panel.add("  speed " + demo::fixed(projectile::length(state.velocity), 2) + " m/s");
        }
        for (const Shot& shot : std::views::reverse(shots_)) {
            if (!shot.landed) {
                continue;
            }
            panel.add(
                "Last landing: x = " + demo::fixed(shot.landed->x, 2) + " m after " +
                    demo::fixed(shot.flight, 2) + " s",
                demo::dim_text_color
            );
            if (shot.predicted) {
                panel.add(
                    "  predicted: x = " + demo::fixed(shot.predicted->x, 2) + " m after " +
                        demo::fixed(shot.predicted_flight, 2) + " s",
                    demo::dim_text_color
                );
            }
            return;
        }
    }

    projectile::PrimitiveWorld world_;
    projectile::Simulation simulation_;
    std::vector<Shot> shots_;
    projectile::Trajectory preview_;
    std::size_t planet_ = 0;
    double gravity_ = planets.front().gravity;
    double speed_ = 20.0;
    bool drag_ = false;
    bool valid_ = true;
};

} // namespace

int main(int argc, char** argv) {
    const std::optional<demo::Options> options = demo::parse_options(argc, argv);
    if (!options) {
        return 2;
    }
    if (!demo::open_window(*options, "Projectile demo: ballistic drop")) {
        return demo::exit_no_window;
    }

    BallisticDrop scene;
    demo::OrbitCamera camera{
        .target = {23.0, 4.5, 0.0}, .distance = 42.0, .yaw = 0.35, .pitch = 0.12
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
