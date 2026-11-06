// Complete simulation workflows: create projectiles, advance fixed steps, detect collisions, end
// flights, and check the final state. Covers the acceptance criteria of Milestone 4:
//
// - many projectiles simulated at once;
// - fast projectiles cannot pass through thin shapes of any supported kind (tunneling);
// - repeated runs reproduce their results exactly;
// - memory stops growing once a simulation reaches its largest size.

#include "projectile/ballistics/trajectory.hpp"
#include "projectile/collision/primitive_world.hpp"
#include "projectile/core/constants.hpp"
#include "projectile/math/math_utils.hpp"
#include "projectile/simulation/simulation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/allocation_counter.hpp"
#include "support/ballistics_support.hpp"
#include "support/random.hpp"
#include "support/simulation_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <utility>
#include <vector>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::require_value;

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr Vec3 gravity{0.0, -standard_gravity, 0.0};

// --- Distances to shapes ------------------------------------------------------------------------

double distance_to(const Plane& plane, const Vec3& point) {
    return std::abs(signed_distance(plane, point));
}

double distance_to(const Sphere& sphere, const Vec3& point) {
    return (std::max)(distance(point, sphere.center) - sphere.radius, 0.0);
}

double distance_to(const AABB& box, const Vec3& point) {
    return distance(point, closest_point(box, point));
}

double distance_to(const Triangle& triangle, const Vec3& point) {
    return distance(point, closest_point(triangle, point));
}

// --- A closed room with obstacles ---------------------------------------------------------------

/// A closed room, 40 m by 40 m and 30 m high with +y up, with a 1 cm thick wall across its
/// middle and an obstacle of every kind. Walls are made of material 1, obstacles of 2 to 5.
struct Room {
    static constexpr double half_width = 20.0;
    static constexpr double height = 30.0;

    AABB thin_wall{.min = {-0.005, 0.0, -10.0}, .max = {0.005, 10.0, 10.0}};
    AABB block{.min = {5.0, 0.0, -12.0}, .max = {12.0, 4.0, -6.0}};
    Sphere ball{.center = {8.0, 5.0, 8.0}, .radius = 2.0};
    Triangle ramp{.a = {-15.0, 0.0, -15.0}, .b = {-5.0, 0.0, -15.0}, .c = {-10.0, 8.0, -5.0}};
    PrimitiveWorld world;

    Room() {
        const double w = half_width;
        REQUIRE_FALSE(world.add(Plane{.normal = {0.0, 1.0, 0.0}, .distance = 0.0}, 1));
        REQUIRE_FALSE(world.add(Plane{.normal = {0.0, -1.0, 0.0}, .distance = -height}, 1));
        REQUIRE_FALSE(world.add(Plane{.normal = {1.0, 0.0, 0.0}, .distance = -w}, 1));
        REQUIRE_FALSE(world.add(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -w}, 1));
        REQUIRE_FALSE(world.add(Plane{.normal = {0.0, 0.0, 1.0}, .distance = -w}, 1));
        REQUIRE_FALSE(world.add(Plane{.normal = {0.0, 0.0, -1.0}, .distance = -w}, 1));
        REQUIRE_FALSE(world.add(thin_wall, 2));
        REQUIRE_FALSE(world.add(block, 3));
        REQUIRE_FALSE(world.add(ball, 4));
        REQUIRE_FALSE(world.add(ramp, 5));
    }

    /// Distance from @p point to the nearest obstacle inside the room.
    [[nodiscard]] double obstacle_distance(const Vec3& point) const {
        return (std::
                    min)({distance_to(thin_wall, point), distance_to(block, point), distance_to(ball, point), distance_to(ramp, point)});
    }

    /// Whether a sphere of @p radius centered at @p point lies in the room, clear of the
    /// obstacles, up to @p tolerance.
    [[nodiscard]] bool clear(const Vec3& point, double radius, double tolerance) const {
        const double w = half_width - radius + tolerance;
        const bool in_room = std::abs(point.x) <= w && std::abs(point.z) <= w &&
                             point.y >= radius - tolerance &&
                             point.y <= height - radius + tolerance;
        return in_room && obstacle_distance(point) >= radius - tolerance;
    }
};

struct Launch {
    ProjectileState state;
    ProjectileConfig config;
};

/// @p count random launches in @p room: fast and slow, small and point-like, short-lived and
/// unending. Each starts clear of every obstacle.
std::vector<Launch> random_launches(const Room& room, std::size_t count) {
    test::Random random;
    std::vector<Launch> launches;
    launches.reserve(count);
    while (launches.size() < count) {
        ProjectileConfig config;
        config.gravity = gravity;
        config.radius = random.integer(0, 3) == 0 ? 0.0 : random.uniform(0.001, 0.1);
        config.lifetime = random.integer(0, 2) == 0 ? random.uniform(0.05, 3.0) : infinity;
        const Vec3 position{
            random.uniform(-18.0, 18.0), random.uniform(1.0, 28.0), random.uniform(-18.0, 18.0)
        };
        if (!room.clear(position, config.radius + 0.01, 0.0)) {
            continue;
        }
        const Vec3 velocity = random.unit_vec3() * random.uniform(5.0, 800.0);
        launches.push_back(
            {.state = require_value(create_projectile(config, position, velocity)),
             .config = config}
        );
    }
    return launches;
}

/// The outcome of running launches in the room until every flight has ended.
struct Outcome {
    std::vector<ProjectileEvent> events;
    std::uint64_t steps = 0;
    std::size_t violations = 0; ///< Step-end positions outside the room or inside an obstacle.
};

/// Simulates @p launches in @p room with @p config until every flight ends, advancing with
/// frames from @p next_frame.
template <typename NextFrame>
Outcome run(
    const Room& room,
    const std::vector<Launch>& launches,
    const SimulationConfig& config,
    NextFrame next_frame
) {
    Outcome outcome;
    Simulation simulation = require_value(Simulation::create(config));
    simulation.set_collision_world(&room.world);
    simulation.set_event_callback([&outcome](const ProjectileEvent& event) {
        outcome.events.push_back(event);
    });
    for (const Launch& launch : launches) {
        REQUIRE(simulation.add_projectile(launch.state, launch.config));
    }
    while (!simulation.projectiles().empty() && simulation.time() < 60.0) {
        simulation.update(next_frame());
        for (const SimulatedProjectile& projectile : simulation.projectiles()) {
            if (!room.clear(projectile.state.position, projectile.config.radius, 1e-9)) {
                ++outcome.violations;
            }
        }
    }
    outcome.steps = simulation.step_count();
    CHECK(simulation.projectiles().empty());
    return outcome;
}

} // namespace

// --- Many projectiles ---------------------------------------------------------------------------

TEST_CASE(
    "hundreds of projectiles fly, collide and expire in a closed room", "[integration][simulation]"
) {
    const Room room;
    const std::vector<Launch> launches = random_launches(room, 500);
    const Outcome outcome = run(room, launches, SimulationConfig{}, [] { return 1.0 / 60.0; });

    // Every flight ended exactly once.
    REQUIRE(outcome.events.size() == launches.size());
    std::set<std::uint64_t> ids;
    for (const ProjectileEvent& event : outcome.events) {
        ids.insert(event.id.value);
    }
    CHECK(ids.size() == launches.size());

    // No projectile ever left the room or entered an obstacle, even the thin wall.
    CHECK(outcome.violations == 0);
    std::size_t hits = 0;
    std::size_t thin_wall_hits = 0;
    for (const ProjectileEvent& event : outcome.events) {
        CAPTURE(event);
        CHECK(room.clear(event.state.position, event.config.radius, 1e-9));
        CHECK_FALSE(event.state.active);
        if (event.kind == ProjectileEventKind::hit) {
            ++hits;
            const HitResult hit = require_value(event.hit);
            thin_wall_hits += hit.material_id == 2 ? 1U : 0U;
            // The contact point is one radius from the center.
            CHECK_THAT(
                distance(hit.point, event.state.position), WithinAbs(event.config.radius, 1e-9)
            );
        } else {
            CHECK(event.config.lifetime < infinity); // unending projectiles must land somewhere
            CHECK(event.state.remaining_lifetime == 0.0);
        }
    }
    CHECK(hits > 250);
    CHECK(thin_wall_hits > 0);
}

// --- Tunneling ----------------------------------------------------------------------------------

namespace {

/// Fires a projectile of @p radius along +x at @p speed from the origin, through a world that
/// holds only @p shape, and returns the event that ends its flight. Also checks that the shape
/// lies between two step ends, so that testing only the positions the projectile ends its steps
/// at would miss it: a sweep is necessary.
template <typename Shape>
ProjectileEvent fire_through(const Shape& shape, double radius, double speed) {
    PrimitiveWorld world;
    REQUIRE_FALSE(world.add(shape, 9));
    ProjectileConfig config;
    config.radius = radius;
    config.lifetime = 1.0;
    const ProjectileState start =
        require_value(create_projectile(config, Vec3{}, Vec3{speed, 0.0, 0.0}));

    // Without the world, no step ends within reach of the shape.
    ProjectileState free_flight = start;
    double step_end_gap = infinity;
    const double time_step = SimulationConfig{}.time_step;
    for (int i = 0; i < 120; ++i) {
        SemiImplicitEuler{}.step(free_flight, config, time_step);
        step_end_gap = (std::min)(step_end_gap, distance_to(shape, free_flight.position));
    }
    CHECK(step_end_gap > radius);

    Simulation simulation;
    simulation.set_collision_world(&world);
    std::optional<ProjectileEvent> ended;
    simulation.set_event_callback([&ended](const ProjectileEvent& event) { ended = event; });
    REQUIRE(simulation.add_projectile(start, config));
    while (!ended && simulation.time() < 2.0) {
        simulation.update(1.0 / 60.0);
    }
    return require_value(ended);
}

} // namespace

TEST_CASE(
    "fast projectiles cannot pass through thin shapes", "[integration][simulation][tunneling]"
) {
    // At 1,000 m/s a step at 120 Hz covers 8.3 m; at 10,000 m/s, 83 m. The shapes are 10 m away
    // and at most 1 cm thick.
    for (const double speed : {1'000.0, 10'000.0}) {
        for (const double radius : {0.0, 0.02}) {
            CAPTURE(speed, radius);

            const ProjectileEvent plane =
                fire_through(Plane{.normal = {-1.0, 0.0, 0.0}, .distance = -10.0}, radius, speed);
            const ProjectileEvent box = fire_through(
                AABB{.min = {10.0, -1.0, -1.0}, .max = {10.01, 1.0, 1.0}}, radius, speed
            );
            const ProjectileEvent triangle = fire_through(
                Triangle{.a = {10.0, -1.0, -1.0}, .b = {10.0, 1.0, -1.0}, .c = {10.0, 0.0, 2.0}},
                radius,
                speed
            );
            const ProjectileEvent sphere =
                fire_through(Sphere{.center = {10.005, 0.0, 0.0}, .radius = 0.005}, radius, speed);

            for (const ProjectileEvent& event : {plane, box, triangle, sphere}) {
                CAPTURE(event);
                REQUIRE(event.kind == ProjectileEventKind::hit);
                const HitResult hit = require_value(event.hit);
                CHECK(hit.material_id == 9);
                CHECK_THAT(hit.point.x, WithinAbs(10.0, 1e-9));
                CHECK_THAT(event.state.position.x, WithinAbs(10.0 - radius, 1e-9));
                CHECK_THAT(event.time, WithinAbs((10.0 - radius) / speed, 1e-12));
            }
        }
    }
}

// --- Reproducibility ----------------------------------------------------------------------------

TEST_CASE(
    "repeated runs reproduce every result exactly", "[integration][simulation][determinism]"
) {
    const Room room;
    const std::vector<Launch> launches = random_launches(room, 200);
    for (const IntegratorKind integrator :
         {IntegratorKind::semi_implicit_euler, IntegratorKind::rk4}) {
        SimulationConfig config;
        config.integrator = integrator;
        const auto frame = [] { return 1.0 / 60.0; };
        const Outcome first = run(room, launches, config, frame);
        const Outcome second = run(room, launches, config, frame);
        CHECK(first.steps == second.steps);
        CHECK(first.events == second.events); // bit-identical
    }
}

TEST_CASE("results do not depend on the frame rate", "[integration][simulation][determinism]") {
    // The same number of fixed steps gives the same result, whatever frames delivered them.
    const Room room;
    const std::vector<Launch> launches = random_launches(room, 200);

    const auto advance = [&](double (*frame)(test::Random&)) {
        test::Random random;
        Simulation simulation;
        simulation.set_collision_world(&room.world);
        std::vector<ProjectileEvent> events;
        simulation.set_event_callback([&events](const ProjectileEvent& event) {
            events.push_back(event);
        });
        for (const Launch& launch : launches) {
            REQUIRE(simulation.add_projectile(launch.state, launch.config));
        }
        while (simulation.step_count() < 600) {
            simulation.update(frame(random));
        }
        // Frames end at different steps; finish with single steps up to a common count.
        while (simulation.step_count() < 610) {
            simulation.step();
        }
        const auto remaining = simulation.projectiles();
        return std::pair{
            events, std::vector<SimulatedProjectile>(remaining.begin(), remaining.end())
        };
    };
    const auto at_60_hz = advance([](test::Random&) { return 1.0 / 60.0; });
    const auto at_144_hz = advance([](test::Random&) { return 1.0 / 144.0; });
    const auto jittery = advance([](test::Random& random) { return random.uniform(0.0, 0.05); });
    CHECK(at_60_hz.first == at_144_hz.first);
    CHECK(at_60_hz.first == jittery.first);
    CHECK(at_60_hz.second == at_144_hz.second);
    CHECK(at_60_hz.second == jittery.second);
}

TEST_CASE(
    "a trajectory preview predicts exactly where the simulation hits", "[integration][simulation]"
) {
    const Room room;
    const std::vector<Launch> launches = random_launches(room, 100);
    for (const IntegratorKind integrator :
         {IntegratorKind::semi_implicit_euler, IntegratorKind::rk4}) {
        SimulationConfig config;
        config.integrator = integrator;
        TrajectoryConfig preview;
        preview.time_step = config.time_step;
        preview.integrator = integrator;
        preview.sample_interval = 0.1;
        preview.max_duration = 60.0;

        const Outcome outcome = run(room, launches, config, [] { return 1.0 / 60.0; });
        REQUIRE(outcome.events.size() == launches.size());
        for (std::size_t k = 0; k < launches.size(); ++k) {
            const Launch& launch = launches.at(k);
            // Ids are given out in order, so event k belongs to the projectile with the k-th id.
            const auto event = std::ranges::find_if(outcome.events, [k](const ProjectileEvent& e) {
                return (e.id.value & 0xFFFF'FFFFU) == k;
            });
            REQUIRE(event != outcome.events.end());
            const Trajectory trajectory =
                require_value(predict_trajectory(launch.state, launch.config, preview, room.world));
            CAPTURE(k, *event);
            if (event->kind == ProjectileEventKind::hit) {
                REQUIRE(trajectory.end == TrajectoryEnd::hit);
                CHECK(trajectory.hit == event->hit);
                CHECK(trajectory.samples.back().position == event->state.position);
                CHECK(trajectory.samples.back().velocity == event->state.velocity);
                CHECK_THAT(trajectory.samples.back().time, WithinAbs(event->time, 1e-9));
            } else {
                CHECK(trajectory.end == TrajectoryEnd::lifetime);
                CHECK(trajectory.samples.back().position == event->state.position);
            }
        }
    }
}

// --- Memory -------------------------------------------------------------------------------------

namespace {

/// Runs @p frames frames of 1/60 s, each spawning 20 projectiles with a lifetime of 0.5 s, so
/// that the number in flight levels off; many hit the room and leave early. The directions turn
/// by 7° per frame and repeat every 360 frames. Returns the number of failed additions, which
/// must be 0, and the largest number of projectiles in flight.
std::pair<int, std::size_t> churn(Simulation& simulation, int frames, std::uint64_t& frame_number) {
    ProjectileConfig config;
    config.gravity = gravity;
    config.radius = 0.05;
    config.lifetime = 0.5;
    int failures = 0;
    std::size_t most_in_flight = 0;
    for (int frame = 0; frame < frames; ++frame, ++frame_number) {
        for (std::uint64_t k = 0; k < 20; ++k) {
            const std::uint64_t degrees = (frame_number * 7U + k * 18U) % 360U;
            const double angle = degrees_to_radians(static_cast<double>(degrees));
            const Vec3 velocity{std::cos(angle) * 60.0, 15.0, std::sin(angle) * 60.0};
            const Result<ProjectileState> state =
                create_projectile(config, Vec3{2.0, 15.0, 2.0}, velocity);
            if (!state || !simulation.add_projectile(*state, config)) {
                ++failures;
            }
        }
        simulation.update(1.0 / 60.0);
        most_in_flight = (std::max)(most_in_flight, simulation.projectiles().size());
    }
    return {failures, most_in_flight};
}

} // namespace

TEST_CASE(
    "a running simulation stops allocating once it reaches its largest size",
    "[integration][simulation][memory]"
) {
    const Room room;
    Simulation simulation;
    simulation.set_collision_world(&room.world);
    std::size_t ended = 0;
    simulation.set_event_callback([&ended](const ProjectileEvent& /*event*/) { ++ended; });
    std::uint64_t frame_number = 0;

    // Warm up for one period of the spawn pattern and a lifetime: every later frame repeats a
    // situation seen before, so the simulation has already reached its largest size.
    REQUIRE(churn(simulation, 360 + 30, frame_number).first == 0);

    // Then run 20 more seconds, adding and ending thousands of projectiles.
    const std::size_t before = test::allocation_count();
    const std::size_t ended_before = ended;
    const auto [failures, most_in_flight] = churn(simulation, 1'200, frame_number);
    const std::size_t allocations = test::allocation_count() - before;

    CHECK(failures == 0);
    CHECK(ended - ended_before > 20'000);
    CHECK(allocations == 0);
    CHECK(most_in_flight <= std::size_t{600}); // 20 per frame, living 30 frames at most
}

TEST_CASE(
    "a simulation with reserved memory never allocates", "[integration][simulation][memory]"
) {
    const Room room;
    Simulation simulation;
    simulation.set_collision_world(&room.world);
    std::size_t ended = 0;
    simulation.set_event_callback([&ended](const ProjectileEvent& /*event*/) { ++ended; });
    simulation.reserve(600); // 20 per frame, living 30 frames at most
    std::uint64_t frame_number = 0;

    const std::size_t before = test::allocation_count();
    const int failures = churn(simulation, 600, frame_number).first;
    const std::size_t allocations = test::allocation_count() - before;

    CHECK(failures == 0);
    CHECK(ended > 5'000);
    CHECK(allocations == 0);
}
