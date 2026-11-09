// projectile_benchmarks: runs Projectile's benchmarks with Google Benchmark.
//
// Every run records how it was built and where it ran, next to the context that Google
// Benchmark records itself (CPU count, caches, frequency scaling): the Projectile version, the
// compiler, the build type, the C++ flags and the CPU model. Pass --benchmark_out=<file>
// --benchmark_out_format=json to keep them; docs/benchmarks.md explains how to run the
// benchmarks reproducibly, and report.py summarizes the results.

#include <projectile/projectile.hpp>

#include "suite.hpp"
#include <benchmark/benchmark.h>

#include <cstdio>
#include <string>
#include <string_view>

#ifdef __linux__
#  include <fstream>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#  include <sys/types.h>

#  include <array>
#elif defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

#  include <array>
#endif

namespace {

/// @p text without leading and trailing spaces and tabs.
std::string trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t");
    return std::string(text.substr(first, last - first + 1));
}

/// The CPU's model name as the operating system reports it, or "unknown".
std::string cpu_model() {
#ifdef __linux__
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        // x86 reports "model name : ..."; other architectures may not report a name at all.
        if (line.starts_with("model name")) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                return trim(std::string_view(line).substr(colon + 1));
            }
        }
    }
#elif defined(__APPLE__)
    std::array<char, 256> name{};
    std::size_t size = name.size();
    if (sysctlbyname("machdep.cpu.brand_string", name.data(), &size, nullptr, 0) == 0) {
        return trim(name.data());
    }
#elif defined(_WIN32)
    std::array<char, 256> name{};
    DWORD size = static_cast<DWORD>(name.size());
    if (RegGetValueA(
            HKEY_LOCAL_MACHINE,
            R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)",
            "ProcessorNameString",
            RRF_RT_REG_SZ,
            nullptr,
            name.data(),
            &size
        ) == ERROR_SUCCESS) {
        return trim(name.data());
    }
#endif
    return "unknown";
}

} // namespace

int main(int argc, char** argv) {
    // On Linux, run again with address-space randomization disabled, as BENCHMARK_MAIN() does:
    // randomized addresses add noise that differs from run to run.
    benchmark::MaybeReenterWithoutASLR(argc, argv);

    projectile::bench::register_raycast_benchmarks();
    projectile::bench::register_integration_benchmarks();
    projectile::bench::register_simulation_benchmarks();

    // Set by benchmarks/CMakeLists.txt from the build configuration.
    benchmark::AddCustomContext("projectile_version", std::string(projectile::version_string()));
    benchmark::AddCustomContext("compiler", PROJECTILE_BENCHMARK_COMPILER);
    benchmark::AddCustomContext("build_type", PROJECTILE_BENCHMARK_BUILD_TYPE);
    benchmark::AddCustomContext("cxx_flags", trim(PROJECTILE_BENCHMARK_CXX_FLAGS));
    benchmark::AddCustomContext("projectile_library", PROJECTILE_BENCHMARK_LIBRARY_TYPE);
    benchmark::AddCustomContext("cpu_model", cpu_model());

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
#ifndef NDEBUG
    std::fputs(
        "warning: these benchmarks were built with assertions enabled, which is not an "
        "optimized build; use the bench preset or a Release build for meaningful results\n",
        stderr
    );
#endif
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
