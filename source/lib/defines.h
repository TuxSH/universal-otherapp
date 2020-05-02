#pragma once

#include <string.h>
#include "ipc.h"
#include "result.h"
#include "svc.h"

#define TRY(expr)               if(R_FAILED(res = (expr))) return res;
#define KERNEL_VERSION_MAJOR    (*(vu8 *)0x1FF80003)
#define KERNEL_VERSION_MINOR    (*(vu8 *)0x1FF80002)
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT

/// Packs a system version from its components.
#define SYSTEM_VERSION(major, minor, revision) \
    (((major)<<24)|((minor)<<16)|((revision)<<8))

static inline u32 convertLinearMemToPhys(const void *addr)
{
    u32 vaddr = (u32)addr;
    switch (vaddr) {
        case 0x14000000 ... 0x1C000000 - 1: return vaddr + 0x0C000000; // LINEAR heap
        case 0x30000000 ... 0x40000000 - 1: return vaddr - 0x10000000; // v8.x+ LINEAR heap
        default: return 0;
    }
}

static inline void __dsb(void)
{
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
}

static inline void __flush_prefetch_buffer(void)
{
    // Similar to isb in newer Arm architecture versions
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");
}
