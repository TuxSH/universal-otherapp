#pragma once

#include "lib/defines.h"

Result httpwn(Handle *outHandle, u32 *outSelfSbufId, u32 *outAlignedBuffer, void *linearWorkbuf, void *hole, Handle gspHandle);
