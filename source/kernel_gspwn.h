#pragma once

#include "lib/defines.h"
#include "../kernelhaxcode_3ds/takeover.h"

/**
 * @brief Maps KHC's L2 table via GPU DMA, assuming GPUPROT bit8 is clear.
 * @param layout KHC data (in LINEAR memory).
 * @param workBuffer Temporary storage in LINEAR memory for this function to use.
 * @param gspHandle gsp:Gpu handle with rights acquired.
 */
void mapL2TableViaGpuDma(const BlobLayout *layout, void *workBuffer, Handle gspHandle);
