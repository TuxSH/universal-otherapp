#include "srv_pm.h"
#include "srv.h"

Result srvPmInit(Handle *outSrvPmHandle, Handle srvHandle)
{
    return srvGetServiceHandle(srvHandle, outSrvPmHandle, "srv:pm");
}

static Result srvPmSendCommand(Handle handle, u32* cmdbuf)
{
    Result rc = 0;
    if (KERNEL_VERSION_MINOR < 39) cmdbuf[0] |= 0x04000000;
    rc = svcSendSyncRequest(handle);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];

    return rc;
}

Result SRVPM_PublishToAll(Handle handle, u32 notificationId)
{
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x2,1,0); // 0x20040
    cmdbuf[1] = notificationId;

    return srvPmSendCommand(handle, cmdbuf);
}
