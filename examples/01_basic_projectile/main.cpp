// 01_basic_projectile: launches a projectile straight up and prints its flight until it lands.
//
// Shows the basic loop: describe the projectile with a ProjectileConfig, create its state, and
// advance the state with an integrator at a fixed time step.

#include <projectile/projectile.hpp>

#include <algorithm>
#include <cstdio>
#include <string_view>

using namespace projectile;

int main() {
    ProjectileConfig config;
    config.mass = 0.01;                             // kg
    config.radius = 0.005;                          // m
    config.initial_velocity = {0.0, 30.0, 0.0};     // m/s, straight up (+y is up in this example)
    config.gravity = {0.0, -standard_gravity, 0.0}; // m/s²

    const Vec3 launch_point{0.0, 10.0, 0.0};
    const Result<ProjectileState> created = create_projectile(config, launch_point);
    if (!created) {
        const std::string_view reason = describe(created.error());
        std::fprintf(
            stderr, "invalid projectile: %.*s\n", static_cast<int>(reason.size()), reason.data()
        );
        return 1;
    }
    ProjectileState state = *created;

    const SemiImplicitEuler integrator;
    constexpr double time_step = 1.0 / 120.0; // s
    constexpr int steps_per_line = 30;        // print every 0.25 s

    std::printf("  time (s)  height (m)  vertical speed (m/s)\n");
    int step = 0;
    double highest = state.position.y;
    while (state.position.y >= 0.0) {
        if (step % steps_per_line == 0) {
            std::printf(
                "%10.2f %11.3f %21.3f\n",
                static_cast<double>(step) * time_step,
                state.position.y,
                state.velocity.y
            );
        }
        integrator.step(state, config, time_step);
        highest = (std::max)(highest, state.position.y);
        ++step;
    }

    // Compare the highest point reached with the exact apex, launch height + v² / (2 g).
    const double exact_apex = launch_point.y + (30.0 * 30.0) / (2.0 * standard_gravity);
    std::printf("\nlanded after %.3f s\n", static_cast<double>(step) * time_step);
    std::printf("highest point %.3f m (exact apex %.3f m)\n", highest, exact_apex);
    return 0;
}
