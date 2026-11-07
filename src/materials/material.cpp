#include "projectile/materials/material.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace projectile {

// The materials are kept sorted by id, for binary search.

std::optional<Error> MaterialTable::add(const Material& material) {
    if (const std::optional<Error> error = validate(material)) {
        return error;
    }
    const auto place = std::ranges::lower_bound(materials_, material.id, {}, &Material::id);
    if (place != materials_.end() && place->id == material.id) {
        *place = material;
    } else {
        materials_.insert(place, material);
    }
    return std::nullopt;
}

bool MaterialTable::remove(std::uint32_t id) noexcept {
    const auto place = std::ranges::lower_bound(materials_, id, {}, &Material::id);
    if (place == materials_.end() || place->id != id) {
        return false;
    }
    materials_.erase(place);
    return true;
}

const Material* MaterialTable::find(std::uint32_t id) const noexcept {
    const auto place = std::ranges::lower_bound(materials_, id, {}, &Material::id);
    return place != materials_.end() && place->id == id ? &*place : nullptr;
}

} // namespace projectile
