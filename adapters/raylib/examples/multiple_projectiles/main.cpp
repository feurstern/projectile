// raylib demo: multiple projectiles. Thousands of projectiles bounce around an arena at once.
//
// A fountain on the central dome sprays projectiles, continuously or in bursts. Each one flies
// its own trajectory, bounces off the floor, the walls and the obstacles (a rubbery material
// that always ricochets), and stops when a bounce leaves it slower than 2 m/s. They are drawn
// as tracers, coloured by speed. The panel shows how many are in flight and what the
// simulation costs: the time each frame's update() takes, and that time per projectile update.
//
// The scripted run (--frames N, see common/demo.hpp) fires two bursts with a stream between
// them, and checks that thousands of projectiles flew at once, that many of them came to rest,
// and that every update was timed.

#include <projectile/projectile.hpp>
#include <projectile/raylib.hpp>

#include "common/demo.hpp"
#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>

namespace {

namespace adapter = projectile::raylib;
using projectile::AABB;
using projectile::Vec3;

constexpr std::uint32_t rubber = 1;

constexpr projectile::Plane floor_plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
constexpr projectile::Sphere dome{.center = {0.0, -2.0, 0.0}, .radius = 4.5};
constexpr std::array<AABB, 4> walls{{
    {.min = {-16.0, 0.0, -16.0}, .max = {16.0, 4.5, -15.0}},
    {.min = {-16.0, 0.0, 15.0}, .max = {16.0, 4.5, 16.0}},
    {.min = {-16.0, 0.0, -15.0}, .max = {-15.0, 4.5, 15.0}},
    {.min = {15.0, 0.0, -15.0}, .max = {16.0, 4.5, 15.0}},
}};
constexpr std::array<AABB, 4> pillars{{
    {.min = {-10.0, 0.0, -10.0}, .max = {-8.5, 5.0, -8.5}},
    {.min = {8.5, 0.0, -10.0}, .max = {10.0, 5.0, -8.5}},
    {.min = {-10.0, 0.0, 8.5}, .max = {-8.5, 5.0, 10.0}},
    {.min = {8.5, 0.0, 8.5}, .max = {10.0, 5.0, 10.0}},
}};

/// The fountain's nozzle, on top of the dome.
constexpr Vec3 nozzle{0.0, 2.6, 0.0};

/// What the keyboard, or the script, asks for in one frame.
struct Commands {
    bool stream = false;      ///< Spray at the spawn rate during this frame.
    int burst = 0;            ///< Spawn this many at once.
    double rate_factor = 1.0; ///< Multiply the spawn rate by this.
    bool toggle_world = false;
    bool clear = false;
    std::optional<double> rate; ///< Set the spawn rate, per s (scripts only).
};

Commands read_input() {
    Commands commands;
    commands.stream = IsKeyDown(KEY_SPACE);
    commands.burst = IsKeyPressed(KEY_B) ? 5'000 : 0;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        commands.rate_factor = 2.0;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        commands.rate_factor = 0.5;
    }
    commands.toggle_world = IsKeyPressed(KEY_W);
    commands.clear = IsKeyPressed(KEY_C);
    return commands;
}

/// The scripted sequence, 150 frames at 25 per second: a burst of 4,000, a stream of 3,000 per
/// second for nearly three seconds, and another burst.
Commands scripted(int frame) {
    Commands commands;
    if (frame == 0) {
        commands.clear = true;
        commands.rate = 3'000.0;
    }
    commands.burst = frame == 5 || frame == 100 ? 4'000 : 0;
    commands.stream = frame >= 25 && frame < 95;
    return commands;
}

/// Running totals of what update() cost, over the last frames.
struct Timing {
    static constexpr std::size_t frames = 30;
    std::array<double, frames> seconds{};      ///< Time spent in update(), per frame.
    std::array<double, frames> updates{};      ///< Projectile updates done, per frame.
    std::array<std::uint32_t, frames> steps{}; ///< Steps run, per frame.
    std::size_t next = 0;
    std::size_t timed_frames = 0; ///< Frames with at least one step, in all.

    void add(double time, double projectile_updates, std::uint32_t step_count) {
        seconds.at(next) = time;
        updates.at(next) = projectile_updates;
        steps.at(next) = step_count;
        next = (next + 1) % frames;
        timed_frames += step_count > 0 ? 1 : 0;
    }

    /// Mean time per frame, in s.
    [[nodiscard]] double frame_time() const {
        double total = 0.0;
        for (const double time : seconds) {
            total += time;
        }
        return total / static_cast<double>(frames);
    }

    /// Mean time per projectile update, in s; zero before any.
    [[nodiscard]] double update_time() const {
        double time = 0.0;
        double count = 0.0;
        for (std::size_t i = 0; i < frames; ++i) {
            time += seconds.at(i);
            count += updates.at(i);
        }
        return count > 0.0 ? time / count : 0.0;
    }
};

/// @p count with thousands separated by commas.
std::string grouped(std::size_t count) {
    std::string digits = std::to_string(count);
    for (std::size_t i = digits.size(); i > 3; i -= 3) {
        digits.insert(i - 3, ",");
    }
    return digits;
}

class Arena {
public:
    Arena() {
        bool invalid = static_cast<bool>(world_.add(floor_plane, rubber));
        invalid = static_cast<bool>(world_.add(dome, rubber)) || invalid;
        for (const AABB& box : walls) {
            invalid = static_cast<bool>(world_.add(box, rubber)) || invalid;
        }
        for (const AABB& box : pillars) {
            invalid = static_cast<bool>(world_.add(box, rubber)) || invalid;
        }
        projectile::Material material;
        material.id = rubber;
        material.restitution = 0.5;
        material.friction = 0.25;
        material.ricochet_angle = std::numbers::pi / 2.0; // every impact bounces
        invalid = static_cast<bool>(materials_.add(material)) || invalid;
        // A bounce that leaves a projectile slower than 2 m/s stops it.
        const projectile::Result<projectile::StandardInteraction> interaction =
            projectile::StandardInteraction::create(2.0);
        valid_ = !invalid && interaction.has_value();
        if (interaction) {
            interaction_ = *interaction;
        }
        simulation_.set_collision_world(&world_);
        simulation_.set_materials(&materials_);
        simulation_.set_interaction(&interaction_);
        simulation_.set_event_callback([this](const projectile::ProjectileEvent& event) {
            if (event.kind == projectile::ProjectileEventKind::hit) {
                ++stopped_;
            }
        });
        simulation_.reserve(50'000);
    }

    // The simulation refers to this object's members.
    Arena(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena& operator=(Arena&&) = delete;
    ~Arena() = default;

    void apply(const Commands& commands, double dt) {
        if (commands.clear) {
            simulation_.clear();
            spawned_ = 0;
            stopped_ = 0;
            peak_ = 0;
        }
        if (commands.toggle_world) {
            world_on_ = !world_on_;
            simulation_.set_collision_world(world_on_ ? &world_ : nullptr);
        }
        rate_ = std::clamp(commands.rate.value_or(rate_ * commands.rate_factor), 250.0, 64'000.0);
        if (commands.stream) {
            due_ += rate_ * dt;
        }
        const auto count = static_cast<int>(due_);
        due_ -= count;
        spawn(count + commands.burst);
    }

    /// Advances the simulation by @p dt seconds, timing update().
    void advance(double dt) {
        const auto active = static_cast<double>(simulation_.projectiles().size());
        const auto start = std::chrono::steady_clock::now();
        const projectile::FrameSteps steps = simulation_.update(dt);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        timing_.add(elapsed.count(), active * steps.steps, steps.steps);
        peak_ = std::max(peak_, simulation_.projectiles().size());
    }

    void draw_scene(const Camera3D& camera) const {
        BeginMode3D(camera);
        adapter::draw_plane(floor_plane, 45.0, demo::ground_color);
        const Color structure =
            world_on_ ? Color{92, 102, 122, 255} : Fade(Color{92, 102, 122, 255}, 0.3F);
        adapter::draw_sphere(dome, world_on_ ? Color{110, 122, 146, 255} : Fade(GRAY, 0.3F));
        for (const AABB& box : walls) {
            adapter::draw_box(box, structure);
        }
        for (const AABB& box : pillars) {
            adapter::draw_box(box, structure);
            adapter::draw_box_wires(box, Fade(BLACK, 0.35F));
        }
        draw_tracers();
        EndMode3D();
    }

    void draw_panels(const demo::Options& options, bool autoplay) const {
        demo::Panel status;
        status.add("Multiple projectiles", demo::accent_color);
        status.add("In flight: " + grouped(simulation_.projectiles().size()));
        status.add(
            "Spawned " + grouped(spawned_) + ", came to rest " + grouped(stopped_),
            demo::dim_text_color
        );
        status.add("Spawn rate: " + grouped(static_cast<std::size_t>(rate_)) + " per second");
        status.add(
            "update(): " + demo::fixed(1e3 * timing_.frame_time(), 2) + " ms per frame, " +
            demo::fixed(1e9 * timing_.update_time(), 0) + " ns per projectile update"
        );
        status.add(
            options.scripted() ? "Frame rate: 25 fps, fixed (scripted run)"
                               : "Frame rate: " + std::to_string(GetFPS()) + " fps",
            demo::dim_text_color
        );
        status.add(
            std::string("Collisions: ") + (world_on_ ? "on, 14 shapes" : "off"),
            demo::dim_text_color
        );
        status.draw(-12, 12);

        demo::Panel controls(18);
        controls.add("Hold Space spray   B burst of 5,000   +/- spawn rate", demo::dim_text_color);
        controls.add(
            "W collisions on/off   C clear   Right-drag orbit   Wheel zoom", demo::dim_text_color
        );
        controls.add(demo::autoplay_hint(options, autoplay), demo::dim_text_color);
        controls.draw(12, -12);
    }

    /// Checks the scripted run: thousands flew at once, many came to rest, and every frame
    /// that ran steps was timed.
    [[nodiscard]] int check(int frames) const {
        const bool passed = valid_ && peak_ >= 5'000 && stopped_ >= 1'000 &&
                            timing_.timed_frames >= static_cast<std::size_t>(frames) - 1 &&
                            timing_.update_time() > 0.0;
        return demo::report(
            passed,
            "multiple_projectiles: " + grouped(spawned_) + " spawned, up to " + grouped(peak_) +
                " in flight at once, " + grouped(stopped_) + " came to rest; update() took " +
                demo::fixed(1e9 * timing_.update_time(), 0) + " ns per projectile update"
        );
    }

private:
    void spawn(int count) {
        projectile::ProjectileConfig config;
        config.mass = 0.05;
        config.radius = 0.05;
        config.gravity = {0.0, -9.81, 0.0};
        config.lifetime = 12.0;
        for (int i = 0; i < count; ++i) {
            // Up to 28 degrees from vertical, in every direction, at 8 to 13 m/s.
            const double tilt = std::acos(
                1.0 - random_.uniform(0.0, 1.0 - std::cos(projectile::degrees_to_radians(28.0)))
            );
            const double heading = random_.uniform(0.0, 2.0 * std::numbers::pi);
            const double speed = random_.uniform(8.0, 13.0);
            const Vec3 velocity = speed * Vec3{
                                              std::sin(tilt) * std::cos(heading),
                                              std::cos(tilt),
                                              std::sin(tilt) * std::sin(heading)
                                          };
            const projectile::Result<projectile::ProjectileState> start =
                projectile::create_projectile(config, nozzle, velocity);
            if (start && simulation_.add_projectile(*start, config)) {
                ++spawned_;
            }
        }
    }

    /// Draws each projectile as a short streak behind it, from blue when slow to orange when
    /// fast. Lines are drawn two pixels wide where the graphics driver allows it.
    void draw_tracers() const {
        rlDrawRenderBatchActive();
        rlSetLineWidth(2.0F);
        const double alpha = simulation_.interpolation_alpha();
        for (const projectile::SimulatedProjectile& projectile : simulation_.projectiles()) {
            const Vec3 head = projectile::interpolated_position(projectile, alpha);
            const Vec3 tail = head - 0.03 * projectile.state.velocity;
            const auto t = static_cast<float>(
                std::clamp(projectile::length(projectile.state.velocity) / 13.0, 0.0, 1.0)
            );
            const Color color = ColorLerp(Color{80, 150, 255, 255}, Color{255, 170, 60, 255}, t);
            DrawLine3D(adapter::to_vector3(tail), adapter::to_vector3(head), color);
        }
        rlDrawRenderBatchActive();
        rlSetLineWidth(1.0F);
    }

    projectile::PrimitiveWorld world_;
    projectile::MaterialTable materials_;
    projectile::StandardInteraction interaction_;
    projectile::Simulation simulation_;
    demo::Random random_{20'261'109};
    Timing timing_;
    double rate_ = 3'000.0; ///< Projectiles per second while spraying.
    double due_ = 0.0;      ///< Projectiles due to spawn, carried between frames.
    std::size_t spawned_ = 0;
    std::size_t stopped_ = 0;
    std::size_t peak_ = 0;
    bool world_on_ = true;
    bool valid_ = true;
};

} // namespace

int main(int argc, char** argv) {
    const std::optional<demo::Options> options = demo::parse_options(argc, argv);
    if (!options) {
        return 2;
    }
    if (!demo::open_window(*options, "Projectile demo: multiple projectiles")) {
        return demo::exit_no_window;
    }

    Arena arena;
    demo::OrbitCamera camera{.target = {0.0, 3.0, 0.0}, .distance = 40.0, .yaw = 0.5, .pitch = 0.5};
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
        arena.apply(autoplay ? scripted(step) : read_input(), dt);
        arena.advance(dt);
        ++script_frame;

        BeginDrawing();
        ClearBackground(demo::background);
        arena.draw_scene(camera.camera());
        arena.draw_panels(*options, autoplay);
        demo::record_frame(*options, frame);
        EndDrawing();
    }
    CloseWindow();
    return options->scripted() ? arena.check(options->frames) : 0;
}
