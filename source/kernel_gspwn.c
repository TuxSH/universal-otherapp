#include "kernel_gspwn.h"
#include "lib/gsp.h"

void mapL2TableViaGpuDma(const BlobLayout *layout, void *workBuffer, Handle gspHandle)
{
    static const u32 s_l1tables[] = { 0x1FFF8000, 0x1FFFC000, 0x1F3F8000, 0x1F3FC000 };
    u32 numCores = IS_N3DS ? 4 : 2;

    // Minimum size of GPU DMA is 16, so we need to pad a bit...
    u32 l1EntryData[4] = { convertLinearMemToPhys(layout->l2table) | 1 };
    memcpy(workBuffer, l1EntryData, 16);
    __dsb();

    // Ignore result
    GSPGPU_FlushDataCache(gspHandle, workBuffer, 16);

    u32 l1EntryPa = convertLinearMemToPhys(workBuffer);

    for (u32 i = 0; i < numCores; i++) {
        u32 dstPa = s_l1tables[i] + (KHC3DS_MAP_ADDR >> 20) * 4;
        gspwn(gspHandle, dstPa, l1EntryPa, 16);
    }

    // No need to clean&invalidate here:
    // https://developer.arm.com/docs/ddi0360/e/memory-management-unit/hardware-page-table-translation
    // "MPCore hardware page table walks do not cause a read from the level one Unified/Data Cache"

    __dsb();
    __flush_prefetch_buffer();
}
