#include "projectile/core/version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <compare>
#include <cstdint>
#include <string>
#include <type_traits>

namespace {

using projectile::Version;

constexpr Version make_version(
    std::uint32_t major_number, std::uint32_t minor_number, std::uint32_t patch_number
) {
    return Version{.major = major_number, .minor = minor_number, .patch = patch_number};
}

// The version described by the headers this test was compiled against.
constexpr Version header_version =
    make_version(PROJECTILE_VERSION_MAJOR, PROJECTILE_VERSION_MINOR, PROJECTILE_VERSION_PATCH);

} // namespace

TEST_CASE("version() reports the version the library was built from", "[core][version]") {
    // The tests build the library from the same source tree, so any difference means the compiled
    // library no longer reflects the version macros.
    const Version linked = projectile::version();

    CHECK(linked.major == header_version.major);
    CHECK(linked.minor == header_version.minor);
    CHECK(linked.patch == header_version.patch);
    CHECK(linked == header_version);
}

TEST_CASE("version_string() is MAJOR.MINOR.PATCH of the linked library", "[core][version]") {
    const std::string expected = std::to_string(PROJECTILE_VERSION_MAJOR) + "." +
                                 std::to_string(PROJECTILE_VERSION_MINOR) + "." +
                                 std::to_string(PROJECTILE_VERSION_PATCH);

    CHECK(projectile::version_string() == expected);
    CHECK(projectile::version_string() == PROJECTILE_VERSION_STRING);
}

TEST_CASE("version_string() refers to storage that outlives the call", "[core][version]") {
    // The documented contract is a view of static storage: repeated calls must observe the same
    // characters at the same address, never a temporary.
    CHECK(projectile::version_string().data() == projectile::version_string().data());
}

TEST_CASE("PROJECTILE_VERSION packs the components into one decodable integer", "[core][version]") {
    STATIC_REQUIRE(PROJECTILE_VERSION / 10000 == PROJECTILE_VERSION_MAJOR);
    STATIC_REQUIRE((PROJECTILE_VERSION / 100) % 100 == PROJECTILE_VERSION_MINOR);
    STATIC_REQUIRE(PROJECTILE_VERSION % 100 == PROJECTILE_VERSION_PATCH);
}

TEST_CASE("Version orders by major, then minor, then patch", "[core][version]") {
    STATIC_REQUIRE(make_version(0, 1, 0) < make_version(0, 1, 1));
    STATIC_REQUIRE(make_version(0, 1, 9) < make_version(0, 2, 0));   // minor outranks patch
    STATIC_REQUIRE(make_version(0, 99, 99) < make_version(1, 0, 0)); // major outranks minor
    STATIC_REQUIRE(make_version(2, 0, 0) > make_version(1, 9, 9));
    STATIC_REQUIRE(make_version(1, 2, 3) == make_version(1, 2, 3));
    STATIC_REQUIRE(make_version(1, 2, 3) != make_version(1, 2, 4));

    // Versions form a total order: any two are either equal or one precedes the other.
    STATIC_REQUIRE(
        std::is_same_v<
            decltype(make_version(1, 0, 0) <=> make_version(1, 0, 0)),
            std::strong_ordering>
    );
}
