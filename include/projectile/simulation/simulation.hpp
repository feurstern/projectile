#pragma once

/// @file
/// @brief Real-time simulation of many projectiles at a fixed time step.

#include "projectile/ballistics/projectile_config.hpp"
#include "projectile/ballistics/projectile_state.hpp"
#include "projectile/collision/collision_world.hpp"
#include "projectile/collision/hit_result.hpp"
#include "projectile/core/export.hpp"
#include "projectile/core/result.hpp"
#include "projectile/math/vec3.hpp"
#include "projectile/simulation/fixed_timestep.hpp"
#include "projectile/simulation/simulation_config.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace projectile {

/// Identifies a projectile in a Simulation.
///
/// An id is valid while its projectile is in the simulation. Once the projectile is removed, its
/// id stays invalid: it never identifies a later projectile. (Strictly, ids repeat after the
/// storage of one projectile has been reused 2³² times.) A default-constructed id identifies no
/// projectile.
struct ProjectileId {
    /// Opaque value, never 0 for a projectile. Use it to hash or print ids.
    std::uint64_t value = 0;

    /// Ids are equal when they identify the same projectile.
    constexpr bool operator==(const ProjectileId&) const = default;

    /// An arbitrary but consistent order, for sorted containers.
    constexpr auto operator<=>(const ProjectileId&) const = default;
};

/// A projectile in a Simulation.
struct SimulatedProjectile {
    ProjectileId id;         ///< Its id.
    ProjectileConfig config; ///< Its parameters.
    ProjectileState state;   ///< Its state after the last step.

    /// Position of its center before the last step, or, for a projectile added since, its
    /// current position. Drawing interpolates between this and `state.position`; see
    /// interpolated_position().
    Vec3 previous_position;

    /// Exact member-wise comparison.
    constexpr bool operator==(const SimulatedProjectile&) const = default;
};

/// Where to draw @p projectile: between its positions before and after the last step, at
/// @p alpha, which is normally Simulation::interpolation_alpha(). Drawing there instead of at
/// `state.position` keeps motion smooth when the frame rate differs from the simulation rate.
[[nodiscard]] constexpr Vec3 interpolated_position(
    const SimulatedProjectile& projectile, double alpha
) noexcept {
    return lerp(projectile.previous_position, projectile.state.position, alpha);
}

/// Why a projectile's flight ended.
enum class ProjectileEventKind : std::uint8_t {
    hit,     ///< It hit the collision world, and stopped at the contact.
    expired, ///< Its lifetime ran out.
};

/// Reports the end of a projectile's flight. The simulation has already removed the projectile.
struct ProjectileEvent {
    /// What happened.
    ProjectileEventKind kind = ProjectileEventKind::hit;

    /// The projectile's id, which is no longer valid.
    ProjectileId id;

    /// Simulation time at which it happened, in s. For a hit, the time of impact, which lies
    /// within the step that found it.
    double time = 0.0;

    /// The projectile's final state, inactive. For a hit, its center and velocity at the contact.
    ProjectileState state;

    /// The projectile's parameters.
    ProjectileConfig config;

    /// The contact, for hits.
    std::optional<HitResult> hit;

    /// Exact member-wise comparison.
    constexpr bool operator==(const ProjectileEvent&) const = default;
};

/// A function that receives a Simulation's events. See Simulation::set_event_callback().
using ProjectileEventCallback = std::function<void(const ProjectileEvent&)>;

/// A real-time simulation of many projectiles, advanced at a fixed time step.
///
/// Add projectiles, then call update() once per frame with the frame's duration:
///
///     Simulation simulation;                         // 120 Hz
///     simulation.set_collision_world(&world);        // optional
///     const Result<ProjectileId> id = simulation.add_projectile(state, config);
///     ...
///     simulation.update(frame_delta);
///
/// update() runs as many fixed steps as the frame's time calls for, through a FixedTimestep, so
/// the result depends only on the number of steps, never on the frame rate. Each step advances
/// every projectile with the configured integrator, then sweeps it through the collision world,
/// if one is set, along the straight line it moved. A projectile ends its flight when it hits the
/// world or its lifetime runs out. It then leaves the simulation, and an event reports it.
///
/// **Projectiles.** add_projectile() returns a ProjectileId that stays valid until the projectile
/// leaves. projectiles() lists the projectiles in a deterministic but unspecified order, which
/// changes when projectiles leave. Pointers from find() and the span from projectiles() are
/// valid until the next call that adds, removes or steps projectiles.
///
/// **Events.** set_event_callback() installs a function that receives every ProjectileEvent,
/// after the step that produced it has finished. The callback may read the simulation, add and
/// remove projectiles, and replace or clear the callback; calls to update() and step() from inside
/// it do nothing. It must not destroy, move or assign the simulation. If it throws, the exception
/// leaves the step, and the step's remaining events are not reported.
///
/// **Memory.** The simulation reuses the memory of projectiles that left, so once it has held its
/// largest number of projectiles, adding and stepping projectiles allocates nothing.
/// reserve() allocates that memory up front.
///
/// **Determinism.** The same projectiles, settings, collision world and sequence of calls give
/// bit-identical results in the same build. See docs/simulation.md for other builds.
///
/// **Threads.** A Simulation may be used by one thread at a time. Steps query the collision world
/// on the calling thread.
class Simulation {
public:
    /// A simulation with the default settings (see SimulationConfig): 120 Hz, semi-implicit
    /// Euler, no collision world.
    Simulation() = default;

    /// Creates a simulation with the settings in @p config, or returns the first problem found
    /// in @p config.
    [[nodiscard]] PROJECTILE_API static Result<Simulation> create(const SimulationConfig& config);

    // --- Projectiles ----------------------------------------------------------------------------

    /// Adds a projectile in @p state with the parameters in @p config, and returns its id. It
    /// moves from the next step on. A projectile added inactive ends its flight at the next step,
    /// as expired.
    ///
    /// Returns the first problem found instead, adding nothing, if @p config or @p state is
    /// invalid (see validate()).
    [[nodiscard]] PROJECTILE_API Result<ProjectileId> add_projectile(
        const ProjectileState& state, const ProjectileConfig& config
    );

    /// Removes the projectile @p id, without reporting an event. Returns false if there is no
    /// such projectile.
    PROJECTILE_API bool remove_projectile(ProjectileId id) noexcept;

    /// Removes every projectile, without reporting events.
    PROJECTILE_API void clear() noexcept;

    /// The projectile @p id, or `nullptr` if there is no such projectile.
    [[nodiscard]] PROJECTILE_API const SimulatedProjectile* find(ProjectileId id) const noexcept;

    /// Whether the projectile @p id is in the simulation.
    [[nodiscard]] bool contains(ProjectileId id) const noexcept { return find(id) != nullptr; }

    /// Every projectile in the simulation.
    [[nodiscard]] std::span<const SimulatedProjectile> projectiles() const noexcept {
        return projectiles_;
    }

    /// Allocates memory for @p count projectiles, so that the simulation allocates nothing while
    /// it holds no more than that.
    PROJECTILE_API void reserve(std::size_t count);

    // --- Collision world and events -------------------------------------------------------------

    /// Sets the collision world projectiles fly through, or none with `nullptr` (the default).
    /// The simulation does not own the world, which must outlive it or be replaced first.
    void set_collision_world(const ICollisionWorld* world) noexcept { world_ = world; }

    /// The collision world projectiles fly through, or `nullptr`.
    [[nodiscard]] const ICollisionWorld* collision_world() const noexcept { return world_; }

    /// Sets the function that receives events, or none with an empty function (the default).
    /// When it is called from inside the callback, the new callback receives events from the
    /// next step on.
    PROJECTILE_API void set_event_callback(ProjectileEventCallback callback);

    // --- Time -----------------------------------------------------------------------------------

    /// Advances the simulation by a frame of @p frame_delta seconds: adds the time to the
    /// accumulator and runs the fixed steps now due, within the limits of SimulationConfig (see
    /// FixedTimestep). Returns the steps run and any time dropped.
    PROJECTILE_API FrameSteps update(double frame_delta);

    /// Runs exactly one fixed step, bypassing the accumulator. For games that drive the
    /// simulation from their own fixed-rate loop.
    PROJECTILE_API void step();

    /// The weight of the newest state for drawing between the last two (0 to 1): how far the
    /// time carried over by update() reaches into the next step. See interpolated_position().
    [[nodiscard]] double interpolation_alpha() const noexcept {
        return timestep_.interpolation_alpha();
    }

    /// Simulated time, in s: the number of steps run times the time step.
    [[nodiscard]] double time() const noexcept {
        return static_cast<double>(step_count_) * config_.time_step;
    }

    /// Number of steps run.
    [[nodiscard]] std::uint64_t step_count() const noexcept { return step_count_; }

    /// The simulation's settings.
    [[nodiscard]] const SimulationConfig& config() const noexcept { return config_; }

private:
    /// Where a projectile is stored. An id names a slot together with the slot's generation,
    /// which changes whenever the slot is reused, so that old ids stop matching.
    struct Slot {
        std::uint32_t generation = 0; ///< Generation of the current or the last projectile.
        std::uint32_t index = 0;      ///< In use: index in projectiles_. Free: the next free slot.
        bool in_use = false;
    };

    static constexpr std::uint32_t no_slot = ~std::uint32_t{0};

    explicit Simulation(const SimulationConfig& config);

    [[nodiscard]] const Slot* slot_of(ProjectileId id) const noexcept;
    void advance(SimulatedProjectile& simulated, double start_time);
    void report(
        ProjectileEventKind kind,
        const SimulatedProjectile& simulated,
        double time,
        const std::optional<HitResult>& hit
    );
    void deliver_events();
    void finish_delivery(ProjectileEventCallback& callback) noexcept;

    SimulationConfig config_;
    FixedTimestep timestep_;
    std::vector<SimulatedProjectile> projectiles_;
    std::vector<Slot> slots_;
    std::uint32_t free_slot_ = no_slot;   ///< First free slot, or no_slot.
    std::vector<ProjectileEvent> events_; ///< Events of the current step.
    ProjectileEventCallback callback_;
    const ICollisionWorld* world_ = nullptr; ///< Not owned.
    std::uint64_t step_count_ = 0;
    bool delivering_ = false;        ///< Whether the callback is running.
    bool callback_replaced_ = false; ///< Whether the callback set a new callback.
};

} // namespace projectile
