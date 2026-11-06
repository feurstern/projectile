// The integrators: their update rules, their exact error under gravity, their order of
// convergence, their stability limits, and how they treat lifetimes and invalid time steps.

#include "projectile/ballistics/integrator.hpp"
#include "projectile/ballistics/trajectory.hpp"
#include "projectile/core/constants.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"

#include <array>
#include <cmath>
#include <limits>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr Vec3 gravity{0.0, -standard_gravity, 0.0};

ProjectileConfig config_with_gravity(const Vec3& acceleration) {
    ProjectileConfig config;
    config.gravity = acceleration;
    return config;
}

/// An active projectile at @p position moving at @p velocity, with an unlimited lifetime.
ProjectileState moving(const Vec3& position, const Vec3& velocity = Vec3{}) {
    ProjectileState state;
    state.position = position;
    state.velocity = velocity;
    return state;
}

/// Advances @p state by @p steps steps of @p dt with @p integrator under @p config.
template <typename Integrator>
ProjectileState run(
    const Integrator& integrator,
    ProjectileState state,
    const ProjectileConfig& config,
    double dt,
    int steps
) {
    for (int i = 0; i < steps; ++i) {
        integrator.step(state, config, dt);
    }
    return state;
}

/// A harmonic oscillator, a = -ω² x: the classic test of how integrators treat oscillation.
struct Oscillator {
    double omega_squared = 1.0;
    [[nodiscard]] Vec3 operator()(const Vec3& position, const Vec3& /*velocity*/) const noexcept {
        return position * -omega_squared;
    }
};

/// Linear drag, a = -k v, with k = 1/s. Starting at rest at the origin with velocity v0, the
/// exact motion is v(t) = v0 e^(-t) and x(t) = v0 (1 - e^(-t)): a known solution with a
/// velocity-dependent force.
struct LinearDrag {
    [[nodiscard]] Vec3 operator()(const Vec3& /*position*/, const Vec3& velocity) const noexcept {
        return -velocity;
    }
};

/// Position error after one second of linear drag integrated with time step 1 / @p steps.
template <typename Integrator>
double drag_error(const Integrator& integrator, int steps) {
    const Vec3 initial_velocity{10.0, 0.0, 0.0};
    ProjectileState state = moving(Vec3{}, initial_velocity);
    const double dt = 1.0 / static_cast<double>(steps);
    for (int i = 0; i < steps; ++i) {
        integrator.step(state, dt, LinearDrag{});
    }
    return distance(state.position, initial_velocity * (1.0 - std::exp(-1.0)));
}

/// Largest |x| reached by a one-dimensional oscillator with ω = 1 stepped @p steps times with
/// time step @p omega_dt, starting at x = 1 at rest.
template <typename Integrator>
double largest_amplitude(const Integrator& integrator, double omega_dt, int steps) {
    const Oscillator field{.omega_squared = 1.0};
    ProjectileState state = moving(Vec3{1.0, 0.0, 0.0});
    double largest = 1.0;
    for (int i = 0; i < steps && largest < 1e6; ++i) {
        integrator.step(state, omega_dt, field);
        largest = (std::max)(largest, std::abs(state.position.x));
    }
    return largest;
}

} // namespace

// --- Update rules ---------------------------------------------------------------------------

TEST_CASE("without forces, both integrators move in a straight line", "[ballistics][integrator]") {
    const ProjectileConfig no_forces;
    const ProjectileState start = moving(Vec3{1.0, 2.0, 3.0}, Vec3{2.0, -1.0, 0.5});
    const Vec3 expected = start.position + start.velocity * 2.0;

    const ProjectileState euler = run(SemiImplicitEuler{}, start, no_forces, 0.25, 8);
    CHECK(euler.position == expected);
    CHECK(euler.velocity == start.velocity);

    const ProjectileState rk4 = run(RungeKutta4{}, start, no_forces, 0.25, 8);
    CHECK_THAT(rk4.position, is_near(expected, 1e-14, 0.0));
    CHECK(rk4.velocity == start.velocity);
}

TEST_CASE("semi-implicit Euler moves with the updated velocity", "[ballistics][integrator]") {
    // v1 = v0 + a dt, then x1 = x0 + v1 dt: unlike explicit Euler, the position uses the new
    // velocity.
    const ProjectileConfig config = config_with_gravity({0.0, -8.0, 0.0});
    ProjectileState state = moving(Vec3{0.0, 10.0, 0.0}, Vec3{3.0, 4.0, 0.0});
    SemiImplicitEuler{}.step(state, config, 0.5);
    CHECK(state.velocity == Vec3{3.0, 0.0, 0.0});  // 4 - 8 * 0.5
    CHECK(state.position == Vec3{1.5, 10.0, 0.0}); // explicit Euler would give y = 12
}

TEST_CASE("the integrators keep the state's acceleration up to date", "[ballistics][integrator]") {
    const Oscillator field{.omega_squared = 4.0};
    ProjectileState euler = moving(Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0});
    ProjectileState rk4 = euler;
    SemiImplicitEuler{}.step(euler, 0.1, field);
    RungeKutta4{}.step(rk4, 0.1, field);
    CHECK(euler.acceleration == field(euler.position, euler.velocity));
    CHECK(rk4.acceleration == field(rk4.position, rk4.velocity));
}

TEST_CASE("IntegratorKind selects the integrator", "[ballistics][integrator]") {
    const ProjectileConfig config = config_with_gravity(gravity);
    const ProjectileState start = moving(Vec3{0.0, 100.0, 0.0}, Vec3{40.0, 30.0, 0.0});

    ProjectileState by_kind = start;
    step(IntegratorKind::semi_implicit_euler, by_kind, config, 0.1);
    CHECK(by_kind == run(SemiImplicitEuler{}, start, config, 0.1, 1));

    by_kind = start;
    step(IntegratorKind::rk4, by_kind, config, 0.1);
    CHECK(by_kind == run(RungeKutta4{}, start, config, 0.1, 1));
}

// --- Accuracy under gravity -------------------------------------------------------------------

TEST_CASE(
    "semi-implicit Euler's error under gravity is exactly ½ g t dt",
    "[ballistics][integrator][numerical]"
) {
    const ProjectileConfig config = config_with_gravity(gravity);
    const ProjectileState start = moving(Vec3{0.0, 50.0, 0.0}, Vec3{20.0, 15.0, -5.0});
    constexpr double duration = 2.0;
    for (const int steps_per_second : {30, 60, 120, 240}) {
        const double dt = 1.0 / steps_per_second;
        const ProjectileState end =
            run(SemiImplicitEuler{}, start, config, dt, 2 * steps_per_second);
        const Vec3 exact = ballistic_position(start.position, start.velocity, gravity, duration);
        const Vec3 predicted_error = gravity * (0.5 * duration * dt);
        CAPTURE(steps_per_second);
        // Up to 480 steps of rounding at coordinates near 50 m stay well below 1e-10.
        CHECK_THAT(end.position - exact, is_near(predicted_error, 1e-10, 0.0));
        CHECK_THAT(
            end.velocity, is_near(ballistic_velocity(start.velocity, gravity, duration), 1e-10, 0.0)
        );
    }
}

TEST_CASE("RK4 is exact under gravity", "[ballistics][integrator][numerical]") {
    const ProjectileConfig config = config_with_gravity(gravity);
    const ProjectileState start = moving(Vec3{0.0, 50.0, 0.0}, Vec3{20.0, 15.0, -5.0});
    const ProjectileState end = run(RungeKutta4{}, start, config, 1.0 / 30.0, 60);
    CHECK_THAT(
        end.position,
        is_near(ballistic_position(start.position, start.velocity, gravity, 2.0), 1e-10, 0.0)
    );
    CHECK_THAT(end.velocity, is_near(ballistic_velocity(start.velocity, gravity, 2.0), 1e-10, 0.0));
}

TEST_CASE("long flights stay finite and accurate", "[ballistics][integrator][numerical]") {
    // Ten minutes at 120 Hz: 72,000 steps of a fast projectile.
    const ProjectileConfig config = config_with_gravity(gravity);
    const ProjectileState start = moving(Vec3{}, Vec3{800.0, 400.0, 0.0});
    constexpr double dt = 1.0 / 120.0;
    constexpr double duration = 600.0;
    const Vec3 exact = ballistic_position(start.position, start.velocity, gravity, duration);

    const ProjectileState euler = run(SemiImplicitEuler{}, start, config, dt, 72'000);
    CHECK_THAT(euler.position - exact, is_near(gravity * (0.5 * duration * dt), 1e-3, 0.0));

    const ProjectileState rk4 = run(RungeKutta4{}, start, config, dt, 72'000);
    CHECK_THAT(rk4.position, is_near(exact, 1e-3, 0.0));
}

// --- Convergence and stability with position-dependent forces --------------------------------

TEST_CASE("semi-implicit Euler converges with order one", "[ballistics][integrator][numerical]") {
    // Halving the time step halves the error.
    const double coarse = drag_error(SemiImplicitEuler{}, 100);
    const double fine = drag_error(SemiImplicitEuler{}, 200);
    CHECK_THAT(std::log2(coarse / fine), WithinAbs(1.0, 0.1));
}

TEST_CASE("RK4 converges with order four", "[ballistics][integrator][numerical]") {
    // Halving the time step divides the error by 16.
    const double coarse = drag_error(RungeKutta4{}, 50);
    const double fine = drag_error(RungeKutta4{}, 100);
    CHECK_THAT(std::log2(coarse / fine), WithinAbs(4.0, 0.1));
}

TEST_CASE(
    "semi-implicit Euler keeps an oscillator's energy bounded",
    "[ballistics][integrator][numerical]"
) {
    // 10,000 steps with ω dt = 0.1: about 160 periods. A non-symplectic method would drift.
    const Oscillator field{.omega_squared = 1.0};
    ProjectileState state = moving(Vec3{1.0, 0.0, 0.0});
    double worst = 0.0;
    for (int i = 0; i < 10'000; ++i) {
        SemiImplicitEuler{}.step(state, 0.1, field);
        const double energy =
            0.5 * (length_squared(state.velocity) + length_squared(state.position));
        worst = (std::max)(worst, std::abs(energy / 0.5 - 1.0));
    }
    CHECK(worst < 0.06);
}

TEST_CASE(
    "the integrators are stable within their documented limits",
    "[ballistics][integrator][numerical]"
) {
    // Semi-implicit Euler is stable for ω dt < 2, RK4 for ω dt < 2√2 ≈ 2.83.
    CHECK(largest_amplitude(SemiImplicitEuler{}, 1.9, 10'000) < 5.0);
    CHECK(largest_amplitude(SemiImplicitEuler{}, 2.1, 10'000) > 1e3);
    CHECK(largest_amplitude(RungeKutta4{}, 2.7, 10'000) <= 1.0);
    CHECK(largest_amplitude(RungeKutta4{}, 3.0, 10'000) > 1e3);
}

// --- Lifetime and invalid steps ------------------------------------------------------------------

TEST_CASE("a step never outlasts the remaining lifetime", "[ballistics][integrator][lifetime]") {
    ProjectileConfig config = config_with_gravity(gravity);
    config.lifetime = 0.1;
    const ProjectileState start =
        require_value(create_projectile(config, Vec3{}, Vec3{10.0, 20.0, 0.0}));

    ProjectileState state = run(RungeKutta4{}, start, config, 0.03, 3);
    CHECK(state.active);
    CHECK_THAT(state.remaining_lifetime, WithinAbs(0.01, 1e-15));

    // The fourth step advances only the 0.01 s left, then the projectile expires.
    RungeKutta4{}.step(state, config, 0.03);
    CHECK_FALSE(state.active);
    CHECK(state.remaining_lifetime == 0.0);
    CHECK_THAT(
        state.position,
        is_near(ballistic_position(start.position, start.velocity, gravity, 0.1), 1e-14, 0.0)
    );

    // An expired projectile no longer moves.
    const ProjectileState expired = state;
    SemiImplicitEuler{}.step(state, config, 0.03);
    CHECK(state == expired);
}

TEST_CASE("an infinite lifetime never runs out", "[ballistics][integrator][lifetime]") {
    const ProjectileConfig config = config_with_gravity(Vec3{});
    const ProjectileState state = run(SemiImplicitEuler{}, ProjectileState{}, config, 1000.0, 1000);
    CHECK(state.active);
    CHECK(state.remaining_lifetime == std::numeric_limits<double>::infinity());
}

TEST_CASE("invalid time steps leave the state unchanged", "[ballistics][integrator]") {
    const ProjectileConfig config = config_with_gravity(gravity);
    const ProjectileState start = moving(Vec3{1.0, 2.0, 3.0}, Vec3{4.0, 5.0, 6.0});
    constexpr std::array invalid_steps{
        0.0,
        -0.1,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
    };
    for (const double dt : invalid_steps) {
        CAPTURE(dt);
        ProjectileState euler = start;
        SemiImplicitEuler{}.step(euler, config, dt);
        CHECK(euler == start);
        ProjectileState rk4 = start;
        RungeKutta4{}.step(rk4, config, dt);
        CHECK(rk4 == start);
    }
}
