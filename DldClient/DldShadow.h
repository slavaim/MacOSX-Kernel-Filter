/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDSHADOW_H
#define _DLDSHADOW_H

#include "DldUserCommon.h"
#include "DldDriverConnection.h"
#include <membership.h>
#include <unistd.h>

kern_return_t
DldSetShadowFile(
    __in io_connect_t    connection,
    __in char* name,
    __in off_t maxSize,
    __in UInt32 shadowFileID
    );

IOReturn
shadowNotificationHandler(io_connect_t connection);

#endif//_DLDSHADOW_H
