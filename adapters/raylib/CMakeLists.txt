# The raylib adapter and its demos. Optional: the core library never depends on them.
#
# There are two ways to build them:
#
#   - with Projectile, from the repository root, with -DPROJECTILE_BUILD_RAYLIB=ON or the demos
#     preset;
#   - on their own, against an installed Projectile:
#
#       cmake -S adapters/raylib -B build/raylib -DCMAKE_PREFIX_PATH=<Projectile's prefix>
#
#     That build sees nothing of Projectile but its installed package, which shows that the
#     adapter and the demos need nothing but the public API.
#
# An installed raylib 5.5 or newer is used if found; otherwise raylib 5.5 is downloaded.

cmake_minimum_required(VERSION 3.20...4.4)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # Standalone: a project of its own, which requires exactly the Projectile version of the
    # sources it comes with.
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ProjectileVersion.cmake")
    project(
        ProjectileRaylib
        VERSION ${PROJECTILE_HEADER_VERSION}
        DESCRIPTION "raylib adapter and demos for the Projectile library"
        LANGUAGES C CXX)

    option(PROJECTILE_BUILD_TESTS "Register the demos as CTest tests" ON)
    option(PROJECTILE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
    option(PROJECTILE_ENABLE_CLANG_TIDY "Run clang-tidy on the adapter and the demos" OFF)
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/CompilerOptions.cmake")

    get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
    if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    endif()
    # As in Projectile's own build: no C++20 modules, so no module dependency scan.
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

    find_package(Projectile ${PROJECTILE_HEADER_VERSION} EXACT CONFIG REQUIRED)
    if(PROJECTILE_BUILD_TESTS)
        enable_testing()
    endif()
else()
    # Part of Projectile's build. raylib is written in C.
    enable_language(C)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------------------------
# raylib
# --------------------------------------------------------------------------------------------

# To build from a local copy of the raylib sources instead of downloading them, configure with
# -DFETCHCONTENT_SOURCE_DIR_RAYLIB=<path>.
find_package(raylib 5.5 CONFIG QUIET)
if(raylib_FOUND)
    message(STATUS "Projectile: using installed raylib ${raylib_VERSION}")
else()
    message(STATUS "Projectile: raylib not found, fetching raylib 5.5")
    include(FetchContent)
    # Only the static library, without raylib's examples. raylib declares compatibility with
    # CMake 3.5, under which option() would ignore variables set here; policy CMP0077 makes it
    # use them. (CMake 4 warns that raylib 5.5 declares such an old version; the warning is
    # harmless.)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e)
    FetchContent_MakeAvailable(raylib)

    # Treat raylib's headers as system headers, so that Projectile's strict warning flags apply
    # to Projectile code only.
    get_target_property(_raylib_include_dirs raylib INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(raylib PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                                            "${_raylib_include_dirs}")
endif()

# --------------------------------------------------------------------------------------------
# The adapter
# --------------------------------------------------------------------------------------------

# Header-only: include <projectile/raylib.hpp>.
add_library(projectile_raylib INTERFACE)
add_library(Projectile::Raylib ALIAS projectile_raylib)
target_include_directories(projectile_raylib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(projectile_raylib INTERFACE Projectile::Projectile raylib)

# Every adapter header must compile on its own, as Projectile's public headers do. One generated
# translation unit per header; this check compiles only and adds no test.
file(
    GLOB_RECURSE _adapter_headers CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/projectile/*.hpp")
set(_adapter_header_checks "")
foreach(_header IN LISTS _adapter_headers)
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/header_check/${_header}.cpp")
    file(CONFIGURE OUTPUT "${_source}" CONTENT "#include <${_header}>\n" @ONLY)
    list(APPEND _adapter_header_checks "${_source}")
endforeach()
add_library(projectile_raylib_header_check OBJECT ${_adapter_header_checks})
target_link_libraries(projectile_raylib_header_check PRIVATE Projectile::Raylib)
projectile_target_warnings(projectile_raylib_header_check)
# The generated sources live in the build tree, outside the reach of the .clang-tidy lookup.
projectile_target_clang_tidy(projectile_raylib_header_check
                             CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/../../.clang-tidy")

# --------------------------------------------------------------------------------------------
# Demos
# --------------------------------------------------------------------------------------------

# With tests enabled, each demo also runs as a test (label "demo"): a scripted run of 150
# frames in a hidden window, which checks that the demo shows what it should. Without a display
# the demo cannot open its window and exits with code 77, which CTest reports as skipped; in CI,
# the tests run under Xvfb.
function(projectile_add_demo name)
    add_executable(raylib_${name} examples/${name}/main.cpp)
    target_link_libraries(raylib_${name} PRIVATE Projectile::Raylib)
    # The demos share examples/common/demo.hpp.
    target_include_directories(raylib_${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    projectile_target_warnings(raylib_${name})
    projectile_target_clang_tidy(raylib_${name})
    if(PROJECTILE_BUILD_TESTS)
        add_test(NAME demo.${name} COMMAND raylib_${name} --frames 150 --hidden)
        set_tests_properties(demo.${name} PROPERTIES LABELS demo SKIP_RETURN_CODE 77)
    endif()
endfunction()

projectile_add_demo(ballistic_drop)
projectile_add_demo(hitscan_visualizer)
projectile_add_demo(material_interaction)
projectile_add_demo(multiple_projectiles)

# --------------------------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------------------------

# Unit tests of the conversions, which need no window (label "raylib"). They use Catch2: the copy
# that Projectile's tests fetched, or an installed one.
if(PROJECTILE_BUILD_TESTS)
    if(NOT TARGET Catch2::Catch2WithMain)
        find_package(Catch2 3 CONFIG QUIET)
    endif()
    if(TARGET Catch2::Catch2WithMain)
        add_executable(projectile_raylib_tests tests/test_conversions.cpp)
        target_link_libraries(projectile_raylib_tests PRIVATE Projectile::Raylib
                                                              Catch2::Catch2WithMain)
        projectile_target_warnings(projectile_raylib_tests)
        projectile_target_clang_tidy(projectile_raylib_tests)
        add_test(NAME raylib.conversions COMMAND projectile_raylib_tests)
        set_tests_properties(raylib.conversions PROPERTIES LABELS raylib)
    else()
        message(STATUS "Projectile: Catch2 3 not found; the raylib adapter's unit tests are skipped")
    endif()
endif()
