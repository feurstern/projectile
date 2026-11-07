// Aerodynamic drag and wind: the force law, exact solutions, convergence, and energy bounds.

#include "projectile/ballistics/energy.hpp"
#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/integrator.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/trajectory.hpp"
#include "projectile/core/constants.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"

#include <cmath>
#include <cstddef>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

/// A projectile with the drag factor ½ ρ C_d A / m = @p factor, in 1/m, and no gravity.
ProjectileConfig with_drag_factor(double factor) {
    ProjectileConfig config;
    config.mass = 2.0;
    config.drag_coefficient = 0.5;
    config.reference_area = 4.0;
    config.air_density = factor * 2.0; // ½ · ρ · 0.5 · 4 / 2 = ρ / 2
    return config;
}

/// Position along the path, at time @p time, of a body that starts at @p speed under quadratic
/// drag `dv/dt = −k v²` alone: `x(t) = ln(1 + k v₀ t) / k`.
double drag_distance(double k, double speed, double time) {
    return std::log1p(k * speed * time) / k;
}

/// Error after one second of stepping the projectile above with @p steps steps of integrator
/// @p Integrator, from 10 m/s against drag factor 0.5.
template <typename Integrator>
double horizontal_error(int steps) {
    const ProjectileConfig config = with_drag_factor(0.5);
    ProjectileState state = require_value(create_projectile(config, Vec3{}, Vec3{10.0, 0.0, 0.0}));
    for (int i = 0; i < steps; ++i) {
        Integrator{}.step(state, config, 1.0 / steps);
    }
    return std::abs(state.position.x - drag_distance(0.5, 10.0, 1.0));
}

} // namespace

// --- The force --------------------------------------------------------------------------------

TEST_CASE(
    "the drag factor is half the density, coefficient and area over the mass", "[ballistics][drag]"
) {
    ProjectileConfig config;
    config.mass = 0.01;
    config.drag_coefficient = 0.3;
    config.reference_area = 2e-4;
    config.air_density = standard_air_density;
    CHECK_THAT(drag_factor(config), WithinRel(0.5 * 1.225 * 0.3 * 2e-4 / 0.01, 1e-15));
    CHECK(forces_of(config).drag_factor == drag_factor(config));

    config.drag_coefficient = 0.0;
    CHECK(drag_factor(config) == 0.0);
    config.drag_coefficient = 0.3;
    config.air_density = 0.0; // a vacuum
    CHECK(drag_factor(config) == 0.0);
}

TEST_CASE("drag opposes the velocity relative to the wind, quadratically", "[ballistics][drag]") {
    const ProjectileForces forces{
        .gravity = {0.0, -10.0, 0.0},
        .drag_factor = 0.5,
        .wind = {2.0, 0.0, 0.0},
    };
    // Moving at 5 m/s against the air (7 m/s relative to it): drag is 0.5 · 7² = 24.5 m/s².
    const Vec3 acceleration = forces(Vec3{}, Vec3{-5.0, 0.0, 0.0});
    CHECK(acceleration == Vec3{24.5, -10.0, 0.0});
    // Moving with the wind: no drag at all.
    CHECK(forces(Vec3{}, Vec3{2.0, 0.0, 0.0}) == Vec3{0.0, -10.0, 0.0});
    // In general the drag is antiparallel to the relative velocity, with magnitude k |u|².
    const Vec3 velocity{3.0, 4.0, -12.0};
    const Vec3 relative = velocity - forces.wind;
    const Vec3 drag = forces(Vec3{}, velocity) - forces.gravity;
    CHECK_THAT(length(drag), WithinRel(0.5 * length_squared(relative), 1e-14));
    CHECK_THAT(dot(drag, relative), WithinRel(-length(drag) * length(relative), 1e-14));
}

TEST_CASE("without drag, wind and air density change nothing", "[ballistics][drag]") {
    ProjectileConfig config;
    config.gravity = {0.0, 0.0, -standard_gravity};
    config.wind = {30.0, -4.0, 0.0};
    config.air_density = 3.0;
    const ProjectileForces forces = forces_of(config);
    CHECK(forces(Vec3{1.0, 2.0, 3.0}, Vec3{100.0, 0.0, 50.0}) == config.gravity);
}

// --- Exact solutions --------------------------------------------------------------------------

TEST_CASE("drag slows a projectile as the exact solution does", "[ballistics][drag][numerical]") {
    // dv/dt = −k v² gives v(t) = v₀ / (1 + k v₀ t).
    constexpr double k = 0.5;
    constexpr double speed = 10.0;
    const ProjectileConfig config = with_drag_factor(k);
    ProjectileState state = require_value(create_projectile(config, Vec3{}, Vec3{speed, 0.0, 0.0}));
    for (int i = 0; i < 240; ++i) {
        RungeKutta4{}.step(state, config, 1.0 / 120.0);
    }
    CHECK_THAT(state.position.x, WithinAbs(drag_distance(k, speed, 2.0), 1e-7));
    CHECK_THAT(state.velocity.x, WithinAbs(speed / (1.0 + k * speed * 2.0), 1e-8));
    CHECK(state.position.y == 0.0);
}

TEST_CASE(
    "a falling projectile approaches terminal velocity without exceeding it",
    "[ballistics][drag][numerical]"
) {
    // Falling from rest under g with drag factor k: v(t) = v_t tanh(g t / v_t), with the
    // terminal velocity v_t = √(g / k), and the drop is (v_t² / g) ln cosh(g t / v_t).
    ProjectileConfig config = with_drag_factor(0.1);
    config.gravity = {0.0, -10.0, 0.0};
    constexpr double terminal = 10.0; // √(10 / 0.1)
    const ProjectileState start = require_value(create_projectile(config, Vec3{}, Vec3{}));

    ProjectileState euler = start;
    ProjectileState rk4 = start;
    for (int i = 1; i <= 360; ++i) {
        SemiImplicitEuler{}.step(euler, config, 1.0 / 120.0);
        RungeKutta4{}.step(rk4, config, 1.0 / 120.0);
        CHECK(length(euler.velocity) < terminal);
        CHECK(length(rk4.velocity) < terminal);
    }
    const double drop = (terminal * terminal / 10.0) * std::log(std::cosh(10.0 * 3.0 / terminal));
    CHECK_THAT(rk4.position.y, WithinAbs(-drop, 1e-8));
    CHECK_THAT(rk4.velocity.y, WithinAbs(-terminal * std::tanh(3.0), 1e-9));
    CHECK_THAT(euler.position.y, WithinAbs(-drop, 0.1)); // first order: a few centimetres off
}

TEST_CASE(
    "wind carries a projectile along until it moves with the air", "[ballistics][drag][numerical]"
) {
    // At rest in a wind w, the relative velocity u = v − w obeys du/dt = −k |u| u, so
    // v(t) = w − w / (1 + k |w| t): the projectile approaches the wind's velocity.
    constexpr double k = 0.5;
    ProjectileConfig config = with_drag_factor(k);
    config.wind = {0.0, 0.0, 5.0};
    ProjectileState state = require_value(create_projectile(config, Vec3{}, Vec3{}));
    for (int i = 0; i < 240; ++i) {
        RungeKutta4{}.step(state, config, 1.0 / 120.0);
        CHECK(state.velocity.z < 5.0);
    }
    CHECK_THAT(
        state.velocity, is_near(Vec3{0.0, 0.0, 5.0 - 5.0 / (1.0 + k * 5.0 * 2.0)}, 1e-8, 0.0)
    );
}

TEST_CASE("integrators converge at their order under drag", "[ballistics][drag][numerical]") {
    // Halving the step halves Euler's error and divides RK4's by 16.
    const double euler_ratio =
        horizontal_error<SemiImplicitEuler>(100) / horizontal_error<SemiImplicitEuler>(200);
    const double rk4_ratio =
        horizontal_error<RungeKutta4>(100) / horizontal_error<RungeKutta4>(200);
    CHECK_THAT(euler_ratio, WithinAbs(2.0, 0.1));
    CHECK_THAT(rk4_ratio, WithinAbs(16.0, 2.0));
}

// --- Energy -----------------------------------------------------------------------------------

TEST_CASE("kinetic energy is half the mass times the squared speed", "[ballistics][energy]") {
    STATIC_REQUIRE(kinetic_energy(2.0, Vec3{3.0, 4.0, 0.0}) == 25.0);
    ProjectileConfig config;
    config.mass = 0.5;
    const ProjectileState state =
        require_value(create_projectile(config, Vec3{}, Vec3{0.0, 0.0, -4.0}));
    CHECK(kinetic_energy(state, config) == 4.0);
    CHECK(speed_for_energy(0.5, 4.0) == 4.0);
    CHECK(speed_for_energy(0.5, 0.0) == 0.0);
    CHECK(speed_for_energy(0.5, -1.0) == 0.0);
}

TEST_CASE("drag only ever takes energy away", "[ballistics][drag][energy]") {
    // Without gravity, every step lowers the kinetic energy relative to the air, for both
    // integrators: in still air, the projectile's own kinetic energy.
    for (const Vec3 wind : {Vec3{}, Vec3{-3.0, 1.0, 0.0}}) {
        ProjectileConfig config = with_drag_factor(0.02);
        config.wind = wind;
        const ProjectileState start =
            require_value(create_projectile(config, Vec3{}, Vec3{40.0, 25.0, -10.0}));
        for (const IntegratorKind kind :
             {IntegratorKind::semi_implicit_euler, IntegratorKind::rk4}) {
            ProjectileState state = start;
            double previous = kinetic_energy(config.mass, state.velocity - wind);
            for (int i = 0; i < 600; ++i) {
                step(kind, state, config, 1.0 / 120.0);
                const double energy = kinetic_energy(config.mass, state.velocity - wind);
                CAPTURE(wind, i);
                CHECK(energy < previous);
                previous = energy;
            }
        }
    }
}

TEST_CASE("predictions include drag and wind", "[ballistics][drag][trajectory]") {
    ProjectileConfig config = with_drag_factor(0.01);
    config.gravity = {0.0, -standard_gravity, 0.0};
    config.wind = {0.0, 0.0, -8.0};
    const ProjectileState start =
        require_value(create_projectile(config, Vec3{}, Vec3{60.0, 30.0, 0.0}));
    TrajectoryConfig settings;
    settings.sample_interval = settings.time_step; // one sample per step
    settings.max_duration = 2.0;
    const Trajectory trajectory = require_value(predict_trajectory(start, config, settings));

    ProjectileState simulated = start;
    for (std::size_t k = 1; k < trajectory.samples.size(); ++k) {
        SemiImplicitEuler{}.step(simulated, config, settings.time_step);
        CHECK(trajectory.samples.at(k).position == simulated.position);
    }
    // The wind blew the projectile sideways.
    CHECK(trajectory.samples.back().position.z < -1.0);
}
