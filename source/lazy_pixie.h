#pragma once

#include "lib/defines.h"
#include "../kernelhaxcode_3ds/takeover.h"

u32 lazyPixiePrepareStaticBufferDescriptors(u32 *sbufs, u32 baseId);
Result lazyPixieTriggerArbwrite(BlobLayout *layout, Handle handle, u32 baseSbufId);
