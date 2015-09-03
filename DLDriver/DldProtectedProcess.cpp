/*
 *  DldProtectedProcess.cpp
 *  DeviceLock
 *
 *  Created by Slava on 11/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

#include "DldProtectedProcess.h"

//--------------------------------------------------------------------

#define super OSObject
OSDefineMetaClassAndStructors( DldProtectedProcess, OSObject)

//--------------------------------------------------------------------

DldProtectedProcess* DldProtectedProcess::withPidAndAcl( __in pid_t  pid, __in_opt DldAclObject* aclObj )
{
    DldProtectedProcess*  protProcess;
    
    assert( preemption_enabled() );
    
    protProcess = new DldProtectedProcess();
    assert( protProcess );
    if( !protProcess ){
        
        DBG_PRINT_ERROR(("new DldProtectedProcess() failed\n"));        
        return NULL;
    } // if( !protProcess )
    
    if( !protProcess->init() ){
        
        DBG_PRINT_ERROR(("protProcess->init() failed\n"));
        protProcess->release();
        return NULL;
    }
    
    protProcess->acl = aclObj;
    protProcess->pid = pid;
    
    if( protProcess->acl )
        protProcess->acl->retain();
    
    return protProcess;
}

//--------------------------------------------------------------------

void DldProtectedProcess::free()
{
    if( this->acl )
        this->acl->release();
    
    super::free();
}

//--------------------------------------------------------------------
