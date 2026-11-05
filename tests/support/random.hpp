#pragma once

// Deterministic pseudo-random inputs for property tests.
//
// The generator is seeded from Catch2's --rng-seed, which CTest pins (see tests/CMakeLists.txt),
// so every run checks the same inputs and a failure can be reproduced by running the test binary
// with that seed. Run the binary with `--rng-seed random` to explore other inputs. The output of
// std::mt19937_64 is fixed by the C++ standard, and the conversion to double below is done by
// hand, so a seed produces the same inputs on every platform.

#include "projectile/math/vec2.hpp"
#include "projectile/math/vec3.hpp"

#include <catch2/catch_get_random_seed.hpp>

#include <cmath>
#include <cstdint>
#include <random>

namespace projectile::test {

class Random {
public:
    Random() : engine_(Catch::getSeed()) {}

    /// Uniformly distributed in [low, high).
    [[nodiscard]] double uniform(double low, double high) {
        return low + (high - low) * unit_interval();
    }

    /// Uniformly distributed integer in [low, high].
    [[nodiscard]] int integer(int low, int high) {
        const auto count = static_cast<std::uint64_t>(static_cast<std::int64_t>(high) - low + 1);
        return static_cast<int>(low + static_cast<std::int64_t>(engine_() % count));
    }

    /// Vector with every component uniformly distributed in [low, high).
    [[nodiscard]] Vec3 vec3(double low, double high) {
        // Braced initializers are evaluated left to right, so the order of draws is fixed.
        return Vec3{uniform(low, high), uniform(low, high), uniform(low, high)};
    }

    /// Vector with every component uniformly distributed in [low, high).
    [[nodiscard]] Vec2 vec2(double low, double high) {
        return Vec2{uniform(low, high), uniform(low, high)};
    }

    /// Unit vector with a uniformly distributed direction.
    [[nodiscard]] Vec3 unit_vec3() {
        // Rejection sampling: points uniform in the unit ball have uniformly distributed
        // directions. Points very near the center are skipped to keep normalization accurate.
        for (;;) {
            const Vec3 candidate = vec3(-1.0, 1.0);
            const double squared = length_squared(candidate);
            if (squared > 1e-4 && squared <= 1.0) {
                return candidate / std::sqrt(squared);
            }
        }
    }

    /// Unit vector with a uniformly distributed direction.
    [[nodiscard]] Vec2 unit_vec2() {
        for (;;) {
            const Vec2 candidate = vec2(-1.0, 1.0);
            const double squared = length_squared(candidate);
            if (squared > 1e-4 && squared <= 1.0) {
                return candidate / std::sqrt(squared);
            }
        }
    }

private:
    /// Uniformly distributed in [0, 1), using 53 random bits (the precision of a double).
    double unit_interval() { return std::ldexp(static_cast<double>(engine_() >> 11U), -53); }

    std::mt19937_64 engine_;
};

} // namespace projectile::test
