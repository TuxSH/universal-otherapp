#include "httpc.h"
#include "srv.h"

// lots of code adapted from libctru

static Result HTTPC_Initialize(Handle handle, u32 sharedmem_size, Handle sharedmem_handle)
{
    u32* cmdbuf=getThreadCommandBuffer();

    cmdbuf[0]=IPC_MakeHeader(0x1,1,4); // 0x10044
    cmdbuf[1]=sharedmem_size; // POST buffer size (page aligned)
    cmdbuf[2]=IPC_Desc_CurProcessId();
    cmdbuf[4]=IPC_Desc_SharedHandles(1);
    cmdbuf[5]=sharedmem_handle;// POST buffer memory block handle

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

static Result HTTPC_CreateContext(Handle handle, HTTPC_RequestMethod method, const char* url, Handle* contextHandle)
{
    u32* cmdbuf=getThreadCommandBuffer();
    u32 l=strlen(url)+1;

    cmdbuf[0]=IPC_MakeHeader(0x2,2,2); // 0x20082
    cmdbuf[1]=l;
    cmdbuf[2]=method;
    cmdbuf[3]=IPC_Desc_Buffer(l,IPC_BUFFER_R);
    cmdbuf[4]=(u32)url;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    if(contextHandle)*contextHandle=cmdbuf[2];

    return cmdbuf[1];
}

static Result HTTPC_InitializeConnectionSession(Handle handle, Handle contextHandle)
{
    u32* cmdbuf=getThreadCommandBuffer();

    cmdbuf[0]=IPC_MakeHeader(0x8,1,2); // 0x80042
    cmdbuf[1]=contextHandle;
    cmdbuf[2]=IPC_Desc_CurProcessId();

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

static Result HTTPC_CloseContext(Handle handle, Handle contextHandle)
{
    u32* cmdbuf=getThreadCommandBuffer();

    cmdbuf[0]=IPC_MakeHeader(0x3,1,0); // 0x30040
    cmdbuf[1]=contextHandle;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

Result httpcInit(Handle srvHandle, Handle *outHandle, Handle *outSharedMemHandle, void *sharedMem, size_t sharedMemSize)
{
    Result res = 0;
    TRY(srvGetServiceHandle(srvHandle, outHandle, "http:C"));
    TRY(svcCreateMemoryBlock(outSharedMemHandle, (u32)sharedMem, sharedMemSize, 0, MEMPERM_READ | MEMPERM_WRITE));
    TRY(HTTPC_Initialize(*outHandle, sharedMemSize, *outSharedMemHandle));
    return res;
}

Result httpcOpenContext(Handle srvHandle, Handle handle, httpcContext *context, HTTPC_RequestMethod method, const char* url)
{
    Result ret=0;

    ret = HTTPC_CreateContext(handle, method, url, &context->httphandle);
    if(R_FAILED(ret))return ret;

    ret = srvGetServiceHandle(srvHandle, &context->servhandle, "http:C");
    if(R_FAILED(ret)) {
        HTTPC_CloseContext(handle, context->httphandle);
        return ret;
    }

    ret = HTTPC_InitializeConnectionSession(context->servhandle, context->httphandle);
    if(R_FAILED(ret)) {
        svcCloseHandle(context->servhandle);
        HTTPC_CloseContext(handle, context->httphandle);
        return ret;
    }

    return 0;
}

Result httpcCloseContext(Handle handle, httpcContext *context)
{
    Result ret=0;

    svcCloseHandle(context->servhandle);
    ret = HTTPC_CloseContext(handle, context->httphandle);

    return ret;
}

Result httpcAddPostDataAscii(httpcContext *context, const char* name, const char* value)
{
    u32* cmdbuf=getThreadCommandBuffer();

    size_t name_len=strlen(name)+1;
    size_t value_len=strlen(value)+1;

    cmdbuf[0]=IPC_MakeHeader(0x12,3,4); // 0x1200C4
    cmdbuf[1]=context->httphandle;
    cmdbuf[2]=name_len;
    cmdbuf[3]=value_len;
    cmdbuf[4]=IPC_Desc_StaticBuffer(name_len,3);
    cmdbuf[5]=(u32)name;
    cmdbuf[6]=IPC_Desc_Buffer(value_len,IPC_BUFFER_R);
    cmdbuf[7]=(u32)value;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(context->servhandle)))return ret;

    return cmdbuf[1];
}
