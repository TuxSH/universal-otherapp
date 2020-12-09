#include "spi.h"
#include "srv.h"

Result spiDefInit(Handle *outSpiDefHandle, Handle srvHandle)
{
    return srvGetServiceHandle(srvHandle, outSpiDefHandle, "SPI::DEF");
}

Result SPI_SetDeviceState(Handle spiHandle, u8 deviceId, u8 state)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(1,2,0);
    cmdbuf[1] = deviceId;
    cmdbuf[2] = state;

    if(R_FAILED(ret = svcSendSyncRequest(spiHandle))) return ret;
    return (Result)cmdbuf[1];
}
