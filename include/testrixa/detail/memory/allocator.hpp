//
//  detail/memory/allocator.hpp
//  the replacement point
//
//  "Replacing the allocator" comes down to two function pointers. The global
//  operator new/delete are redefined once per program and do nothing but call
//  through them, so tracking can be switched on and off at run time without
//  recompiling anything.
//
//  With tracking off the cost is one atomic load and an indirect call. Builds
//  that cannot pay even that define TESTRIXA_MEM_DISABLE, which removes the
//  operator replacement entirely.
//
//  ODR
//  ---
//  A replacement operator new must exist exactly once in a program, and being
//  a header-only library does not exempt us -- `inline` does not help here.
//  The definitions are therefore emitted only in the translation unit that
//  defines TEST_RUN_TERM (the one that already supplies main()), or in one the
//  user marks with TESTRIXA_MEMORY_IMPLEMENTATION.
//

#ifndef TESTRIXA_DETAIL_MEMORY_ALLOCATOR_HPP
#define TESTRIXA_DETAIL_MEMORY_ALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <new>

#include <testrixa/detail/memory/platform.hpp>

TRX_BEGIN_NAMESPACE

namespace memory {

using AllocateFn   = void* (*)(std::size_t bytes, std::size_t alignment);
using DeallocateFn = void  (*)(void* pointer, std::size_t bytes, std::size_t alignment);

struct Allocator {
    AllocateFn   allocate;
    DeallocateFn deallocate;
};

// ---------------------------------------------------------------------------
// Bypass -- the re-entrancy guard
//
// The tracker allocates while recording an allocation, and so does the test
// framework while printing a result. Both must not be seen by the tracker:
// the first would recurse, the second would report the framework's own memory
// as the code under test's leak.
//
//     { Bypass guard; ... allocations here go straight to the system ... }
// ---------------------------------------------------------------------------
struct Bypass {
    Bypass() : m_previous(active()) { active() = true; }
    ~Bypass() { active() = m_previous; }

    Bypass(const Bypass&) = delete;
    Bypass& operator=(const Bypass&) = delete;

    static bool& active() {
        // Function-local thread_local: no static init order concern, and each
        // thread carries its own flag.
        static thread_local bool flag = false;
        return flag;
    }

private:
    bool m_previous;
};

// ---------------------------------------------------------------------------
// The system allocator -- the default, and the floor everything falls back to
// ---------------------------------------------------------------------------
inline void* systemAllocate(std::size_t bytes, std::size_t alignment) {
    return platform::rawAllocateAligned(bytes, alignment);
}

inline void systemDeallocate(void* pointer, std::size_t, std::size_t alignment) {
    platform::rawFreeAligned(pointer, alignment);
}

inline Allocator systemAllocator() {
    return Allocator{ &systemAllocate, &systemDeallocate };
}

// ---------------------------------------------------------------------------
// The replacement point
// ---------------------------------------------------------------------------
namespace detail {

inline std::atomic<AllocateFn>& allocateSlot() {
    static std::atomic<AllocateFn> slot{ &systemAllocate };
    return slot;
}

inline std::atomic<DeallocateFn>& deallocateSlot() {
    static std::atomic<DeallocateFn> slot{ &systemDeallocate };
    return slot;
}

} // namespace detail

inline Allocator current() {
    return Allocator{
        detail::allocateSlot().load(std::memory_order_acquire),
        detail::deallocateSlot().load(std::memory_order_acquire)
    };
}

// Returns the allocator that was in place, so callers can restore it.
inline Allocator install(const Allocator& next) {
    const Allocator previous = current();
    detail::allocateSlot().store(next.allocate, std::memory_order_release);
    detail::deallocateSlot().store(next.deallocate, std::memory_order_release);
    return previous;
}

// Restores the system allocator.
inline Allocator uninstall() {
    return install(systemAllocator());
}

// The entry points the global operators call.
inline void* allocate(std::size_t bytes, std::size_t alignment) {
    if (Bypass::active()) {
        return platform::rawAllocateAligned(bytes, alignment);
    }
    return detail::allocateSlot().load(std::memory_order_acquire)(bytes, alignment);
}

inline void deallocate(void* pointer, std::size_t bytes, std::size_t alignment) {
    if (!pointer) return;
    if (Bypass::active()) {
        platform::rawFreeAligned(pointer, alignment);
        return;
    }
    detail::deallocateSlot().load(std::memory_order_acquire)(pointer, bytes, alignment);
}

} // namespace memory

TRX_END_NAMESPACE

// ---------------------------------------------------------------------------
// Global operator new / delete
//
// Every form has to be here. A missing one silently falls back to the default
// implementation, and then an allocation the tracker never saw arrives at a
// deallocation it does -- reported as an invalid free -- or the reverse, which
// shows up as a leak that does not exist.
// ---------------------------------------------------------------------------
#if !defined(TESTRIXA_MEM_DISABLE) && \
    (defined(TEST_RUN_TERM) || defined(TESTRIXA_MEMORY_IMPLEMENTATION))

namespace {

inline void* trxOperatorNew(std::size_t bytes, std::size_t alignment) {
    // [basic.stc.dynamic.allocation]: a zero-byte request still returns a
    // distinct non-null pointer.
    if (bytes == 0) bytes = 1;

    for (;;) {
        void* result = testrixa::memory::allocate(bytes, alignment);
        if (result) return result;

        // Honour the new_handler protocol before giving up.
        std::new_handler handler = std::get_new_handler();
        if (!handler) return nullptr;
        handler();
    }
}

} // namespace

void* operator new(std::size_t bytes) {
    void* result = trxOperatorNew(bytes, testrixa::memory::platform::defaultAlignment());
    if (!result) throw std::bad_alloc();
    return result;
}

void* operator new[](std::size_t bytes) {
    void* result = trxOperatorNew(bytes, testrixa::memory::platform::defaultAlignment());
    if (!result) throw std::bad_alloc();
    return result;
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    return trxOperatorNew(bytes, testrixa::memory::platform::defaultAlignment());
}

void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
    return trxOperatorNew(bytes, testrixa::memory::platform::defaultAlignment());
}

void operator delete(void* pointer) noexcept {
    testrixa::memory::deallocate(pointer, 0, testrixa::memory::platform::defaultAlignment());
}

void operator delete[](void* pointer) noexcept {
    testrixa::memory::deallocate(pointer, 0, testrixa::memory::platform::defaultAlignment());
}

void operator delete(void* pointer, std::size_t bytes) noexcept {
    testrixa::memory::deallocate(pointer, bytes, testrixa::memory::platform::defaultAlignment());
}

void operator delete[](void* pointer, std::size_t bytes) noexcept {
    testrixa::memory::deallocate(pointer, bytes, testrixa::memory::platform::defaultAlignment());
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    testrixa::memory::deallocate(pointer, 0, testrixa::memory::platform::defaultAlignment());
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    testrixa::memory::deallocate(pointer, 0, testrixa::memory::platform::defaultAlignment());
}

#if defined(__cpp_aligned_new)

void* operator new(std::size_t bytes, std::align_val_t alignment) {
    void* result = trxOperatorNew(bytes, (std::size_t)alignment);
    if (!result) throw std::bad_alloc();
    return result;
}

void* operator new[](std::size_t bytes, std::align_val_t alignment) {
    void* result = trxOperatorNew(bytes, (std::size_t)alignment);
    if (!result) throw std::bad_alloc();
    return result;
}

void* operator new(std::size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return trxOperatorNew(bytes, (std::size_t)alignment);
}

void* operator new[](std::size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return trxOperatorNew(bytes, (std::size_t)alignment);
}

void operator delete(void* pointer, std::align_val_t alignment) noexcept {
    testrixa::memory::deallocate(pointer, 0, (std::size_t)alignment);
}

void operator delete[](void* pointer, std::align_val_t alignment) noexcept {
    testrixa::memory::deallocate(pointer, 0, (std::size_t)alignment);
}

void operator delete(void* pointer, std::size_t bytes, std::align_val_t alignment) noexcept {
    testrixa::memory::deallocate(pointer, bytes, (std::size_t)alignment);
}

void operator delete[](void* pointer, std::size_t bytes, std::align_val_t alignment) noexcept {
    testrixa::memory::deallocate(pointer, bytes, (std::size_t)alignment);
}

void operator delete(void* pointer, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    testrixa::memory::deallocate(pointer, 0, (std::size_t)alignment);
}

void operator delete[](void* pointer, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    testrixa::memory::deallocate(pointer, 0, (std::size_t)alignment);
}

#endif // __cpp_aligned_new

#endif // replacement enabled

#endif // TESTRIXA_DETAIL_MEMORY_ALLOCATOR_HPP
