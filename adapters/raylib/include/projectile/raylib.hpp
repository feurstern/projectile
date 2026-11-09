#pragma once

/// @file
/// @brief The raylib adapter: conversions between Projectile's types and raylib's, and debug
/// drawing of shapes, queries, trajectories and projectiles.
///
/// The adapter is header-only and optional: it depends on Projectile and raylib, and Projectile
/// never depends on it. docs/integration.md shows how to use it.

#include <projectile/raylib/conversions.hpp>
#include <projectile/raylib/draw.hpp>
