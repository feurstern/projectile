# Projectile test suite: unit tests, a header self-containment check, and package-consumption
# tests. Catch2 is needed only here; the core library has no dependencies.

# Build every target in this directory, including a fetched Catch2, as C++20. Catch2 detects
# std::string_view support per translation unit, so the Catch2 library must be compiled with the
# same standard as the tests, or StringMaker<std::string_view> fails to link.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------------------------
# Catch2
# --------------------------------------------------------------------------------------------

# An installed Catch2 3.x is preferred, which allows fully offline builds. Otherwise the pinned
# release below is downloaded. To build from a local copy of the Catch2 sources instead,
# configure with -DFETCHCONTENT_SOURCE_DIR_CATCH2=<path>.
find_package(Catch2 3 CONFIG QUIET)
if(Catch2_FOUND)
    message(STATUS "Projectile: using installed Catch2 ${Catch2_VERSION}")
else()
    message(STATUS "Projectile: Catch2 3 not found, fetching Catch2 v3.16.1")
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz
        URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

    # Treat Catch2's headers as system headers so that Projectile's strict warning flags apply
    # to Projectile code only.
    get_target_property(_catch2_include_dirs Catch2 INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(Catch2 PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                                            "${_catch2_include_dirs}")
endif()
include(Catch)

# --------------------------------------------------------------------------------------------
# Unit tests
# --------------------------------------------------------------------------------------------

add_executable(projectile_unit_tests unit/core/test_version.cpp)
target_link_libraries(projectile_unit_tests PRIVATE Projectile::Projectile Catch2::Catch2WithMain)
projectile_target_warnings(projectile_unit_tests)
projectile_target_clang_tidy(projectile_unit_tests)

catch_discover_tests(projectile_unit_tests PROPERTIES LABELS unit)

# --------------------------------------------------------------------------------------------
# Header self-containment
# --------------------------------------------------------------------------------------------

# Every public header must compile on its own, so that users can include any of them first.
# One generated translation unit per header; a header that relies on something it does not
# include fails to build. This check compiles only and adds no CTest test.
file(
    GLOB_RECURSE _projectile_public_headers CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/projectile/*.hpp")

set(_header_check_sources "")
foreach(_header IN LISTS _projectile_public_headers)
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/header_check/${_header}.cpp")
    file(CONFIGURE OUTPUT "${_source}" CONTENT "#include <${_header}>\n" @ONLY)
    list(APPEND _header_check_sources "${_source}")
endforeach()

add_library(projectile_header_check OBJECT ${_header_check_sources})
target_link_libraries(projectile_header_check PRIVATE Projectile::Projectile)
projectile_target_warnings(projectile_header_check)
# The generated sources live in the build tree, outside the reach of the .clang-tidy lookup.
projectile_target_clang_tidy(projectile_header_check CONFIG_FILE "${PROJECT_SOURCE_DIR}/.clang-tidy")

# --------------------------------------------------------------------------------------------
# Package consumption
# --------------------------------------------------------------------------------------------

add_subdirectory(package)
