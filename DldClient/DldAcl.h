/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDACL_H
#define _DLDACL_H

#include "DldUserCommon.h"
#include "DldDriverConnection.h"
#include <membership.h>
#include <unistd.h>

kern_return_t
DldSetAclFromText(
    __in io_connect_t    connection,
    __in DldDeviceType   deviceType,
    __in DldAclType      aclType,
    __in char*           acl_text
    );

void
printAclOnConsole( __in acl_t   acl );

#endif//_DLDACL_H