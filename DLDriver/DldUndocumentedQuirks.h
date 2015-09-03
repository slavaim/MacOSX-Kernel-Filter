/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDUNDOCUMENTEDQUIRKS_H
#define DLDUNDOCUMENTEDQUIRKS_H

#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/vm.h>
#include "DldCommon.h"

//--------------------------------------------------------------------

bool DldInitUndocumentedQuirks();

void DldFreeUndocumentedQuirks();

//--------------------------------------------------------------------

task_t DldBsdProcToTask( __in proc_t proc );
proc_t DldTaskToBsdProc( __in task_t task );

//--------------------------------------------------------------------

int DldDisablePreemption();

void DldEnablePreemption( __in int cookie );

//--------------------------------------------------------------------

void
DldKdpRegister();

//--------------------------------------------------------------------

#endif//DLDUNDOCUMENTEDQUIRKS_H