#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "lib/gsp.h"
#include "httpwn.h"
#include "lazy_pixie.h"

#include "kernelhaxcode_3ds_bin.h"

typedef struct ExploitChainLayout {
    u8 workBuf[0x1000];
    u8 hole[0x1000];
    BlobLayout blobLayout;
} ExploitChainLayout;


static Result doExploitChain(ExploitChainLayout *layout, Handle gspHandle)
{
    Handle httpcHandle;
    u32 selfSbufId, alignedHttpBuffer;
    Result res = 0;

    memset(layout, 0, sizeof(ExploitChainLayout));
    memcpy(layout->blobLayout.code, kernelhaxcode_3ds_bin, kernelhaxcode_3ds_bin_size);
    khc3dsPrepareL2Table(&layout->blobLayout);

    TRY(httpwn(&httpcHandle, &selfSbufId, &alignedHttpBuffer, layout->workBuf, layout->hole, gspHandle));
    TRY(lazyPixie(&layout->blobLayout, httpcHandle, selfSbufId, alignedHttpBuffer));

    return khc3dsRunExploitChain();
}

Result otherappMain(u32 paramBlk)
{
    Handle gspHandle = **(Handle **)(paramBlk + 0x58);
    ExploitChainLayout *layout = (ExploitChainLayout *)((paramBlk + 0x1000) & ~0xFFF);

    return doExploitChain(layout, gspHandle);
}
