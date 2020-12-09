#include "gsp.h"

// lots of code adapted from libctru

Result GSPGPU_WriteHWRegs(Handle handle, u32 regAddr, const u32* data, u8 size)
{
    if(size>0x80 || !data)return -1;

    u32* cmdbuf=getThreadCommandBuffer();
    cmdbuf[0]=IPC_MakeHeader(0x1,2,2); // 0x10082
    cmdbuf[1]=regAddr;
    cmdbuf[2]=size;
    cmdbuf[3]=IPC_Desc_StaticBuffer(size, 0);
    cmdbuf[4]=(u32)data;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

Result GSPGPU_WriteHWRegsWithMask(Handle handle, u32 regAddr, const u32* data, u8 datasize, const u32* maskdata, u8 masksize)
{
    if(datasize>0x80 || !data)return -1;

    u32* cmdbuf=getThreadCommandBuffer();
    cmdbuf[0]=IPC_MakeHeader(0x2,2,4); // 0x20084
    cmdbuf[1]=regAddr;
    cmdbuf[2]=datasize;
    cmdbuf[3]=IPC_Desc_StaticBuffer(datasize, 0);
    cmdbuf[4]=(u32)data;
    cmdbuf[5]=IPC_Desc_StaticBuffer(masksize, 1);
    cmdbuf[6]=(u32)maskdata;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

Result GSPGPU_FlushDataCache(Handle handle, const void* adr, u32 size)
{
    u32* cmdbuf=getThreadCommandBuffer();
    cmdbuf[0]=IPC_MakeHeader(0x8,2,2); // 0x80082
    cmdbuf[1]=(u32)adr;
    cmdbuf[2]=size;
    cmdbuf[3]=IPC_Desc_SharedHandles(1);
    cmdbuf[4]=CUR_PROCESS_HANDLE;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

// Source: https://github.com/smealum/udsploit/blob/master/source/kernel.c#L11
void gspSetTextureCopyPhys(Handle handle, u32 outPa, u32 inPa, u32 size, u32 inDim, u32 outDim, u32 flags)
{
    // Ignore results... only reason it would be invalid is if the handle itself is invalid
    const u32 enableBit = 1;

    GSPGPU_WriteHWRegs(handle, 0x1EF00C00 - 0x1EB00000, (u32[]){inPa >> 3, outPa >> 3}, 0x8);
    GSPGPU_WriteHWRegs(handle, 0x1EF00C20 - 0x1EB00000, (u32[]){size, inDim, outDim}, 0xC);
    GSPGPU_WriteHWRegs(handle, 0x1EF00C10 - 0x1EB00000, &flags, 4);
    GSPGPU_WriteHWRegsWithMask(handle, 0x1EF00C18 - 0x1EB00000, &enableBit, 4, &enableBit, 4);

    svcSleepThread(25 * 1000 * 1000LL); // should be enough
}
