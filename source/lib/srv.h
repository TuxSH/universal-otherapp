// Copied and adapted from libctru

#pragma once

#include "defines.h"

/// Initializes the service API.
Result srvInit(Handle *outHandle);

/**
 * @brief Retrieves a service handle, retrieving from the environment handle list if possible.
 * @param out Pointer to write the handle to.
 * @param name Name of the service.
 * @return 0 if no error occured,
 *         0xD8E06406 if the caller has no right to access the service,
 *         0xD0401834 if the requested service port is full and srvGetServiceHandle is non-blocking (see @ref srvSetBlockingPolicy).
 */
Result srvGetServiceHandle(Handle handle, Handle* out, const char* name);

/// Registers the current process as a client to the service API.
Result srvRegisterClient(Handle handle);
