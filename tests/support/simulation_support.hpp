#pragma once

// Catch2 support for the simulation types: readable values in failure messages.

#include "projectile/simulation/fixed_timestep.hpp"
#include "projectile/simulation/simulation.hpp"

#include <catch2/catch_tostring.hpp>

#include "support/ballistics_support.hpp"
#include "support/collision_matchers.hpp"

#include <string>

template <>
struct Catch::StringMaker<projectile::FrameSteps> {
    static std::string convert(const projectile::FrameSteps& frame) {
        return "FrameSteps{steps " + std::to_string(frame.steps) + ", dropped time " +
               projectile::test::format(frame.dropped_time) + "}";
    }
};

template <>
struct Catch::StringMaker<projectile::ProjectileId> {
    static std::string convert(projectile::ProjectileId id) {
        return "ProjectileId{" + std::to_string(id.value) + "}";
    }
};

template <>
struct Catch::StringMaker<projectile::ProjectileEventKind> {
    static std::string convert(projectile::ProjectileEventKind kind) {
        return kind == projectile::ProjectileEventKind::hit ? "hit" : "expired";
    }
};

template <>
struct Catch::StringMaker<projectile::ProjectileEvent> {
    static std::string convert(const projectile::ProjectileEvent& event) {
        using projectile::test::format;
        std::string text = "ProjectileEvent{" +
                           StringMaker<projectile::ProjectileEventKind>::convert(event.kind) +
                           ", " + StringMaker<projectile::ProjectileId>::convert(event.id) +
                           ", time " + format(event.time) + ", " +
                           StringMaker<projectile::ProjectileState>::convert(event.state);
        if (event.hit) {
            text += ", " + StringMaker<projectile::HitResult>::convert(*event.hit);
        }
        return text + "}";
    }
};
