#pragma once

#include "lib/defines.h"
#include "../kernelhaxcode_3ds/takeover.h"

struct SmpwnContext;
typedef struct SmpwnContext SmpwnContext;

/**
 * @brief Initializes srv: and exploits sm.
 * @pre System version is 7.0 or above.
 * @param[out] outSrvHandle Pointer to write the initialized srv: handle to.
 * @param[in, out] ctx Context. You need to pass big enough empty storage.
 * @post All the other functions in this file can be used.
 */
Result smpwn(Handle *outSrvHandle, SmpwnContext *ctx);

/**
 * @brief Returns the number of services before \ref smpwn was run.
 * @pre \ref smpwn has been called.
 * @param ctx Context.
 */
u32 smGetInitialServiceCount(const SmpwnContext *ctx);

/**
 * @brief Copies data into sm's memory.
 * @pre \ref smpwn has been called.
 * @param ctx Context.
 * @param smDst Destination address in sm's memory.
 * @param src Data to copy.
 * @param size Number of bytes to copy.
 */
Result smWriteData(const SmpwnContext *ctx, u32 smDst, const void *src, size_t size);

/**
 * @brief Quickly cleans up smpwn.
 * @param ctx Context.
 * @pre \ref smpwn has been called.
 * @pre Things like the APT thread must not be running.
 * @post Cleanup done, backdoor kept, handles leaked.
 */
Result smPartiallyCleanupSmpwn(const SmpwnContext *ctx);

/**
 * @brief Removes sm service restrictions.
 * @pre \ref smpwn has been called.
 * @param ctx Context.
 * @post Any process can now have access to any service AND get new srv:pm session handles.
 */
Result smRemoveRestrictions(const SmpwnContext *ctx);
