#include "projectile/core/result.hpp"

#include <catch2/catch_test_macros.hpp>

#include "support/ballistics_support.hpp"

#include <array>
#include <cstddef>
#include <set>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using projectile::Error;
using projectile::Result;

TEST_CASE("a Result holding a value gives access to it", "[core][result]") {
    const Result<int> result = 42;
    CHECK(result.has_value());
    CHECK(static_cast<bool>(result));
    CHECK(*result == 42);
    CHECK(result.value() == 42);
    CHECK_THROWS_AS(result.error(), std::bad_variant_access);
}

TEST_CASE("a Result holding an error reports it and refuses value access", "[core][result]") {
    const Result<int> result = Error::invalid_mass;
    CHECK_FALSE(result.has_value());
    CHECK_FALSE(static_cast<bool>(result));
    CHECK(result.error() == Error::invalid_mass);
    CHECK_THROWS_AS(result.value(), std::bad_variant_access);
    CHECK_THROWS_AS(*result, std::bad_variant_access);
}

TEST_CASE("a Result gives access to members and can be moved from", "[core][result]") {
    Result<std::vector<int>> result = std::vector<int>{1, 2, 3};
    CHECK(result->size() == 3);
    result->push_back(4);
    const std::vector<int> moved = std::move(result).value();
    CHECK(moved == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("a Result can be used in constant expressions", "[core][result]") {
    constexpr Result<int> value = 7;
    STATIC_REQUIRE(value.has_value());
    STATIC_REQUIRE(*value == 7);
    constexpr Result<int> error = Error::invalid_radius;
    STATIC_REQUIRE_FALSE(error.has_value());
    STATIC_REQUIRE(error.error() == Error::invalid_radius);
}

TEST_CASE("every error has its own description", "[core][result]") {
    constexpr std::array errors{
        Error::invalid_mass,
        Error::invalid_radius,
        Error::invalid_position,
        Error::invalid_velocity,
        Error::invalid_gravity,
        Error::invalid_lifetime,
        Error::invalid_time_step,
        Error::invalid_sample_interval,
        Error::invalid_duration,
    };
    std::set<std::string_view> descriptions;
    for (const Error error : errors) {
        CAPTURE(error);
        CHECK_FALSE(projectile::describe(error).empty());
        CHECK(projectile::describe(error) != "unknown error");
        descriptions.insert(projectile::describe(error));
    }
    CHECK(descriptions.size() == errors.size());
    // A value outside the enumeration, as from a corrupted byte, still gets a description.
    CHECK(projectile::describe(static_cast<Error>(255)) == "unknown error");
}
