#include <stdafx.h>
#include "memory.h"

// Forward declarations for function table patches
extern "C" void sub_823F4D38_patch(PPCContext&, uint8_t*);
extern "C" void sub_83625C50_patch(PPCContext&, uint8_t*);
extern "C" void sub_836408A0_patch(PPCContext&, uint8_t*);

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

    // Patch dispatch table for functions called via PPC_CALL_INDIRECT_FUNC.
    // Weak alias overrides in imports.cpp don't catch indirect calls.
    InsertFunction(0x823F4D38, (PPCFunc*)sub_823F4D38_patch);
    InsertFunction(0x83625C50, (PPCFunc*)sub_83625C50_patch);
    InsertFunction(0x836408A0, (PPCFunc*)sub_836408A0_patch);

    // Protect the function lookup table as read-only to prevent
    // game writes from corrupting host function pointers.
    // The VEH handler in crash_guard.h handles write faults by
    // temporarily unprotecting pages that the game legitimately needs.
    constexpr size_t kPageSize = 0x1000;
    constexpr size_t kFuncTableOffset = PPC_IMAGE_BASE + PPC_IMAGE_SIZE;
    constexpr size_t kFuncTableSize = (PPC_CODE_SIZE * 2) + sizeof(PPCFunc*);
    const size_t protectBegin = AlignDown(kFuncTableOffset, kPageSize);
    const size_t protectEnd = AlignUp(kFuncTableOffset + kFuncTableSize, kPageSize);
    if (protectEnd > protectBegin)
    {
        // Temporarily make writable to re-populate function table
#ifdef _WIN32
        DWORD oldProtect{};
        VirtualProtect(base + protectBegin, protectEnd - protectBegin, PAGE_READWRITE, &oldProtect);
#else
        mprotect(base + protectBegin, protectEnd - protectBegin, PROT_READ | PROT_WRITE);
#endif
        // Re-insert all function mappings (they may have been loaded before protection)
        for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
        {
            if (PPCFuncMappings[i].host != nullptr)
                InsertFunction(PPCFuncMappings[i].guest, PPCFuncMappings[i].host);
        }
        // Now make read-only
#ifdef _WIN32
        VirtualProtect(base + protectBegin, protectEnd - protectBegin, PAGE_READONLY, &oldProtect);
#else
        mprotect(base + protectBegin, protectEnd - protectBegin, PROT_READ);
#endif
    }
}

void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}
