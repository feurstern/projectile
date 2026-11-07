#pragma once

/// @file
/// @brief Umbrella header that includes the complete public API of Projectile.
///
/// Including individual headers instead keeps compile times down; this header is a
/// convenience for applications and examples.

#include "projectile/ballistics/energy.hpp"
#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/integrator.hpp"
#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/ballistics/trajectory.hpp"
#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/collision/intersection.hpp"
#include "projectile/collision/primitive_world.hpp"
#include "projectile/collision/raycast.hpp"
#include "projectile/collision/sweep.hpp"
#include "projectile/core/constants.hpp"
#include "projectile/core/export.hpp"
#include "projectile/core/result.hpp"
#include "projectile/core/version.hpp"
#include "projectile/materials/interaction.hpp"
#include "projectile/materials/material.hpp"
#include "projectile/materials/penetration.hpp"
#include "projectile/materials/ricochet.hpp"
#include "projectile/math/aabb.hpp"
#include "projectile/math/math_utils.hpp"
#include "projectile/math/plane.hpp"
#include "projectile/math/ray.hpp"
#include "projectile/math/segment.hpp"
#include "projectile/math/sphere.hpp"
#include "projectile/math/triangle.hpp"
#include "projectile/math/vec2.hpp"
#include "projectile/math/vec3.hpp"
#include "projectile/simulation/fixed_timestep.hpp"
#include "projectile/simulation/simulation.hpp"
#include "projectile/simulation/simulation_config.hpp"
