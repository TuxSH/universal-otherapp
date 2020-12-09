#pragma once

#include "defines.h"

Result srvPmInit(Handle *outSrvPmHandle, Handle srvHandle);
Result SRVPM_PublishToAll(Handle handle, u32 notificationId);
