/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDDRIVERCONNECTION_H
#define _DLDDRIVERCONNECTION_H

#include "DldUserCommon.h"

extern
kern_return_t
DldOpenDlDriver(
    __out io_connect_t*    connection
    );

#endif//_DLDDRIVERCONNECTION_H