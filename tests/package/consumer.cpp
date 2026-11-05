// A minimal downstream program used by the package tests. It includes the Projectile headers
// and links the Projectile library exactly as a real project would, then checks that the
// headers, the linked library and the CMake package all agree on the version.

#include <projectile/projectile.hpp>

#include <cstdio>
#include <cstdlib>
#include <string_view>

#ifndef CONSUMER_EXPECTED_VERSION
#  error "CONSUMER_EXPECTED_VERSION must be defined by the consumer's build"
#endif

int main() {
    constexpr projectile::Version header_version{
        .major = PROJECTILE_VERSION_MAJOR,
        .minor = PROJECTILE_VERSION_MINOR,
        .patch = PROJECTILE_VERSION_PATCH,
    };
    const std::string_view library_version = projectile::version_string();
    const std::string_view package_version = CONSUMER_EXPECTED_VERSION;

    std::printf(
        "Projectile headers %s, library %.*s, CMake package %.*s\n",
        PROJECTILE_VERSION_STRING,
        static_cast<int>(library_version.size()),
        library_version.data(),
        static_cast<int>(package_version.size()),
        package_version.data()
    );

    if (projectile::version() != header_version || library_version != PROJECTILE_VERSION_STRING ||
        library_version != package_version) {
        std::fputs("error: Projectile version mismatch\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
