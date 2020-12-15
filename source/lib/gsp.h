#pragma once

#include "defines.h"

Result GSPGPU_WriteHWRegsWithMask(Handle handle, u32 regAddr, const u32* data, u8 datasize, const u32* maskdata, u8 masksize);
Result GSPGPU_WriteHWRegs(Handle handle, u32 regAddr, const u32* data, u8 size);
Result GSPGPU_FlushDataCache(Handle handle, const void* adr, u32 size);

Result gspWriteHwReg32(Handle handle, u32 regPa, u32 value);
void gspSetTextureCopyPhys(Handle handle, u32 outPa, u32 inPa, u32 size, u32 inDim, u32 outDim, u32 flags);

static inline void gspSetLcdFill(Handle handle, bool top, u8 r, u8 g, u8 b)
{
    u32 val = BIT(24) | ((u32)b << 16) | ((u32)g << 8) | r;
    gspWriteHwReg32(handle, top ? 0x10202204 : 0x10202A04, val);
}

static inline void gspwn(Handle handle, u32 outPa, u32 inPa, u32 size)
{
    gspSetTextureCopyPhys(handle, outPa, inPa, size, 0, 0, 8);
}

static inline void gspDoFullCleanInvCacheTrick(Handle handle)
{
    // Ignore results, this shall always succeed, unless "handle" is invalid

    // Trigger full DCache + L2C clean&inv using this cute little trick (just need to pass a size value higher than the cache size)
    // (but not too high; dcache+l2c size on n3ds is 0x700000; and any non-null userland addr gsp accepts)
    GSPGPU_FlushDataCache(handle, (const void *)0x14000000, 0x700000);
}
