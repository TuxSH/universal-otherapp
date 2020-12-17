#pragma once

#include "lib/defines.h"
#include "../kernelhaxcode_3ds/takeover.h"

/**
 * @brief Run memchunkhaxv1, exploiting the kernel, and grant access to svc 0x7b.
 * @param workBuffer Temporary storage in LINEAR memory for this function to use, >= 64 KB.
 * @param gspHandle gsp:Gpu handle with rights acquired.
 */
Result memchunkhax(void *workBuffer, Handle gspHandle);

/**
 * @brief Maps KHC's L2 table via svc 0x7b.
 * @param layout KHC data (in LINEAR memory).
 */
void mapL2TableViaSvc0x7b(const BlobLayout *layout);
