// Materials and the material table.

#include "projectile/materials/material.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/ballistics_support.hpp"

#include <functional>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

using namespace projectile;

namespace {

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

/// A material that ricochets, and can be penetrated.
Material steel() {
    Material material;
    material.id = 3;
    material.resistance = 5e5;
    material.restitution = 0.6;
    material.friction = 0.2;
    material.penetration_cost = 2'000.0;
    material.ricochet_angle = 0.5;
    return material;
}

} // namespace

TEST_CASE("a default material is valid and stops every projectile", "[materials]") {
    const Material material;
    CHECK_FALSE(validate(material).has_value());
    CHECK(material.penetration_cost == infinity); // impenetrable
    CHECK(material.ricochet_angle == 0.0);        // never ricochets
    CHECK(material.restitution == 0.0);
}

TEST_CASE("validate reports each invalid material property", "[materials]") {
    using Change = std::function<void(Material&)>;
    const std::vector<std::pair<Change, Error>> cases{
        {[](Material& m) { m.resistance = -1.0; }, Error::invalid_resistance},
        {[](Material& m) { m.resistance = infinity; }, Error::invalid_resistance},
        {[](Material& m) { m.restitution = 1.5; }, Error::invalid_restitution},
        {[](Material& m) { m.restitution = not_a_number; }, Error::invalid_restitution},
        {[](Material& m) { m.friction = -0.1; }, Error::invalid_friction},
        {[](Material& m) { m.friction = not_a_number; }, Error::invalid_friction},
        {[](Material& m) { m.penetration_cost = -1.0; }, Error::invalid_penetration_cost},
        {[](Material& m) { m.penetration_cost = not_a_number; }, Error::invalid_penetration_cost},
        {[](Material& m) { m.ricochet_angle = -0.1; }, Error::invalid_ricochet_angle},
        {[](Material& m) { m.ricochet_angle = 2.0; }, Error::invalid_ricochet_angle},
    };
    for (const auto& [change, expected] : cases) {
        Material material = steel();
        change(material);
        CAPTURE(expected);
        CHECK(validate(material) == expected);
    }
    Material extremes = steel();
    extremes.restitution = 1.0;
    extremes.friction = 1.0;
    extremes.ricochet_angle = std::numbers::pi / 2.0;
    extremes.penetration_cost = infinity;
    CHECK_FALSE(validate(extremes).has_value());
}

TEST_CASE("a material table looks up materials by id", "[materials][table]") {
    MaterialTable table;
    CHECK(table.empty());
    Material wood;
    wood.id = 1;
    wood.penetration_cost = 40.0;
    REQUIRE_FALSE(table.add(steel()));
    REQUIRE_FALSE(table.add(wood));
    CHECK(table.size() == 2);

    CHECK(table.find(3) != nullptr);
    CHECK(*table.find(3) == steel());
    CHECK(table.material_for(1) == wood);
    CHECK(table.find(2) == nullptr);
    CHECK(table.material_for(2) == Material{}); // the fallback stops projectiles
}

TEST_CASE("adding a material with a known id replaces it", "[materials][table]") {
    MaterialTable table;
    REQUIRE_FALSE(table.add(steel()));
    Material softer = steel();
    softer.penetration_cost = 500.0;
    REQUIRE_FALSE(table.add(softer));
    CHECK(table.size() == 1);
    CHECK(table.material_for(3).penetration_cost == 500.0);
}

TEST_CASE("invalid materials are rejected and change nothing", "[materials][table]") {
    MaterialTable table;
    REQUIRE_FALSE(table.add(steel()));
    Material broken = steel();
    broken.restitution = 2.0;
    CHECK(table.add(broken) == Error::invalid_restitution);
    CHECK(table.material_for(3) == steel());
    CHECK(table.set_fallback(broken) == Error::invalid_restitution);
    CHECK(table.fallback() == Material{});
}

TEST_CASE("the fallback material can be replaced", "[materials][table]") {
    MaterialTable table;
    Material soft;
    soft.penetration_cost = 0.0; // projectiles pass through unknown surfaces
    REQUIRE_FALSE(table.set_fallback(soft));
    CHECK(table.material_for(42) == soft);
}

TEST_CASE("materials can be removed", "[materials][table]") {
    MaterialTable table;
    REQUIRE_FALSE(table.add(steel()));
    CHECK(table.remove(3));
    CHECK_FALSE(table.remove(3));
    CHECK(table.empty());
    REQUIRE_FALSE(table.add(steel()));
    table.clear();
    CHECK(table.find(3) == nullptr);
}

TEST_CASE("many materials keep their own ids", "[materials][table]") {
    MaterialTable table;
    // Added out of order, looked up in order.
    for (const std::uint32_t id : {50U, 7U, 23U, 1U, 99U, 64U}) {
        Material material;
        material.id = id;
        material.resistance = static_cast<double>(id);
        REQUIRE_FALSE(table.add(material));
    }
    for (const std::uint32_t id : {1U, 7U, 23U, 50U, 64U, 99U}) {
        REQUIRE(table.find(id) != nullptr);
        CHECK(table.find(id)->resistance == static_cast<double>(id));
    }
    CHECK(table.find(8) == nullptr);
}
