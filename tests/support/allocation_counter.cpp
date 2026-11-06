// Replaces the global allocation functions with versions that count calls; see
// allocation_counter.hpp.
//
// Every form except the over-aligned ones is replaced, so that memory is always released by the
// counterpart of the function that allocated it. (Sanitizers report a mismatch otherwise, for
// example between their own nothrow operator new and a replaced operator delete.) Over-aligned
// allocations, which Projectile does not make, keep their default functions as a matching set.

#include "support/allocation_counter.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {

// Global, because operator new is.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<std::size_t> allocations{0};

// An allocator replacement manages raw memory by definition.
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

/// Counts an allocation and returns @p size bytes of memory, or nullptr.
void* allocate(std::size_t size) noexcept {
    allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}

/// Allocates @p size bytes, throwing std::bad_alloc on failure.
void* allocate_or_throw(std::size_t size) {
    if (void* memory = allocate(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void release(void* memory) noexcept {
    std::free(memory);
}

// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

} // namespace

std::size_t projectile::test::allocation_count() noexcept {
    return allocations.load(std::memory_order_relaxed);
}

void* operator new(std::size_t size) {
    return allocate_or_throw(size);
}

void* operator new[](std::size_t size) {
    return allocate_or_throw(size);
}

void* operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
    return allocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
    return allocate(size);
}

void operator delete(void* memory) noexcept {
    release(memory);
}

void operator delete[](void* memory) noexcept {
    release(memory);
}

void operator delete(void* memory, std::size_t /*size*/) noexcept {
    release(memory);
}

void operator delete[](void* memory, std::size_t /*size*/) noexcept {
    release(memory);
}

void operator delete(void* memory, const std::nothrow_t& /*tag*/) noexcept {
    release(memory);
}

void operator delete[](void* memory, const std::nothrow_t& /*tag*/) noexcept {
    release(memory);
}
