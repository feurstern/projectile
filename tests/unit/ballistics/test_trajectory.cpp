#include "projectile/ballistics/trajectory.hpp"
#include "projectile/collision/intersection.hpp"
#include "projectile/core/constants.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"
#include "support/random.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr Vec3 gravity{0.0, -standard_gravity, 0.0};
constexpr double infinity = std::numeric_limits<double>::infinity();

ProjectileConfig falling() {
    ProjectileConfig config;
    config.gravity = gravity;
    return config;
}

/// The ground plane y = 0, facing up, made of material 5. Test projectiles start above it, so
/// sphere sweeps only need the plane raised by the radius.
class GroundWorld final : public ICollisionWorld {
public:
    [[nodiscard]] std::optional<HitResult> raycast(
        const Ray& ray, double max_distance
    ) const override {
        std::optional<HitResult> hit = intersect(ray, ground_, max_distance);
        if (hit) {
            hit->material_id = 5;
        }
        return hit;
    }

    [[nodiscard]] std::optional<HitResult> sweep_sphere(
        const Vec3& start, const Vec3& end, double radius
    ) const override {
        const Plane raised{.normal = ground_.normal, .distance = ground_.distance + radius};
        std::optional<HitResult> hit = intersect(Segment{.start = start, .end = end}, raised);
        if (hit) {
            hit->point -= hit->normal * radius; // from the center to the contact on the ground
            hit->material_id = 5;
        }
        return hit;
    }

private:
    Plane ground_{.normal = {0.0, 1.0, 0.0}, .distance = 0.0};
};

} // namespace

// --- Exact motion
// ---------------------------------------------------------------------------------

TEST_CASE(
    "ballistic_position and ballistic_velocity give the exact motion", "[ballistics][trajectory]"
) {
    constexpr Vec3 velocity{10.0, 20.0, 0.0};
    constexpr Vec3 acceleration{0.0, -10.0, 0.0};
    STATIC_REQUIRE(
        ballistic_position(Vec3{}, velocity, acceleration, 2.0) == Vec3{20.0, 20.0, 0.0}
    );
    STATIC_REQUIRE(ballistic_velocity(velocity, acceleration, 2.0) == Vec3{10.0, 0.0, 0.0});
    STATIC_REQUIRE(
        ballistic_position(Vec3{1.0, 2.0, 3.0}, velocity, acceleration, 0.0) == Vec3{1.0, 2.0, 3.0}
    );
}

// --- Sampling -----------------------------------------------------------------------------------

TEST_CASE("a prediction samples every interval up to its duration", "[ballistics][trajectory]") {
    const ProjectileState start =
        require_value(create_projectile(falling(), Vec3{0.0, 10.0, 0.0}, Vec3{30.0, 20.0, 0.0}));
    const TrajectoryConfig settings; // 1/60 s samples for 5 s
    const Trajectory trajectory = require_value(predict_trajectory(start, falling(), settings));

    REQUIRE(trajectory.samples.size() == 301);
    CHECK(
        trajectory.samples.front() ==
        TrajectorySample{.time = 0.0, .position = start.position, .velocity = start.velocity}
    );
    for (std::size_t k = 0; k < trajectory.samples.size(); ++k) {
        CHECK_THAT(trajectory.samples.at(k).time, WithinAbs(static_cast<double>(k) / 60.0, 1e-12));
    }
    CHECK(trajectory.end == TrajectoryEnd::max_duration);
    CHECK_FALSE(trajectory.hit.has_value());
}

TEST_CASE(
    "RK4 predictions match the exact path at every sample", "[ballistics][trajectory][numerical]"
) {
    // Samples every 0.05 s fall between the 1/120 s steps; the interpolation is exact too.
    const ProjectileState start =
        require_value(create_projectile(falling(), Vec3{0.0, 10.0, 0.0}, Vec3{30.0, 20.0, -4.0}));
    TrajectoryConfig settings;
    settings.sample_interval = 0.05;
    settings.max_duration = 3.0;
    settings.integrator = IntegratorKind::rk4;
    const Trajectory trajectory = require_value(predict_trajectory(start, falling(), settings));

    REQUIRE(trajectory.samples.size() == 61);
    for (const TrajectorySample& sample : trajectory.samples) {
        CAPTURE(sample);
        CHECK_THAT(
            sample.position,
            is_near(
                ballistic_position(start.position, start.velocity, gravity, sample.time), 1e-10, 0.0
            )
        );
        CHECK_THAT(
            sample.velocity,
            is_near(ballistic_velocity(start.velocity, gravity, sample.time), 1e-10, 0.0)
        );
    }
}

TEST_CASE("Euler predictions reproduce a simulation's steps exactly", "[ballistics][trajectory]") {
    const ProjectileConfig config = falling();
    const ProjectileState start =
        require_value(create_projectile(config, Vec3{0.0, 10.0, 0.0}, Vec3{30.0, 20.0, 0.0}));
    TrajectoryConfig settings;
    settings.sample_interval = settings.time_step; // one sample per step
    settings.max_duration = 1.0;
    const Trajectory trajectory = require_value(predict_trajectory(start, config, settings));

    REQUIRE(trajectory.samples.size() == 121);
    ProjectileState simulated = start;
    for (std::size_t k = 1; k < trajectory.samples.size(); ++k) {
        SemiImplicitEuler{}.step(simulated, config, settings.time_step);
        CAPTURE(k);
        CHECK(trajectory.samples.at(k).position == simulated.position);
        CHECK(trajectory.samples.at(k).velocity == simulated.velocity);
    }
    // And they carry Euler's known error, ½ g t dt.
    const TrajectorySample& last = trajectory.samples.back();
    const Vec3 exact = ballistic_position(start.position, start.velocity, gravity, last.time);
    CHECK_THAT(
        last.position - exact, is_near(gravity * (0.5 * last.time * settings.time_step), 1e-10, 0.0)
    );
}

// --- Ends -----------------------------------------------------------------------------------

TEST_CASE("a prediction ends when the projectile expires", "[ballistics][trajectory]") {
    ProjectileConfig config = falling();
    config.lifetime = 1.234;
    const ProjectileState start =
        require_value(create_projectile(config, Vec3{}, Vec3{10.0, 10.0, 0.0}));
    TrajectoryConfig settings;
    settings.integrator = IntegratorKind::rk4;
    const Trajectory trajectory = require_value(predict_trajectory(start, config, settings));

    CHECK(trajectory.end == TrajectoryEnd::lifetime);
    const TrajectorySample& last = trajectory.samples.back();
    CHECK_THAT(last.time, WithinAbs(1.234, 1e-12));
    CHECK_THAT(
        last.position,
        is_near(ballistic_position(start.position, start.velocity, gravity, 1.234), 1e-10, 0.0)
    );
}

TEST_CASE(
    "an inactive projectile or a zero duration predicts only the start", "[ballistics][trajectory]"
) {
    ProjectileState inactive = require_value(create_projectile(falling(), Vec3{1.0, 2.0, 3.0}));
    inactive.active = false;
    const Trajectory expired =
        require_value(predict_trajectory(inactive, falling(), TrajectoryConfig{}));
    CHECK(expired.samples.size() == 1);
    CHECK(expired.end == TrajectoryEnd::lifetime);

    TrajectoryConfig instant;
    instant.max_duration = 0.0;
    const Trajectory start_only = require_value(
        predict_trajectory(require_value(create_projectile(falling(), Vec3{})), falling(), instant)
    );
    CHECK(start_only.samples.size() == 1);
    CHECK(start_only.end == TrajectoryEnd::max_duration);
}

TEST_CASE(
    "a prediction stops where the projectile hits the world", "[ballistics][trajectory][collision]"
) {
    const GroundWorld world;
    const ProjectileState start =
        require_value(create_projectile(falling(), Vec3{0.0, 10.0, 0.0}, Vec3{5.0, 0.0, 0.0}));
    TrajectoryConfig settings;
    settings.integrator = IntegratorKind::rk4;
    const Trajectory trajectory =
        require_value(predict_trajectory(start, falling(), settings, world));

    REQUIRE(trajectory.end == TrajectoryEnd::hit);
    const HitResult hit = require_value(trajectory.hit);
    CHECK(hit.material_id == 5);
    CHECK(hit.normal == Vec3{0.0, 1.0, 0.0});

    // Falling 10 m takes sqrt(2 · 10 / g). Between steps the query sees the straight chord, which
    // shifts the impact by microseconds.
    const double impact_time = std::sqrt(2.0 * 10.0 / standard_gravity);
    const TrajectorySample& last = trajectory.samples.back();
    CHECK_THAT(last.time, WithinAbs(impact_time, 1e-4));
    CHECK_THAT(last.position.y, WithinAbs(0.0, 1e-12));
    CHECK_THAT(last.position.x, WithinAbs(5.0 * impact_time, 1e-3));
    for (std::size_t k = 0; k + 1 < trajectory.samples.size(); ++k) {
        CHECK(trajectory.samples.at(k).position.y > 0.0);
    }
}

TEST_CASE(
    "a projectile with a radius stops when its surface touches",
    "[ballistics][trajectory][collision]"
) {
    const GroundWorld world;
    ProjectileConfig config = falling();
    config.radius = 0.5;
    const ProjectileState start = require_value(create_projectile(config, Vec3{0.0, 10.0, 0.0}));
    const Trajectory trajectory =
        require_value(predict_trajectory(start, config, TrajectoryConfig{}, world));

    const HitResult hit = require_value(trajectory.hit);
    CHECK_THAT(trajectory.samples.back().position.y, WithinAbs(0.5, 1e-12)); // the center
    CHECK_THAT(hit.point.y, WithinAbs(0.0, 1e-12));                          // the contact
}

TEST_CASE("predictions can refill a trajectory without reallocating", "[ballistics][trajectory]") {
    const ProjectileConfig config = falling();
    Trajectory buffer;
    REQUIRE_FALSE(predict_trajectory(
        require_value(create_projectile(config, Vec3{}, Vec3{10.0, 10.0, 0.0})),
        config,
        TrajectoryConfig{},
        buffer
    ));
    const TrajectorySample* storage = buffer.samples.data();
    const std::size_t capacity = buffer.samples.capacity();
    const std::vector<TrajectorySample> first = buffer.samples;

    REQUIRE_FALSE(predict_trajectory(
        require_value(create_projectile(config, Vec3{}, Vec3{12.0, 8.0, 0.0})),
        config,
        TrajectoryConfig{},
        buffer
    ));
    CHECK(buffer.samples.data() == storage);
    CHECK(buffer.samples.capacity() == capacity);
    CHECK(buffer.samples != first);
}

TEST_CASE(
    "invalid input is rejected with its reason, leaving the output untouched",
    "[ballistics][trajectory]"
) {
    const ProjectileConfig config = falling();
    const ProjectileState start =
        require_value(create_projectile(config, Vec3{}, Vec3{10.0, 10.0, 0.0}));
    Trajectory buffer;
    REQUIRE_FALSE(predict_trajectory(start, config, TrajectoryConfig{}, buffer));
    const std::vector<TrajectorySample> before = buffer.samples;

    TrajectoryConfig settings;
    settings.sample_interval = 0.0;
    CHECK(predict_trajectory(start, config, settings, buffer) == Error::invalid_sample_interval);
    settings = TrajectoryConfig{};
    settings.time_step = -0.01;
    CHECK(predict_trajectory(start, config, settings, buffer) == Error::invalid_time_step);
    settings = TrajectoryConfig{};
    settings.max_duration = infinity;
    CHECK(predict_trajectory(start, config, settings, buffer) == Error::invalid_duration);

    ProjectileConfig heavy_nothing = config;
    heavy_nothing.mass = 0.0;
    CHECK(
        predict_trajectory(start, heavy_nothing, TrajectoryConfig{}).error() == Error::invalid_mass
    );
    ProjectileState lost = start;
    lost.position.x = std::numeric_limits<double>::quiet_NaN();
    CHECK(predict_trajectory(lost, config, TrajectoryConfig{}).error() == Error::invalid_position);

    CHECK(buffer.samples == before);
}

TEST_CASE(
    "samples are evenly spaced and never share a time", "[ballistics][trajectory][property]"
) {
    test::Random random;
    for (int i = 0; i < 2'000; ++i) {
        TrajectoryConfig settings;
        settings.sample_interval = random.uniform(0.01, 0.5);
        settings.time_step = random.uniform(0.001, 0.2);
        settings.max_duration = random.uniform(0.0, 3.0);
        settings.integrator =
            random.integer(0, 1) == 0 ? IntegratorKind::semi_implicit_euler : IntegratorKind::rk4;
        ProjectileConfig config = falling();
        config.lifetime = random.uniform(0.0, 1.0) < 0.3 ? random.uniform(0.01, 3.0) : infinity;
        const ProjectileState start = require_value(
            create_projectile(config, random.vec3(-10.0, 10.0), random.vec3(-50.0, 50.0))
        );
        CAPTURE(
            settings.sample_interval, settings.time_step, settings.max_duration, config.lifetime
        );

        const Trajectory trajectory = require_value(predict_trajectory(start, config, settings));
        const std::vector<TrajectorySample>& samples = trajectory.samples;
        REQUIRE_FALSE(samples.empty());
        CHECK(samples.front().time == 0.0);
        CHECK_THAT(
            samples.back().time, WithinAbs((std::min)(settings.max_duration, config.lifetime), 1e-9)
        );
        if (std::abs(config.lifetime - settings.max_duration) > 1e-6) {
            CHECK(
                trajectory.end == (config.lifetime < settings.max_duration
                                       ? TrajectoryEnd::lifetime
                                       : TrajectoryEnd::max_duration)
            );
        }
        for (std::size_t k = 1; k < samples.size(); ++k) {
            CHECK(samples.at(k).time > samples.at(k - 1).time);
            if (k + 1 < samples.size()) {
                CHECK_THAT(
                    samples.at(k).time,
                    WithinAbs(static_cast<double>(k) * settings.sample_interval, 1e-9)
                );
            }
        }
    }
}
