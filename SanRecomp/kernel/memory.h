#pragma once

#ifndef _WIN32
#define MEM_COMMIT  0x00001000  
#define MEM_RESERVE 0x00002000  
#endif

struct Memory
{
    uint8_t* base{};

    Memory();

    bool IsInMemoryRange(const void* host) const noexcept
    {
        return host >= base && host < (base + PPC_MEMORY_SIZE);
    }

    void* Translate(size_t offset) const noexcept
    {
        if (offset)
            assert(offset < PPC_MEMORY_SIZE);

        return base + offset;
    }

    uint32_t MapVirtual(const void* host) const noexcept
    {
        if (host)
            assert(IsInMemoryRange(host));

        return static_cast<uint32_t>(static_cast<const uint8_t*>(host) - base);
    }

    PPCFunc* FindFunction(uint32_t guest) const noexcept
    {
        PPCFunc* fn = PPC_LOOKUP_FUNC(base, guest);
        // Guard against corrupted table entries from game writes to fn table region.
        // Auto-heal: re-lookup from PPCFuncMappings if entry looks corrupted.
        if ((uintptr_t)fn == 0 || (uintptr_t)fn == 0xFFFFFFFFFFFFFFFFull) {
            // Search PPCFuncMappings for the correct host function
            for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++) {
                if (PPCFuncMappings[i].guest == guest) {
                    fn = PPCFuncMappings[i].host;
                    PPC_LOOKUP_FUNC(base, guest) = fn;  // repair the slot
                    static int s_heal_count = 0;
                    if (++s_heal_count <= 10) {
                        printf("[MEMORY] Auto-healed fn table entry for PPC=0x%08X\n", guest);
                        fflush(stdout);
                    }
                    return fn;
                }
            }
            return nullptr;
        }
        return fn;
    }

    void InsertFunction(uint32_t guest, PPCFunc* host)
    {
        PPC_LOOKUP_FUNC(base, guest) = host;
    }
};

extern "C" void* MmGetHostAddress(uint32_t ptr);
extern Memory g_memory;
