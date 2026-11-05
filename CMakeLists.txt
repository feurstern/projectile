cmake_minimum_required(VERSION 3.20...4.4)

# include/projectile/core/version.hpp holds the version number; read it before project().
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ProjectileVersion.cmake")

project(
    Projectile
    VERSION ${PROJECTILE_HEADER_VERSION}
    DESCRIPTION "Engine-independent C++ projectile simulation library"
    LANGUAGES CXX)

# Re-run configuration when the version header changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECTILE_HEADER_VERSION_FILE}")

# --------------------------------------------------------------------------------------------
# Project mode
# --------------------------------------------------------------------------------------------

# Projectile is "top level" when built on its own rather than through add_subdirectory() or
# FetchContent. (PROJECT_IS_TOP_LEVEL would do this, but needs CMake 3.21.)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(PROJECTILE_IS_TOP_LEVEL ON)
else()
    set(PROJECTILE_IS_TOP_LEVEL OFF)
endif()

if(PROJECTILE_IS_TOP_LEVEL)
    if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
        message(FATAL_ERROR
            "In-source builds are not supported. Remove CMakeCache.txt and CMakeFiles/, then "
            "configure into a separate directory, e.g. 'cmake --preset debug'.")
    endif()

    get_property(PROJECTILE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT PROJECTILE_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
        message(STATUS "Projectile: no build type specified, defaulting to Release")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
    endif()

    # Keep executables and DLLs in one directory so that shared-library builds on Windows can
    # run their tests without PATH changes.
    if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    endif()
endif()

# --------------------------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------------------------

# Tests and install rules default to ON only for top-level builds, so projects that embed
# Projectile do not download test dependencies or install Projectile by accident.
option(PROJECTILE_BUILD_TESTS "Build the Projectile test suite" ${PROJECTILE_IS_TOP_LEVEL})
option(PROJECTILE_INSTALL "Generate Projectile's install and package-config rules"
       ${PROJECTILE_IS_TOP_LEVEL})
option(PROJECTILE_WARNINGS_AS_ERRORS "Treat compiler warnings in Projectile targets as errors" OFF)
option(PROJECTILE_ENABLE_CLANG_TIDY "Run clang-tidy on Projectile targets during the build" OFF)
# The standard BUILD_SHARED_LIBS variable selects a shared library instead of a static one.

include(GNUInstallDirs)
include("${PROJECT_SOURCE_DIR}/cmake/CompilerOptions.cmake")

projectile_check_compiler_version()

# Projectile does not use C++20 modules. Skipping the module dependency scan (CMake 3.28+)
# speeds up builds and avoids depending on clang-scan-deps. Applies to this directory only.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# --------------------------------------------------------------------------------------------
# Core library
# --------------------------------------------------------------------------------------------

add_library(projectile src/core/version.cpp)
add_library(Projectile::Projectile ALIAS projectile)

target_compile_features(projectile PUBLIC cxx_std_20)
target_include_directories(
    projectile
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

set_target_properties(
    projectile
    PROPERTIES CXX_EXTENSIONS OFF
               EXPORT_NAME Projectile
               VERSION ${PROJECT_VERSION}
               # Before 1.0, a minor release may break the ABI.
               SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

get_target_property(PROJECTILE_LIBRARY_TYPE projectile TYPE)
if(PROJECTILE_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
    # See include/projectile/core/export.hpp. PROJECTILE_SHARED propagates to consumers.
    target_compile_definitions(projectile PUBLIC PROJECTILE_SHARED PRIVATE PROJECTILE_BUILDING)
    # Export only what PROJECTILE_API marks, on every platform.
    set_target_properties(projectile PROPERTIES CXX_VISIBILITY_PRESET hidden
                                                VISIBILITY_INLINES_HIDDEN ON)
endif()

projectile_target_warnings(projectile)
projectile_target_clang_tidy(projectile)

# --------------------------------------------------------------------------------------------
# Installation and CMake package
# --------------------------------------------------------------------------------------------

if(PROJECTILE_INSTALL)
    include(CMakePackageConfigHelpers)

    set(PROJECTILE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Projectile")

    install(
        TARGETS projectile
        EXPORT ProjectileTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

    install(
        DIRECTORY include/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
        PATTERN "*.hpp")

    install(
        EXPORT ProjectileTargets
        NAMESPACE Projectile::
        DESTINATION "${PROJECTILE_INSTALL_CMAKEDIR}")

    configure_package_config_file(
        cmake/ProjectileConfig.cmake.in "${PROJECT_BINARY_DIR}/ProjectileConfig.cmake"
        INSTALL_DESTINATION "${PROJECTILE_INSTALL_CMAKEDIR}")

    # Before 1.0, minor releases may break compatibility, so a request for 0.1 must not accept
    # 0.2. Switch to SameMajorVersion with the 1.0 release.
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/ProjectileConfigVersion.cmake" COMPATIBILITY SameMinorVersion)

    install(FILES "${PROJECT_BINARY_DIR}/ProjectileConfig.cmake"
                  "${PROJECT_BINARY_DIR}/ProjectileConfigVersion.cmake"
            DESTINATION "${PROJECTILE_INSTALL_CMAKEDIR}")

    install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
endif()

# --------------------------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------------------------

if(PROJECTILE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
