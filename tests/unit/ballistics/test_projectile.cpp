#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/core/constants.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/ballistics_support.hpp"

#include <functional>
#include <limits>
#include <utility>
#include <vector>

using namespace projectile;
using projectile::test::require_value;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

/// A valid configuration with gravity along -y.
ProjectileConfig falling_config() {
    ProjectileConfig config;
    config.gravity = {0.0, -standard_gravity, 0.0};
    return config;
}

} // namespace

TEST_CASE("standard_gravity is the defined standard value", "[ballistics][constants]") {
    STATIC_REQUIRE(standard_gravity == 9.80665);
}

TEST_CASE("a default ProjectileConfig is valid and assumes no up axis", "[ballistics][config]") {
    const ProjectileConfig config;
    CHECK_FALSE(validate(config).has_value());
    CHECK(config.gravity == Vec3{}); // no gravity until it is configured
    CHECK(config.radius == 0.0);     // a point
    CHECK(config.lifetime == infinity);
}

TEST_CASE("validate reports each invalid parameter with its own error", "[ballistics][config]") {
    using Change = std::function<void(ProjectileConfig&)>;
    const std::vector<std::pair<Change, Error>> cases{
        {[](ProjectileConfig& c) { c.mass = 0.0; }, Error::invalid_mass},
        {[](ProjectileConfig& c) { c.mass = -1.0; }, Error::invalid_mass},
        {[](ProjectileConfig& c) { c.mass = not_a_number; }, Error::invalid_mass},
        {[](ProjectileConfig& c) { c.mass = infinity; }, Error::invalid_mass},
        {[](ProjectileConfig& c) { c.radius = -0.1; }, Error::invalid_radius},
        {[](ProjectileConfig& c) { c.radius = not_a_number; }, Error::invalid_radius},
        {[](ProjectileConfig& c) { c.radius = infinity; }, Error::invalid_radius},
        {[](ProjectileConfig& c) { c.initial_velocity.x = not_a_number; }, Error::invalid_velocity},
        {[](ProjectileConfig& c) { c.gravity.y = -infinity; }, Error::invalid_gravity},
        {[](ProjectileConfig& c) { c.lifetime = 0.0; }, Error::invalid_lifetime},
        {[](ProjectileConfig& c) { c.lifetime = -1.0; }, Error::invalid_lifetime},
        {[](ProjectileConfig& c) { c.lifetime = not_a_number; }, Error::invalid_lifetime},
    };
    for (const auto& [change, expected] : cases) {
        ProjectileConfig config = falling_config();
        change(config);
        CAPTURE(expected);
        CHECK(validate(config) == expected);
    }
}

TEST_CASE(
    "create_projectile starts an active projectile from the configuration", "[ballistics][state]"
) {
    ProjectileConfig config = falling_config();
    config.initial_velocity = {1.0, 2.0, 3.0};
    config.lifetime = 4.0;

    const ProjectileState state = require_value(create_projectile(config, Vec3{5.0, 6.0, 7.0}));
    CHECK(state.position == Vec3{5.0, 6.0, 7.0});
    CHECK(state.velocity == config.initial_velocity);
    CHECK(state.acceleration == config.gravity);
    CHECK(state.remaining_lifetime == 4.0);
    CHECK(state.active);
    CHECK_FALSE(validate(state).has_value());
}

TEST_CASE("create_projectile can be given its velocity", "[ballistics][state]") {
    const ProjectileState state =
        require_value(create_projectile(falling_config(), Vec3{}, Vec3{0.0, 0.0, -40.0}));
    CHECK(state.velocity == Vec3{0.0, 0.0, -40.0});
}

TEST_CASE("create_projectile reports why it rejects its input", "[ballistics][state]") {
    ProjectileConfig invalid = falling_config();
    invalid.mass = 0.0;
    CHECK(create_projectile(invalid, Vec3{}).error() == Error::invalid_mass);
    CHECK(
        create_projectile(falling_config(), Vec3{not_a_number, 0.0, 0.0}).error() ==
        Error::invalid_position
    );
    CHECK(
        create_projectile(falling_config(), Vec3{}, Vec3{0.0, infinity, 0.0}).error() ==
        Error::invalid_velocity
    );
}

TEST_CASE("validate checks a projectile state", "[ballistics][state]") {
    ProjectileState state;
    CHECK_FALSE(validate(state).has_value()); // infinite lifetime is valid
    state.remaining_lifetime = 0.0;
    CHECK_FALSE(validate(state).has_value()); // so is an exhausted one

    state.remaining_lifetime = -1.0;
    CHECK(validate(state) == Error::invalid_lifetime);
    state.remaining_lifetime = not_a_number;
    CHECK(validate(state) == Error::invalid_lifetime);

    state = ProjectileState{};
    state.position.z = infinity;
    CHECK(validate(state) == Error::invalid_position);
    state = ProjectileState{};
    state.velocity.x = not_a_number;
    CHECK(validate(state) == Error::invalid_velocity);
}

TEST_CASE("the forces on a projectile are its uniform gravity", "[ballistics][forces]") {
    constexpr ProjectileForces forces{.gravity = {0.0, 0.0, -9.0}};
    STATIC_REQUIRE(forces(Vec3{}, Vec3{}) == Vec3{0.0, 0.0, -9.0});
    STATIC_REQUIRE(forces(Vec3{100.0, -5.0, 3.0}, Vec3{300.0, 0.0, 0.0}) == Vec3{0.0, 0.0, -9.0});
    CHECK(forces_of(falling_config()).gravity == Vec3{0.0, -standard_gravity, 0.0});
    STATIC_REQUIRE(AccelerationField<ProjectileForces>);
}
