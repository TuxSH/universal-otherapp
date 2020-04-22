#pragma once

#include <string.h>
#include "ipc.h"
#include "result.h"
#include "svc.h"

#define TRY(expr)               if(R_FAILED(res = (expr))) panic(res);
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT

/// Packs a system version from its components.
#define SYSTEM_VERSION(major, minor, revision) \
    (((major)<<24)|((minor)<<16)|((revision)<<8))

static inline void lcdFill(u32 r, u32 g, u32 b)
{
    // TODO change it
    *(vu32 *)0x90202204 = BIT(24) | r << 16 | g << 8 | b;
}

static __attribute__((noinline, used)) void panic(Result res)
{
    (void)res;
    lcdFill(0xFF,0,0);
    for (;;);
}

static inline u32 convertLinearMemToPhys(const void *addr)
{
    u32 vaddr = (u32)addr;
    switch (vaddr) {
        case 0x14000000 ... 0x1C000000 - 1: return vaddr + 0x0C000000; // LINEAR heap
        case 0x30000000 ... 0x40000000 - 1: return vaddr + 0x10000000; // v8.x+ LINEAR heap
        default: return 0;
    }
}
