// FixedTimestep: the accumulator that turns frame times into fixed steps, within limits.

#include "projectile/simulation/fixed_timestep.hpp"
#include "projectile/simulation/simulation_config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"
#include "support/random.hpp"
#include "support/simulation_support.hpp"

#include <cstdint>
#include <initializer_list>
#include <limits>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::require_value;

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();

/// Timing that makes the accumulator's arithmetic exact: steps of 1/128 s.
SimulationConfig exact_timing() {
    SimulationConfig config;
    config.time_step = 1.0 / 128.0;
    config.max_substeps = 8;
    config.max_frame_delta = 0.25;
    return config;
}

} // namespace

TEST_CASE("the default timing is 120 Hz with bounded catch-up", "[simulation][timestep]") {
    const SimulationConfig defaults;
    CHECK(defaults.time_step == 1.0 / 120.0);
    CHECK(defaults.max_substeps == 8);
    CHECK(defaults.max_frame_delta == 0.25);
    CHECK(defaults.integrator == IntegratorKind::semi_implicit_euler);
    CHECK_FALSE(validate(defaults).has_value());

    const FixedTimestep timestep;
    CHECK(timestep.time_step() == defaults.time_step);
    CHECK(timestep.max_substeps() == defaults.max_substeps);
    CHECK(timestep.max_frame_delta() == defaults.max_frame_delta);
    CHECK(timestep.pending_time() == 0.0);
}

TEST_CASE("invalid timing settings are rejected with their reason", "[simulation][timestep]") {
    const auto error_of = [](void (*change)(SimulationConfig&)) {
        SimulationConfig config;
        change(config);
        return FixedTimestep::create(config).error();
    };
    CHECK(error_of([](SimulationConfig& c) { c.time_step = 0.0; }) == Error::invalid_time_step);
    CHECK(error_of([](SimulationConfig& c) { c.time_step = -0.01; }) == Error::invalid_time_step);
    CHECK(error_of([](SimulationConfig& c) {
              c.time_step = not_a_number;
          }) == Error::invalid_time_step);
    CHECK(error_of([](SimulationConfig& c) {
              c.time_step = infinity;
          }) == Error::invalid_time_step);
    CHECK(error_of([](SimulationConfig& c) { c.max_substeps = 0; }) == Error::invalid_max_substeps);
    CHECK(error_of([](SimulationConfig& c) {
              c.max_frame_delta = c.time_step / 2.0;
          }) == Error::invalid_max_frame_delta);
    CHECK(error_of([](SimulationConfig& c) {
              c.max_frame_delta = infinity;
          }) == Error::invalid_max_frame_delta);
    CHECK(error_of([](SimulationConfig& c) {
              c.max_frame_delta = not_a_number;
          }) == Error::invalid_max_frame_delta);
}

TEST_CASE("frames longer than a step run several steps", "[simulation][timestep]") {
    FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
    // 1/64 s frames at 128 Hz: two steps each, nothing carried over.
    for (int frame = 0; frame < 10; ++frame) {
        CHECK(timestep.advance(1.0 / 64.0) == FrameSteps{.steps = 2, .dropped_time = 0.0});
        CHECK(timestep.pending_time() == 0.0);
        CHECK(timestep.interpolation_alpha() == 0.0);
    }
}

TEST_CASE("frames shorter than a step carry their time over", "[simulation][timestep]") {
    FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
    // 1/512 s frames at 128 Hz: a step every fourth frame, with alpha rising in between.
    for (int round = 0; round < 3; ++round) {
        CHECK(timestep.advance(1.0 / 512.0).steps == 0);
        CHECK(timestep.interpolation_alpha() == 0.25);
        CHECK(timestep.advance(1.0 / 512.0).steps == 0);
        CHECK(timestep.interpolation_alpha() == 0.5);
        CHECK(timestep.advance(1.0 / 512.0).steps == 0);
        CHECK(timestep.interpolation_alpha() == 0.75);
        CHECK(timestep.advance(1.0 / 512.0).steps == 1);
        CHECK(timestep.interpolation_alpha() == 0.0);
    }
}

TEST_CASE(
    "the number of steps depends only on the total time", "[simulation][timestep][property]"
) {
    // Frame lengths are multiples of 1/4096 s, so the accumulator's sums are exact.
    test::Random random;
    for (int run = 0; run < 200; ++run) {
        FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
        int ticks = 0; // total time in units of 1/4096 s
        std::uint64_t steps = 0;
        for (int frame = 0; frame < 500; ++frame) {
            const int frame_ticks = random.integer(0, 256); // up to 1/16 s: within the limits
            ticks += frame_ticks;
            steps += timestep.advance(frame_ticks / 4096.0).steps;
        }
        CHECK(steps == static_cast<std::uint64_t>(ticks / 32)); // 32 ticks per step
        CHECK(timestep.pending_time() == (ticks % 32) / 4096.0);
    }
}

TEST_CASE("frame deltas that are not positive add no time", "[simulation][timestep]") {
    FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
    REQUIRE(timestep.advance(1.0 / 256.0).steps == 0); // half a step pending
    for (const double frame_delta : {0.0, -1.0, -infinity, not_a_number}) {
        CAPTURE(frame_delta);
        CHECK(timestep.advance(frame_delta) == FrameSteps{});
        CHECK(timestep.pending_time() == 1.0 / 256.0);
    }
}

TEST_CASE("a long frame is cut to the maximum frame delta", "[simulation][timestep]") {
    SimulationConfig config = exact_timing();
    config.max_substeps = 64; // enough to run every step of the maximum delta at once
    FixedTimestep timestep = require_value(FixedTimestep::create(config));

    const FrameSteps frame = timestep.advance(10.0);
    CHECK(frame.steps == 32); // 0.25 s at 128 Hz
    CHECK(frame.dropped_time == 9.75);
    CHECK(timestep.pending_time() == 0.0);

    const FrameSteps forever = timestep.advance(infinity);
    CHECK(forever.steps == 32);
    CHECK(forever.dropped_time == infinity);
}

TEST_CASE("steps beyond the substep limit are caught up later", "[simulation][timestep]") {
    FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
    // 13 steps are due; 8 run now, and the other 5 at the next update, even without new time.
    CHECK(timestep.advance(13.0 / 128.0) == FrameSteps{.steps = 8, .dropped_time = 0.0});
    CHECK(timestep.interpolation_alpha() == 1.0); // more than a step is still owed
    CHECK(timestep.advance(0.0) == FrameSteps{.steps = 5, .dropped_time = 0.0});
    CHECK(timestep.pending_time() == 0.0);
}

TEST_CASE(
    "a simulation that cannot keep up drops time instead of spiralling", "[simulation][timestep]"
) {
    FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
    // Frames of 0.25 s need 32 steps each, but at most 8 run: the backlog stops growing at the
    // maximum frame delta, and the excess is dropped.
    CHECK(timestep.advance(0.25) == FrameSteps{.steps = 8, .dropped_time = 0.0});
    CHECK(timestep.pending_time() == 0.1875);
    CHECK(timestep.advance(0.25) == FrameSteps{.steps = 8, .dropped_time = 0.125});
    for (int frame = 0; frame < 100; ++frame) {
        CHECK(timestep.advance(0.25) == FrameSteps{.steps = 8, .dropped_time = 0.1875});
        CHECK(timestep.pending_time() == 0.25);
    }
}

TEST_CASE("reset discards the time carried over", "[simulation][timestep]") {
    FixedTimestep timestep = require_value(FixedTimestep::create(exact_timing()));
    REQUIRE(timestep.advance(1.5 / 128.0).steps == 1);
    REQUIRE(timestep.pending_time() > 0.0);
    timestep.reset();
    CHECK(timestep.pending_time() == 0.0);
    CHECK(timestep.interpolation_alpha() == 0.0);
}

TEST_CASE("the default 120 Hz timing runs two steps per 60 Hz frame", "[simulation][timestep]") {
    FixedTimestep timestep;
    std::uint64_t steps = 0;
    for (int frame = 0; frame < 600; ++frame) {
        steps += timestep.advance(1.0 / 60.0).steps;
    }
    CHECK(steps == 1'200);
    CHECK_THAT(timestep.pending_time(), WithinAbs(0.0, 1e-9));
}
