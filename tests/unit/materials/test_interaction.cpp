// Impact angles, ricochet, penetration, and the standard interaction model's decisions.

#include "projectile/ballistics/energy.hpp"
#include "projectile/materials/interaction.hpp"
#include "projectile/materials/penetration.hpp"
#include "projectile/materials/ricochet.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/ballistics_support.hpp"
#include "support/random.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <numbers>

using namespace projectile;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double pi = std::numbers::pi;

/// A surface facing +y, as a ray or sweep would report it.
constexpr Vec3 up{0.0, 1.0, 0.0};

Material steel() {
    Material material;
    material.id = 3;
    material.resistance = 1e5;
    material.restitution = 0.5;
    material.friction = 0.25;
    material.penetration_cost = 1'000.0;
    material.ricochet_angle = degrees_to_radians(20.0);
    return material;
}

/// An impact of a projectile of @p mass kg at @p velocity on the ground y = 0 of @p material.
Impact impact(const Vec3& velocity, const Material& material, double mass, double thickness = 0.0) {
    ProjectileConfig config;
    config.mass = mass;
    return Impact{
        .state = require_value(create_projectile(config, Vec3{}, velocity)),
        .config = config,
        .hit =
            HitResult{
                .distance = 1.0,
                .point = {},
                .normal = up,
                .material_id = material.id,
                .front_face = true,
            },
        .material = material,
        .thickness = thickness,
    };
}

/// Velocity of @p speed m/s striking the ground at @p angle radians from its surface.
Vec3 striking(double speed, double angle) {
    return Vec3{std::cos(angle), -std::sin(angle), 0.0} * speed;
}

} // namespace

// --- Angles and ricochet ----------------------------------------------------------------------

TEST_CASE("the impact angle is measured from the surface", "[materials][ricochet]") {
    CHECK(impact_angle(Vec3{0.0, -5.0, 0.0}, up) == pi / 2.0); // head-on
    CHECK(impact_angle(Vec3{5.0, 0.0, 0.0}, up) == 0.0);       // grazing
    CHECK_THAT(impact_angle(Vec3{3.0, -3.0, 0.0}, up), WithinAbs(pi / 4.0, 1e-15));
    CHECK_THAT(impact_angle(striking(100.0, 0.3), up), WithinAbs(0.3, 1e-15));
    CHECK(impact_angle(Vec3{1.0, 2.0, 0.0}, up) == 0.0); // moving away
    CHECK(impact_angle(Vec3{}, up) == 0.0);
}

TEST_CASE("a ricochet reverses and scales the part into the surface", "[materials][ricochet]") {
    // 4 m/s along the surface, 3 m/s into it.
    STATIC_REQUIRE(ricochet_velocity(Vec3{4.0, -3.0, 0.0}, up, 0.5, 0.25) == Vec3{3.0, 1.5, 0.0});
    // A perfect, frictionless ricochet is a reflection.
    STATIC_REQUIRE(ricochet_velocity(Vec3{4.0, -3.0, 0.0}, up, 1.0, 0.0) == Vec3{4.0, 3.0, 0.0});
    // No restitution: the projectile slides along the surface.
    STATIC_REQUIRE(ricochet_velocity(Vec3{4.0, -3.0, 0.0}, up, 0.0, 0.0) == Vec3{4.0, 0.0, 0.0});
}

TEST_CASE(
    "with friction 1 − e, a ricochet is the reflection scaled by e",
    "[materials][ricochet][property]"
) {
    // V' = e R, the ricochet formula of the specification.
    test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Vec3 normal = random.unit_vec3();
        const Vec3 velocity = random.vec3(-500.0, 500.0);
        const double e = random.uniform(0.0, 1.0);
        CHECK_THAT(
            ricochet_velocity(velocity, normal, e, 1.0 - e),
            is_near(reflect(velocity, normal) * e, 1e-9)
        );
    }
}

TEST_CASE("a ricochet never speeds a projectile up", "[materials][ricochet][property]") {
    test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        const Vec3 normal = random.unit_vec3();
        const Vec3 velocity = random.vec3(-500.0, 500.0);
        // One draw per statement: the order of evaluation of function arguments is unspecified.
        const double restitution = random.uniform(0.0, 1.0);
        const double friction = random.uniform(0.0, 1.0);
        const Vec3 outgoing = ricochet_velocity(velocity, normal, restitution, friction);
        CHECK(length(outgoing) <= length(velocity) * (1.0 + 1e-12));
    }
}

// --- Penetration ------------------------------------------------------------------------------

TEST_CASE("penetration costs a fixed energy plus a cost per metre", "[materials][penetration]") {
    Material wood;
    wood.penetration_cost = 40.0;
    wood.resistance = 2'000.0;
    STATIC_REQUIRE(penetration_energy(Material{}, 0.0) == infinity);
    CHECK(penetration_energy(wood, 0.0) == 40.0);
    CHECK(penetration_energy(wood, 0.02) == 80.0);
}

// --- The standard model -----------------------------------------------------------------------

TEST_CASE("shallow impacts ricochet", "[materials][interaction]") {
    // 10° from the surface, below steel's 20° threshold.
    const Vec3 velocity = striking(300.0, degrees_to_radians(10.0));
    const InteractionResult result = resolve_standard(impact(velocity, steel(), 0.01), 0.1);
    CHECK(result.outcome == InteractionOutcome::ricocheted);
    CHECK_THAT(
        result.outgoing_velocity, is_near(ricochet_velocity(velocity, up, 0.5, 0.25), 1e-12)
    );
    CHECK(result.outgoing_velocity.y > 0.0); // leaving the surface
    CHECK_THAT(
        result.energy_remaining, WithinRel(kinetic_energy(0.01, result.outgoing_velocity), 1e-15)
    );
}

TEST_CASE("steep impacts with enough energy penetrate", "[materials][interaction]") {
    // 60° from the surface; 0.01 kg at 600 m/s carries 1800 J, against a cost of 1000 J.
    const Vec3 velocity = striking(600.0, degrees_to_radians(60.0));
    const InteractionResult result = resolve_standard(impact(velocity, steel(), 0.01), 0.1);
    CHECK(result.outcome == InteractionOutcome::penetrated);
    CHECK_THAT(result.energy_remaining, WithinRel(800.0, 1e-12));
    CHECK_THAT(length(result.outgoing_velocity), WithinRel(400.0, 1e-12)); // √(2 · 800 / 0.01)
    CHECK_THAT(
        result.outgoing_velocity / 400.0, is_near(velocity / 600.0, 1e-12)
    ); // the same direction
}

TEST_CASE("thickness makes penetration costlier", "[materials][interaction]") {
    const Vec3 velocity = striking(600.0, pi / 2.0);
    // 1000 J plus 1e5 J/m: 7 mm of steel costs 1700 J, less than the 1800 J the projectile has;
    // 9 mm costs 1900 J, more.
    CHECK(
        resolve_standard(impact(velocity, steel(), 0.01, 0.007), 0.1).outcome ==
        InteractionOutcome::penetrated
    );
    const InteractionResult stopped = resolve_standard(impact(velocity, steel(), 0.01, 0.009), 0.1);
    CHECK(stopped == InteractionResult{});
}

TEST_CASE("impacts without enough energy stop the projectile", "[materials][interaction]") {
    const InteractionResult result =
        resolve_standard(impact(striking(300.0, pi / 2.0), steel(), 0.01), 0.1); // 450 J
    CHECK(result.outcome == InteractionOutcome::stopped);
    CHECK(result.outgoing_velocity == Vec3{});
    CHECK(result.energy_remaining == 0.0);
}

TEST_CASE(
    "the default material stops projectiles at any angle and speed", "[materials][interaction]"
) {
    for (const double angle : {0.01, 0.3, 1.0, pi / 2.0}) {
        CHECK(
            resolve_standard(impact(striking(5'000.0, angle), Material{}, 1.0), 0.1).outcome ==
            InteractionOutcome::stopped
        );
    }
}

TEST_CASE("projectiles that would fly on too slowly stop", "[materials][interaction]") {
    // A ricochet from 1 m/s leaves steel at about 0.74 m/s.
    const Vec3 slow = striking(1.0, degrees_to_radians(10.0));
    CHECK(
        resolve_standard(impact(slow, steel(), 0.01), 1.0).outcome == InteractionOutcome::stopped
    );
    // A projectile left with 0.5 m/s after penetrating stops when the stopping speed is 1 m/s.
    Material paper;
    paper.penetration_cost = 0.5 * 0.01 * (100.0 * 100.0 - 0.25);
    const InteractionResult result =
        resolve_standard(impact(striking(100.0, pi / 2.0), paper, 0.01), 1.0);
    CHECK(result.outcome == InteractionOutcome::stopped);
}

TEST_CASE(
    "a material that costs nothing leaves the velocity unchanged", "[materials][interaction]"
) {
    Material smoke;
    smoke.penetration_cost = 0.0;
    const Vec3 velocity = striking(321.0, 0.7);
    const InteractionResult result = resolve_standard(impact(velocity, smoke, 0.02), 0.1);
    CHECK(result.outcome == InteractionOutcome::penetrated);
    CHECK(result.outgoing_velocity == velocity);
}

TEST_CASE("a ricochet angle of 90° makes every impact ricochet", "[materials][interaction]") {
    Material rubber;
    rubber.restitution = 1.0;
    rubber.ricochet_angle = pi / 2.0;
    const InteractionResult result =
        resolve_standard(impact(Vec3{0.0, -10.0, 0.0}, rubber, 1.0), 0.1);
    CHECK(result.outcome == InteractionOutcome::ricocheted);
    CHECK(result.outgoing_velocity == Vec3{0.0, 10.0, 0.0});
}

TEST_CASE(
    "the standard model keeps speed and energy within their bounds",
    "[materials][interaction][property]"
) {
    test::Random random;
    for (int i = 0; i < 10'000; ++i) {
        Material material;
        material.resistance = random.uniform(0.0, 1e5);
        material.restitution = random.uniform(0.0, 1.0);
        material.friction = random.uniform(0.0, 1.0);
        material.penetration_cost = random.integer(0, 9) == 0 ? infinity : random.uniform(0.0, 2e3);
        material.ricochet_angle = random.uniform(0.0, pi / 2.0);
        const double impact_speed = random.uniform(0.0, 1'000.0);
        const double angle = random.uniform(0.0, pi / 2.0);
        const Vec3 velocity = striking(impact_speed, angle);
        const double mass = random.uniform(0.001, 1.0);
        const double stopping_speed = random.uniform(0.0, 5.0);
        const Impact hit = impact(velocity, material, mass, random.uniform(0.0, 0.1));
        const InteractionResult result = resolve_standard(hit, stopping_speed);
        CAPTURE(velocity, mass, stopping_speed, result.outgoing_velocity);

        const double energy = kinetic_energy(mass, velocity);
        const double speed = length(result.outgoing_velocity);
        CHECK(speed <= length(velocity) * (1.0 + 1e-12));
        CHECK(result.energy_remaining <= energy * (1.0 + 1e-12));
        CHECK_THAT(
            result.energy_remaining,
            WithinRel(kinetic_energy(mass, result.outgoing_velocity), 1e-12)
        );
        switch (result.outcome) {
        case InteractionOutcome::stopped:
            CHECK(result.outgoing_velocity == Vec3{});
            CHECK(result.energy_remaining == 0.0);
            break;
        case InteractionOutcome::ricocheted:
            CHECK(speed >= stopping_speed);
            CHECK(impact_angle(velocity, up) <= material.ricochet_angle);
            CHECK(dot(result.outgoing_velocity, up) >= 0.0); // never into the surface
            break;
        case InteractionOutcome::penetrated:
            CHECK(speed >= stopping_speed);
            CHECK(energy > penetration_energy(material, hit.thickness));
            CHECK_THAT(
                result.outgoing_velocity / speed, is_near(velocity / length(velocity), 1e-12)
            );
            break;
        }
    }
}

// --- Replaceable models -----------------------------------------------------------------------

TEST_CASE("StandardInteraction applies the standard model", "[materials][interaction]") {
    const StandardInteraction standard;
    CHECK(standard.stopping_speed() == StandardInteraction::default_stopping_speed);
    const Impact shallow = impact(striking(300.0, 0.1), steel(), 0.01);
    CHECK(standard.resolve(shallow) == resolve_standard(shallow, standard.stopping_speed()));

    const StandardInteraction strict = require_value(StandardInteraction::create(5.0));
    CHECK(strict.stopping_speed() == 5.0);
    CHECK(StandardInteraction::create(-1.0).error() == Error::invalid_stopping_speed);
    CHECK(StandardInteraction::create(infinity).error() == Error::invalid_stopping_speed);
}

namespace {

/// A model of a game's own: everything sticks, except surfaces of material 9, which bounce
/// projectiles straight back at full speed.
class StickyUnlessBouncy final : public IMaterialInteraction {
public:
    [[nodiscard]] InteractionResult resolve(const Impact& impact) const override {
        if (impact.hit.material_id != 9) {
            return InteractionResult{};
        }
        return InteractionResult{
            .outcome = InteractionOutcome::ricocheted,
            .outgoing_velocity = -impact.state.velocity,
            .energy_remaining = kinetic_energy(impact.state, impact.config),
        };
    }
};

} // namespace

TEST_CASE("interaction models are replaceable", "[materials][interaction]") {
    const std::unique_ptr<IMaterialInteraction> model = std::make_unique<StickyUnlessBouncy>();
    Material bouncy;
    bouncy.id = 9;
    const Vec3 velocity{2.0, -7.0, 0.0};
    CHECK(model->resolve(impact(velocity, steel(), 1.0)).outcome == InteractionOutcome::stopped);
    const InteractionResult bounce = model->resolve(impact(velocity, bouncy, 1.0));
    CHECK(bounce.outcome == InteractionOutcome::ricocheted);
    CHECK(bounce.outgoing_velocity == -velocity);
}
