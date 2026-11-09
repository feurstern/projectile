# Projectile's benchmarks: one program, projectile_benchmarks, built with
# -DPROJECTILE_BUILD_BENCHMARKS=ON or the bench preset. docs/benchmarks.md describes what they
# measure and how to get reproducible results. The core library never depends on this directory.

set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------------------------
# Google Benchmark
# --------------------------------------------------------------------------------------------

# An installed Google Benchmark 1.10 or newer is preferred, which allows fully offline builds
# (main.cpp needs 1.10's MaybeReenterWithoutASLR). Otherwise the pinned release below is
# downloaded. To build from a local copy of its sources instead, configure with
# -DFETCHCONTENT_SOURCE_DIR_BENCHMARK=<path>.
find_package(benchmark 1.10 CONFIG QUIET)
if(benchmark_FOUND)
    message(STATUS "Projectile: using installed Google Benchmark ${benchmark_VERSION}")
else()
    message(STATUS "Projectile: Google Benchmark not found, fetching Google Benchmark v1.10.0")
    include(FetchContent)
    # Only the static library: no tests, no install rules, and no -Werror, which would let a
    # warning from a newer compiler in Google Benchmark's own code fail the build.
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_INSTALL_DOCS OFF)
    set(BENCHMARK_ENABLE_WERROR OFF)
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.10.0.tar.gz
        URL_HASH SHA256=acc5c7c0fbed855c4aaa2d7c3c36fb82b0c15c28f4e151e3f679c2fc0aa404af)
    FetchContent_MakeAvailable(benchmark)

    # Treat Google Benchmark's headers as system headers, so that Projectile's strict warning
    # flags apply to Projectile code only.
    get_target_property(_benchmark_include_dirs benchmark INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(benchmark PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                                               "${_benchmark_include_dirs}")
endif()

# --------------------------------------------------------------------------------------------
# projectile_benchmarks
# --------------------------------------------------------------------------------------------

add_executable(
    projectile_benchmarks
    main.cpp
    integration_benchmark.cpp
    raycast_benchmark.cpp
    simulation_benchmark.cpp)
target_link_libraries(projectile_benchmarks PRIVATE Projectile::Projectile benchmark::benchmark)
if(WIN32)
    # main.cpp reads the CPU model from the registry.
    target_link_libraries(projectile_benchmarks PRIVATE advapi32)
endif()
projectile_target_warnings(projectile_benchmarks)
projectile_target_clang_tidy(projectile_benchmarks)

# Every result records how the benchmarks were built (see main.cpp). The C++ flags are
# CMAKE_CXX_FLAGS plus those of the configuration being built; characters that would end or
# split the generator expressions are escaped.
set(_benchmark_cxx_flags "${CMAKE_CXX_FLAGS}")
foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_config}" _config_upper)
    set(_config_flags "${CMAKE_CXX_FLAGS_${_config_upper}}")
    string(REPLACE ">" "$<ANGLE-R>" _config_flags "${_config_flags}")
    string(REPLACE "," "$<COMMA>" _config_flags "${_config_flags}")
    string(REPLACE ";" "$<SEMICOLON>" _config_flags "${_config_flags}")
    string(APPEND _benchmark_cxx_flags "$<$<CONFIG:${_config}>: ${_config_flags}>")
endforeach()

target_compile_definitions(
    projectile_benchmarks
    PRIVATE "PROJECTILE_BENCHMARK_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
            "PROJECTILE_BENCHMARK_BUILD_TYPE=\"$<CONFIG>\""
            "PROJECTILE_BENCHMARK_CXX_FLAGS=\"${_benchmark_cxx_flags}\""
            "PROJECTILE_BENCHMARK_LIBRARY_TYPE=\"$<TARGET_PROPERTY:projectile,TYPE>\"")

# With tests enabled, a quick run of every benchmark, one iteration each, checks that they all
# work (label "benchmark"). It measures nothing; see docs/benchmarks.md for real runs.
if(PROJECTILE_BUILD_TESTS)
    add_test(NAME benchmark.smoke COMMAND projectile_benchmarks --benchmark_min_time=1x)
    set_tests_properties(benchmark.smoke PROPERTIES LABELS benchmark)
endif()
