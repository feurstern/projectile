#include "projectile/simulation/simulation.hpp"

#include "projectile/ballistics/integrator.hpp"

#include "ballistics/step_contact.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
    double time,
    const std::optional<HitResult>& hit
) {
    events_.push_back(
        ProjectileEvent{
            .kind = kind,
            .id = simulated.id,
            .time = time,
            .state = simulated.state,
            .config = simulated.config,
            .hit = hit,
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
        remove_projectile(event.id);
    }
    deliver_events();
}

void Simulation::advance(SimulatedProjectile& simulated, double start_time) {
    ProjectileState& state = simulated.state;
    simulated.previous_position = state.position;
    if (!state.active) {
        report(ProjectileEventKind::expired, simulated, start_time, std::nullopt);
        return;
    }
    const ProjectileState from = state;
    // How long the projectile moves in this step: the integrators stop at the end of its life.
    const double duration = (std::min)(config_.time_step, from.remaining_lifetime);
    // Qualified: inside the class, the name would find the member step().
    projectile::step(config_.integrator, state, simulated.config, config_.time_step);

    if (world_ != nullptr && duration > 0.0) {
        if (const std::optional<detail::StepContact> contact =
                detail::sweep_step(*world_, from, state, simulated.config.radius)) {
            state =
                detail::contact_state(from, state, duration, contact->fraction, simulated.config);
            report(
                ProjectileEventKind::hit,
                simulated,
                start_time + contact->fraction * duration,
                contact->hit
            );
            return;
        }
    }
    if (!state.active) {
        report(ProjectileEventKind::expired, simulated, start_time + duration, std::nullopt);
    }
}

} // namespace projectile
