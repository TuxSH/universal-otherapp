#pragma once

#include "lib/defines.h"

Result httpwn(Handle *outHandle, u32 baseSbufId, void *linearWorkbuf, void *hole, const u32 *sbufs, u32 numSbufs, Handle gspHandle);
