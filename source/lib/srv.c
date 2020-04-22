#include "srv.h"

// Code from libctru

Result srvInit(Handle *outHandle)
{
	Result rc = svcConnectToPort(outHandle, "srv:");
	if (R_SUCCEEDED(rc)) {
		rc = srvRegisterClient(*outHandle);
	}

	return rc;
}

Result srvGetServiceHandle(Handle handle, Handle* out, const char* name)
{

	Result rc = 0;
	u32* cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x5,4,0); // 0x50100
	strncpy((char*) &cmdbuf[1], name,8);
	cmdbuf[3] = strnlen(name, 8);
	cmdbuf[4] = 0; // blocking

	if(R_FAILED(rc = svcSendSyncRequest(handle)))return rc;

	if(out) *out = cmdbuf[3];

	return cmdbuf[1];
}

Result srvRegisterClient(Handle handle)
{
	Result rc = 0;
	u32* cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x1,0,2); // 0x10002
	cmdbuf[1] = IPC_Desc_CurProcessId();

	if(R_FAILED(rc = svcSendSyncRequest(handle)))return rc;

	return cmdbuf[1];
}
