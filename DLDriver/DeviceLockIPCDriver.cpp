/*
 *  DeviceLockIPCDriver.cpp
 *  DeviceLock
 *
 *  Created by Slava on 6/01/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */

#include <sys/proc.h>
#include "DeviceLockIPCDriver.h"
#include "DldIPCUserClient.h"
#include "DldServiceProtection.h"
#include "DldUndocumentedQuirks.h"

//--------------------------------------------------------------------

//
// the standard IOKit declarations
//
#define super IOService

OSDefineMetaClassAndStructors(com_devicelock_driver_DeviceLockIPCDriver, IOService)

//--------------------------------------------------------------------

bool
com_devicelock_driver_DeviceLockIPCDriver::start(
    __in IOService *provider
    )
{
    if( super::start( provider ) ){
        //
        // kick off the service registration ( do we need it? )
        //
        this->registerService();
        return true;
    } else
        return false;
}

//--------------------------------------------------------------------

void
com_devicelock_driver_DeviceLockIPCDriver::stop(
    __in IOService * provider
    )
{
    super::stop( provider );
}

//--------------------------------------------------------------------

bool com_devicelock_driver_DeviceLockIPCDriver::init()
{
    return super::init();
}

//--------------------------------------------------------------------

IOReturn com_devicelock_driver_DeviceLockIPCDriver::newUserClient(
    __in task_t owningTask,
    __in void*,
    __in UInt32 type,
    __in IOUserClient **handler
    )
{
    
    DldIPCUserClient*  client = NULL;
    
    //
    // Check that this is a user client type that we support.
    // type is known only to this driver's user and kernel
    // classes. It could be used, for example, to define
    // read or write privileges. In this case, we look for
    // a private value.
    //
    if( type != kDldUserIPCClientCookie )
        return kIOReturnBadArgument;
    
    //
    // check security, there are two cases
    //  - the firts is when the Admin ACL us NULL, so the check is made for the super user
    //  - the second is when the Admin ACL is not NULL, so the check is made against this ACL
    //
    proc_t owningProc = DldTaskToBsdProc( owningTask );
    assert( owningProc );
    if( ! owningProc )
        return kIOReturnBadArgument;
    
    kauth_cred_t cred = kauth_cred_proc_ref( owningProc );
    assert( cred );
    if( ! cred ){
        
        DBG_PRINT_ERROR(("kauth_cred_proc_ref( current_proc() ) returned NULL\n"));
        return kIOReturnNotPrivileged;
    }

    assert( cred );
    
    uid_t proc_uid = kauth_cred_getuid(cred);
    
    bool isAccessGranted = false;
    
    if( gServiceProtection->useDefaultSystemSecurity() ){
        
        //
        // the default system policy allows access only for a super user
        //
        isAccessGranted = (0x0 == proc_suser( owningProc ));
        
    } else {
        
        //
        // service admins' ACL is the same as the service process ACL
        //
        DldProtectedProcess* protectedProcess = gServiceProtection->getProtectedProcessRef( gServiceUserClient.getUserClientPid() );
        if( protectedProcess ){
            
            isAccessGranted = gServiceProtection->isAccessAllowed( protectedProcess->getAclObject(),
                                                                  DL_PROCESS_TERMINATE,
                                                                  cred );
            protectedProcess->release();
            protectedProcess = NULL;
            
        } else {
            
            //
            // there is no service admins' ACL, grant access to super user only
            //
            isAccessGranted = (0x0 == proc_suser( owningProc ));
        }
    }
    kauth_cred_unref( &cred );
    cred = NULL;
    
    if( ! isAccessGranted ){
        
        DLD_COMM_LOG(COMMON, ("An attempt to open the IPC driver by an unprivileged user, PID = %d\n", proc_pid( owningProc ) ));
        // carry on, the service will take care of untrusted clients
        //return kIOReturnNotPrivileged;
    }
    
    
    //
    // Construct a new client instance for the requesting task.
    // This is, essentially  client = new DldIOUserClient;
    //                               ... create metaclasses ...
    //                               client->setTask(owningTask)
    //
    client = DldIPCUserClient::withTask( owningTask, isAccessGranted, proc_uid );
    assert( client );
    if( client == NULL ){
        
        DBG_PRINT_ERROR(("Can not create a user client for the task = %p\n", (void*)owningTask ));
        return kIOReturnNoResources;
    }
    
    //
    // Attach the client to our driver
    //
    if( !client->attach( this ) ) {
        
        assert( !"client->attach( this ) failed" );
        DBG_PRINT_ERROR(("Can attach a user client for the task = %p\n", (void*)owningTask ));
        
        client->release();
        return kIOReturnNoResources;
    }
    
    //
    // Start the client so it can accept requests
    //
    if( !client->start( this ) ){
        
        assert( !"client->start( this ) failed" );
        DBG_PRINT_ERROR(("Can start a user client for the task = %p\n", (void*)owningTask ));
        
        client->detach( this );
        client->release();
        return kIOReturnNoResources;
    }
    
    *handler = client;
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------
