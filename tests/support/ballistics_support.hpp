#pragma once

// Catch2 support for the ballistics types: readable values in failure messages, and a helper for
// unwrapping Result values.

#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/ballistics/trajectory.hpp"
#include "projectile/core/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

#include "support/collision_matchers.hpp"

#include <string>

namespace projectile::test {

/// Returns the value held by @p result, failing the current test with the error's description if
/// it holds an error.
template <typename T>
[[nodiscard]] T require_value(const Result<T>& result) {
    if (!result) {
        FAIL("unexpected error: " << describe(result.error()));
    }
    return result.value();
}

} // namespace projectile::test

template <>
struct Catch::StringMaker<projectile::Error> {
    static std::string convert(projectile::Error error) {
        return std::string(projectile::describe(error));
    }
};

template <>
struct Catch::StringMaker<projectile::ProjectileState> {
    static std::string convert(const projectile::ProjectileState& state) {
        using projectile::test::format;
        return "ProjectileState{position " + format(state.position) + ", velocity " +
               format(state.velocity) + ", acceleration " + format(state.acceleration) +
               ", remaining lifetime " + format(state.remaining_lifetime) +
               (state.active ? ", active}" : ", inactive}");
    }
};

template <>
struct Catch::StringMaker<projectile::TrajectorySample> {
    static std::string convert(const projectile::TrajectorySample& sample) {
        using projectile::test::format;
        return "TrajectorySample{time " + format(sample.time) + ", position " +
               format(sample.position) + ", velocity " + format(sample.velocity) + "}";
    }
};
