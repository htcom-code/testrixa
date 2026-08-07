//
//  testMemory.cpp
//  testrixa
//
//  Checks that the allocator replacement is actually wired to the global
//  operators -- that `new` and `delete` in ordinary code reach whatever
//  allocator is installed, including the over-aligned forms, and that Bypass
//  takes traffic off the hook.
//
//  A counting allocator is installed only for the few statements under test and
//  removed before any CHECK runs: the assertions themselves allocate (group
//  bookkeeping, string formatting) and would otherwise be counted.
//

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

#include <testrixa/testrixa.h>

namespace {

std::atomic<long> g_allocations{0};
std::atomic<long> g_frees{0};

void* countingAllocate(std::size_t bytes, std::size_t alignment) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return testrixa::memory::platform::rawAllocateAligned(bytes, alignment);
}

void countingDeallocate(void* pointer, std::size_t, std::size_t alignment) {
    g_frees.fetch_add(1, std::memory_order_relaxed);
    testrixa::memory::platform::rawFreeAligned(pointer, alignment);
}

// Keeps an allocation from being optimised away. [expr.new]/10 lets the
// compiler omit a new/delete pair whose lifetime is strictly nested, and at -O2
// clang does exactly that -- the counters would stay at zero and the test would
// be measuring nothing.
inline void escape(void* pointer) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(pointer) : "memory");
#else
    static volatile void* sink;
    sink = pointer;
#endif
}

// Forces a value to be materialised here, so the optimiser cannot sink the
// computation that produced it past a later deallocation.
inline void escapeValue(std::uintptr_t value) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "r"(value) : "memory");
#else
    static volatile std::uintptr_t sink;
    sink = value;
#endif
}

struct alignas(64) OverAligned {
    char payload[64];
};

} // namespace

TESTCASE_BEGIN(testMemory)

TESTCASE_BASIC(testMemory) {
    using namespace testrixa::memory;

    TESTCASE_GROUP_START(OPERATOR_NEW_IS_HOOKED) {
        g_allocations.store(0);
        g_frees.store(0);

        const Allocator previous = install(Allocator{ &countingAllocate, &countingDeallocate });
        int* value = new int(7);
        escape(value);
        const long afterNew = g_allocations.load();
        delete value;
        const long afterDelete = g_frees.load();
        install(previous);

        CHECK_SAME(afterNew, 1L);
        CHECK_SAME(afterDelete, 1L);
    } TESTCASE_GROUP_END(OPERATOR_NEW_IS_HOOKED)

    TESTCASE_GROUP_START(ARRAY_NEW_IS_HOOKED) {
        g_allocations.store(0);
        g_frees.store(0);

        const Allocator previous = install(Allocator{ &countingAllocate, &countingDeallocate });
        int* values = new int[16];
        escape(values);
        const long afterNew = g_allocations.load();
        delete[] values;
        const long afterDelete = g_frees.load();
        install(previous);

        CHECK_SAME(afterNew, 1L);
        CHECK_SAME(afterDelete, 1L);
    } TESTCASE_GROUP_END(ARRAY_NEW_IS_HOOKED)

    TESTCASE_GROUP_START(ALIGNED_NEW_IS_HOOKED) {
        g_allocations.store(0);
        g_frees.store(0);

        const Allocator previous = install(Allocator{ &countingAllocate, &countingDeallocate });
        OverAligned* object = new OverAligned();
        escape(object);
        const long afterNew        = g_allocations.load();
        // Integer, not pointer -- see testLeak.cpp for why.
        const std::uintptr_t address = (std::uintptr_t)object;
        escapeValue(address);
        delete object;
        const bool aligned         = (address % 64) == 0;
        const long afterDelete = g_frees.load();
        install(previous);

        CHECK_SAME(afterNew, 1L);
        CHECK_SAME(afterDelete, 1L);
        CHECK(aligned);
    } TESTCASE_GROUP_END(ALIGNED_NEW_IS_HOOKED)

    TESTCASE_GROUP_START(NOTHROW_NEW_IS_HOOKED) {
        g_allocations.store(0);
        g_frees.store(0);

        const Allocator previous = install(Allocator{ &countingAllocate, &countingDeallocate });
        int* value = new (std::nothrow) int(3);
        escape(value);
        const long afterNew = g_allocations.load();
        ::operator delete(value, std::nothrow);
        const long afterDelete = g_frees.load();
        install(previous);

        CHECK_SAME(afterNew, 1L);
        CHECK_SAME(afterDelete, 1L);
    } TESTCASE_GROUP_END(NOTHROW_NEW_IS_HOOKED)

    TESTCASE_GROUP_START(STL_CONTAINERS_ARE_HOOKED) {
        g_allocations.store(0);

        const Allocator previous = install(Allocator{ &countingAllocate, &countingDeallocate });
        {
            std::vector<int> numbers;
            numbers.reserve(128);
            escape(numbers.data());
        }
        const long seen = g_allocations.load();
        install(previous);

        // The default std::allocator goes through ::operator new, so a
        // container that reserves must show up on the hook.
        CHECK(seen >= 1);
    } TESTCASE_GROUP_END(STL_CONTAINERS_ARE_HOOKED)

    TESTCASE_GROUP_START(BYPASS_TAKES_TRAFFIC_OFF_THE_HOOK) {
        g_allocations.store(0);
        g_frees.store(0);

        const Allocator previous = install(Allocator{ &countingAllocate, &countingDeallocate });
        {
            Bypass guard;
            int* value = new int(11);
            escape(value);
            delete value;
        }
        const long allocationsSeen = g_allocations.load();
        const long freesSeen       = g_frees.load();
        install(previous);

        CHECK_SAME(allocationsSeen, 0L);
        CHECK_SAME(freesSeen, 0L);
    } TESTCASE_GROUP_END(BYPASS_TAKES_TRAFFIC_OFF_THE_HOOK)

    TESTCASE_GROUP_START(BYPASS_RESTORES_THE_PREVIOUS_STATE) {
        const bool before = Bypass::active();
        {
            Bypass outer;
            const bool inOuter = Bypass::active();
            {
                Bypass inner;
                CHECK(Bypass::active());
            }
            // nesting must not switch tracking back on early
            CHECK_SAME(Bypass::active(), inOuter);
        }
        CHECK_SAME(Bypass::active(), before);
    } TESTCASE_GROUP_END(BYPASS_RESTORES_THE_PREVIOUS_STATE)

    TESTCASE_GROUP_START(INSTALL_RETURNS_THE_PREVIOUS_ALLOCATOR) {
        const Allocator system    = current();
        const Allocator previous  = install(Allocator{ &countingAllocate, &countingDeallocate });
        const Allocator installed = current();
        const Allocator restored  = install(previous);

        CHECK(previous.allocate == system.allocate);
        CHECK(installed.allocate == &countingAllocate);
        CHECK(restored.allocate == &countingAllocate);
        CHECK(current().allocate == system.allocate);
    } TESTCASE_GROUP_END(INSTALL_RETURNS_THE_PREVIOUS_ALLOCATOR)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testMemory) {
    TESTCASE_RETURN
}

TESTCASE_END(testMemory)
