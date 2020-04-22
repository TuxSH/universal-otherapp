#pragma once

#include "defines.h"

Result GSPGPU_ReadHWRegs(Handle handle, u32 regAddr, u32* data, u8 size);
Result GSPGPU_WriteHWRegs(Handle handle, u32 regAddr, u32* data, u8 size);
Result GSPGPU_FlushDataCache(Handle handle, const void* adr, u32 size);

Result gspSetTextureCopyPhys(Handle handle, u32 outPa, u32 inPa, u32 size, u32 inDim, u32 outDim, u32 flags);
static inline Result gspwn(Handle handle, u32 outPa, u32 inPa, u32 size)
{
    return gspSetTextureCopyPhys(handle, outPa, inPa, size, 0, 0, 8);
}

