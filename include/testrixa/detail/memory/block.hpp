//
//  detail/memory/block.hpp
//  block layout
//
//  Each tracked allocation carries its own bookkeeping:
//
//      raw -> [ Header ][ front redzone ][ payload ][ rear redzone ]
//                                        ^ the pointer the caller gets
//
//  Putting the header in the block removes the side table a tracker would
//  otherwise need. That matters more than it sounds: a hash map of live
//  pointers allocates, and allocating inside the allocator is how a tracker
//  recurses into itself. Here a free is header = payload - offset, O(1), no
//  lookup and no allocation.
//
//  Finding the header needs `offset`, which depends on the alignment -- and
//  the alignment is available at deallocation too, because the language
//  guarantees an aligned new is matched by an aligned delete. So the offset is
//  recomputed rather than stored, and nothing has to be read from memory that
//  an overflow may already have damaged.
//
//  The front redzone sits between header and payload on purpose: it is what an
//  underflowing write hits before it reaches the header.
//

#ifndef TESTRIXA_DETAIL_MEMORY_BLOCK_HPP
#define TESTRIXA_DETAIL_MEMORY_BLOCK_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <testrixa/detail/memory/platform.hpp>

TRX_BEGIN_NAMESPACE

namespace memory {

// Distinct enough that a stray value is unlikely to look like one of ours.
constexpr std::uint64_t kLiveMagic  = 0x54525841'4C495645ull; // "TRXA" "LIVE"
constexpr std::uint64_t kFreedMagic = 0x54525841'46524545ull; // "TRXA" "FREE"

constexpr std::uint8_t  kRedzoneByte = 0xFD;

struct BlockHeader {
    std::uint64_t magic;
    std::size_t   size;        // bytes the caller asked for
    std::size_t   alignment;
    std::size_t   redzone;     // bytes on each side, frozen at allocation
    std::uint64_t sequence;    // allocation order, stable identifier
    std::uint64_t allocatedAt; // nanoseconds
    std::uint32_t threadId;
    std::uint32_t reserved;
    BlockHeader*  previous;
    BlockHeader*  next;

    // Where the allocation came from. Null unless CheckBacktrace is on, which
    // it never is by default -- capturing costs more than every other checker
    // together. The array is raw-allocated so recording a backtrace does not
    // travel back through the allocator being tracked.
    void**        backtrace;
    int           frames;
};

// Distance from the raw pointer to the payload. Chosen so the payload keeps the
// requested alignment: the raw block is allocated aligned, so an offset that is
// a multiple of the alignment preserves it.
inline std::size_t payloadOffset(std::size_t alignment, std::size_t redzone) {
    if (alignment < platform::defaultAlignment()) alignment = platform::defaultAlignment();
    return platform::alignUp(sizeof(BlockHeader) + redzone, alignment);
}

inline std::size_t totalBlockSize(std::size_t bytes, std::size_t alignment, std::size_t redzone) {
    return payloadOffset(alignment, redzone) + bytes + redzone;
}

inline void* payloadOf(void* raw, std::size_t alignment, std::size_t redzone) {
    return (void*)((std::uint8_t*)raw + payloadOffset(alignment, redzone));
}

inline BlockHeader* headerOf(void* payload, std::size_t alignment, std::size_t redzone) {
    return (BlockHeader*)((std::uint8_t*)payload - payloadOffset(alignment, redzone));
}

inline std::uint8_t* frontRedzoneOf(BlockHeader* header) {
    return (std::uint8_t*)header + sizeof(BlockHeader);
}

inline std::size_t frontRedzoneLength(const BlockHeader* header) {
    // Whatever padding alignment added is part of the front guard as well.
    return payloadOffset(header->alignment, header->redzone) - sizeof(BlockHeader);
}

inline std::uint8_t* rearRedzoneOf(BlockHeader* header) {
    return (std::uint8_t*)header
         + payloadOffset(header->alignment, header->redzone)
         + header->size;
}

inline void paintRedzones(BlockHeader* header) {
    if (header->redzone == 0) return;
    std::memset(frontRedzoneOf(header), kRedzoneByte, frontRedzoneLength(header));
    std::memset(rearRedzoneOf(header),  kRedzoneByte, header->redzone);
}

enum class Guard { Intact, FrontDamaged, RearDamaged };

inline Guard inspectRedzones(const BlockHeader* header) {
    if (header->redzone == 0) return Guard::Intact;

    const std::uint8_t* front = (const std::uint8_t*)header + sizeof(BlockHeader);
    const std::size_t frontLength = frontRedzoneLength(header);
    for (std::size_t i = 0; i < frontLength; ++i) {
        if (front[i] != kRedzoneByte) return Guard::FrontDamaged;
    }

    const std::uint8_t* rear = (const std::uint8_t*)header
                             + payloadOffset(header->alignment, header->redzone)
                             + header->size;
    for (std::size_t i = 0; i < header->redzone; ++i) {
        if (rear[i] != kRedzoneByte) return Guard::RearDamaged;
    }

    return Guard::Intact;
}

} // namespace memory

TRX_END_NAMESPACE

#endif // TESTRIXA_DETAIL_MEMORY_BLOCK_HPP
