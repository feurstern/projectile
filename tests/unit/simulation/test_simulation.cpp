// Simulation: projectiles and their ids, fixed-step updates, collisions, events and interpolation.

#include "projectile/ballistics/integrator.hpp"
#include "projectile/collision/primitive_world.hpp"
#include "projectile/core/constants.hpp"
#include "projectile/simulation/simulation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"
#include "support/simulation_support.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr Vec3 gravity{0.0, -standard_gravity, 0.0};

ProjectileConfig falling() {
    ProjectileConfig config;
    config.gravity = gravity;
    return config;
}

ProjectileState launched(const Vec3& position, const Vec3& velocity) {
    return require_value(create_projectile(falling(), position, velocity));
}

ProjectileId add(
    Simulation& simulation, const ProjectileState& state, const ProjectileConfig& config = falling()
) {
    return require_value(simulation.add_projectile(state, config));
}

/// The ground plane y = 0, made of material 7.
PrimitiveWorld ground() {
    PrimitiveWorld world;
    REQUIRE_FALSE(world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 7));
    return world;
}

} // namespace

// --- Settings -----------------------------------------------------------------------------------

TEST_CASE("a default simulation is empty, at time zero and at 120 Hz", "[simulation]") {
    const Simulation simulation;
    CHECK(simulation.projectiles().empty());
    CHECK(simulation.time() == 0.0);
    CHECK(simulation.step_count() == 0);
    CHECK(simulation.config().time_step == 1.0 / 120.0);
    CHECK(simulation.collision_world() == nullptr);
    CHECK(simulation.interpolation_alpha() == 0.0);
}

TEST_CASE("a simulation is created from valid settings only", "[simulation]") {
    SimulationConfig config;
    config.time_step = 1.0 / 60.0;
    config.integrator = IntegratorKind::rk4;
    const Simulation simulation = require_value(Simulation::create(config));
    CHECK(simulation.config().time_step == 1.0 / 60.0);
    CHECK(simulation.config().integrator == IntegratorKind::rk4);

    config.max_substeps = 0;
    CHECK(Simulation::create(config).error() == Error::invalid_max_substeps);
}

// --- Projectiles and ids ------------------------------------------------------------------------

TEST_CASE("a projectile is added with its state and configuration", "[simulation][lifecycle]") {
    Simulation simulation;
    ProjectileConfig config = falling();
    config.radius = 0.25;
    const ProjectileState state = launched({1.0, 2.0, 3.0}, {4.0, 5.0, 6.0});
    const ProjectileId id = add(simulation, state, config);

    CHECK(id != ProjectileId{});
    REQUIRE(simulation.contains(id));
    const SimulatedProjectile& projectile = *simulation.find(id);
    CHECK(projectile.id == id);
    CHECK(projectile.state == state);
    CHECK(projectile.config == config);
    CHECK(projectile.previous_position == state.position);
    CHECK(simulation.projectiles().size() == 1);
    CHECK_FALSE(simulation.contains(ProjectileId{}));
}

TEST_CASE("invalid projectiles are rejected with their reason", "[simulation][lifecycle]") {
    Simulation simulation;
    ProjectileConfig massless = falling();
    massless.mass = 0.0;
    CHECK(simulation.add_projectile(launched({}, {}), massless).error() == Error::invalid_mass);
    ProjectileState lost = launched({}, {});
    lost.position.x = std::numeric_limits<double>::quiet_NaN();
    CHECK(simulation.add_projectile(lost, falling()).error() == Error::invalid_position);
    CHECK(simulation.projectiles().empty());
}

TEST_CASE("removing a projectile leaves the others untouched", "[simulation][lifecycle]") {
    Simulation simulation;
    std::vector<ProjectileId> ids;
    ids.reserve(5);
    for (int i = 0; i < 5; ++i) {
        ids.push_back(add(simulation, launched({static_cast<double>(i), 0.0, 0.0}, {})));
    }
    CHECK(simulation.remove_projectile(ids.at(1)));
    CHECK_FALSE(simulation.remove_projectile(ids.at(1))); // already gone
    CHECK(simulation.remove_projectile(ids.at(4)));       // the last one

    CHECK(simulation.projectiles().size() == 3);
    CHECK_FALSE(simulation.contains(ids.at(1)));
    CHECK_FALSE(simulation.contains(ids.at(4)));
    for (const int i : {0, 2, 3}) {
        const SimulatedProjectile* projectile =
            simulation.find(ids.at(static_cast<std::size_t>(i)));
        REQUIRE(projectile != nullptr);
        CHECK(projectile->state.position.x == i);
    }
}

TEST_CASE(
    "the id of a removed projectile never identifies a later one", "[simulation][lifecycle]"
) {
    Simulation simulation;
    std::vector<ProjectileId> removed;
    for (int round = 0; round < 100; ++round) {
        const ProjectileId first = add(simulation, launched({}, {}));
        const ProjectileId second = add(simulation, launched({}, {}));
        removed.push_back(first);
        REQUIRE(simulation.remove_projectile(first));
        if (round % 10 == 0) {
            removed.push_back(second);
            simulation.clear();
        }
    }
    for (const ProjectileId id : removed) {
        CHECK_FALSE(simulation.contains(id));
    }
    // The slots of removed projectiles are reused: memory stays bounded.
    CHECK(simulation.projectiles().size() == 9);
}

TEST_CASE("clear removes every projectile", "[simulation][lifecycle]") {
    Simulation simulation;
    const ProjectileId id = add(simulation, launched({}, {}));
    simulation.clear();
    CHECK(simulation.projectiles().empty());
    CHECK_FALSE(simulation.contains(id));
    CHECK(add(simulation, launched({}, {})) != id);
}

// --- Time and motion ----------------------------------------------------------------------------

TEST_CASE("update runs the fixed steps a frame's time calls for", "[simulation]") {
    Simulation simulation;
    CHECK(simulation.update(1.0 / 60.0) == FrameSteps{.steps = 2, .dropped_time = 0.0});
    CHECK(simulation.step_count() == 2);
    CHECK(simulation.time() == 2.0 / 120.0);
    CHECK(simulation.update(1.0 / 240.0).steps == 0);
    CHECK(simulation.interpolation_alpha() == 0.5);
}

TEST_CASE("step runs exactly one step, ignoring the accumulator", "[simulation]") {
    Simulation simulation;
    REQUIRE(simulation.update(1.0 / 240.0).steps == 0);
    simulation.step();
    CHECK(simulation.step_count() == 1);
    CHECK(simulation.interpolation_alpha() == 0.5);
}

TEST_CASE("projectiles move exactly as the integrator moves them", "[simulation]") {
    for (const IntegratorKind kind : {IntegratorKind::semi_implicit_euler, IntegratorKind::rk4}) {
        SimulationConfig config;
        config.integrator = kind;
        Simulation simulation = require_value(Simulation::create(config));
        ProjectileState expected = launched({0.0, 10.0, 0.0}, {30.0, 20.0, -5.0});
        const ProjectileId id = add(simulation, expected);

        for (int frame = 0; frame < 120; ++frame) {
            simulation.update(1.0 / 60.0);
        }
        for (int i = 0; i < 240; ++i) {
            step(kind, expected, falling(), config.time_step);
        }
        CAPTURE(kind == IntegratorKind::rk4);
        CHECK(simulation.find(id)->state == expected);
    }
}

TEST_CASE("projectiles move independently of each other", "[simulation]") {
    const std::vector<ProjectileState> states{
        launched({0.0, 1.0, 0.0}, {10.0, 10.0, 0.0}),
        launched({5.0, 2.0, 1.0}, {-3.0, 40.0, 2.0}),
        launched({-1.0, 0.0, 9.0}, {0.0, 0.0, -100.0}),
    };
    Simulation together;
    std::vector<ProjectileId> ids;
    ids.reserve(states.size());
    for (const ProjectileState& state : states) {
        ids.push_back(add(together, state));
    }
    for (int i = 0; i < 300; ++i) {
        together.step();
    }
    for (std::size_t k = 0; k < states.size(); ++k) {
        Simulation alone;
        const ProjectileId id = add(alone, states.at(k));
        for (int i = 0; i < 300; ++i) {
            alone.step();
        }
        CHECK(together.find(ids.at(k))->state == alone.find(id)->state);
    }
}

// --- End of flight ------------------------------------------------------------------------------

TEST_CASE("a projectile expires when its lifetime runs out", "[simulation][lifecycle]") {
    Simulation simulation;
    std::vector<ProjectileEvent> events;
    simulation.set_event_callback([&events](const ProjectileEvent& event) {
        events.push_back(event);
    });
    ProjectileConfig config = falling();
    config.lifetime = 0.1;
    const ProjectileId id =
        add(simulation, require_value(create_projectile(config, {}, {5.0, 5.0, 0.0})), config);

    for (int frame = 0; frame < 7; ++frame) { // 14 steps; the lifetime ends in the 12th
        simulation.update(1.0 / 60.0);
    }
    REQUIRE(events.size() == 1);
    const ProjectileEvent& event = events.front();
    CHECK(event.kind == ProjectileEventKind::expired);
    CHECK(event.id == id);
    CHECK_THAT(event.time, WithinAbs(0.1, 1e-12));
    CHECK_FALSE(event.state.active);
    CHECK(event.state.remaining_lifetime == 0.0);
    CHECK(event.config == config);
    CHECK_FALSE(event.hit.has_value());
    CHECK_FALSE(simulation.contains(id));
}

TEST_CASE(
    "a projectile added inactive expires at the next step without moving", "[simulation][lifecycle]"
) {
    Simulation simulation;
    std::vector<ProjectileEvent> events;
    simulation.set_event_callback([&events](const ProjectileEvent& event) {
        events.push_back(event);
    });
    simulation.step();
    ProjectileState spent = launched({1.0, 2.0, 3.0}, {10.0, 0.0, 0.0});
    spent.active = false;
    const ProjectileId id = add(simulation, spent);
    simulation.step();

    REQUIRE(events.size() == 1);
    CHECK(events.front().kind == ProjectileEventKind::expired);
    CHECK(events.front().id == id);
    CHECK(events.front().time == simulation.config().time_step); // at the start of the step
    CHECK(events.front().state == spent);
    CHECK(simulation.projectiles().empty());
}

TEST_CASE("a projectile stops where it hits the collision world", "[simulation][collision]") {
    const PrimitiveWorld world = ground();
    SimulationConfig config;
    config.integrator = IntegratorKind::rk4;
    Simulation simulation = require_value(Simulation::create(config));
    simulation.set_collision_world(&world);
    std::vector<ProjectileEvent> events;
    simulation.set_event_callback([&events](const ProjectileEvent& event) {
        events.push_back(event);
    });
    ProjectileConfig ball = falling();
    ball.radius = 0.5;
    const ProjectileId id = add(
        simulation, require_value(create_projectile(ball, {0.0, 10.0, 0.0}, {3.0, 0.0, 0.0})), ball
    );

    for (int frame = 0; frame < 120 && events.empty(); ++frame) {
        simulation.update(1.0 / 60.0);
    }
    REQUIRE(events.size() == 1);
    const ProjectileEvent& event = events.front();
    CHECK(event.kind == ProjectileEventKind::hit);
    CHECK(event.id == id);
    // The center falls 9.5 m before the surface touches the ground. Between steps the query sees
    // the straight chord, which shifts the impact by microseconds.
    const double impact_time = std::sqrt(2.0 * 9.5 / standard_gravity);
    CHECK_THAT(event.time, WithinAbs(impact_time, 1e-4));
    CHECK_THAT(event.state.position.y, WithinAbs(0.5, 1e-12));
    CHECK_THAT(event.state.position.x, WithinAbs(3.0 * impact_time, 1e-3));
    CHECK_THAT(event.state.velocity.y, WithinAbs(-standard_gravity * impact_time, 1e-3));
    CHECK_FALSE(event.state.active);
    const HitResult hit = require_value(event.hit);
    CHECK_THAT(hit.point.y, WithinAbs(0.0, 1e-12));
    CHECK(hit.normal == Vec3{0.0, 1.0, 0.0});
    CHECK(hit.material_id == 7);
    CHECK_FALSE(simulation.contains(id));
    // The event's time lies within the step that found the hit.
    CHECK(event.time <= simulation.time());
    CHECK(event.time > simulation.time() - simulation.config().time_step);
}

TEST_CASE(
    "without a collision world, projectiles pass through everything", "[simulation][collision]"
) {
    const PrimitiveWorld world = ground();
    Simulation simulation;
    simulation.set_collision_world(&world);
    simulation.set_collision_world(nullptr);
    const ProjectileId id = add(simulation, launched({0.0, 1.0, 0.0}, {}));
    for (int i = 0; i < 240; ++i) {
        simulation.step();
    }
    CHECK(simulation.find(id)->state.position.y < -10.0);
}

// --- Events -------------------------------------------------------------------------------------

TEST_CASE(
    "events arrive after their step, with the projectile already gone", "[simulation][events]"
) {
    const PrimitiveWorld world = ground();
    Simulation simulation;
    simulation.set_collision_world(&world);
    const ProjectileId survivor = add(simulation, launched({0.0, 100.0, 0.0}, {}));
    const ProjectileId lander = add(simulation, launched({0.0, 0.001, 0.0}, {0.0, -1.0, 0.0}));
    int calls = 0;
    simulation.set_event_callback([&](const ProjectileEvent& event) {
        ++calls;
        CHECK(event.id == lander);
        CHECK_FALSE(simulation.contains(lander));
        CHECK(simulation.contains(survivor));
        CHECK(simulation.step_count() == 1); // the step has finished
    });
    simulation.step();
    CHECK(calls == 1);
}

TEST_CASE("a callback may add and remove projectiles", "[simulation][events]") {
    const PrimitiveWorld world = ground();
    Simulation simulation;
    simulation.set_collision_world(&world);
    const ProjectileId bystander = add(simulation, launched({0.0, 100.0, 0.0}, {}));
    add(simulation, launched({0.0, 0.001, 0.0}, {0.0, -1.0, 0.0}));
    const ProjectileState spawn = launched({0.0, 50.0, 0.0}, {1.0, 0.0, 0.0});
    ProjectileId spawned;
    simulation.set_event_callback([&](const ProjectileEvent& /*event*/) {
        simulation.remove_projectile(bystander);
        spawned = require_value(simulation.add_projectile(spawn, falling()));
    });
    simulation.step();
    CHECK_FALSE(simulation.contains(bystander));
    REQUIRE(simulation.contains(spawned));
    CHECK(simulation.find(spawned)->state == spawn); // it moves from the next step on
    simulation.step();
    CHECK(simulation.find(spawned)->state.position.x > 0.0);
}

TEST_CASE("a callback cannot step the simulation", "[simulation][events]") {
    Simulation simulation;
    ProjectileConfig short_lived = falling();
    short_lived.lifetime = 0.001;
    add(simulation, require_value(create_projectile(short_lived, {})), short_lived);
    int calls = 0;
    simulation.set_event_callback([&](const ProjectileEvent& /*event*/) {
        ++calls;
        CHECK(simulation.update(1.0) == FrameSteps{});
        simulation.step();
        CHECK(simulation.step_count() == 1);
    });
    simulation.step();
    CHECK(calls == 1);
    CHECK(simulation.step_count() == 1);
}

TEST_CASE("a callback can replace itself", "[simulation][events]") {
    Simulation simulation;
    ProjectileConfig short_lived = falling();
    short_lived.lifetime = 0.001;
    int first_calls = 0;
    int second_calls = 0;
    const auto second = [&second_calls](const ProjectileEvent& /*event*/) { ++second_calls; };
    simulation.set_event_callback([&](const ProjectileEvent& /*event*/) {
        ++first_calls;
        simulation.set_event_callback(second);
    });
    for (int i = 0; i < 3; ++i) {
        add(simulation, require_value(create_projectile(short_lived, {})), short_lived);
        simulation.step();
    }
    CHECK(first_calls == 1);
    CHECK(second_calls == 2);

    // Clearing the callback from inside it stops the events.
    simulation.set_event_callback([&](const ProjectileEvent& /*event*/) {
        ++first_calls;
        simulation.set_event_callback(nullptr);
    });
    for (int i = 0; i < 3; ++i) {
        add(simulation, require_value(create_projectile(short_lived, {})), short_lived);
        simulation.step();
    }
    CHECK(first_calls == 2);
}

TEST_CASE("an exception from a callback leaves the simulation usable", "[simulation][events]") {
    Simulation simulation;
    ProjectileConfig short_lived = falling();
    short_lived.lifetime = 0.001;
    int calls = 0;
    simulation.set_event_callback([&calls](const ProjectileEvent& /*event*/) {
        if (++calls == 1) {
            throw std::runtime_error("game logic failed");
        }
    });
    add(simulation, require_value(create_projectile(short_lived, {})), short_lived);
    CHECK_THROWS_AS(simulation.step(), std::runtime_error);
    CHECK(simulation.projectiles().empty());

    // The callback is still installed and receives the next events.
    add(simulation, require_value(create_projectile(short_lived, {})), short_lived);
    simulation.step();
    CHECK(calls == 2);
}

TEST_CASE("without a callback, ended projectiles still leave", "[simulation][events]") {
    Simulation simulation;
    ProjectileConfig short_lived = falling();
    short_lived.lifetime = 0.001;
    const ProjectileId id =
        add(simulation, require_value(create_projectile(short_lived, {})), short_lived);
    simulation.step();
    CHECK_FALSE(simulation.contains(id));
}

// --- Interpolation ------------------------------------------------------------------------------

TEST_CASE("interpolation data lies between the last two states", "[simulation][interpolation]") {
    Simulation simulation;
    const ProjectileId id = add(simulation, launched({}, {12.0, 0.0, 0.0}));
    const Vec3 start = simulation.find(id)->state.position;
    CHECK(interpolated_position(*simulation.find(id), 0.7) == start); // added since the last step

    // A 1/90 s frame at 120 Hz runs one step and carries a third of a step over.
    REQUIRE(simulation.update(1.0 / 90.0).steps == 1);
    const SimulatedProjectile& projectile = *simulation.find(id);
    CHECK(projectile.previous_position == start);
    const double alpha = simulation.interpolation_alpha();
    CHECK_THAT(alpha, WithinAbs(1.0 / 3.0, 1e-12));
    CHECK_THAT(
        interpolated_position(projectile, alpha),
        is_near(lerp(start, projectile.state.position, 1.0 / 3.0), 1e-12)
    );
}
