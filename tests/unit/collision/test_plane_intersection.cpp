#include "projectile/collision/intersection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/collision_matchers.hpp"

using namespace projectile;
using Catch::Matchers::WithinAbs;
using projectile::test::is_near;
using projectile::test::require_value;

namespace {

// The plane y = 2, facing +y.
constexpr Plane plane{.normal = {0.0, 1.0, 0.0}, .distance = 2.0};
constexpr Vec3 up{0.0, 1.0, 0.0};
constexpr Vec3 down{0.0, -1.0, 0.0};

} // namespace

// --- Rays -------------------------------------------------------------------------------------

TEST_CASE("a ray hits the front of a plane", "[collision][plane]") {
    const HitResult hit =
        require_value(intersect(Ray{.origin = {1.0, 5.0, -1.0}, .direction = down}, plane));
    CHECK(hit.distance == 3.0);
    CHECK(hit.point == Vec3{1.0, 2.0, -1.0});
    CHECK(hit.normal == up);
    CHECK(hit.front_face);
    CHECK(hit.material_id == 0);
}

TEST_CASE("an oblique ray hits a plane at the expected distance", "[collision][plane]") {
    const Ray ray{.origin = {0.0, 4.0, 0.0}, .direction = {0.6, -0.8, 0.0}};
    const HitResult hit = require_value(intersect(ray, plane));
    CHECK_THAT(hit.distance, WithinAbs(2.5, 1e-15));
    CHECK_THAT(hit.point, is_near(Vec3{1.5, 2.0, 0.0}, 1e-15, 0.0));
}

TEST_CASE("a ray hitting a plane from behind reports a back face", "[collision][plane]") {
    const Ray ray{.origin = {1.0, -1.0, 0.0}, .direction = up};
    const HitResult hit = require_value(intersect(ray, plane));
    CHECK(hit.distance == 3.0);
    CHECK(hit.point == Vec3{1.0, 2.0, 0.0});
    CHECK(hit.normal == down); // facing the side the ray came from
    CHECK_FALSE(hit.front_face);

    CHECK_FALSE(intersect(ray, plane, 100.0, Culling::back_faces).has_value());
}

TEST_CASE("a ray parallel to a plane never hits it", "[collision][plane]") {
    CHECK_FALSE(
        intersect(Ray{.origin = {0.0, 5.0, 0.0}, .direction = {1.0, 0.0, 0.0}}, plane).has_value()
    );
    // Not even when it lies in the plane.
    CHECK_FALSE(
        intersect(Ray{.origin = {0.0, 2.0, 0.0}, .direction = {1.0, 0.0, 0.0}}, plane).has_value()
    );
}

TEST_CASE("a ray pointing away from a plane misses it", "[collision][plane]") {
    CHECK_FALSE(intersect(Ray{.origin = {0.0, 5.0, 0.0}, .direction = up}, plane).has_value());
    CHECK_FALSE(intersect(Ray{.origin = {0.0, -5.0, 0.0}, .direction = down}, plane).has_value());
}

TEST_CASE("a ray starting on a plane hits it at distance zero", "[collision][plane]") {
    const HitResult into =
        require_value(intersect(Ray{.origin = {3.0, 2.0, 4.0}, .direction = down}, plane));
    CHECK(into.distance == 0.0);
    CHECK(into.point == Vec3{3.0, 2.0, 4.0});
    CHECK(into.front_face);

    // A plane has no inside, so the start counts as a crossing in either direction.
    const HitResult away =
        require_value(intersect(Ray{.origin = {3.0, 2.0, 4.0}, .direction = up}, plane));
    CHECK(away.distance == 0.0);
    CHECK_FALSE(away.front_face);
}

TEST_CASE("max_distance limits plane hits, inclusively", "[collision][plane]") {
    const Ray ray{.origin = {0.0, 5.0, 0.0}, .direction = down};
    CHECK(intersect(ray, plane, 3.0).has_value());
    CHECK_FALSE(intersect(ray, plane, 2.999).has_value());
    CHECK_FALSE(intersect(ray, plane, 0.0).has_value());
    CHECK(intersect(Ray{.origin = {0.0, 2.0, 0.0}, .direction = down}, plane, 0.0).has_value());
}

// --- Segments ---------------------------------------------------------------------------------

TEST_CASE("a segment crossing a plane hits it", "[collision][plane][segment]") {
    const Segment segment{.start = {1.0, 5.0, -1.0}, .end = {1.0, -1.0, -1.0}};
    const HitResult hit = require_value(intersect(segment, plane));
    CHECK(hit.distance == 3.0);
    CHECK(hit.point == Vec3{1.0, 2.0, -1.0});
    CHECK(hit.normal == up);
    CHECK(hit.front_face);
}

TEST_CASE(
    "a segment endpoint on a plane is hit exactly at the endpoint", "[collision][plane][segment]"
) {
    const Segment ending_on_plane{.start = {0.3, 5.7, 0.1}, .end = {0.7, 2.0, 0.9}};
    const HitResult end_hit = require_value(intersect(ending_on_plane, plane));
    CHECK(end_hit.point == ending_on_plane.end);
    CHECK(end_hit.distance == length(ending_on_plane));

    const Segment starting_on_plane{.start = {0.7, 2.0, 0.9}, .end = {0.3, -5.7, 0.1}};
    const HitResult start_hit = require_value(intersect(starting_on_plane, plane));
    CHECK(start_hit.point == starting_on_plane.start);
    CHECK(start_hit.distance == 0.0);
}

TEST_CASE("a segment that stops short of a plane misses it", "[collision][plane][segment]") {
    CHECK_FALSE(
        intersect(Segment{.start = {0.0, 5.0, 0.0}, .end = {0.0, 3.0, 0.0}}, plane).has_value()
    );
}

TEST_CASE("parallel and zero-length segments never hit a plane", "[collision][plane][segment]") {
    CHECK_FALSE(
        intersect(Segment{.start = {0.0, 5.0, 0.0}, .end = {4.0, 5.0, 0.0}}, plane).has_value()
    );
    CHECK_FALSE(
        intersect(Segment{.start = {0.0, 2.0, 0.0}, .end = {4.0, 2.0, 0.0}}, plane).has_value()
    );
    CHECK_FALSE(
        intersect(Segment{.start = {0.0, 2.0, 0.0}, .end = {0.0, 2.0, 0.0}}, plane).has_value()
    );
}

TEST_CASE(
    "a segment crossing a plane from behind reports a back face", "[collision][plane][segment]"
) {
    const Segment segment{.start = {0.0, -1.0, 0.0}, .end = {0.0, 5.0, 0.0}};
    const HitResult hit = require_value(intersect(segment, plane));
    CHECK(hit.distance == 3.0);
    CHECK(hit.normal == down);
    CHECK_FALSE(hit.front_face);
    CHECK_FALSE(intersect(segment, plane, Culling::back_faces).has_value());
}
