#pragma once

/// @file
/// @brief Symbol-export macro for the compiled Projectile library.
///
/// `PROJECTILE_API` marks declarations that belong to the library's binary interface.
///
/// - Static builds: expands to nothing.
/// - Shared builds: the build system defines `PROJECTILE_SHARED` for the library and for
///   everything that uses it (the CMake package does this automatically), and additionally
///   `PROJECTILE_BUILDING` while compiling the library itself. The macro then selects
///   `__declspec(dllexport)` / `__declspec(dllimport)` on Windows, or default symbol
///   visibility on GCC and Clang, where the library is built with hidden visibility.
///
/// Build systems other than CMake must define these macros the same way.

#ifdef PROJECTILE_SHARED
#  if defined(_WIN32) || defined(__CYGWIN__)
#    ifdef PROJECTILE_BUILDING
#      define PROJECTILE_API __declspec(dllexport)
#    else
#      define PROJECTILE_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) || defined(__clang__)
#    define PROJECTILE_API __attribute__((visibility("default")))
#  else
#    define PROJECTILE_API
#  endif
#else
#  define PROJECTILE_API
#endif
