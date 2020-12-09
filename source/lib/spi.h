#pragma once

#include "defines.h"

Result spiDefInit(Handle *outSpiDefHandle, Handle srvHandle);
Result SPI_SetDeviceState(Handle spiHandle, u8 deviceId, u8 state);
