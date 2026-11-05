#pragma once

// Catch2 support for collision results: exact values in failure messages.

#include "projectile/collision/hit_result.hpp"

#include <catch2/catch_tostring.hpp>

#include "support/math_matchers.hpp"

#include <string>

template <>
struct Catch::StringMaker<projectile::HitResult> {
    static std::string convert(const projectile::HitResult& hit) {
        using projectile::test::format;
        return "HitResult{distance " + format(hit.distance) + ", point " + format(hit.point) +
               ", normal " + format(hit.normal) + ", material " + std::to_string(hit.material_id) +
               (hit.front_face ? ", front face}" : ", back face}");
    }
};
