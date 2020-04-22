#pragma once

#include "defines.h"

/// HTTP context.
typedef struct {
    Handle servhandle; ///< Service handle.
    u32 httphandle;    ///< HTTP handle.
} httpcContext;

/// HTTP request method.
typedef enum {
    HTTPC_METHOD_GET = 0x1,
    HTTPC_METHOD_POST = 0x2,
    HTTPC_METHOD_HEAD = 0x3,
    HTTPC_METHOD_PUT = 0x4,
    HTTPC_METHOD_DELETE = 0x5
} HTTPC_RequestMethod;

Result httpcInit(Handle srvHandle, Handle *outHandle, Handle *outSharedMemHandle, void *sharedMem, size_t sharedMemSize);
Result httpcOpenContext(Handle srvHandle, Handle handle, httpcContext *context, HTTPC_RequestMethod method, const char* url);
Result httpcCloseContext(Handle handle, httpcContext *context);
Result httpcAddPostDataAscii(httpcContext *context, const char* name, const char* value);
