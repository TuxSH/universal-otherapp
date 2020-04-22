#include "gsp.h"

// lots of code adapted from libctru

Result GSPGPU_ReadHWRegs(Handle handle, u32 regAddr, u32* data, u8 size)
{
    if(size>0x80 || !data)return -1;

    u32* cmdbuf=getThreadCommandBuffer();
    cmdbuf[0]=IPC_MakeHeader(0x4,2,0); // 0x40080
    cmdbuf[1]=regAddr;
    cmdbuf[2]=size;
    cmdbuf[0x40]=IPC_Desc_StaticBuffer(size, 0);
    cmdbuf[0x40+1]=(u32)data;

    Result ret=0;
    if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

    return cmdbuf[1];
}

Result GSPGPU_WriteHWRegs(Handle handle, u32 regAddr, u32* data, u8 size)
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
Result gspSetTextureCopyPhys(Handle handle, u32 outPa, u32 inPa, u32 size, u32 inDim, u32 outDim, u32 flags)
{
    u32 enableReg = 0;
    Result res = 0;

    TRY(GSPGPU_ReadHWRegs(handle, 0x1EF00C18 - 0x1EB00000, &enableReg, 4));
    TRY(GSPGPU_WriteHWRegs(handle, 0x1EF00C00 - 0x1EB00000, (u32[]){inPa >> 3, outPa >> 3}, 0x8));
    TRY(GSPGPU_WriteHWRegs(handle, 0x1EF00C20 - 0x1EB00000, (u32[]){size, inDim, outDim}, 0xC));
    TRY(GSPGPU_WriteHWRegs(handle, 0x1EF00C10 - 0x1EB00000, &flags, 4));
    TRY(GSPGPU_WriteHWRegs(handle, 0x1EF00C18 - 0x1EB00000, (u32[]){enableReg | 1}, 4));

    svcSleepThread(25 * 1000 * 1000LL); // should be enough
    return 0;
}
