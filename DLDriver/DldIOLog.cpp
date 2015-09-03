/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOLog.h"
#include <sys/proc.h>

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIOLog, OSObject )

//--------------------------------------------------------------------

DldIOLog* DldIOLog::newLog()
{
    DldIOLog* newLog = new DldIOLog();
    assert( newLog );
    if( !newLog )
        return newLog;
    
    if( !newLog->init() ){
        
        newLog->release();
        return NULL;
    }
    
    return newLog;
}

//--------------------------------------------------------------------

bool DldIOLog::isUserClientPresent()
{
    return ( NULL != this->userClient );
}

//--------------------------------------------------------------------

IOReturn DldIOLog::registerUserClient( __in DldIOUserClient* client )
{
    bool registered;
    
    if( this->pendingUnregistration ){
        
        DBG_PRINT_ERROR(("this->pendingUnregistration\n"));
        return kIOReturnError;
    }
    
    registered = OSCompareAndSwapPtr( NULL, (void*)client, &this->userClient );
    assert( registered );
    if( !registered ){
        
        DBG_PRINT_ERROR(("!registered\n"));
        return kIOReturnError;
    }
    
    client->retain();
    
    return registered? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn DldIOLog::unregisterUserClient( __in DldIOUserClient* client )
{
    bool   unregistered;
    DldIOUserClient*  currentClient;
    
    currentClient = (DldIOUserClient*)this->userClient;
    assert( currentClient == client );
    if( currentClient != client ){
        
        DBG_PRINT_ERROR(("currentClient != client\n"));
        return kIOReturnError;
    }
    
    this->pendingUnregistration = true;
    
    unregistered = OSCompareAndSwapPtr( (void*)currentClient, NULL, &this->userClient );
    assert( unregistered && NULL == this->userClient );
    if( !unregistered ){
        
        DBG_PRINT_ERROR(("!unregistered\n"));
        
        this->pendingUnregistration = false;
        return kIOReturnError;
    }
    
    do { // wait for any existing client invocations to return
        
        struct timespec ts = { 1, 0 }; // one second
        (void)msleep( &this->clientInvocations,      // wait channel
                      NULL,                          // mutex
                      PUSER,                         // priority
                      "DldIOLog::unregisterUserClient()", // wait message
                      &ts );                         // sleep interval
        
    } while( this->clientInvocations != 0 );
    
    currentClient->release();
    this->pendingUnregistration = false;
    
    return unregistered? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn DldIOLog::logData( __in DldDriverDataLogInt*  intData )
{
    
    IOReturn          RC;
    DldIOUserClient*  currentClient;
    
    //
    // if ther is no user client, then nobody call for logging
    //
    if( NULL == this->userClient || this->pendingUnregistration )
        return kIOReturnSuccess;
    
    OSIncrementAtomic( &this->clientInvocations );
    
    currentClient = (DldIOUserClient*)this->userClient;
    
    //
    // if the current client is NULL or can't be atomicaly exchanged
    // with the same value then the unregistration is in progress,
    // the call to OSCompareAndSwapPtr( NULL, NULL, &this->userClient )
    // checks the this->userClient for NULL atomically 
    //
    if( !currentClient ||
        !OSCompareAndSwapPtr( currentClient, currentClient, &this->userClient ) ||
        OSCompareAndSwapPtr( NULL, NULL, &this->userClient ) ){
        
        //
        // the unregistration is in the progress and waiting for all
        // invocations to return
        //
        assert( this->pendingUnregistration );
        if( 0x1 == OSDecrementAtomic( &this->clientInvocations ) ){
            
            //
            // this was the last invocation
            //
            wakeup( &this->clientInvocations );
        }
        
        return kIOReturnSuccess;
    }
    
    bool   repeat = false;
    SInt32 resendCounter = gGlobalSettings.maxAttemptsToResendBuffer;
    bool   kernelThread = ( 0x0 == proc_pid( current_proc() ) );
    do{
        
        RC = currentClient->logData( intData );
        
        //
        // allow to sleep if this is not a kernel request 
        //
        repeat = ( kIOReturnNoMemory == RC ) &&
                 ( ! kernelThread ) &&
                 ( (--resendCounter) > 0 ) &&
                 ( 0x0 != preemption_enabled() );
        if( repeat ){
            
            IOSleep(  gGlobalSettings.millisecondsWaitForFreeSpaceInNoificationsBuffer );
        }
    }while( repeat );
                           
    //
    // do not exchange or add any condition before OSDecrementAtomic as it must be always done!
    //
    if( 0x1 == OSDecrementAtomic( &this->clientInvocations ) && NULL == this->userClient ){
        
        //
        // this was the last invocation
        //
        wakeup( &this->clientInvocations );
    }
    
    return RC;
    
}

//--------------------------------------------------------------------

