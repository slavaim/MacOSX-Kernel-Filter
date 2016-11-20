/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */
#ifndef _DLDUSERCOMMON_H
#define _DLDUSERCOMMON_H

#include <IOKit/IOKitLib.h>
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IODataQueueClient.h>

#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/acl.h>

#include "../DLDriver/DldUserToKernel.h"

#define PROGNAME "DldClient"

#define PRINT_ERROR(msg) \
{ printf( "%s: %s : %s (%d) : ", PROGNAME, __FILE__, __PRETTY_FUNCTION__, __LINE__ ); printf msg; }

#define __in
#define __out
#define __inout
#define __in_opt
#define __out_opt

#endif//_DLDUSERCOMMON_H
