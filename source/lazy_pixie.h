#pragma once

#include "lib/defines.h"

#define MAP_ADDR        0x80000000

typedef struct BlobLayout {
    u8 padding0[0x1000]; // to account for firmlaunch params in case we're placed at FCRAM+0
    u8 code[0x20000];
    u32 l2table[0x100];
    u32 padding[0x400 - 0x100];
} BlobLayout;


Result lazyPixie(BlobLayout *layout, Handle handle, u32 selfSbufId, u32 alignedBuffer);
