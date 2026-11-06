#pragma once

// Counts heap allocations, to prove that code allocates nothing.
//
// allocation_counter.cpp replaces the global operator new, so this works only in a test program
// that links that file: the integration tests do, the unit tests do not.
//
// Limitation: on Windows, a shared Projectile library (a DLL) has its own operator new, which
// this replacement does not reach. There, allocations inside the library go uncounted.

#include <cstddef>

namespace projectile::test {

/// Number of calls to the global operator new (plain, array or nothrow) so far in this program.
[[nodiscard]] std::size_t allocation_count() noexcept;

} // namespace projectile::test
