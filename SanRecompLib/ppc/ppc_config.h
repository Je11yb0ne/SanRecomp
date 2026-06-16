#pragma once
#ifndef PPC_CONFIG_H_INCLUDED
#define PPC_CONFIG_H_INCLUDED

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

#ifdef PPC_INCLUDE_DETAIL
#include "ppc_detail.h"
#endif

#endif
