//
//  malloc_shim.h
//  C allocation coverage
//
//  The global operator new replacement catches everything C++ allocates. It
//  does not catch malloc, and for a library that calls itself a C/C++ testing
//  platform that is a hole, not a detail: a C source file under test allocates
//  nothing the memory suite can see, and the report says "no leaks" about code
//  it never looked at.
//
//  This header closes the hole by redefining the C allocation functions in the
//  translation unit that includes it, so they route through the same allocator
//  the tracker installs.
//
//      #include <stdlib.h>            // system headers FIRST
//      #include <string.h>
//      #include <testrixa/malloc_shim.h>   // then this, LAST
//
//      char* p = (char*)malloc(32);   // now visible to the tracker
//      free(p);
//
//  Ordering matters and is not negotiable. These are macros; a system header
//  parsed after them would see `std::testrixa::memory::shim::allocate` where it
//  declared `std::malloc`, and fail. Include this last, always.
//
//  Scope, stated plainly: this covers the translation units that include it and
//  nothing else. Allocations inside a prebuilt .a or .so are still invisible.
//  Full coverage needs platform interposition (LD_PRELOAD, malloc zones), which
//  is a separate mechanism -- see the coverage line the memory report prints.
//

#ifndef TESTRIXA_MALLOC_SHIM_H
#define TESTRIXA_MALLOC_SHIM_H

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

#include <testrixa/detail/memory/tracker.hpp>

TRX_BEGIN_NAMESPACE

namespace memory {
namespace shim {

// Every entry point uses the default alignment: that is what malloc promises,
// and it keeps the header findable when realloc has to look one up.
inline void* allocate(std::size_t bytes) {
    if (bytes == 0) bytes = 1;          // malloc(0) may return a unique pointer
    return memory::allocate(bytes, platform::defaultAlignment());
}

inline void release(void* pointer) {
    memory::deallocate(pointer, 0, platform::defaultAlignment());
}

inline void* callocate(std::size_t count, std::size_t size) {
    // calloc must detect the multiplication overflowing, or it hands back a
    // buffer smaller than the caller believes it asked for.
    if (count != 0 && size > (std::size_t)-1 / count) return nullptr;

    const std::size_t bytes = count * size;
    void* result = allocate(bytes);
    if (result) std::memset(result, 0, bytes ? bytes : 1);
    return result;
}

inline void* reallocate(void* pointer, std::size_t bytes) {
    if (!pointer) return allocate(bytes);
    if (bytes == 0) { release(pointer); return nullptr; }

    // Only a block we allocated can be grown by hand; for anything else the
    // system allocator still owns it and knows its size.
    std::size_t previous = 0;
    if (!Tracker::instance().trackedSize(pointer, previous)) {
        return std::realloc(pointer, bytes);
    }

    void* result = allocate(bytes);
    if (!result) return nullptr;        // the original stays valid, as realloc requires

    std::memcpy(result, pointer, previous < bytes ? previous : bytes);
    release(pointer);
    return result;
}

inline char* duplicate(const char* text) {
    if (!text) return nullptr;
    const std::size_t length = std::strlen(text) + 1;
    char* result = (char*)allocate(length);
    if (result) std::memcpy(result, text, length);
    return result;
}

inline char* duplicateN(const char* text, std::size_t limit) {
    if (!text) return nullptr;
    std::size_t length = 0;
    while (length < limit && text[length]) ++length;
    char* result = (char*)allocate(length + 1);
    if (result) {
        std::memcpy(result, text, length);
        result[length] = '\0';
    }
    return result;
}

} // namespace shim
} // namespace memory

TRX_END_NAMESPACE

// ---------------------------------------------------------------------------
// The redefinitions. Everything above is an ordinary function; only this part
// is invasive, and only for the translation unit that includes this header.
// ---------------------------------------------------------------------------
#define TRX_MALLOC(bytes)          ::testrixa::memory::shim::allocate(bytes)
#define TRX_CALLOC(count, size)    ::testrixa::memory::shim::callocate(count, size)
#define TRX_REALLOC(ptr, bytes)    ::testrixa::memory::shim::reallocate(ptr, bytes)
#define TRX_FREE(ptr)              ::testrixa::memory::shim::release(ptr)
#define TRX_STRDUP(text)           ::testrixa::memory::shim::duplicate(text)
#define TRX_STRNDUP(text, limit)   ::testrixa::memory::shim::duplicateN(text, limit)

#ifndef TESTRIXA_NO_MALLOC_SHIM

#undef  malloc
#undef  calloc
#undef  realloc
#undef  free
#undef  strdup
#undef  strndup

#define malloc(bytes)          TRX_MALLOC(bytes)
#define calloc(count, size)    TRX_CALLOC(count, size)
#define realloc(ptr, bytes)    TRX_REALLOC(ptr, bytes)
#define free(ptr)              TRX_FREE(ptr)
#define strdup(text)           TRX_STRDUP(text)
#define strndup(text, limit)   TRX_STRNDUP(text, limit)

#endif // TESTRIXA_NO_MALLOC_SHIM

#endif // TESTRIXA_MALLOC_SHIM_H
