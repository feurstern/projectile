#include "projectile/core/version.hpp"

#include <string_view>

namespace projectile {

// Both functions capture the macro values at the time the library is compiled, which is what
// lets programs detect a header/library mismatch at run time.

Version version() noexcept {
    return Version{
        .major = PROJECTILE_VERSION_MAJOR,
        .minor = PROJECTILE_VERSION_MINOR,
        .patch = PROJECTILE_VERSION_PATCH,
    };
}

std::string_view version_string() noexcept {
    return PROJECTILE_VERSION_STRING;
}

} // namespace projectile
