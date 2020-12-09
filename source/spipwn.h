#pragma once

#include "lib/defines.h"
#include "../kernelhaxcode_3ds/takeover.h"

/**
 * @brief Exploits the spi sysmodule to write to GPUPROT.
 * @pre Needs access to all services, including srv:pm.
 * @pre Needs to be on system version 8.0 or above.
 * @param srvHandle SRV handle.
 * @post GPU DMA can now write to kernel memory in AXIWRAM.
 */
Result spipwn(Handle srvHandle);
