// Impacts in the simulation: ricochets, penetration and stops decided by an interaction model,
// the flight that continues after them, and the limits that keep impacts in check.

#include "projectile/ballistics/energy.hpp"
#include "projectile/collision/primitive_world.hpp"
#include "projectile/core/constants.hpp"
#include "projectile/materials/interaction.hpp"
#include "projectile/materials/material.hpp"
#include "projectile/simulation/simulation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"
#include "support/simulation_support.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <vector>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr double pi = std::numbers::pi;
constexpr double skin = 1e-6; // how far the simulation moves projectiles clear of a surface

/// A simulation that records its events, with a world, materials and the standard model.
struct Scene {
    PrimitiveWorld world;
    MaterialTable materials;
    StandardInteraction interaction;
    Simulation simulation;
    std::vector<ProjectileEvent> events;

    Scene() {
        simulation.set_collision_world(&world);
        simulation.set_materials(&materials);
        simulation.set_interaction(&interaction);
        simulation.set_event_callback([this](const ProjectileEvent& event) {
            events.push_back(event);
        });
    }

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;
    ~Scene() = default;

    /// Adds a projectile of @p mass kg and @p radius m without gravity at @p position, moving at
    /// @p velocity.
    ProjectileId fire(const Vec3& position, const Vec3& velocity, double mass, double radius) {
        ProjectileConfig config;
        config.mass = mass;
        config.radius = radius;
        return require_value(simulation.add_projectile(
            require_value(create_projectile(config, position, velocity)), config
        ));
    }

    void run(double seconds) {
        const auto steps = static_cast<int>(std::lround(seconds / simulation.config().time_step));
        for (int i = 0; i < steps; ++i) {
            simulation.step();
        }
    }
};

Material ricocheting(std::uint32_t id) {
    Material material;
    material.id = id;
    material.restitution = 0.5;
    material.ricochet_angle = degrees_to_radians(30.0);
    return material;
}

Material wood(std::uint32_t id) {
    Material material;
    material.id = id;
    material.penetration_cost = 100.0;
    material.resistance = 1'000.0;
    return material;
}

} // namespace

TEST_CASE(
    "without an interaction model every contact stops the projectile", "[simulation][impacts]"
) {
    Scene scene;
    REQUIRE_FALSE(scene.materials.add(ricocheting(1)));
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1));
    scene.simulation.set_interaction(nullptr);
    scene.fire({0.0, 1.01, 0.0}, {20.0, -5.0, 0.0}, 1.0, 0.0);
    scene.run(0.5);
    REQUIRE(scene.events.size() == 1);
    CHECK(scene.events.front().kind == ProjectileEventKind::hit);
    CHECK_FALSE(scene.events.front().interaction.has_value());
}

TEST_CASE("a projectile ricochets and flies on for the rest of the step", "[simulation][impacts]") {
    // Without gravity, from 1.01 m up at (20, −5) m/s: it strikes the ground 14° from the surface
    // at t = 0.202 s and x = 4.04 m, and leaves at (20, 2.5) m/s.
    Scene scene;
    REQUIRE_FALSE(scene.materials.add(ricocheting(1)));
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1));
    const ProjectileId id = scene.fire({0.0, 1.01, 0.0}, {20.0, -5.0, 0.0}, 1.0, 0.0);
    scene.run(0.5);

    REQUIRE(scene.events.size() == 1);
    const ProjectileEvent& event = scene.events.front();
    CHECK(event.kind == ProjectileEventKind::ricochet);
    CHECK(event.id == id);
    CHECK_THAT(event.time, WithinAbs(0.202, 1e-12));
    CHECK_THAT(event.state.position, is_near(Vec3{4.04, 0.0, 0.0}, 1e-12));
    CHECK_THAT(event.state.velocity, is_near(Vec3{20.0, -5.0, 0.0}, 1e-12)); // as it struck
    CHECK(event.state.active);
    const InteractionResult result = require_value(event.interaction);
    CHECK(result.outcome == InteractionOutcome::ricocheted);
    CHECK_THAT(result.outgoing_velocity, is_near(Vec3{20.0, 2.5, 0.0}, 1e-12));
    CHECK_THAT(result.energy_remaining, WithinRel(0.5 * (400.0 + 6.25), 1e-12));

    // It flew on along the reflected path, without losing the rest of its step.
    REQUIRE(scene.simulation.contains(id));
    const Vec3 expected = Vec3{4.04, skin, 0.0} + Vec3{20.0, 2.5, 0.0} * (0.5 - 0.202);
    CHECK_THAT(scene.simulation.find(id)->state.position, is_near(expected, 1e-9));
}

TEST_CASE("a projectile penetrates a box and flies on beyond it", "[simulation][impacts]") {
    // 0.01 kg at 300 m/s carries 450 J. Ten centimetres of wood cost 100 + 1000 · 0.1 = 200 J,
    // which leaves 250 J: √(2 · 250 / 0.01) ≈ 223.6 m/s.
    const double speed_after = std::sqrt(2.0 * 250.0 / 0.01);
    for (const double radius : {0.0, 0.02}) {
        CAPTURE(radius);
        Scene scene;
        REQUIRE_FALSE(scene.materials.add(wood(2)));
        REQUIRE_FALSE(scene.world.add(AABB{.min = {5.0, -1.0, -1.0}, .max = {5.1, 1.0, 1.0}}, 2));
        const ProjectileId id = scene.fire({0.1, 0.0, 0.0}, {300.0, 0.0, 0.0}, 0.01, radius);
        scene.run(0.5);

        REQUIRE(scene.events.size() == 1);
        const ProjectileEvent& event = scene.events.front();
        CHECK(event.kind == ProjectileEventKind::penetration);
        const double impact_time = (5.0 - radius - 0.1) / 300.0;
        CHECK_THAT(event.time, WithinAbs(impact_time, 1e-12));
        const InteractionResult result = require_value(event.interaction);
        CHECK_THAT(result.energy_remaining, WithinAbs(250.0, 0.01)); // the skin costs 1 mJ
        CHECK_THAT(length(result.outgoing_velocity), WithinAbs(speed_after, 1e-3));

        // It continues from just beyond the far face, clear of the box.
        REQUIRE(scene.simulation.contains(id));
        const double expected =
            5.1 + radius + skin + result.outgoing_velocity.x * (0.5 - impact_time);
        CHECK_THAT(scene.simulation.find(id)->state.position.x, WithinAbs(expected, 1e-6));
    }
}

TEST_CASE("a surface without thickness costs only its penetration cost", "[simulation][impacts]") {
    // A single triangle has no thickness: the resistance per metre costs nothing.
    Scene scene;
    REQUIRE_FALSE(scene.materials.add(wood(2)));
    REQUIRE_FALSE(scene.world.add(
        Triangle{.a = {5.0, -1.0, -1.0}, .b = {5.0, 1.0, -1.0}, .c = {5.0, 0.0, 2.0}}, 2
    ));
    scene.fire({0.1, 0.0, 0.0}, {300.0, 0.0, 0.0}, 0.01, 0.0);
    scene.run(0.1);
    REQUIRE(scene.events.size() == 1);
    CHECK(scene.events.front().kind == ProjectileEventKind::penetration);
    CHECK_THAT(
        require_value(scene.events.front().interaction).energy_remaining, WithinRel(350.0, 1e-12)
    );
}

TEST_CASE("a projectile without enough energy stops at the surface", "[simulation][impacts]") {
    Scene scene;
    Material concrete = wood(2);
    concrete.penetration_cost = 5'000.0;
    REQUIRE_FALSE(scene.materials.add(concrete));
    REQUIRE_FALSE(scene.world.add(AABB{.min = {5.0, -1.0, -1.0}, .max = {5.1, 1.0, 1.0}}, 2));
    const ProjectileId id = scene.fire({0.1, 0.0, 0.0}, {300.0, 0.0, 0.0}, 0.01, 0.0);
    scene.run(0.1);
    REQUIRE(scene.events.size() == 1);
    const ProjectileEvent& event = scene.events.front();
    CHECK(event.kind == ProjectileEventKind::hit);
    CHECK(require_value(event.interaction) == InteractionResult{});
    CHECK_FALSE(event.state.active);
    CHECK_THAT(event.state.position.x, WithinAbs(5.0, 1e-12));
    CHECK_FALSE(scene.simulation.contains(id));
}

TEST_CASE(
    "materials come from the table, and unknown ids from its fallback", "[simulation][impacts]"
) {
    // The ground (material 1) makes the projectile ricochet into a wall of material 2, which the
    // table does not know: the fallback stops the projectile there.
    Scene scene;
    REQUIRE_FALSE(scene.materials.add(ricocheting(1)));
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1));
    REQUIRE_FALSE(scene.world.add(AABB{.min = {8.0, 0.0, -1.0}, .max = {9.0, 5.0, 1.0}}, 2));
    scene.fire({0.0, 1.01, 0.0}, {20.0, -5.0, 0.0}, 1.0, 0.0);
    scene.run(1.0);
    REQUIRE(scene.events.size() == 2);
    CHECK(scene.events.at(0).kind == ProjectileEventKind::ricochet);
    CHECK(scene.events.at(1).kind == ProjectileEventKind::hit);
    CHECK(require_value(scene.events.at(1).hit).material_id == 2);

    // Without a table, every surface is made of the default material, which stops projectiles.
    Scene bare;
    REQUIRE_FALSE(bare.world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1));
    bare.simulation.set_materials(nullptr);
    bare.fire({0.0, 1.01, 0.0}, {20.0, -5.0, 0.0}, 1.0, 0.0);
    bare.run(1.0);
    REQUIRE(bare.events.size() == 1);
    CHECK(bare.events.front().kind == ProjectileEventKind::hit);
}

TEST_CASE("a bouncing ball comes to rest", "[simulation][impacts]") {
    // Each bounce keeps 60 % of the speed, until a bounce would leave slower than the stopping
    // speed of 0.1 m/s: then the ball stops on the ground.
    Scene scene;
    Material floor;
    floor.id = 1;
    floor.restitution = 0.6;
    floor.friction = 0.1;
    floor.ricochet_angle = pi / 2.0; // every impact bounces
    REQUIRE_FALSE(scene.materials.add(floor));
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1));
    ProjectileConfig ball;
    ball.radius = 0.1;
    ball.gravity = {0.0, -standard_gravity, 0.0};
    const ProjectileId id = require_value(scene.simulation.add_projectile(
        require_value(create_projectile(ball, {0.0, 1.0, 0.0}, {0.5, 0.0, 0.0})), ball
    ));
    for (int i = 0; i < 1'200 && scene.simulation.contains(id); ++i) {
        scene.simulation.step();
        CHECK(scene.simulation.contains(id) == (scene.simulation.find(id) != nullptr));
        if (const SimulatedProjectile* const projectile = scene.simulation.find(id)) {
            CHECK(projectile->state.position.y >= ball.radius); // never sinks into the floor
        }
    }
    REQUIRE_FALSE(scene.simulation.contains(id)); // it came to rest within 10 s
    REQUIRE(scene.events.size() >= 6);
    double previous_speed = std::numeric_limits<double>::infinity();
    for (std::size_t k = 0; k + 1 < scene.events.size(); ++k) {
        const ProjectileEvent& bounce = scene.events.at(k);
        CHECK(bounce.kind == ProjectileEventKind::ricochet);
        const double speed = length(bounce.state.velocity);
        CHECK(speed < previous_speed); // every bounce is lower than the last
        previous_speed = speed;
    }
    const ProjectileEvent& rest = scene.events.back();
    CHECK(rest.kind == ProjectileEventKind::hit);
    CHECK_THAT(rest.state.position.y, WithinAbs(ball.radius, 1e-9));
}

TEST_CASE("impacts per step are limited", "[simulation][impacts]") {
    // Between two walls 1 cm apart, a projectile at 1000 m/s would bounce 800 times per step.
    // With at most 4 impacts per step, it bounces 4 times and waits for the next step.
    Scene scene;
    Material mirror;
    mirror.id = 1;
    mirror.restitution = 1.0;
    mirror.ricochet_angle = pi / 2.0;
    REQUIRE_FALSE(scene.materials.add(mirror));
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {1.0, 0.0, 0.0}, .distance = 0.0}, 1));
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -0.01}, 1));
    const ProjectileId id = scene.fire({0.005, 0.0, 0.0}, {1'000.0, 0.0, 0.0}, 0.01, 0.0);
    for (std::size_t step = 1; step <= 10; ++step) {
        scene.simulation.step();
        CHECK(scene.events.size() == 4 * step);
        const double x = scene.simulation.find(id)->state.position.x;
        CHECK(x > 0.0);
        CHECK(x < 0.01);
    }
    for (const ProjectileEvent& event : scene.events) {
        CHECK(event.kind == ProjectileEventKind::ricochet);
        CHECK_THAT(
            length(require_value(event.interaction).outgoing_velocity), WithinRel(1'000.0, 1e-12)
        );
    }
}

namespace {

/// Bounces projectiles straight back at full speed, from every surface.
class Reverse final : public IMaterialInteraction {
public:
    [[nodiscard]] InteractionResult resolve(const Impact& impact) const override {
        return InteractionResult{
            .outcome = InteractionOutcome::ricocheted,
            .outgoing_velocity = -impact.state.velocity,
            .energy_remaining = kinetic_energy(impact.state, impact.config),
        };
    }
};

/// A broken model: answers with a velocity that is not finite.
class Broken final : public IMaterialInteraction {
public:
    [[nodiscard]] InteractionResult resolve(const Impact& /*impact*/) const override {
        return InteractionResult{
            .outcome = InteractionOutcome::ricocheted,
            .outgoing_velocity = {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
            .energy_remaining = 0.0,
        };
    }
};

} // namespace

TEST_CASE("a simulation uses the interaction model it is given", "[simulation][impacts]") {
    Scene scene;
    const Reverse reverse;
    scene.simulation.set_interaction(&reverse);
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -5.0}, 1));
    const ProjectileId id = scene.fire({0.1, 0.0, 0.0}, {10.0, 0.0, 0.0}, 1.0, 0.0);
    scene.run(1.0);
    REQUIRE(scene.events.size() == 1);
    CHECK(scene.events.front().kind == ProjectileEventKind::ricochet);
    // It struck the wall at t = 0.49 s and has been flying back, from just clear of it, since.
    CHECK_THAT(
        scene.simulation.find(id)->state.position.x, WithinAbs(5.0 - skin - 10.0 * 0.51, 1e-9)
    );
    CHECK_THAT(scene.simulation.find(id)->state.velocity, is_near(Vec3{-10.0, 0.0, 0.0}, 1e-12));
}

TEST_CASE("a model's velocity that is not finite stops the projectile", "[simulation][impacts]") {
    Scene scene;
    const Broken broken;
    scene.simulation.set_interaction(&broken);
    REQUIRE_FALSE(scene.world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -5.0}, 1));
    const ProjectileId id = scene.fire({0.1, 0.0, 0.0}, {10.0, 0.0, 0.0}, 1.0, 0.0);
    scene.run(1.0);
    REQUIRE(scene.events.size() == 1);
    CHECK(scene.events.front().kind == ProjectileEventKind::hit);
    CHECK(require_value(scene.events.front().interaction) == InteractionResult{});
    CHECK_FALSE(scene.simulation.contains(id));
}

TEST_CASE("the number of impacts per step is validated", "[simulation][impacts]") {
    SimulationConfig config;
    CHECK(config.max_impacts_per_step == 4);
    config.max_impacts_per_step = 0;
    CHECK(Simulation::create(config).error() == Error::invalid_max_impacts);
}
