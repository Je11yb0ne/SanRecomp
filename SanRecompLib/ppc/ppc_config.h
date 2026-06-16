#pragma once
#ifndef PPC_CONFIG_H_INCLUDED
#define PPC_CONFIG_H_INCLUDED

// MMIO busy-wait detection — from rexglue analysis
#define PPC_MMIO_DEBUG

#define PPC_IMAGE_BASE 0x82000000ull
#define PPC_IMAGE_SIZE 0x1DC0000ull
#define PPC_CODE_BASE 0x82210000ull
#define PPC_CODE_SIZE 0x15584F4ull

#ifdef PPC_INCLUDE_DETAIL
#include "ppc_detail.h"
#endif

#endif
