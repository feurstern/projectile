// raylib demo: hitscan visualizer. Rays are cast into a world of planes, boxes, spheres and
// triangles, as hitscan weapons do.
//
// A shot leaves from a muzzle below and to the right of the camera, toward the point under the
// cursor, as in a first-person game; raycast() finds what it hits first. Each hit is drawn with
// its surface normal, and the panel shows the latest one's point, normal, distance and
// material. Fly around to see earlier rays from the side.
//
// The scripted run (--frames N, see common/demo.hpp) watches the scene from three viewpoints,
// fires nine shots at chosen objects and one into the sky, and checks that each shot hit the
// object it was aimed at, or nothing.

#include <projectile/projectile.hpp>
#include <projectile/raylib.hpp>

#include "common/demo.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace {

namespace adapter = projectile::raylib;
using projectile::AABB;
using projectile::HitResult;
using projectile::Sphere;
using projectile::Triangle;
using projectile::Vec3;

// Material ids: the world reports them in each HitResult.
constexpr std::uint32_t ground = 1;
constexpr std::uint32_t crate = 2;
constexpr std::uint32_t steel_ball = 3;
constexpr std::uint32_t ramp = 4;
constexpr std::uint32_t pyramid = 5;
constexpr std::uint32_t wall = 6;

std::string_view name_of(std::uint32_t material) {
    switch (material) {
    case ground:
        return "the ground";
    case crate:
        return "a crate";
    case steel_ball:
        return "a steel ball";
    case ramp:
        return "the ramp";
    case pyramid:
        return "the pyramid";
    case wall:
        return "the wall";
    default:
        return "something";
    }
}

/// Shots reach 200 m.
constexpr double max_range = 200.0;

struct Box {
    AABB box;
    std::uint32_t material = 0;
    Color color{};
};

struct Ball {
    Sphere sphere;
    std::uint32_t material = 0;
    Color color{};
};

struct Face {
    Triangle triangle;
    std::uint32_t material = 0;
    Color color{};
};

constexpr Color crate_color{176, 128, 82, 255};
constexpr Color steel_color{150, 170, 190, 255};
constexpr Color ramp_color{84, 110, 150, 255};
constexpr Color stone_color{196, 170, 120, 255};
constexpr Color stone_shade{170, 146, 100, 255};
constexpr Color wall_color{150, 76, 64, 255};

constexpr std::array boxes{
    Box{.box = {.min = {-4.75, 0.0, -2.75}, .max = {-3.25, 1.5, -1.25}},
        .material = crate,
        .color = crate_color},
    Box{.box = {.min = {-2.7, 0.0, 0.5}, .max = {-1.7, 1.0, 1.5}},
        .material = crate,
        .color = crate_color},
    Box{.box = {.min = {2.0, 0.0, -4.0}, .max = {4.0, 2.0, -2.0}},
        .material = crate,
        .color = crate_color},
    Box{.box = {.min = {-12.0, 0.0, -9.0}, .max = {12.0, 5.0, -8.0}},
        .material = wall,
        .color = wall_color},
};

constexpr std::array balls{
    Ball{
        .sphere = {.center = {0.0, 1.0, -1.0}, .radius = 1.0},
        .material = steel_ball,
        .color = steel_color
    },
    Ball{
        .sphere = {.center = {5.0, 0.7, 1.5}, .radius = 0.7},
        .material = steel_ball,
        .color = steel_color
    },
};

// A ramp rising toward -z, and a four-sided pyramid.
constexpr Vec3 ramp_a{-8.0, 0.0, 4.0};
constexpr Vec3 ramp_b{-5.0, 0.0, 4.0};
constexpr Vec3 ramp_c{-5.0, 2.0, 0.5};
constexpr Vec3 ramp_d{-8.0, 2.0, 0.5};
constexpr Vec3 apex{6.0, 3.0, -5.0};
constexpr std::array faces{
    Face{
        .triangle = {.a = ramp_a, .b = ramp_b, .c = ramp_c}, .material = ramp, .color = ramp_color
    },
    Face{
        .triangle = {.a = ramp_a, .b = ramp_c, .c = ramp_d}, .material = ramp, .color = ramp_color
    },
    Face{
        .triangle = {.a = {4.5, 0.0, -3.5}, .b = {7.5, 0.0, -3.5}, .c = apex},
        .material = pyramid,
        .color = stone_color
    },
    Face{
        .triangle = {.a = {7.5, 0.0, -3.5}, .b = {7.5, 0.0, -6.5}, .c = apex},
        .material = pyramid,
        .color = stone_shade
    },
    Face{
        .triangle = {.a = {7.5, 0.0, -6.5}, .b = {4.5, 0.0, -6.5}, .c = apex},
        .material = pyramid,
        .color = stone_color
    },
    Face{
        .triangle = {.a = {4.5, 0.0, -6.5}, .b = {4.5, 0.0, -3.5}, .c = apex},
        .material = pyramid,
        .color = stone_shade
    },
};

constexpr projectile::Plane ground_plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};

/// A scripted shot: when, at what, and the material it should hit (none for a miss).
struct ScriptedShot {
    int frame = 0;
    Vec3 target;
    std::optional<std::uint32_t> material;
};

// Four shots from each of the first two viewpoints, then one at the wall and one into the sky.
constexpr std::array script{
    ScriptedShot{.frame = 8, .target = {0.0, 1.0, -1.0}, .material = steel_ball},
    ScriptedShot{.frame = 18, .target = {-4.0, 0.75, -2.0}, .material = crate},
    ScriptedShot{.frame = 28, .target = {-6.5, 1.0, 2.25}, .material = ramp},
    ScriptedShot{.frame = 38, .target = {-6.0, 3.5, -8.0}, .material = wall},
    ScriptedShot{.frame = 60, .target = {2.0, 0.0, 5.0}, .material = ground},
    ScriptedShot{.frame = 70, .target = {6.0, 1.0, -4.0}, .material = pyramid},
    ScriptedShot{.frame = 80, .target = {3.0, 1.0, -2.0}, .material = crate},
    ScriptedShot{.frame = 90, .target = {5.0, 0.7, 1.5}, .material = steel_ball},
    ScriptedShot{.frame = 112, .target = {8.0, 4.0, -8.0}, .material = wall},
    ScriptedShot{.frame = 124, .target = {2.0, 40.0, -60.0}, .material = std::nullopt},
};

/// Where the camera is in the scripted run. It stops at three viewpoints, left, middle and right
/// of the scene, so that each volley can be watched from one place, and glides between them;
/// the earlier rays then show from the side.
demo::FlyCamera scripted_camera(int frame) {
    constexpr std::array<Vec3, 3> viewpoints{
        {{-7.0, 4.0, 12.0}, {0.5, 5.0, 13.0}, {8.0, 3.2, 10.0}}
    };
    constexpr std::array<int, 2> glide_starts{42, 94}; // each glide takes 14 frames
    std::size_t from = 0;
    double progress = 0.0;
    for (const int start : glide_starts) {
        if (frame >= start + 14) {
            ++from;
        } else if (frame > start) {
            progress = static_cast<double>(frame - start) / 14.0;
            break;
        }
    }
    const std::size_t to = std::min(from + 1, viewpoints.size() - 1);
    const double smooth = progress * progress * (3.0 - 2.0 * progress);
    demo::FlyCamera camera{
        .position = projectile::lerp(viewpoints.at(from), viewpoints.at(to), smooth),
        .yaw = 0.0,
        .pitch = 0.0,
    };
    camera.turn_toward({0.0, 1.0, -2.0});
    return camera;
}

/// One shot: where it left from, where it ended, and what it hit.
struct Shot {
    Vec3 from;
    Vec3 to;
    std::optional<HitResult> hit;
};

class HitscanVisualizer {
public:
    HitscanVisualizer() {
        bool invalid = static_cast<bool>(world_.add(ground_plane, ground));
        for (const Box& box : boxes) {
            invalid = static_cast<bool>(world_.add(box.box, box.material)) || invalid;
        }
        for (const Ball& ball : balls) {
            invalid = static_cast<bool>(world_.add(ball.sphere, ball.material)) || invalid;
        }
        for (const Face& face : faces) {
            invalid = static_cast<bool>(world_.add(face.triangle, face.material)) || invalid;
        }
        valid_ = !invalid;
    }

    /// Fires along @p aim, the ray through the crosshair: from the muzzle of @p camera's gun,
    /// toward the point the aim meets. Returns the shot.
    Shot fire(const projectile::Ray& aim, const demo::FlyCamera& camera) {
        const std::optional<HitResult> aimed_at = projectile::raycast(world_, aim, max_range);
        const Vec3 aim_point = aimed_at ? aimed_at->point : projectile::point_at(aim, max_range);
        const Vec3 from = muzzle(camera);
        Shot shot{.from = from, .to = aim_point, .hit = std::nullopt};
        if (const std::optional<projectile::Ray> ray =
                projectile::make_ray(from, aim_point - from)) {
            shot.hit = projectile::raycast(world_, *ray, max_range);
            shot.to = shot.hit ? shot.hit->point : projectile::point_at(*ray, max_range);
        }
        shots_.push_back(shot);
        if (shots_.size() > max_shots_shown) {
            shots_.pop_front();
        }
        ++fired_;
        if (shot.hit) {
            ++hits_;
        }
        return shot;
    }

    void clear() { shots_.clear(); }

    void toggle_normals() { show_normals_ = !show_normals_; }

    [[nodiscard]] bool valid() const { return valid_; }

    void draw_scene(const Camera3D& camera) const {
        BeginMode3D(camera);
        adapter::draw_plane(ground_plane, 30.0, demo::ground_color);
        DrawGrid(30, 1.0F);
        for (const Box& box : boxes) {
            adapter::draw_box(box.box, box.color);
            adapter::draw_box_wires(box.box, Fade(BLACK, 0.4F));
        }
        for (const Ball& ball : balls) {
            adapter::draw_sphere(ball.sphere, ball.color);
            DrawSphereWires(
                adapter::to_vector3(ball.sphere.center),
                static_cast<float>(ball.sphere.radius) * 1.002F,
                10,
                16,
                Fade(BLACK, 0.25F)
            );
        }
        for (const Face& face : faces) {
            adapter::draw_triangle(face.triangle, face.color);
        }
        for (const Shot& shot : shots_) {
            draw_shot(shot, &shot == &shots_.back());
        }
        EndMode3D();

        if (!shots_.empty() && shots_.back().hit) {
            demo::draw_label(
                camera,
                shots_.back().hit->point,
                demo::format(shots_.back().hit->point, 2),
                RAYWHITE
            );
        }
    }

    void draw_panels(const demo::Options& options, bool autoplay) const {
        demo::Panel status;
        status.add("Hitscan visualizer", demo::accent_color);
        status.add(
            "Shots: " + std::to_string(fired_) + "   hits: " + std::to_string(hits_) +
            "   misses: " + std::to_string(fired_ - hits_)
        );
        if (!shots_.empty()) {
            const Shot& latest = shots_.back();
            if (latest.hit) {
                status.add(
                    "Latest shot hit " + std::string(name_of(latest.hit->material_id)) + ":",
                    demo::dim_text_color
                );
                status.add("  point " + demo::format(latest.hit->point, 2) + " m");
                status.add("  normal " + demo::format(latest.hit->normal, 2));
                status.add("  distance " + demo::fixed(latest.hit->distance, 2) + " m");
            } else {
                status.add("Latest shot missed everything", demo::dim_text_color);
            }
        }
        status.draw(-12, 12);

        demo::Panel controls(18);
        controls.add(
            "Left click shoot   W/A/S/D move   Q/E down/up   Right-drag look", demo::dim_text_color
        );
        controls.add("N normals   C clear", demo::dim_text_color);
        controls.add(demo::autoplay_hint(options, autoplay), demo::dim_text_color);
        controls.draw(12, -12);
    }

    [[nodiscard]] std::size_t fired() const { return fired_; }

private:
    static constexpr std::size_t max_shots_shown = 16;

    /// The gun's muzzle: a little ahead of the camera, to its right and below it.
    [[nodiscard]] static Vec3 muzzle(const demo::FlyCamera& camera) {
        const Vec3 forward = camera.forward();
        const Vec3 right =
            projectile::try_normalize(projectile::cross(forward, Vec3{0.0, 1.0, 0.0}))
                .value_or(Vec3{1.0, 0.0, 0.0});
        const Vec3 up = projectile::cross(right, forward);
        return camera.position + 0.6 * forward + 0.35 * right - 0.3 * up;
    }

    void draw_shot(const Shot& shot, bool latest) const {
        const Color ray_color = latest ? demo::accent_color : Fade(demo::accent_color, 0.45F);
        // A tracer, thin at the muzzle so that it does not fill the view next to the camera.
        DrawCylinderEx(
            adapter::to_vector3(shot.from),
            adapter::to_vector3(shot.to),
            0.002F,
            0.015F,
            6,
            ray_color
        );
        if (!shot.hit) {
            return;
        }
        if (show_normals_) {
            adapter::draw_hit(
                *shot.hit, 0.9, latest ? RED : Fade(RED, 0.6F), latest ? GREEN : Fade(GREEN, 0.6F)
            );
        } else {
            DrawSphere(adapter::to_vector3(shot.hit->point), 0.07F, latest ? RED : Fade(RED, 0.6F));
        }
    }

    projectile::PrimitiveWorld world_;
    std::deque<Shot> shots_;
    std::size_t fired_ = 0;
    std::size_t hits_ = 0;
    bool show_normals_ = true;
    bool valid_ = true;
};

/// Draws a crosshair at @p position on the screen.
void draw_crosshair(Vector2 position) {
    const int x = static_cast<int>(position.x);
    const int y = static_cast<int>(position.y);
    DrawLine(x - 12, y, x - 4, y, RAYWHITE);
    DrawLine(x + 4, y, x + 12, y, RAYWHITE);
    DrawLine(x, y - 12, x, y - 4, RAYWHITE);
    DrawLine(x, y + 4, x, y + 12, RAYWHITE);
}

/// The first scripted shot at or after frame @p step, or nullptr after the last one.
const ScriptedShot* next_scripted_shot(int step) {
    for (const ScriptedShot& shot : script) {
        if (shot.frame >= step) {
            return &shot;
        }
    }
    return nullptr;
}

/// Plays frame @p step of the script: moves the camera, aims at the next target, and fires at
/// it when its frame comes. Returns where the crosshair is; counts in @p matched the shots that
/// hit what they were aimed at.
Vector2 play_script(
    HitscanVisualizer& scene, demo::FlyCamera& camera, int step, std::size_t& matched
) {
    camera = scripted_camera(step);
    if (step == 0) {
        scene.clear();
    }
    const ScriptedShot* next = next_scripted_shot(step);
    if (next == nullptr) {
        return GetWorldToScreen(adapter::to_vector3(script.back().target), camera.camera());
    }
    if (next->frame == step) {
        if (const std::optional<projectile::Ray> aim =
                projectile::make_ray(camera.position, next->target - camera.position)) {
            const Shot shot = scene.fire(*aim, camera);
            const bool as_expected =
                shot.hit ? next->material == shot.hit->material_id : !next->material.has_value();
            matched += as_expected ? 1 : 0;
        }
    }
    return GetWorldToScreen(adapter::to_vector3(next->target), camera.camera());
}

/// Flies the camera and shoots from the keyboard and the mouse. Returns where the crosshair is.
Vector2 play_interactive(HitscanVisualizer& scene, demo::FlyCamera& camera, double dt) {
    camera.update_from_input(6.0, dt);
    const Vector2 crosshair = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (const std::optional<projectile::Ray> aim =
                adapter::to_ray(GetScreenToWorldRay(crosshair, camera.camera()))) {
            static_cast<void>(scene.fire(*aim, camera));
        }
    }
    if (IsKeyPressed(KEY_N)) {
        scene.toggle_normals();
    }
    if (IsKeyPressed(KEY_C)) {
        scene.clear();
    }
    return crosshair;
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<demo::Options> options = demo::parse_options(argc, argv);
    if (!options) {
        return 2;
    }
    if (!demo::open_window(*options, "Projectile demo: hitscan visualizer")) {
        return demo::exit_no_window;
    }

    HitscanVisualizer scene;
    demo::FlyCamera camera = scripted_camera(0);
    bool autoplay = options->scripted();
    int script_frame = 0;
    std::size_t matched = 0; // scripted shots that hit what they were aimed at
    for (int frame = 0; !demo::finished(*options, frame); ++frame) {
        if (!options->scripted() && IsKeyPressed(KEY_TAB)) {
            autoplay = !autoplay;
            script_frame = 0;
        }
        // Interactive autoplay repeats the script; a scripted run plays it once.
        const int step = options->scripted() ? script_frame : script_frame % 150;
        const Vector2 crosshair = autoplay
                                      ? play_script(scene, camera, step, matched)
                                      : play_interactive(scene, camera, demo::frame_time(*options));
        ++script_frame;

        BeginDrawing();
        ClearBackground(demo::background);
        scene.draw_scene(camera.camera());
        draw_crosshair(crosshair);
        scene.draw_panels(*options, autoplay);
        demo::record_frame(*options, frame);
        EndDrawing();
    }
    CloseWindow();
    if (!options->scripted()) {
        return 0;
    }
    const bool passed = scene.valid() && matched == script.size() && scene.fired() == script.size();
    return demo::report(
        passed,
        "hitscan_visualizer: " + std::to_string(matched) + " of " + std::to_string(script.size()) +
            " scripted shots hit what they were aimed at"
    );
}
