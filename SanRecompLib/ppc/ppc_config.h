#pragma once
#ifndef PPC_CONFIG_H_INCLUDED
#define PPC_CONFIG_H_INCLUDED

#include <cstdint>
#include <cstdio>

// MMIO busy-wait detection — from rexglue analysis
#define PPC_MMIO_DEBUG
// Busy-wait break strategy: rotate values so each break writes something different.
// The game waits for change; if we always write the same value, it loops forever.
// By rotating, the game sees a change and may proceed to the next state.
#define PPC_BUSYWAIT_BREAK_VAL 0  // Base value — actual logic in ppc_context.h rotates

#define PPC_IMAGE_BASE 0x82000000ull
#define PPC_IMAGE_SIZE 0x1DC0000ull
#define PPC_CODE_BASE 0x82210000ull
#define PPC_CODE_SIZE 0x15584F4ull

// ============================================================================
// GPU MMIO Interception
// Xbox 360 GPU registers are at physical addresses 0x7FC80000-0x7FCFFFFF.
// GTA V writes GPU commands directly to these MMIO addresses instead of using
// the PM4 ring buffer. We intercept writes to capture GPU commands.
// ============================================================================
#define GPU_MMIO_BASE 0x7FC80000u
#define GPU_MMIO_END  0x7FD00000u

inline void _gpu_mmio_store32(uint8_t* base, uint32_t addr, uint32_t val) {
    if (addr >= GPU_MMIO_BASE && addr < GPU_MMIO_END) {
        uint32_t regIdx = (addr - GPU_MMIO_BASE) / 4;
        static uint32_t s_count = 0;
        if (++s_count <= 20) {
            printf("[GPU-MMIO] STORE reg=0x%04X (%d) val=0x%08X\n", regIdx, regIdx, val);
            fflush(stdout);
        }
        extern void CaptureGpuWrite(uint32_t r, uint32_t v);
        CaptureGpuWrite(regIdx, val);
    }
    *(volatile uint32_t*)(base + (addr)) = __builtin_bswap32(val);
}

inline uint32_t _gpu_mmio_load32(uint8_t* base, uint32_t addr) {
    uint32_t val = __builtin_bswap32(*(volatile uint32_t*)(base + addr));
    if (addr >= GPU_MMIO_BASE && addr < GPU_MMIO_END) {
        static uint32_t s_count = 0;
        if (++s_count <= 10) {
            uint32_t reg = (addr - GPU_MMIO_BASE) / 4;
            printf("[GPU-MMIO] LOAD  reg=0x%04X (%d) val=0x%08X\n", reg, reg, val);
            fflush(stdout);
        }
    }
    return val;
}

// Override MMIO load/store macros BEFORE ppc_context.h defines defaults
#define PPC_MM_STORE_U32(x, y)  _gpu_mmio_store32(base, (x), (y))
#define PPC_MM_LOAD_U32(x)      _gpu_mmio_load32(base, (x))

#ifdef PPC_INCLUDE_DETAIL
#include "ppc_detail.h"
#endif

#endif
