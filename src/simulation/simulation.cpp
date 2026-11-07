#include "projectile/simulation/simulation.hpp"

#include "projectile/ballistics/forces.hpp"
#include "projectile/ballistics/integrator.hpp"
#include "projectile/collision/raycast.hpp"
#include "projectile/collision/sweep.hpp"
#include "projectile/math/ray.hpp"

#include "ballistics/step_contact.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace projectile {

namespace {

/// Element @p index of @p items. The slot table guarantees that the indices it holds are in
/// range; standard-library debug modes, such as _GLIBCXX_ASSERTIONS, check them.
template <typename Items>
[[nodiscard]] decltype(auto) element(Items& items, std::size_t index) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return items[index];
}

// An id holds the generation of its slot in the upper 32 bits and the slot's index in the lower
// 32. Generations start at 1, so no projectile has the id 0.

[[nodiscard]] constexpr ProjectileId make_id(
    std::uint32_t slot, std::uint32_t generation
) noexcept {
    return ProjectileId{.value = (std::uint64_t{generation} << 32U) | slot};
}

[[nodiscard]] constexpr std::uint32_t slot_index(ProjectileId id) noexcept {
    return static_cast<std::uint32_t>(id.value & 0xFFFF'FFFFU);
}

[[nodiscard]] constexpr std::uint32_t generation_of(ProjectileId id) noexcept {
    return static_cast<std::uint32_t>(id.value >> 32U);
}

/// The generation after @p generation, wrapping around from the largest to 1.
[[nodiscard]] constexpr std::uint32_t next_generation(std::uint32_t generation) noexcept {
    return generation == ~std::uint32_t{0} ? 1U : generation + 1U;
}

// --- Impacts --------------------------------------------------------------------------------

/// How far a projectile that survives an impact is moved clear of the surfaces it touches, in m,
/// so that the next sweep does not find the same contact again.
constexpr double contact_skin = 1e-6;

/// The material a projectile struck when surfaces have no material table: one that stops it.
constexpr Material default_material{};

/// Whether events of @p kind end a projectile's flight.
[[nodiscard]] constexpr bool ends_flight(ProjectileEventKind kind) noexcept {
    return kind == ProjectileEventKind::hit || kind == ProjectileEventKind::expired;
}

/// How far a ray in the unit @p direction from the contact @p hit runs through the object struck:
/// up to where it leaves the object, a back face of the same material. Zero if the ray does not
/// leave it there, as for a plane or a single triangle, which have no thickness.
[[nodiscard]] double thickness_at(
    const ICollisionWorld& world, const HitResult& hit, const Vec3& direction
) {
    const Ray ray{.origin = hit.point + direction * contact_skin, .direction = direction};
    const std::optional<HitResult> exit =
        raycast(world, ray, std::numeric_limits<double>::infinity());
    if (exit && !exit->front_face && exit->material_id == hit.material_id) {
        return exit->distance + contact_skin;
    }
    return 0.0;
}

/// Where the center of a projectile of @p radius, which was at @p center when it struck @p hit,
/// flies on from after ricocheting off the surface or penetrating it in the unit @p direction,
/// through an object of @p thickness.
[[nodiscard]] Vec3 continuation_point(
    const ICollisionWorld& world,
    const Vec3& center,
    const HitResult& hit,
    InteractionOutcome outcome,
    const Vec3& direction,
    double radius,
    double thickness
) {
    if (outcome == InteractionOutcome::ricocheted) {
        // The normal faces the projectile; a step along it clears the surface.
        return center + hit.normal * contact_skin;
    }
    // Penetrated: move on through the object until the projectile no longer overlaps it, which
    // a sweep reports as a back-face contact. The reach covers the thickness and the projectile's
    // own size, which an oblique path crosses at a slant.
    const Vec3 start = center + direction * contact_skin;
    const double slant = (std::max)(-dot(direction, hit.normal), 0.05);
    const double reach =
        thickness + (2.0 * radius + 2.0 * contact_skin) / slant + 2.0 * radius + contact_skin;
    const Vec3 end = start + direction * reach;
    const std::optional<HitResult> probe = sweep_sphere(world, start, end, radius);
    if (!probe) {
        return end; // clear of everything along the way
    }
    if (!probe->front_face) {
        return start + direction * (probe->distance + contact_skin); // where it leaves the object
    }
    // It reaches another object first. Stop just short of it: a projectile left exactly touching
    // a surface could, by rounding, overlap it, and a sweep from inside a surface does not stop
    // there. The next sweep finds the object at once.
    return start + direction * (probe->distance - contact_skin);
}

} // namespace

Simulation::Simulation(const SimulationConfig& config)
    : config_(config), timestep_(FixedTimestep::create(config).value()) {}

Result<Simulation> Simulation::create(const SimulationConfig& config) {
    if (const std::optional<Error> error = validate(config)) {
        return *error;
    }
    return Simulation(config);
}

// --- Projectiles --------------------------------------------------------------------------------

Result<ProjectileId> Simulation::add_projectile(
    const ProjectileState& state, const ProjectileConfig& config
) {
    if (const std::optional<Error> error = validate(config)) {
        return *error;
    }
    if (const std::optional<Error> error = validate(state)) {
        return *error;
    }
    if (free_slot_ == no_slot) {
        if (slots_.size() >= no_slot) {
            throw std::length_error("projectile::Simulation: too many projectiles");
        }
        // The new slot joins the free list at once, so that the simulation stays consistent
        // if the allocation below fails.
        slots_.push_back(Slot{.generation = 0, .index = no_slot, .in_use = false});
        free_slot_ = static_cast<std::uint32_t>(slots_.size() - 1);
    }
    const std::uint32_t slot_number = free_slot_;
    Slot& slot = element(slots_, slot_number);
    const std::uint32_t generation = next_generation(slot.generation);
    const ProjectileId id = make_id(slot_number, generation);
    projectiles_.push_back(
        SimulatedProjectile{
            .id = id,
            .config = config,
            .state = state,
            .previous_position = state.position,
        }
    );
    // Nothing below can fail.
    free_slot_ = slot.index;
    slot = Slot{
        .generation = generation,
        .index = static_cast<std::uint32_t>(projectiles_.size() - 1),
        .in_use = true,
    };
    return id;
}

bool Simulation::remove_projectile(ProjectileId id) noexcept {
    const Slot* const found = slot_of(id);
    if (found == nullptr) {
        return false;
    }
    const std::uint32_t index = found->index;
    // Move the last projectile into the gap. When the projectile removed is the last one, this
    // changes nothing.
    const SimulatedProjectile& last = projectiles_.back();
    element(slots_, slot_index(last.id)).index = index;
    element(projectiles_, index) = last;
    projectiles_.pop_back();

    Slot& freed = element(slots_, slot_index(id));
    freed.in_use = false;
    freed.index = free_slot_;
    free_slot_ = slot_index(id);
    return true;
}

void Simulation::clear() noexcept {
    // Free the slots but keep their generations, so that the old ids stay invalid.
    for (const SimulatedProjectile& projectile : projectiles_) {
        Slot& slot = element(slots_, slot_index(projectile.id));
        slot.in_use = false;
        slot.index = free_slot_;
        free_slot_ = slot_index(projectile.id);
    }
    projectiles_.clear();
}

const SimulatedProjectile* Simulation::find(ProjectileId id) const noexcept {
    const Slot* const slot = slot_of(id);
    return slot != nullptr ? &element(projectiles_, slot->index) : nullptr;
}

void Simulation::reserve(std::size_t count) {
    projectiles_.reserve(count);
    slots_.reserve(count);
    events_.reserve(count); // a step reports at most one event per projectile
}

const Simulation::Slot* Simulation::slot_of(ProjectileId id) const noexcept {
    const std::uint32_t index = slot_index(id);
    if (index >= slots_.size()) {
        return nullptr;
    }
    const Slot& slot = element(slots_, index);
    return slot.in_use && slot.generation == generation_of(id) ? &slot : nullptr;
}

// --- Events -------------------------------------------------------------------------------------

void Simulation::set_event_callback(ProjectileEventCallback callback) {
    callback_ = std::move(callback);
    callback_replaced_ = delivering_;
}

void Simulation::report(
    ProjectileEventKind kind,
    const SimulatedProjectile& simulated,
    const ProjectileState& state,
    double time,
    const std::optional<HitResult>& hit,
    const std::optional<InteractionResult>& interaction
) {
    events_.push_back(
        ProjectileEvent{
            .kind = kind,
            .id = simulated.id,
            .time = time,
            .state = state,
            .config = simulated.config,
            .hit = hit,
            .interaction = interaction,
        }
    );
}

void Simulation::deliver_events() {
    if (events_.empty() || !callback_) {
        return;
    }
    // Call a callback moved out of the simulation, so that it may replace itself safely.
    ProjectileEventCallback callback = std::move(callback_);
    callback_ = nullptr;
    callback_replaced_ = false;
    delivering_ = true;
    try {
        // Pass copies: the callback may call reserve(), which can move the events.
        for (std::size_t i = 0; i < events_.size(); ++i) {
            const ProjectileEvent event = element(events_, i);
            callback(event);
        }
    } catch (...) {
        finish_delivery(callback);
        throw;
    }
    finish_delivery(callback);
}

void Simulation::finish_delivery(ProjectileEventCallback& callback) noexcept {
    delivering_ = false;
    if (!callback_replaced_) {
        callback_ = std::move(callback);
    }
}

// --- Time ---------------------------------------------------------------------------------------

FrameSteps Simulation::update(double frame_delta) {
    if (delivering_) {
        return {};
    }
    const FrameSteps frame = timestep_.advance(frame_delta);
    for (std::uint32_t i = 0; i < frame.steps; ++i) {
        step();
    }
    return frame;
}

void Simulation::step() {
    if (delivering_) {
        return;
    }
    const double start_time = time();
    events_.clear();
    for (SimulatedProjectile& simulated : projectiles_) {
        advance(simulated, start_time);
    }
    ++step_count_;
    // Projectiles whose flight ended leave before their events are reported.
    for (const ProjectileEvent& event : events_) {
        if (ends_flight(event.kind)) {
            remove_projectile(event.id);
        }
    }
    deliver_events();
}

void Simulation::advance(SimulatedProjectile& simulated, double start_time) {
    ProjectileState& state = simulated.state;
    const ProjectileConfig& config = simulated.config;
    simulated.previous_position = state.position;
    if (!state.active) {
        report(
            ProjectileEventKind::expired, simulated, state, start_time, std::nullopt, std::nullopt
        );
        return;
    }
    // The projectile moves for the whole step, unless impacts it survives split the motion into
    // parts; each part starts at `time` and lasts `remaining`.
    double time = start_time;
    double remaining = config_.time_step;
    for (std::uint32_t impacts = 0;;) {
        const ProjectileState from = state;
        // How long it moves: the integrators stop at the end of its life.
        const double duration = (std::min)(remaining, from.remaining_lifetime);
        // Qualified: inside the class, the name would find the member step().
        projectile::step(config_.integrator, state, config, remaining);

        std::optional<detail::StepContact> contact;
        if (world_ != nullptr && duration > 0.0) {
            contact = detail::sweep_step(*world_, from, state, config.radius);
        }
        if (!contact) {
            if (!state.active) {
                report(
                    ProjectileEventKind::expired,
                    simulated,
                    state,
                    time + duration,
                    std::nullopt,
                    std::nullopt
                );
            }
            return;
        }

        const ProjectileState at_contact =
            detail::contact_state(from, state, duration, contact->fraction, config);
        const double contact_time = time + contact->fraction * duration;
        if (interaction_ == nullptr) {
            state = at_contact;
            report(
                ProjectileEventKind::hit, simulated, state, contact_time, contact->hit, std::nullopt
            );
            return;
        }

        // The thickness is measured along the path the projectile struck on.
        const std::optional<Vec3> incoming = try_normalize(at_contact.velocity);
        const double thickness = incoming ? thickness_at(*world_, contact->hit, *incoming) : 0.0;
        InteractionResult result = resolve(at_contact, config, contact->hit, thickness);
        const std::optional<Vec3> direction = try_normalize(result.outgoing_velocity);
        if (result.outcome == InteractionOutcome::stopped || !direction) {
            if (result.outcome != InteractionOutcome::stopped) {
                result = InteractionResult{}; // a velocity of zero, or not finite, stops it
            }
            state = at_contact;
            report(ProjectileEventKind::hit, simulated, state, contact_time, contact->hit, result);
            return;
        }

        ProjectileState struck = at_contact;
        struck.active = true;
        report(
            result.outcome == InteractionOutcome::ricocheted ? ProjectileEventKind::ricochet
                                                             : ProjectileEventKind::penetration,
            simulated,
            struck,
            contact_time,
            contact->hit,
            result
        );
        const Vec3 position = continuation_point(
            *world_,
            at_contact.position,
            contact->hit,
            result.outcome,
            *direction,
            config.radius,
            thickness
        );
        state = ProjectileState{
            .position = position,
            .velocity = result.outgoing_velocity,
            .acceleration = forces_of(config)(position, result.outgoing_velocity),
            .remaining_lifetime = at_contact.remaining_lifetime,
            .active = true,
        };
        remaining -= contact->fraction * duration;
        time = contact_time;
        if (++impacts >= config_.max_impacts_per_step || !(remaining > 0.0)) {
            return; // it waits here until the next step
        }
    }
}

InteractionResult Simulation::resolve(
    const ProjectileState& at_contact,
    const ProjectileConfig& config,
    const HitResult& hit,
    double thickness
) const {
    ProjectileState struck = at_contact;
    struck.active = true;
    return interaction_->resolve(
        Impact{
            .state = struck,
            .config = config,
            .hit = hit,
            .material = materials_ != nullptr ? materials_->material_for(hit.material_id)
                                              : default_material,
            .thickness = thickness,
        }
    );
}

} // namespace projectile
