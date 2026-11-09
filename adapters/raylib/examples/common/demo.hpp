#pragma once

/// @file
/// @brief What the raylib demos share: command-line options, the window, the frame clock, frame
/// recording, text panels and cameras. Example code, not part of the adapter.
///
/// Every demo runs in one of two modes:
///
/// - **Interactive**, without options: the keyboard and the mouse control it, in real time. Tab
///   switches to the demo's scripted sequence and back.
/// - **Scripted**, with `--frames N`: the demo plays its scripted sequence for N frames of
///   exactly 1/25 s of simulated time each, however long they take to draw. Then it checks that
///   the sequence did what it should, prints the result, and exits with 0 if it did.
///   `--record DIR` also saves every frame as a PNG (tools/record_demos.py turns them into the
///   README's GIFs), and `--hidden` hides the window, for automated runs.

#include <projectile/projectile.hpp>
#include <projectile/raylib.hpp>

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <ios>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace demo {

using projectile::Vec3;
using projectile::raylib::to_vector3;

/// Exit code of a demo that could not open its window, for example without a display. CTest
/// reports such a demo's test as skipped.
inline constexpr int exit_no_window = 77;

/// Simulated time per frame in scripted runs: 25 frames per second, a rate that GIF files play
/// back exactly (4 hundredths of a second per frame).
inline constexpr double scripted_frame_time = 1.0 / 25.0;

/// Frames that `--record` records when `--frames` is not given: six seconds.
inline constexpr int default_recorded_frames = 150;

/// The window's size, in pixels.
inline constexpr int window_width = 1280;
inline constexpr int window_height = 720;

// Colours shared by the demos.
inline constexpr Color background{24, 26, 33, 255};
inline constexpr Color panel_background{0, 0, 0, 160};
inline constexpr Color text_color{230, 232, 238, 255};
inline constexpr Color dim_text_color{150, 156, 170, 255};
inline constexpr Color accent_color{255, 196, 64, 255};
inline constexpr Color ground_color{58, 64, 74, 255};
inline constexpr Color grid_color{80, 88, 100, 255};

/// How a demo runs.
struct Options {
    int frames = 0;         ///< Length of a scripted run, in frames; 0 for an interactive run.
    std::string record_dir; ///< Directory to save each frame to as a PNG; empty for none.
    bool hidden = false;    ///< Whether to hide the window.

    /// Whether this is a scripted run.
    [[nodiscard]] bool scripted() const noexcept { return frames > 0; }
};

/// The positive integer that @p text spells, if it does and it is at most 999,999.
[[nodiscard]] inline std::optional<int> parse_count(std::string_view text) {
    if (text.empty() || text.size() > 6) {
        return std::nullopt;
    }
    int value = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
        value = 10 * value + (digit - '0');
    }
    return value > 0 ? std::optional<int>(value) : std::nullopt;
}

/// Reads `--frames N`, `--record DIR` and `--hidden` from the command line. Returns
/// std::nullopt, after printing the usage, if the command line is invalid.
[[nodiscard]] inline std::optional<Options> parse_options(int argc, char** argv) {
    const std::span<char*> raw_arguments(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string_view> arguments(raw_arguments.begin(), raw_arguments.end());
    Options options;
    bool valid = true;
    for (std::size_t i = 1; i < arguments.size() && valid; ++i) {
        const std::string_view argument = arguments.at(i);
        const bool has_value = i + 1 < arguments.size();
        if (argument == "--frames" && has_value) {
            const std::optional<int> frames = parse_count(arguments.at(++i));
            valid = frames.has_value();
            options.frames = frames.value_or(0);
        } else if (argument == "--record" && has_value) {
            options.record_dir = arguments.at(++i);
        } else if (argument == "--hidden") {
            options.hidden = true;
        } else {
            valid = false;
        }
    }
    if (!valid) {
        std::fputs(
            "usage: demo [--frames N] [--record DIR] [--hidden]\n"
            "  --frames N    play the scripted sequence for N frames of 1/25 s, then check it\n"
            "  --record DIR  save every frame to DIR as a PNG (150 frames unless --frames)\n"
            "  --hidden      hide the window\n",
            stderr
        );
        return std::nullopt;
    }
    if (!options.record_dir.empty() && !options.scripted()) {
        options.frames = default_recorded_frames;
    }
    return options;
}

/// Whether there may be a display to open a window on. raylib 5.5 crashes, instead of failing,
/// when it cannot open its window (raylib 6 fixed that), so the demos check first. On Linux and
/// the BSDs, raylib draws through X11, which needs the DISPLAY environment variable; other
/// systems always have a display.
[[nodiscard]] inline bool display_available() {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    // Read once, before the demo starts any thread.
    const char* display = std::getenv("DISPLAY"); // NOLINT(concurrency-mt-unsafe)
    return display != nullptr && *display != '\0';
#else
    return true;
#endif
}

/// Opens the demo's window. Returns false if it cannot, for example without a display.
[[nodiscard]] inline bool open_window(const Options& options, const char* title) {
    if (!display_available()) {
        std::fputs("demo: no display to open a window on (DISPLAY is not set)\n", stderr);
        return false;
    }
    auto flags = static_cast<unsigned int>(FLAG_MSAA_4X_HINT);
    if (options.hidden) {
        flags |= static_cast<unsigned int>(FLAG_WINDOW_HIDDEN);
    }
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(window_width, window_height, title);
    if (!IsWindowReady()) {
        std::fputs("demo: could not open a window; is there a display?\n", stderr);
        return false;
    }
    // Scripted runs draw as fast as they can: their frame time is fixed, not measured.
    SetTargetFPS(options.scripted() ? 0 : 60);
    if (!options.record_dir.empty()) {
        std::error_code error;
        std::filesystem::create_directories(options.record_dir, error);
    }
    return true;
}

/// Simulated time, in s, that the current frame covers: fixed in scripted runs, measured in
/// interactive ones.
[[nodiscard]] inline double frame_time(const Options& options) {
    return options.scripted() ? scripted_frame_time : static_cast<double>(GetFrameTime());
}

/// Whether a scripted run should stop, or an interactive one has been closed.
[[nodiscard]] inline bool finished(const Options& options, int frame) {
    return WindowShouldClose() || (options.scripted() && frame >= options.frames);
}

/// Saves the frame drawn so far as `frame_NNNN.png` in the recording directory, if there is one.
/// Call it before EndDrawing(): it reads the frame from the back buffer, which EndDrawing()
/// hands over to the screen.
inline void record_frame(const Options& options, int frame) {
    if (options.record_dir.empty()) {
        return;
    }
    rlDrawRenderBatchActive(); // draw what raylib has batched up, so that the frame is complete
    const Image image = LoadImageFromScreen();
    std::string number = std::to_string(frame);
    number.insert(0, number.size() < 4 ? 4 - number.size() : 0, '0');
    const std::filesystem::path path =
        std::filesystem::path(options.record_dir) / ("frame_" + number + ".png");
    if (!ExportImage(image, path.string().c_str())) {
        std::fputs("demo: could not save a frame\n", stderr);
    }
    UnloadImage(image);
}

/// A small random number generator (SplitMix64) whose output is the same with every compiler
/// and standard library, unlike the distributions of `<random>`: scripted runs, and the GIFs
/// recorded from them, come out the same everywhere.
class Random {
public:
    explicit Random(std::uint64_t seed) : state_(seed) {}

    /// Uniform in [@p low, @p high).
    double uniform(double low, double high) {
        state_ += 0x9E37'79B9'7F4A'7C15U;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30U)) * 0xBF58'476D'1CE4'E5B9U;
        z = (z ^ (z >> 27U)) * 0x94D0'49BB'1331'11EBU;
        z ^= z >> 31U;
        return low + (high - low) * (static_cast<double>(z >> 11U) * 0x1.0p-53);
    }

private:
    std::uint64_t state_;
};

/// The last line of a demo's controls: that the run is scripted, or whether autoplay is on.
[[nodiscard]] inline const char* autoplay_hint(const Options& options, bool autoplay) {
    if (options.scripted()) {
        return "Scripted run";
    }
    return autoplay ? "Tab: autoplay ON (the scripted sequence)" : "Tab autoplay";
}

/// Prints the outcome of a scripted run and returns the demo's exit code: 0 if it @p passed.
[[nodiscard]] inline int report(bool passed, const std::string& summary) {
    std::fputs((summary + (passed ? "\n" : "\nFAILED\n")).c_str(), passed ? stdout : stderr);
    return passed ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------------------------

/// @p value with @p decimals digits after the decimal point. Values that round to zero print
/// without a minus sign.
[[nodiscard]] inline std::string fixed(double value, int decimals) {
    const double half_unit = 0.5 * std::pow(10.0, -decimals);
    std::ostringstream text;
    text.setf(std::ios::fixed);
    text.precision(decimals);
    text << (std::abs(value) < half_unit ? 0.0 : value);
    return text.str();
}

/// @p v as `(x, y, z)`, with @p decimals digits after the decimal point.
[[nodiscard]] inline std::string format(const Vec3& v, int decimals) {
    return "(" + fixed(v.x, decimals) + ", " + fixed(v.y, decimals) + ", " + fixed(v.z, decimals) +
           ")";
}

/// Lines of text on a translucent panel: the demos' heads-up displays.
class Panel {
public:
    explicit Panel(int font_size = 20) : font_size_(font_size) {}

    /// Adds a line.
    void add(std::string text, Color color = text_color) {
        lines_.push_back(Line{.text = std::move(text), .color = color});
    }

    /// Draws the panel @p x pixels from the left edge of the window and @p y pixels from the top.
    /// A negative @p x measures from the right edge instead, and a negative @p y from the
    /// bottom.
    void draw(int x, int y) const {
        const int padding = font_size_ / 2;
        const int line_height = font_size_ + 4;
        int text_width = 0;
        for (const Line& line : lines_) {
            text_width = std::max(text_width, MeasureText(line.text.c_str(), font_size_));
        }
        const int width = text_width + 2 * padding;
        const int height = static_cast<int>(lines_.size()) * line_height + 2 * padding;
        const int left = x >= 0 ? x : GetScreenWidth() + x - width;
        const int top = y >= 0 ? y : GetScreenHeight() + y - height;
        DrawRectangle(left, top, width, height, panel_background);
        int line_top = top + padding;
        for (const Line& line : lines_) {
            DrawText(line.text.c_str(), left + padding, line_top, font_size_, line.color);
            line_top += line_height;
        }
    }

private:
    struct Line {
        std::string text;
        Color color;
    };

    std::vector<Line> lines_;
    int font_size_;
};

/// Draws @p text where @p point appears on the screen, if the camera faces it.
inline void draw_label(
    const Camera3D& camera,
    const Vec3& point,
    const std::string& text,
    Color color,
    int font_size = 18
) {
    const Vec3 eye = projectile::raylib::to_vec3(camera.position);
    const Vec3 view = projectile::raylib::to_vec3(camera.target) - eye;
    if (projectile::dot(point - eye, view) <= 0.0) {
        return;
    }
    const Vector2 screen = GetWorldToScreen(to_vector3(point), camera);
    DrawText(
        text.c_str(),
        static_cast<int>(screen.x) + 8,
        static_cast<int>(screen.y) - 8,
        font_size,
        color
    );
}

// ---------------------------------------------------------------------------------------------
// Cameras
// ---------------------------------------------------------------------------------------------

/// A camera with +y up from @p position toward @p target.
[[nodiscard]] inline Camera3D look_at(const Vec3& position, const Vec3& target, float fov = 45.0F) {
    return Camera3D{
        .position = to_vector3(position),
        .target = to_vector3(target),
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = fov,
        .projection = CAMERA_PERSPECTIVE,
    };
}

/// A camera that circles a target: drag with the right mouse button to turn it, and turn the
/// mouse wheel to zoom.
struct OrbitCamera {
    Vec3 target;            ///< The point it looks at.
    double distance = 30.0; ///< From the target, in m.
    double yaw = 0.0;       ///< Angle around the vertical axis, in radians; 0 looks along -z.
    double pitch = 0.3;     ///< Angle above the horizontal, in radians.

    /// Turns and zooms with the mouse. Scripted runs set the angles themselves.
    void update_from_mouse() {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 delta = GetMouseDelta();
            yaw -= 0.005 * static_cast<double>(delta.x);
            pitch = std::clamp(pitch + 0.005 * static_cast<double>(delta.y), -0.2, 1.4);
        }
        distance = std::clamp(
            distance * (1.0 - 0.1 * static_cast<double>(GetMouseWheelMove())), 2.0, 400.0
        );
    }

    /// The raylib camera.
    [[nodiscard]] Camera3D camera() const {
        const Vec3 offset{
            distance * std::cos(pitch) * std::sin(yaw),
            distance * std::sin(pitch),
            distance * std::cos(pitch) * std::cos(yaw),
        };
        return look_at(target + offset, target);
    }
};

/// A first-person camera that flies: W, A, S and D move it, Q and E lower and raise it, and
/// dragging with the right mouse button turns it.
struct FlyCamera {
    Vec3 position;      ///< Where it is.
    double yaw = 0.0;   ///< Heading around the vertical axis, in radians; 0 looks along -z.
    double pitch = 0.0; ///< Angle above the horizontal, in radians.

    /// The unit vector it looks along.
    [[nodiscard]] Vec3 forward() const {
        return {
            -std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)
        };
    }

    /// Turns it to look at @p target.
    void turn_toward(const Vec3& target) {
        if (const std::optional<Vec3> direction = projectile::try_normalize(target - position)) {
            yaw = std::atan2(-direction->x, -direction->z);
            pitch = std::asin(std::clamp(direction->y, -1.0, 1.0));
        }
    }

    /// Moves and turns it from the keyboard and the mouse, at @p speed m/s for @p dt seconds.
    void update_from_input(double speed, double dt) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 delta = GetMouseDelta();
            yaw -= 0.004 * static_cast<double>(delta.x);
            pitch = std::clamp(pitch - 0.004 * static_cast<double>(delta.y), -1.4, 1.4);
        }
        const Vec3 ahead = forward();
        const Vec3 right = projectile::cross(ahead, Vec3{0.0, 1.0, 0.0});
        Vec3 motion;
        motion += IsKeyDown(KEY_W) ? ahead : Vec3{};
        motion -= IsKeyDown(KEY_S) ? ahead : Vec3{};
        motion += IsKeyDown(KEY_D) ? right : Vec3{};
        motion -= IsKeyDown(KEY_A) ? right : Vec3{};
        motion.y += (IsKeyDown(KEY_E) ? 1.0 : 0.0) - (IsKeyDown(KEY_Q) ? 1.0 : 0.0);
        if (const std::optional<Vec3> direction = projectile::try_normalize(motion)) {
            position += (speed * dt) * *direction;
        }
    }

    /// The raylib camera.
    [[nodiscard]] Camera3D camera() const { return look_at(position, position + forward(), 60.0F); }
};

} // namespace demo
