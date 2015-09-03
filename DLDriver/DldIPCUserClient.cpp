/*
 *  IPCUserClient.cpp
 *  DeviceLock
 *
 *  Created by Slava on 7/01/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */

#include <sys/proc.h>
#include "DldUndocumentedQuirks.h"
#include "DldIPCUserClient.h"
#include "DldServiceProtection.h"
#include "DldKernelToUserEvents.h"
#include "DldIOUserClient.h"

//--------------------------------------------------------------------

#define super IOUserClient

OSDefineMetaClassAndStructors( DldIPCUserClient, IOUserClient )

//--------------------------------------------------------------------

static const IOExternalMethod sMethods[ kt_DldUserClientMethodsMax ] =
{
    // 0x0 kt_DldUserIPCClientInvalidRequest
    {
        NULL,
        (IOMethod)&DldIPCUserClient::invalidRequest,
        kIOUCScalarIScalarO,
        0,
        0
    },
    // 0x1 kt_DldUserIPCClientIpcRequest
    {
        NULL,
        (IOMethod)&DldIPCUserClient::ipcRequest,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },

};

//--------------------------------------------------------------------

DldIPCUserClient::WaitBlock    DldIPCUserClient::WaitBlocks[DldIPCUserClient::WaitBlocksNumber];
IOSimpleLock*                  DldIPCUserClient::WaitLock;

bool DldIPCUserClient::InitIPCUserClientStaticPart()
{
    DldIPCUserClient::WaitLock = IOSimpleLockAlloc();
    assert( DldIPCUserClient::WaitLock );
    if( ! DldIPCUserClient::WaitLock )
        return false;
    
    return true;
}

//--------------------------------------------------------------------

DldIPCUserClient* DldIPCUserClient::withTask( __in task_t owningTask, __in bool trustedClient, uid_t proc_uid )
{
    DldIPCUserClient* client;
    
    DBG_PRINT(("DldIPCUserClient::withTask( %p, %d )\n", (void*)owningTask, (int)trustedClient ));
    
    client = new DldIPCUserClient();
    if( !client )
        return NULL;
    
    if (client->init() == false) {
        
        client->release();
        return NULL;
    }

    client->fClientUID = proc_uid;
    client->fClient = owningTask;
    client->trustedClient = trustedClient;
    client->fClientProc = DldTaskToBsdProc( owningTask );
    assert( client->fClientProc );
    if( client->fClientProc )
        client->fClientPID = proc_pid( client->fClientProc );
    
    return client;
}

//--------------------------------------------------------------------

IOExternalMethod*
DldIPCUserClient::getTargetAndMethodForIndex( __in IOService **target, __in UInt32 index)
{
    if( index >= (UInt32)kt_DldUserIPCClientMethodsMax )
        return NULL;

    *target = this;    
    return (IOExternalMethod *)&sMethods[index];
}

//--------------------------------------------------------------------

IOReturn DldIPCUserClient::invalidRequest(void)
{
    return kIOReturnUnsupported;
}

//--------------------------------------------------------------------

IOReturn
DldIPCUserClient::ipcRequest(
                        __in  void *vInBuffer,
                        __out void *vOutBuffer,
                        __in  void *vInSize,
                        __in  void *vOutSizeP,
                        void *, void *)
{
    IOReturn        RC = kIOReturnSuccess;
    DldIpcRequest*  ipcRequest = (DldIpcRequest*)vInBuffer;
    vm_size_t       inSize = (vm_size_t)vInSize;
    
    if( inSize < sizeof( *ipcRequest ) ||
        ipcRequest->size < sizeof( *ipcRequest ) ||
        inSize < ipcRequest->size )
        return kIOReturnBadArgument;
    

    //
    // get a service's client to send a notification to
    //
    DldIOUserClient* serviceUserClient = gServiceUserClient.getUserClient();
    if( ! serviceUserClient )
        return kIOReturnOffline;
    
    SInt32 eventID = gKerneToUserEvents.getNextEventNumber();
    
    UInt32 waitBlockIndex;
    if( ! this->acquireWaitBlock( &waitBlockIndex, eventID ) ){
        
        gServiceUserClient.releaseUserClient();
        return kIOReturnNoResources;
    }
    
    DldDriverEventData  eventTemplate;
    bzero( &eventTemplate, sizeof( eventTemplate ) );
    eventTemplate.Header.id = eventID;
    eventTemplate.Header.size = sizeof( eventTemplate );
    eventTemplate.Header.trustedClient = this->trustedClient;
    eventTemplate.Header.waitBlockIndex = waitBlockIndex;
    
    //
    // send a notification to the service
    //
    switch( ipcRequest->operation ){
            
        case DLD_IPCOP_STOP_SERVICE:
        {
                
            DldDriverEventData  event;
            
            event = eventTemplate;
            event.Header.type = DLD_EVENT_FORCED_STOP;
            
            RC = serviceUserClient->eventNotification( &event );
            
            break;
        }
            
        case DLD_IPCOP_IMPORT_DLS:
        {
            //
            // check the request consistency
            //
            if( ipcRequest->Tail.ImportDLS.filePathLength + sizeof( *ipcRequest ) > ipcRequest->size ){
                
                DBG_PRINT_ERROR(("ipcRequest->Tail.ImportDLS.filePathLength + sizeof( *ipcRequest ) > ipcRequest->size,"
                                  "ipcRequest->Tail.ImportDLS.filePathLength is %d,"
                                  "sizeof( *ipcRequest ) is %d,"
                                  "ipcRequest->size is %d\n",
                                  ipcRequest->Tail.ImportDLS.filePathLength,
                                  sizeof( *ipcRequest ),
                                  ipcRequest->size ));
                RC = kIOReturnBadArgument;
                break;
            }
            
            if( ipcRequest->Tail.ImportDLS.filePathLength < sizeof("/X") ){
                
                
                DBG_PRINT_ERROR(("the DLS file path is too short to be valid\n"));
                RC = kIOReturnBadArgument;
                break;
            }
            
            //
            // check that the zero terminator is present
            //
            if( '\0' != ((const char*)( ipcRequest+1 ))[ ipcRequest->Tail.ImportDLS.filePathLength - 1 ] ){
                
                DBG_PRINT_ERROR(("the DLS file path is not zero terminated\n"));
                RC = kIOReturnBadArgument;
                break;
            }
            
            //
            // check that this is a fully qualified path
            //
            if( '/' != ((const char*)( ipcRequest+1 ))[ 0 ] ){
                
                DBG_PRINT_ERROR(("the DLS file path is not a fully qualified one\n"));
                RC = kIOReturnBadArgument;
                break;
            }
            
            vm_size_t sizeOfEvent = sizeof( DldDriverEventData ) + ipcRequest->Tail.ImportDLS.filePathLength;
            
            DldDriverEventData*  event = (DldDriverEventData*)IOMalloc( sizeOfEvent );
            assert( event );
            if( !event ){
                
                DBG_PRINT_ERROR(("allocating memory for an event failed\n"));
                RC = kIOReturnNoMemory;
                break;
            }
            
            bcopy( &eventTemplate, event, min( sizeof(eventTemplate), sizeOfEvent ) );

            event->Header.size = sizeOfEvent;
            event->Header.type = DLD_EVENT_IMPORT_DLS;
            
            event->Tail.ImportDLS.adminRequest   = ipcRequest->Tail.ImportDLS.adminRequest;
            event->Tail.ImportDLS.filePathLength = ipcRequest->Tail.ImportDLS.filePathLength;
            event->Tail.ImportDLS.senderUid = fClientUID;
            bcopy( (const void*)(ipcRequest+1), (void*)(event+1), ipcRequest->Tail.ImportDLS.filePathLength );
            
            RC = serviceUserClient->eventNotification( event );
            
            IOFree( event, sizeOfEvent );
            event = NULL;
            break;
        }
            
        case DLD_IPCOP_GPUPDATE:
        {
            
            DldDriverEventData  event;
            
            event = eventTemplate;
            event.Header.type = DLD_EVENT_GPUPDATE;
            
            RC = serviceUserClient->eventNotification( &event );
            
            break;
        }
            
        default:
            RC = kIOReturnBadArgument;
            break;
    } // end switch
    
    gServiceUserClient.releaseUserClient();
    serviceUserClient = NULL;
    
    if( kIOReturnSuccess == RC ){
        
        RC = this->waitForCompletion( waitBlockIndex );
        
    } else {
        
        this->releaseWaitBlock( waitBlockIndex );
    }
    
    return RC;
}

//--------------------------------------------------------------------

bool DldIPCUserClient::acquireWaitBlock( __out UInt32* waitBlockIndex, __in SInt32 eventID )
{
    UInt32 index = (-1);
    
    //
    // skip the first element with the 0x0 index, this value is used to indicate that no
    // waiting block has been allocated
    //
    for( UInt32 i = 1; i < DLD_STATIC_ARRAY_SIZE(DldIPCUserClient::WaitBlocks); ++i ){
        
        if( ! OSCompareAndSwap( 0x0, 0x1, &DldIPCUserClient::WaitBlocks[ i ].inUse ) )
            continue;
        
        assert( 0x1 == DldIPCUserClient::WaitBlocks[ i ].inUse );
        
        DldIPCUserClient::WaitBlocks[ i ].completed = false;
        DldIPCUserClient::WaitBlocks[ i ].eventID = eventID;
        index = i;
        *waitBlockIndex = i;
        break;
    } // end for
    
    return ( (-1) != index );
}

//--------------------------------------------------------------------

void DldIPCUserClient::releaseWaitBlock( __in UInt32 waitBlockIndex )
{
    assert( waitBlockIndex < DLD_STATIC_ARRAY_SIZE(DldIPCUserClient::WaitBlocks) );
    assert( 0x0 != DldIPCUserClient::WaitBlocks[waitBlockIndex].inUse );
    
    //
    // the lock acquiring is required for synchroniztion with DldIPCUserClient::ProcessResponse
    //
    IOSimpleLockLock( DldIPCUserClient::WaitLock );
    { // start of the locked region
        DldIPCUserClient::WaitBlocks[waitBlockIndex].completed = false;
        DldIPCUserClient::WaitBlocks[waitBlockIndex].eventID = 0x0;
        DldMemoryBarrier();
        DldIPCUserClient::WaitBlocks[waitBlockIndex].inUse = 0x0;
    } // end of the locked region
    IOSimpleLockUnlock( DldIPCUserClient::WaitLock );
}

//--------------------------------------------------------------------

IOReturn  DldIPCUserClient::waitForCompletion( __in unsigned int waitBlockIndex )
{
    IOReturn       error = kIOReturnTimeout;
    wait_result_t  wait = THREAD_AWAKENED;
    
    assert( preemption_enabled() );
    assert( 0x0 != waitBlockIndex && waitBlockIndex < DLD_STATIC_ARRAY_SIZE(DldIPCUserClient::WaitBlocks) );
    
    //
    // wait for a response from the service
    //
    IOSimpleLockLock( DldIPCUserClient::WaitLock );
    { // start of the locked region
        
        if( !DldIPCUserClient::WaitBlocks[waitBlockIndex].completed ){
            //
            // wait for response with 2 minutes timeout
            //
            wait = assert_wait_timeout((event_t)&DldIPCUserClient::WaitBlocks[waitBlockIndex],
                                       THREAD_UNINT,
                                       120000, /*120secs*/
                                       1000*NSEC_PER_USEC);
        }
    } // end of the locked region
    IOSimpleLockUnlock( DldIPCUserClient::WaitLock );
    
    if( THREAD_WAITING == wait ){
        
        wait = thread_block( THREAD_CONTINUE_NULL );
    }
    
    assert( 0x0 != DldIPCUserClient::WaitBlocks[waitBlockIndex].inUse );
    
    //
    // if the service has not responded then this is an error as the service is probably dead
    //
    if( THREAD_TIMED_OUT == wait ){
        
        error = kIOReturnTimeout;
        
    } else {
        
        assert( DldIPCUserClient::WaitBlocks[waitBlockIndex].completed );
        error = DldIPCUserClient::WaitBlocks[waitBlockIndex].operationCompletionStatus;
    }
    
    //
    // release the block
    //
    this->releaseWaitBlock( waitBlockIndex );
    
    return error;
}

//--------------------------------------------------------------------

IOReturn DldIPCUserClient::ProcessResponse( __in DldDriverEventData* response )
{
    UInt32 waitBlockIndex = response->Header.waitBlockIndex;
    
    if( DLD_EVENT_RESPONSE != response->Header.type ){
        
        DBG_PRINT_ERROR(("DLD_EVENT_RESPONSE != response->Header.type\n"));
        return kIOReturnBadArgument;
    }
    
    if( waitBlockIndex >= DLD_STATIC_ARRAY_SIZE(DldIPCUserClient::WaitBlocks) ){
        
        DBG_PRINT_ERROR(("response->Head.waitBlockIndex is out of range\n"));
        return kIOReturnBadArgument;
    }
    
    if( DldIPCUserClient::WaitBlocks[waitBlockIndex].eventID != response->Header.id ){
        
        DBG_PRINT_ERROR(("DldIPCUserClient::WaitBlocks[ %d ].eventID != response->Header.id\n", waitBlockIndex));
        return kIOReturnBadArgument;
    }
    
    bool wakeUp = false;
    
    IOSimpleLockLock( DldIPCUserClient::WaitLock );
    { // start of the locked region
        
        if( DldIPCUserClient::WaitBlocks[waitBlockIndex].eventID == response->Header.id ){
            
            assert( 0x0 != DldIPCUserClient::WaitBlocks[waitBlockIndex].inUse );
            DldIPCUserClient::WaitBlocks[waitBlockIndex].completed = true;
            DldIPCUserClient::WaitBlocks[waitBlockIndex].operationCompletionStatus = response->Tail.Response.operationCompletionStatus;
            wakeUp = true;
        }
        
    } // end of the locked region
    IOSimpleLockUnlock( DldIPCUserClient::WaitLock );
    
    if( ! wakeUp )
        return kIOReturnBadArgument;
    
    //
    // there is a slim chance to wake up a just acquired block that was released by timeout, we neglect this case
    //
    wakeup( (event_t)&DldIPCUserClient::WaitBlocks[waitBlockIndex] );
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------