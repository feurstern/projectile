#pragma once

/// @file
/// @brief Library version information.
///
/// Projectile follows [Semantic Versioning](https://semver.org). While the major version is 0,
/// minor releases may contain breaking changes.
///
/// The `PROJECTILE_VERSION_*` macros describe the headers a program was compiled against.
/// projectile::version() describes the compiled library the program is linked with. Comparing
/// the two detects a mismatched installation, such as new headers with an old shared library.
///
/// This header is the single source of truth for the version number: the CMake build reads the
/// three component macros below, so their `#define NAME value` form must be preserved.

#include "projectile/core/export.hpp"

#include <compare>
#include <cstdint>
#include <string_view>

/// Major version of the Projectile headers.
#define PROJECTILE_VERSION_MAJOR 0
/// Minor version of the Projectile headers.
#define PROJECTILE_VERSION_MINOR 1
/// Patch version of the Projectile headers.
#define PROJECTILE_VERSION_PATCH 0

/// Version of the headers as one integer, `MAJOR * 10000 + MINOR * 100 + PATCH`, for preprocessor
/// checks such as `#if PROJECTILE_VERSION >= 200` (version 0.2.0 or newer). The minor and patch
/// components are always kept below 100.
#define PROJECTILE_VERSION                                                                         \
    ((PROJECTILE_VERSION_MAJOR * 10000) + (PROJECTILE_VERSION_MINOR * 100) +                       \
     PROJECTILE_VERSION_PATCH)

/// @cond PROJECTILE_DETAIL
#define PROJECTILE_DETAIL_STRINGIFY_IMPL(value) #value
#define PROJECTILE_DETAIL_STRINGIFY(value) PROJECTILE_DETAIL_STRINGIFY_IMPL(value)
/// @endcond

/// Version of the headers as a string literal, for example `"0.1.0"`.
#define PROJECTILE_VERSION_STRING                                                                  \
    PROJECTILE_DETAIL_STRINGIFY(PROJECTILE_VERSION_MAJOR)                                          \
    "." PROJECTILE_DETAIL_STRINGIFY(PROJECTILE_VERSION_MINOR) "." PROJECTILE_DETAIL_STRINGIFY(     \
        PROJECTILE_VERSION_PATCH                                                                   \
    )

namespace projectile {

/// A semantic version number.
///
/// Versions compare lexicographically: first by major, then minor, then patch.
struct Version {
    std::uint32_t major = 0; ///< Incremented for incompatible API changes (from 1.0 on).
    std::uint32_t minor = 0; ///< Incremented for new features; may break the API before 1.0.
    std::uint32_t patch = 0; ///< Incremented for backward-compatible fixes.

    /// Lexicographic comparison by (major, minor, patch). Also provides `==` and `!=`.
    constexpr auto operator<=>(const Version&) const = default;
};

/// Returns the version of the compiled Projectile library linked into the program.
///
/// The value is fixed when the library is built. It differs from the `PROJECTILE_VERSION_*`
/// macros when the program was compiled against headers from another release.
[[nodiscard]] PROJECTILE_API Version version() noexcept;

/// Returns the version of the compiled Projectile library as `"MAJOR.MINOR.PATCH"`.
///
/// The view refers to static storage inside the library and stays valid for the lifetime of
/// the program.
[[nodiscard]] PROJECTILE_API std::string_view version_string() noexcept;

} // namespace projectile
