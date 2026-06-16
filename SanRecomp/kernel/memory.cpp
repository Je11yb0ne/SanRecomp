#include <stdafx.h>
#include "memory.h"

static constexpr size_t AlignDown(size_t value, size_t alignment) noexcept
{
    return value & ~(alignment - 1);
}

static constexpr size_t AlignUp(size_t value, size_t alignment) noexcept
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

Memory::Memory()
{
#ifdef _WIN32
    base = (uint8_t*)VirtualAlloc((void*)0x100000000ull, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        base = (uint8_t*)VirtualAlloc(nullptr, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        return;

    // Some titles (e.g., GTA V) legitimately touch low memory (including address 0).
    // Do not install a null-page guard in that case.
#else
    base = (uint8_t*)mmap((void*)0x100000000ull, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
        base = (uint8_t*)mmap(NULL, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
    {
        base = nullptr;
        return;
    }

    // Some titles (e.g., GTA V) legitimately touch low memory (including address 0).
    // Do not install a null-page guard in that case.
#endif

    // Initialize null-page sentinel to prevent infinite loops in memory allocator
    // linked-list traversal when encountering uninitialized pools.
    {
        *(volatile uint32_t*)(base + 0x00) = __builtin_bswap32(0x10);
        *(volatile uint32_t*)(base + 0x10) = __builtin_bswap32(0x10);
        *(volatile uint32_t*)(base + 0x14) = __builtin_bswap32(0x10);
        *(volatile uint16_t*)(base + 0x08) = __builtin_bswap16(0xFFFF);
    }

    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        if (PPCFuncMappings[i].host != nullptr)
            InsertFunction(PPCFuncMappings[i].guest, PPCFuncMappings[i].host);
    }

    // NOTE: Function table protection DISABLED for GTA V.
    // The game writes to memory beyond the XEX image (0x83DC0000+) for
    // runtime data (heaps, thread stacks, etc.). Original LibertyRecomp
    // protected this region as read-only to guard against host pointer
    // corruption, but GTA V needs full read/write access.
}

void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}
