/* 
 * Copyright (c) 2012 Slava Imameev. All rights reserved.
 */

#include <sys/proc.h>
#include <IOKit/IODataQueueShared.h>

#include "DldSocketObject.h"
#include "DldSocketFilter.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldSocketObject, OSObject )

//--------------------------------------------------------------------

SInt32          DldSocketObject::SocketObjectsCounter = 0x0;
bool            DldSocketObject::Initialized = false;
DldSocketObject::DldSocketsListHead DldSocketObject::SocketsList;
IORWLock*       DldSocketObject::SocketsListLock;
DldSocketObject::DldSocketsListToReportHead DldSocketObject::SocketsListToReport;
IORWLock*       DldSocketObject::SocketsListToReportLock;
OSMallocTag		DldSocketObject::gOSMallocTag;
SInt32          DldSocketObject::gSocketSequence = 0x1;

//--------------------------------------------------------------------

// only for the test, borrowed from the apple open sources
IOReturn IODataQueueDequeue(IODataQueueMemory *dataQueue, void *data, UInt32 *dataSize)
{
    IOReturn retVal = kIOReturnSuccess;
    IODataQueueEntry *entry = 0;
    UInt32 newHead = 0;
    
    if (dataQueue) {
        if (dataQueue->head != dataQueue->tail) {
            IODataQueueEntry *head = (IODataQueueEntry *)((char *)dataQueue->queue + dataQueue->head);
            // we wraped around to beginning, so read from there
			// either there was not even room for the header
			if ((dataQueue->head + DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
				// or there was room for the header, but not for the data
				((dataQueue->head + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize)) {
                entry = dataQueue->queue;
                newHead = dataQueue->queue->size + DATA_QUEUE_ENTRY_HEADER_SIZE;
                // else it is at the end
            } else {
                entry = head;
                newHead = dataQueue->head + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE;
            }
        }
        
        if (entry) {
            if (data) {
                if (dataSize) {
                    if (entry->size <= *dataSize) {
                        memcpy(data, &entry->data, entry->size);
                        *dataSize = entry->size;
                        dataQueue->head = newHead;
                    } else {
                        *dataSize = entry->size;
                        retVal = kIOReturnNoSpace;
                    }
                } else {
                    retVal = kIOReturnBadArgument;
                }
            } else {
                dataQueue->head = newHead;
            }
        } else {
            retVal = kIOReturnUnderrun;
        }
    } else {
        retVal = kIOReturnBadArgument;
    }
    
    return retVal;
}

//--------------------------------------------------------------------

errno_t DldSocketObject::InitSocketObjectsSubsystem()
{
    assert( ! DldSocketObject::Initialized );
    
    //
    // don't want the flag set to OSMT_PAGEABLE since
    // it would indicate that the memory was pageable.
    //
    gOSMallocTag = OSMalloc_Tagalloc( "DLDriverNKE", OSMT_DEFAULT );
	if( NULL == gOSMallocTag )
        return ENOMEM;
    
    if( DldSocketObject::Initialized )
        return KERN_SUCCESS;
    
    TAILQ_INIT( &DldSocketObject::SocketsList );
    TAILQ_INIT( &DldSocketObject::SocketsListToReport );
    
    DldSocketObject::SocketsListLock = IORWLockAlloc();
    assert( DldSocketObject::SocketsListLock );
    if( ! DldSocketObject::SocketsListLock ){
        
        DBG_PRINT_ERROR(("DldSocketObject::SocketsListLock = IORWLockAlloc() failed\n"));
        return ENOMEM;
    }
    
    DldSocketObject::SocketsListToReportLock = IORWLockAlloc();
    assert( DldSocketObject::SocketsListToReportLock );
    if( ! DldSocketObject::SocketsListToReportLock ){
        
        DBG_PRINT_ERROR(("DldSocketObject::SocketsListToReportLock = IORWLockAlloc() failed\n"));
        return ENOMEM;
    }
    
    errno_t    error;
    thread_t   thread;
    
    error = kernel_thread_start ( ( thread_continue_t ) &DldSocketObject::InjectionThreadRoutine,
                                  NULL,
                                  &thread );
	assert( KERN_SUCCESS == error );
    if ( KERN_SUCCESS != error ){
        
        return error;
    }
    
    //
    // release the thread object
    //
    thread_deallocate( thread );
    
    DldSocketObject::Initialized = true;
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

void
DldSocketObject::InjectionThreadRoutine( void* context )
{
    do{
        
        //
        // sleep with a timeout
        //
        struct timespec ts = { 1, 0 };       // one second
        int    dummyEvent;
        
        (void)msleep( &dummyEvent,                  // wait channel
                      NULL,                          // mutex
                      PUSER,                         // priority
                      "DldSocketObject::InjectionThreadRoutine()", // wait message
                      &ts );                         // sleep interval
        
    } while( ! gSocketFilter );
    
#ifdef _DLD_SOCKET_FILTER_USER_EMULATION
    IOOptionBits         options;
    IOMemoryDescriptor*  memoryDescriptor;
    IOUserClient*        userClient;
    
    do{
        
        userClient = gSocketFilter->getUserClient();
        
        //
        // sleep with a timeout
        //
        struct timespec ts = { 1, 0 };       // one second
        int    dummyEvent;
        
        (void)msleep( &dummyEvent,                  // wait channel
                      NULL,                          // mutex
                      PUSER,                         // priority
                      "DldSocketObject::InjectionThreadRoutine()", // wait message
                      &ts );                         // sleep interval
        
    } while( ! userClient );
        
    // a test, disregard errors
    IOReturn RC = userClient->clientMemoryForType( kt_DldNotifyTypeSocketFilter, &options, &memoryDescriptor );
    assert( kIOReturnSuccess == RC );
    
    IOMemoryMap*  map = memoryDescriptor->map();
    assert( map );
    
    IODataQueueMemory*  dataQueue = (IODataQueueMemory*)map->getVirtualAddress();
    assert( dataQueue );
    
#endif // _DLD_SOCKET_FILTER_USER_EMULATION
    
    while( true ){
        
        TAILQ_HEAD( DldSocketsListHead, DldSocketObject ) localSocketsList;
        TAILQ_INIT( &localSocketsList );
        
#ifdef _DLD_SOCKET_FILTER_USER_EMULATION
        
#error "this is an emulation for a user space notification acceptor, should not be used if a user space acceptor is present"

        DldSocketFilterNotification notification;
        UInt32   dataSize = sizeof( notification );
        
        while( kIOReturnSuccess == IODataQueueDequeue( dataQueue, &notification, &dataSize ) ){
            
            assert( sizeof( notification ) == dataSize );
            
            //
            // release buffers
            //
            
            if( DldSocketFilterEventDataIn == notification.event || DldSocketFilterEventDataOut == notification.event ){
                
                gSocketFilter->releaseDataBuffersAndDeliverNotifications( notification.eventData.inputoutput.buffers );
            }
            
            dataSize = sizeof( notification );
            
        } // end while
        
#endif // _DLD_SOCKET_FILTER_USER_EMULATION
        
        IORWLockRead( DldSocketObject::SocketsListLock );
        { // start of the lock
            
            DldSocketObject*  sockObj;
            
            TAILQ_FOREACH( sockObj, &DldSocketObject::SocketsList, socketListEntry)
            {
                assert( sockObj->flags.insertedInSocketsList );
                
                if( TAILQ_EMPTY( &sockObj->pendingQueue ) )
                    continue;
                
                //
                // so there is some deferred data and the socket is not closed,
                // retain the object and add it to the local list
                //
                sockObj->retain();
                
                TAILQ_INSERT_TAIL( &localSocketsList, sockObj, injectionSocketListEntry );
                
            } // end TAILQ_FOREACH
            
            if( sockObj )
                sockObj->retain();
            
        } // end of the lock
        IORWLockUnlock( DldSocketObject::SocketsListLock );
        
        while( ! TAILQ_EMPTY( &localSocketsList ) ){
            
            DldSocketObject*  sockObj;
            
            sockObj = TAILQ_FIRST( &localSocketsList );
            TAILQ_REMOVE( &localSocketsList, sockObj, injectionSocketListEntry );
            
            //
            // inject all packets, the function acquires the detach lock so there is no need
            // to acquire it to be sure that the socket is valid
            //
            sockObj->reinjectDeferredData( DldSocketDataAll );
            
            /*
            if( sockObj->acquireDetachingLock() ){
                
                int reserve = 0x1; // 0x0 is invalid value, EINVAL is returned
                
                sock_setsockopt( sockObj->so, SOL_SOCKET, SO_RCVBUF, &reserve, sizeof(reserve));
                sockObj->releaseDetachingLock();
            }
             */
            
            sockObj->release();
            
        } // end while
        
        //
        // sleep with a timeout
        //
        struct timespec ts = { 1, 0 };       // one second
        int    dummyEvent;
        
        (void)msleep( &dummyEvent,                  // wait channel
                      NULL,                          // mutex
                      PUSER,                         // priority
                      "DldSocketObject::InjectionThreadRoutine()", // wait message
                      &ts );                         // sleep interval
        
        //
        // TO DO - move to the watchdog thread
        //
        DldSocketObject::DeliverWaitingNotifications();
        
    } // end while
    
#ifdef _DLD_SOCKET_FILTER_USER_EMULATION
    map->release();
    memoryDescriptor->release();
    gSocketFilter->releaseUserClient();
#endif // _DLD_SOCKET_FILTER_USER_EMULATION
}

//--------------------------------------------------------------------

void DldSocketObject::RemoveSocketObjectsSubsystem()
{
    assert( 0x0 == DldSocketObject::SocketObjectsCounter );
    
    if( DldSocketObject::SocketsListLock ){
        
        IORWLockFree( DldSocketObject::SocketsListLock );
        DldSocketObject::SocketsListLock = NULL;
    }
    
    if( DldSocketObject::SocketsListToReportLock ){
        
        IORWLockFree( DldSocketObject::SocketsListToReportLock );
        DldSocketObject::SocketsListToReportLock = NULL;
    }
    
    if (gOSMallocTag)
    {
        OSMalloc_Tagfree(gOSMallocTag);
        gOSMallocTag = NULL;
    }
}

//--------------------------------------------------------------------

bool DldSocketObject::init()
{
    assert( DldSocketObject::Initialized );
    
    TAILQ_INIT( &this->pendingQueue );
    TAILQ_INIT( (DldSocketsListHead*)&this->socketListEntry );
    
    if( ! super::init() ){
        
        DBG_PRINT_ERROR(("super::init() failed"));
        return false;
    }
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( ! this->rwLock ){
        
        DBG_PRINT_ERROR(("this->rwLock = IORWLockAlloc() failed\n"));
        return false;
    }
    
    this->injectionMutex = IOLockAlloc();
    assert( this->injectionMutex );
    if( ! this->injectionMutex ){
        
        DBG_PRINT_ERROR(("this->injectionMutex = IOLockAlloc() failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

/*
 an example for a free function callstack
 #0  DldSocketObject::free (this=0xba6b200) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:190
 #1  0x004fbdbe in OSObject::taggedRelease (this=0xba6b200, tag=0x0) at /SourceCache/xnu/xnu-1504.7.4/libkern/c++/OSObject.cpp:183
 #2  0x004fbdd9 in OSObject::release (this=0xba6b200) at /SourceCache/xnu/xnu-1504.7.4/libkern/c++/OSObject.cpp:255
 #3  0x471715fc in DldSocketFilter::FltDetach (cookie=0xba6b200, so=0x5d9bc84) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:364
 #4  0x4717163a in DldSocketFilter::FltDetachIPv4 (cookie=0xba6b200, so=0x5d9bc84) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:374
 #5  0x004c3df0 in sflt_detach_private (entry=0x630eaa4, unregistering=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:381
 #6  0x004c402f in sflt_termsock (so=0x5d9bc84) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:92
 #7  0x004ab889 in sofreelastref (so=0x5d9bc84, dealloc=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:796
 #8  0x004aba19 in sofree (so=0x5d9bc84) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:3993
 #9  0x004ad5fa in soclose_locked (so=0x5d9bc84) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:1019
 #10 0x004ad646 in soclose (so=0x5d9bc84) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:1033
 #11 0x004696fc in fo_close [inlined] () at :4869
 #12 0x004696fc in closef_finish [inlined] () at :4068
 #13 0x004696fc in closef_locked (fp=0x6782210, fg=0x832f478, p=0x6781a80) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:4167
 #14 0x0046b4ad in close_internal_locked (p=0x6781a80, fd=42, fp=0x6782210, flags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:1950
 #15 0x0046b57e in close_nocancel (p=0x6781a80, uap=0x6d33c90, retval=0x6d33cd4) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:1852
 #16 0x004ed78d in unix_syscall (state=0x66825e0) at /SourceCache/xnu/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:205
 */

void DldSocketObject::free()
{
    assert( TAILQ_EMPTY( &this->pendingQueue ) );
    assert( 0x0 == this->flags.insertedInSocketsList );
    assert( DldSocketObject::SocketObjectsCounter > 0x0 );
    assert( ! this->insertedInSocketsListToReport );
    assert( 0x0 == this->packetsWaitingForReporting );
    
    if( this->injectionMutex )
        IOLockFree( this->injectionMutex );
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
    OSDecrementAtomic( &DldSocketObject::SocketObjectsCounter );
    
    super::free();
}

//--------------------------------------------------------------------

DldSocketObject* DldSocketObject::withSocket(
    __in socket_t so,
    __in mbuf_tag_id_t gidtag,
    __in sa_family_t sa_family
    )
{
    DldSocketObject*   socketObj;
    
    socketObj = new DldSocketObject();
    assert( socketObj );
    if( ! socketObj ){
        
        DBG_PRINT_ERROR(("socketObj = new DldSocketObject() failed\n"));
        return NULL;
    }
    
    OSIncrementAtomic( &DldSocketObject::SocketObjectsCounter );
    
    if( ! socketObj->init() ){
        
        DBG_PRINT_ERROR(("socketObj->init() failed\n"));
        socketObj->release();
        return NULL;
    }
    
    socketObj->sa_family = sa_family;
    socketObj->socket = so;
    socketObj->gidtag = gidtag;
    socketObj->lockCount = 0x1;
    socketObj->capturingMode = DldCapturingModeAll; // by default capture all new connections' data
    socketObj->socketId.socket = (UInt64)so;
    socketObj->socketId.socketSequence = OSIncrementAtomic( &DldSocketObject::gSocketSequence );
    
    //
    // get the receive buffer size,
    // TO DO - intercept the buffer change in the events callback(!)
    //
    int  optlen = sizeof( socketObj->origReceiveBufferSize );
    errno_t error = sock_getsockopt( so,
                                     SOL_SOCKET,
                                     SO_RCVBUF,
                                     &socketObj->origReceiveBufferSize,
                                     &optlen);
    assert( 0x0 == error && 0x0 != socketObj->origReceiveBufferSize );
    
#if defined( DBG )
    socketObj->signature = SOCKET_OBJECT_SIGNATURE;
#endif // DBG
    
    return socketObj;
}

//--------------------------------------------------------------------

void
DldSocketObject::insertInSocketsList()
{
    assert( 0x0 == this->flags.insertedInSocketsList );
    assert( DldSocketObject::SocketObjectsCounter > 0x0 );
    assert( preemption_enabled() );
    assert( ! DldIsSocketObjectInList( this->socket ) );
    
    this->LockExclusive();
    IORWLockWrite( DldSocketObject::SocketsListLock );
    { // start of the lock
        
        if( 0x0 == this->flags.insertedInSocketsList ){
            
            TAILQ_INSERT_HEAD( &DldSocketObject::SocketsList,
                               this,
                               socketListEntry );
            
            //
            // inserted in the list of sockets,
            // all objects in the list are retained
            //
            this->flags.insertedInSocketsList = 0x1;
            this->retain();
            
        } // end if( 0x0 == this->flags.insertedInSocketsList )
        
    } // end of the lock
    IORWLockUnlock( DldSocketObject::SocketsListLock );
    this->UnlockExclusive();
}

//--------------------------------------------------------------------

void
DldSocketObject::removeFromSocketsList()
{
    assert( this->flags.insertedInSocketsList );
    assert( DldSocketObject::SocketObjectsCounter > 0x0 );
    assert( preemption_enabled() );
    
    bool  wasInSocketsList = false;
    bool  wasInSocketsToReportList = false;
    
    //
    // the flags are under the socket lock protection while the list entry is protected by its own lock
    //
    this->LockExclusive();
    IORWLockWrite( DldSocketObject::SocketsListLock );
    { // start of the lock
        
        if( this->flags.insertedInSocketsList ){
            
            TAILQ_REMOVE( &DldSocketObject::SocketsList,
                          this,
                          socketListEntry );
            
            this->flags.insertedInSocketsList = 0x0;
            wasInSocketsList = true;
        }
        
    } // end of the lock
    IORWLockUnlock( DldSocketObject::SocketsListLock );
    this->UnlockExclusive();
    
    
    IORWLockWrite( DldSocketObject::SocketsListToReportLock );
    { // start of the lock
        
        if( this->insertedInSocketsListToReport ){
            
            TAILQ_REMOVE( &DldSocketObject::SocketsListToReport, this, socketsListToReportEntry );
            this->insertedInSocketsListToReport = false;
            wasInSocketsToReportList = true;
        } // end if( ! TAILQ_EMPTY( &DldSocketObject::SocketsListToReport ) )
    }// end of the lock
    IORWLockUnlock( DldSocketObject::SocketsListToReportLock );
    
    //
    // ATTENTION! after relising the this pointer might be invalid!
    // Do not touch the object past this point!
    //
    
    //
    // the list retained the object
    //
    if( wasInSocketsList )
        this->release();
    
    if( wasInSocketsToReportList )
        this->release();
}

//--------------------------------------------------------------------

DldSocketObject*
DldSocketObject::GetSocketObjectRef( __in socket_t so )
/*
 if the value is not known provide NULL for so and INVALID_SOCKET_HANDLE for handle,
 a caller must release the returned object
 */
{
    assert( NULL != so );
    
    DldSocketObject*  sockObj;
    
    IORWLockRead( DldSocketObject::SocketsListLock );
    { // start of the lock
        
        TAILQ_FOREACH( sockObj, &DldSocketObject::SocketsList, socketListEntry)
        {
            assert( sockObj->flags.insertedInSocketsList );
            
            if( sockObj->socket == so )
                break;
        } // end TAILQ_FOREACH
        
        if( sockObj ){
            
            assert( 0x0 != sockObj->socketId.socketSequence );
            assert( (UInt64)so ==  sockObj->socketId.socket );
            sockObj->retain();
        }
        
    } // end of the lock
    IORWLockUnlock( DldSocketObject::SocketsListLock );
        
    return sockObj;
}

//--------------------------------------------------------------------

void DldSocketObject::LockShared()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
    
    assert( NULL == this->rwLockExclusiveThread );
    
}

void DldSocketObject::UnlockShared()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    assert( NULL == this->rwLockExclusiveThread );
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

void DldSocketObject::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined( DBG )
    assert( current_thread() != this->rwLockExclusiveThread );
#endif//DBG
    
    IORWLockWrite( this->rwLock );
    
#if defined( DBG )
    this->rwLockExclusiveThread = current_thread();
#endif//DBG
    
}

void DldSocketObject::UnlockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined( DBG )
    assert( current_thread() == this->rwLockExclusiveThread );
    this->rwLockExclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

void DldSocketObject::logSocketPreEvent( __in sflt_event_t event )
{
    
    assert( preemption_enabled() );
    
    this->LockExclusive();
    { // start of the lock
        
        switch( event ){
            case sock_evt_connecting:
                this->flags.f_sock_evt_pre_connecting = 0x1;
                break;
            case sock_evt_connected:
                this->flags.f_sock_evt_pre_connected = 0x1;
                break;
            case sock_evt_disconnecting:
                this->flags.f_sock_evt_pre_disconnecting = 0x1;
                break;
            case sock_evt_disconnected:
                this->flags.f_sock_evt_pre_disconnected = 0x1;
                //__asm__ volatile( "int $0x3" );
                break;
            case sock_evt_flush_read:
                this->flags.f_sock_evt_pre_flush_read = 0x1;
                break;
            case sock_evt_shutdown:
                this->flags.f_sock_evt_pre_shutdown = 0x1;
                break;
            case sock_evt_cantrecvmore:
                this->flags.f_sock_evt_pre_cantrecvmore = 0x1;
                break;
            case sock_evt_cantsendmore:
                this->flags.f_sock_evt_pre_cantsendmore = 0x1;
                break;
            case sock_evt_closing:
                this->flags.f_sock_evt_pre_closing = 0x1;
                break;
            case sock_evt_bound:
                this->flags.f_sock_evt_bound = 0x1;
                break;
        }
    } // start of the lock
    this->UnlockExclusive();
}

//--------------------------------------------------------------------

void DldSocketObject::logSocketPostEvent( __in sflt_event_t event )
{
    
    assert( preemption_enabled() );
    
    this->LockExclusive();
    { // start of the lock
        #if DBG
        this->eventsLog[ this->nextEventsLogPosition ] = event;
        this->nextEventsLogPosition = (++this->nextEventsLogPosition) % SO_EVENTS_LOG_SIZE;
        #endif//DBG
        
        switch( event ){
            case sock_evt_connecting:
                this->flags.f_sock_evt_connecting = 0x1;
                break;
            case sock_evt_connected:
                this->flags.f_sock_evt_connected = 0x1;
                break;
            case sock_evt_disconnecting:
                this->flags.f_sock_evt_disconnecting = 0x1;
                break;
            case sock_evt_disconnected:
                this->flags.f_sock_evt_disconnected = 0x1;
                //__asm__ volatile( "int $0x3" );
                break;
            case sock_evt_flush_read:
                this->flags.f_sock_evt_flush_read = 0x1;
                break;
            case sock_evt_shutdown:
                this->flags.f_sock_evt_shutdown = 0x1;
                break;
            case sock_evt_cantrecvmore:
                this->flags.f_sock_evt_cantrecvmore = 0x1;
                break;
            case sock_evt_cantsendmore:
                this->flags.f_sock_evt_cantsendmore = 0x1;
                break;
            case sock_evt_closing:
                this->flags.f_sock_evt_closing = 0x1;
                break;
            case sock_evt_bound:
                this->flags.f_sock_evt_bound = 0x1;
                break;
        }
    } // start of the lock
    this->UnlockExclusive();
}

//--------------------------------------------------------------------

/*
 my_mbuf_freem is implemented to deal with the fact that the data_in and data_out functions are passed
 mbuf_t* parameters instead of mbuf_t parameters. The mbuf_freem routine can handle a null parameter, but
 the kext has to deal with the fact that it could be passed a NULL *mbuf_t parameter (normally the control
 parameter). 
 */
static void my_mbuf_freem( __inout mbuf_t *mbuf )
{
	if (mbuf != NULL)
	{
		if (*mbuf != NULL)
		{
			mbuf_freem(*mbuf);
		}
	}
}

//--------------------------------------------------------------------

static SInt32   gPendingDataNextIndex = 0x0; // only for debug, to have a unique ID for each packet

DldSocketObject::PendingPktQueueItem*
DldSocketObject::allocatePkt(
    __in mbuf_t mbufData,
    __in mbuf_t mbufControl,
    __in sflt_data_flag_t flags,
    __in bool   isInboundData
    )
{
    PendingPktQueueItem*  pkt;
    
    assert( preemption_enabled() );
    assert( mbufData || mbufControl );
    
    if( !mbufData && !mbufControl ){
        
        DBG_PRINT_ERROR(("mbufData and mbufControl are both NULL\n"));
        return NULL;
    }
    
    //
    // use new OSMalloc call which makes it easier to track memory allocations under Tiger
    //
    assert( DldSocketObject::gOSMallocTag );
    pkt = (DldSocketObject::PendingPktQueueItem*)OSMalloc( sizeof(*pkt), DldSocketObject::gOSMallocTag );
    assert( pkt );
    if( ! pkt )
        return NULL;

    bzero( pkt, sizeof( *pkt ) );
    
    uint32_t totalbytes = 0x0;
    
    if( mbufData )
        totalbytes += mbuf_pkthdr_len( mbufData );
    
    if( mbufControl )
        totalbytes += mbuf_pkthdr_len( mbufControl );
    
    // it is quite possible to observe a zero length data
    // assert( 0x0 != totalbytes );
    
    if( isInboundData ){
        OSIncrementAtomic( &this->numberOfPendingInPackets );		// increment packet count
        OSAddAtomic( totalbytes, &this->totalPendingBytesIn );
    } else {
        OSIncrementAtomic( &this->numberOfPendingOutPackets );		// increment packet count
        OSAddAtomic( totalbytes, &this->totalPendingBytesOut );
    }
    
    //
    // initialize the pending structure
    //
    pkt->allocationTime = mach_absolute_time();
    pkt->sflt_flags = flags;
    pkt->data = mbufData;
    pkt->control = mbufControl;
    pkt->totalbytes = totalbytes;
    pkt->dataInbound = isInboundData;
#if DBG
    pkt->dataIndex = OSIncrementAtomic( &gPendingDataNextIndex );
#else
    pkt->dataIndex = OSIncrementAtomic( &this->pendingDataNextIndex ); // OSIncrementAtomic returns the value before increment
#endif
    
    return pkt;
}

//--------------------------------------------------------------------

/*
    @typedef DldSocketObject::FltDataOut

    @discussion sf_data_out_func is called to filter outbound data. If
        your filter intercepts data for later reinjection, it must queue
        all outbound data to preserve the order of the data when
        reinjecting. Use sock_inject_data_out to later reinject this
        data. Warning: This filter is on the data path, do not block or
        spend excessive time.
    @param so The socket the filter is attached to.
    @param to The address the data is being sent to, may be NULL if the socket
        is connected.
    @param data The data being received. Control data may appear in the
        mbuf chain, be sure to check the mbuf types to find control
        data.
    @param control Control data being passed separately from the data.
    @param flags Flags to indicate if this is out of band data or a
        record.
    @result Return:
        0 - The caller will continue with normal processing of the data.
        EJUSTRETURN - The caller will stop processing the data, the data will not be freed.
        Anything Else - The caller will free the data and stop processing.

    Note: as this is a TCP connection, the "to" parameter will be NULL - for UDP, the
        "to" field will point to a valid sockaddr structure. In this case, you must copy
        the contents of the "to" field to local memory when swallowing the packet so that
        you have a valid sockaddr to pass in the inject call.
*/
errno_t	
DldSocketObject::FltDataOut(
    __in socket_t so,
    __in const struct sockaddr *to,
    __inout mbuf_t *data,
    __inout mbuf_t *control,
    __in sflt_data_flag_t flags
    )
{
    return FltData( so, to, data, control, flags, DldSocketDataDirectionOut );
}

//--------------------------------------------------------------------

/* 
    @typedef FltDataIn
 
    @discussion FltDataIn is called to filter incoming data. If
        your filter intercepts data for later reinjection, it must queue
        all incoming data to preserve the order of the data. Use
        sock_inject_data_in to later reinject this data if you return
        EJUSTRETURN. Warning: This filter is on the data path, do not
        block or spend excessive time.
    @param so The socket the filter is attached to.
    @param from The addres the data is from, may be NULL if the socket
        is connected.
    @param data The data being received. Control data may appear in the
        mbuf chain, be sure to check the mbuf types to find control
        data.
    @param control Control data being passed separately from the data.
    @param flags Flags to indicate if this is out of band data or a
        record.
    @result Return:
        0 - The caller will continue with normal processing of the data.
        EJUSTRETURN - The caller will stop processing the data, the data will not be freed.
        Anything Else - The caller will free the data and stop processing.
 
    Note: as this is a TCP connection, the "from" parameter will be NULL - for UDP, the
    "from" field will point to a valid sockaddr structure. In this case, you must copy
    the contents of the "from" field to local memory when swallowing the packet so that
    you have a valid sockaddr to pass in the inject call.
 */
errno_t
DldSocketObject::FltDataIn(
    __in socket_t so,
    __in const struct sockaddr *from,
    __inout mbuf_t *data,
    __inout mbuf_t *control,
    __in sflt_data_flag_t flags
    )
{
    return FltData( so, from, data, control, flags, DldSocketDataDirectionIn ); 
}

//--------------------------------------------------------------------

/*
 
    Notes for both the FltDataIn and the FltDataOut implementations.
    For these functions, the mbuf_t parameter is passed by reference. The kext can 
    manipulate the mbuf such as prepending an mbuf_t, splitting the mbuf and saving some
    tail portion of data, etc. As a reminder, you are responsible to ensure that
    data is processed in the correct order that is is received. If the kext splits the 
    mbuf_t, and returns the lead portion of data, then return KERN_SUCCESS, even though
    the nke swallows the tail portion.
 
    @typedef DldSocketObject::FltData
 
    @discussion sf_data_out_func is called to filter outbound/inbound data. If
        your filter intercepts data for later reinjection, it must queue
        all outbound data to preserve the order of the data when
        reinjecting. Use sock_inject_data_out to later reinject this
        data. Warning: This filter is on the data path, do not block or
        spend excessive time.
    @param cookie Cookie value specified when the filter attach was
        called.
    @param so The socket the filter is attached to.
    @param addr The address the data is from OR being sent to, may be NULL if the socket
        is connected.
    @param data The data being received. Control data may appear in the
        mbuf chain, be sure to check the mbuf types to find control
        data.
    @param control Control data being passed separately from the data.
    @param flags Flags to indicate if this is out of band data or a
        record.
    @result Return:
        0 - The caller will continue with normal processing of the data.
        EJUSTRETURN - The caller will stop processing the data, the data will not be freed.
        Anything Else - The caller will free the data and stop processing.
 
    Note: as this is a TCP connection, the "to" parameter will be NULL - for UDP, the
    "to" field will point to a valid sockaddr structure. In this case, you must copy
    the contents of the "to" field to local memory when swallowing the packet so that
    you have a valid sockaddr to pass in the inject call.
 
 an example of a call stack for input data
 #0  0x470d3ced in DldSocketObject::FltData (this=0xb72ab00, so=0x5dacc24, addr=0x0, data=0x2c1ebaa4, control=0x0, flags=0, direction=DldSocketDataDirectionIn) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:1043
 #1  0x470d429a in DldSocketObject::FltDataIn (this=0xb72ab00, so=0x5dacc24, from=0x0, data=0x2c1ebaa4, control=0x0, flags=0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:828
 #2  0x470d1079 in DldSocketFilter::FltDataIn (cookie=0xb72ab00, so=0x5dacc24, from=0x0, data=0x2c1ebaa4, control=0x0, flags=0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:627
 #3  0x004c4421 in sflt_data_in (so=0x5dacc24, from=0x0, data=0x2c1ebaa4, control=0x0, flags=0, filtered=0x2c1eba2c) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:176
 #4  0x004b0ac5 in sbappendstream (sb=0x5dacc68, m=0x2dcc1b00) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket2.c:724
 #5  0x0034fd57 in tcp_input (m=0x2dcc1b00, off0=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_input.c:1627
 #6  0x003473f6 in ip_proto_dispatch_in (m=0x2dcc1b00, hlen=20, proto=6 '\006', inject_ipfref=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:613
 #7  0x00348a95 in ip_input (m=0x2dcc1b00) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:1372
 #8  0x00348bbc in ip_proto_input (protocol=2, packet_list=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:519
 #9  0x0032f121 in proto_input (protocol=2, packet_list=0x2dcc1b00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/kpi_protocol.c:298
 #10 0x0031b7c1 in ether_inet_input (ifp=0x5e9e404, protocol_family=2, m_list=0x2dcc1b00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/ether_inet_pr_module.c:215
 #11 0x00317a3c in dlil_ifproto_input (ifproto=0x63fa384, m=0x2dcc1b00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1079
 #12 0x0031a1b4 in dlil_input_packet_list (ifp_param=0x0, m=0x2dcc1b00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1197
 #13 0x0031a422 in dlil_input_thread_func (inputthread=0x5e7ab84) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:878 
 */

errno_t	
DldSocketObject::FltData(
    __in socket_t so,
    __in const struct sockaddr *addr, // a peer's address
    __inout mbuf_t *data,
    __inout mbuf_t *control,
    __in sflt_data_flag_t flags,
    __in DldSocketDataDirection  direction
    )
{
	errno_t   error = 0x0;
    bool      isInboundData = ( DldSocketDataDirectionIn == direction );
    DldSocketObject::WaitEntry waitEntry;
    
    assert( preemption_enabled() );
    assert( DldSocketDataDirectionOut == direction || DldSocketDataDirectionIn == direction );
    
    //
    // see description above
    //
    assert( !addr );
	if (addr){
        
		DBG_PRINT_ERROR(("addr field not NULL!\n"));
    }
    
    if( DldCapturingModeNothing == this->capturingMode ){
        
        //
        // there is no need to synchronize with concurrent threads trying to insert data
        // in the pending queue ( i.e. threads that passed this point as the capturing mode was
        // DldCapturingModeAll but have not yet inserted pending packets so the pending packets
        // count might be 0x0 ) because the socket's user must be able to process such scenarious
        // if it issues outbound data concurrently or does not synchronize inbound and outbound
        // data streams
        //
        // we need to synchronize only with ALREADY PENDING data 
        //
        if( DldSocketDataDirectionOut == direction ){
            
            //
            // we can simply wait in the caller's context
            //
            this->checkForInjectionCompletion( DldSocketDataAll, true );
            return 0x0;
            
        } else {
            
            assert( DldSocketDataDirectionIn == direction );
            
            //
            // it is not a good idea to wait in the kernel's input data processing thread that is shared
            // by all connections, so continue injection until we are lucky and processed all deferred data,
            // so check the pending data streams without blocking
            //
            if( this->checkForInjectionCompletion( DldSocketDataAll, false ) ){
                
                //
                // there are no waiting inbound and outbound packets
                //
                return 0x0;
            }
            
            //
            // the pending queues are not empty so continue injection to correctly serialize input and output data streams
            //
        }
        
    } // end if( DldCapturingModeNothing == this->capturingMode )
    
    //
	// check whether we have seen this packet previously 
    //
	if( this->checkTag( data, this->gidtag, DLD_SOCKTAG_ID_TYPE, isInboundData ? DLD_INBOUND_DONE : DLD_OUTBOUND_DONE ) ){
        //
		// we have processed this packet previously since out or in tag was attached.
        // bail on further processing
        //
        DLD_COMM_LOG ( NET_PACKET_FLOW, ("bypass so: 0x%p, mbuf_t: 0x%p , isInboundData = %d\n", so, *data, (int)isInboundData ));
		return 0x0;
	}
	//
    // If we reach this point, then we have not previously seen this packet. 
    // First lets get some statistics from the packet.
    //
    

	DLD_COMM_LOG ( NET_PACKET_FLOW, ("so: 0x%p data: 0x%p pending, bytes %d, isInboundData = %d\n",
                                     so, *data, (int)mbuf_pkthdr_len(*data), (int)isInboundData ));
	
	//
    // If we make the pending packet and later re-inject the packet, we have to be
    //  prepared to see the packet through this routine once again. In fact, if
    //  after re-injecting the packet, another nke swallows and re-injects the packet
    //  we will see the packet an additional time. Rather than cache the mbuf_t reference
    //  we tag the mbuf_t and search for it's presence which we have already done above
    // to decide if we have processed the packet.
    //
	error = this->setTag( data, this->gidtag, DLD_SOCKTAG_ID_TYPE, isInboundData ? DLD_INBOUND_DONE : DLD_OUTBOUND_DONE );
    assert( KERN_SUCCESS == error );
	if( KERN_SUCCESS == error )
	{
        DldSocketObject::PendingPktQueueItem*  pendingPkt;
        
        //
        // normally for output data the both data and control are non NULL, for input the control can be NULL
        //
        pendingPkt = this->allocatePkt( (data ? *data : NULL),
                                        (control ? *control : NULL),
                                        flags,
                                        isInboundData );
        assert( pendingPkt );
		if( NULL != pendingPkt ){
            
            DldSocketFilterNotification     notification;
            bool   wait = false;
            bool   sendNotification = true;
            bool   releaseBuffers = false;
            
            INIT_SOCKET_NOTIFICATION( notification,
                                      this,
                                      isInboundData ? DldSocketFilterEventDataIn : DldSocketFilterEventDataOut );
            
            notification.eventData.inputoutput.dataIndex = pendingPkt->dataIndex;
            
            mbuf_t  mbuf = data ? *data : NULL;
            
            if( mbuf ){
                
                notification.eventData.inputoutput.dataSize = mbuf_pkthdr_len( mbuf );
                
                error = gSocketFilter->copyDataToBuffers( mbuf, notification.eventData.inputoutput.buffers );
                if( error ){
                    
                    assert( UINT8_MAX == notification.eventData.inputoutput.buffers[0] );
                    
                    //
                    // disable the notification sending, the notification will be sent when there will be enough
                    // space to pass the data
                    //
                    sendNotification = false;
                    releaseBuffers = true;
                    
                }
            } // end if( mbuf )
            
			this->LockExclusive();
            { // start of the lock
                //
                // queue the item into the input/output queue for processing
                //
                TAILQ_INSERT_TAIL( &this->pendingQueue, pendingPkt, pendingQueueEntry );
                
                //
                // the following should be done under the exclusive lock(!) AFTER the pending packet
                // has been added to the list, so when the socket descriptor will be discovered
                // in the SocketsListToReport it has at least one pending packet
                //
                // the second check is for a case when the socket has packets that are waiting for reporting,
                // in that case all following packets reporting must be also postponned to preserve the packets order
                //
                pendingPkt->flags.errorWhileAllocatingBuffers = (error) ? 0x1 : 0x0;
                pendingPkt->flags.notReportedEntryInFront = (0x0 != this->packetsWaitingForReporting) ? 0x1 : 0x0;
                if( error || 0x0 != this->packetsWaitingForReporting ){
                    
                    sendNotification = false;
                    releaseBuffers = true;
                    
                    //
                    // if this is an output data then block a caller, in case of input data reduce the socket
                    // receive buffer this results in stopping the TCP window from growing and collapcing eventually
                    // to 0x0, CAVEAT - the window is not shrinked as this breaks some peers instead Mac OS X TCP
                    // implementation stops the window from moving its right side and constatntly reducing it when
                    // data arrives by moving its left side, the result is a collapsed window of 0x0 size
                    //
                    if( DldSocketDataDirectionOut == direction ){
                        
                        //
                        // set the wait entry pointer, when the data will be passed to dissectors the event will be set
                        // in a signal state
                        //
                        waitEntry.waitSatisfied = false;
                        pendingPkt->waitEntry = &waitEntry;
                        wait = true;
                        
                    } else {
                        
                        assert( DldSocketDataDirectionIn == direction );
                        if( this->acquireDetachingLock() ){
                            
                            int     reserve = 0x1; // 0x0 is invalid value, EINVAL is returned
                            errno_t optErr; 
                            
                            //
                            // say the system to start receive window collapsing, when the data will be sent
                            // to dissectors the window size will be restored
                            //
                            optErr = sock_setsockopt( this->socket, SOL_SOCKET, SO_RCVBUF, &reserve, sizeof(reserve));
                            assert( 0x0 == optErr );
                            
                            this->releaseDetachingLock();
                        } // end if( this->acquireDetachingLock() )
                    }
                    
                    pendingPkt->needToBeReported = true;
                    if( 0x0 == OSIncrementAtomic( &this->packetsWaitingForReporting ) ){
                        
                        //
                        // insert in the list of sockets that wait for data to be delivered
                        //
                        IORWLockWrite( DldSocketObject::SocketsListToReportLock );
                        { // start of the lock
                            
                            if( ! this->insertedInSocketsListToReport ){
                                
                                //
                                // all objects in the list are refrenced to avoid premature object destroying
                                //
                                this->retain();
                                
                                //
                                // insert in the list
                                //
                                TAILQ_INSERT_TAIL( &DldSocketObject::SocketsListToReport, this, socketsListToReportEntry );
                                this->insertedInSocketsListToReport = true;
                            } // end if( ! this->insertedInSocketsListToReport )
                            
                        } // end of the lock
                        IORWLockUnlock( DldSocketObject::SocketsListToReportLock );
                    } // if( 0x0 == OSIncrementAtomic( &this->packetsWaitingForReporting ) )
                    
                } // end if( error )
                
                pendingPkt->flags.waitWasAsserted = (wait) ? 0x1 : 0x0;
#if DBG
                this->verifyPendingPacketsQueue( false );
#endif // DBG
                
            } // end of the lock
			this->UnlockExclusive();
            
            //
            // pendingPkt cannot be touched after the lock is released as it can be fetched
            // from the queue and destroyed by a concurrent thread
            //
            DLD_DBG_MAKE_POINTER_INVALID( pendingPkt );
            
            if( sendNotification ){
                
                assert( ! wait );
                
                DldIOUserClient* userClient = gSocketFilter->getUserClient();
                if( userClient ){
                    
                    error = userClient->socketFilterNotification( &notification );
                    gSocketFilter->releaseUserClient();
                    DLD_DBG_MAKE_POINTER_INVALID( userClient );
                    
                } else {
                    
                    error = ENOENT;
                }
                
                if( error ){
                    
                    //
                    // process an error,
                    // release all acquired buffers, TO DO - the process might hang as the queued packet will block the socket calls
                    //
                    releaseBuffers = true;
                    DBG_PRINT_ERROR(("socketFilterNotification() failed\n"));
                    
                    //
                    // TO DO - place the packet in the list of packets waiting for reporting
                    //
                } // end if( error )
            } // end if( sendNotification )
            
            
            if( releaseBuffers ){
                
                gSocketFilter->releaseDataBuffersAndDeliverNotifications( notification.eventData.inputoutput.buffers );
                
            } // end if( releaseBuffers )
            
            
            //
            // wait in a loop with a timeout as there is a window between
            // checking for waitSatisfied and putting a thread on a wait
            // state, a wakeup can be lost when this window is opened
            //
            while( wait && (! waitEntry.waitSatisfied ) ){
                
                assert( ! sendNotification );
                
                struct timespec ts = { 1, 0 };       // one second
                
                (void)msleep( &waitEntry,                   // wait channel
                              NULL,                         // mutex
                              PUSER,                        // priority
                              "DldSocketObject::FltData()", // wait message
                              &ts );                        // sleep interval
            } // while( wait && (! waitEntry.waitSatisfied ) )
			
			error = EJUSTRETURN;
		}
		else // for if( NULL != pendingPkt )
		{
			// see notes above in the data_in function
			DBG_PRINT_ERROR(("failed to allocate memory for queue item, dropping packet, isInboundData = %d\n", (int)isInboundData ));
			error = ENOMEM;
		}
	}
	else
	{
		// see notes above in the data_in function
		DBG_PRINT_ERROR(("mbuf_tag_allocate returned an error %d\n", error));
		error = ENOMEM;
	}
    
	return error;
}

//--------------------------------------------------------------------

//
// a caller should not hold any locks
//
void
DldSocketObject::DeliverWaitingNotifications()
{
    
    assert( preemption_enabled() );
    
    while( ! TAILQ_EMPTY( &DldSocketObject::SocketsListToReport ) ){
        
        DldSocketObject*  sockObj = NULL;
        bool              reinsertInList = false;
        errno_t           error = 0x0;
        
        //
        // REMEMBER! All object in the list are referenced!
        //
        IORWLockWrite( DldSocketObject::SocketsListToReportLock );
        { // start of the lock
            
            if( ! TAILQ_EMPTY( &DldSocketObject::SocketsListToReport ) ){
                
                //
                // remove the first object from the list
                //
                sockObj = TAILQ_FIRST( &DldSocketObject::SocketsListToReport );
                TAILQ_REMOVE( &DldSocketObject::SocketsListToReport, sockObj, socketsListToReportEntry );
                
                assert( sockObj->insertedInSocketsListToReport );
                sockObj->insertedInSocketsListToReport = false;
            } // end if( ! TAILQ_EMPTY( &DldSocketObject::SocketsListToReport ) )
        }// end of the lock
        IORWLockUnlock( DldSocketObject::SocketsListToReportLock );
        
        if( ! sockObj )
            break;
        
        DldSocketFilterNotification     notification;
        bool  releaseBuffers = false;
        
        //
        // send notifications, do this under the lock to avoid pending packet and mbuf destroying
        //
        sockObj->LockExclusive();
        { // start of the lock
            
            DldSocketObject::PendingPktQueueItem*	pendingPkt;
            
            //
            // iterate the queue looking for matching entries,
            // TO DO - it might be more optimal to have a separate list for these entries
            // instead wondering from the back skipping reported entries
            //
            TAILQ_FOREACH( pendingPkt, &sockObj->pendingQueue, pendingQueueEntry)
            {
                //
                // if releaseBuffers is true then the previous notification failed but instead
                // of breaking the cycle has been continued, this is a severe logic error
                // that results in buffers acquired forever
                //
                assert( ! releaseBuffers );
                
                //
                // should the packet be reported?
                // all packtes that require reporting are at the tail of the list
                //
                if( ! pendingPkt->needToBeReported )
                    continue;
                
                assert( sockObj->packetsWaitingForReporting > 0x0 );
                assert( pendingPkt->data );
                
                if( pendingPkt->data ){
                    
                    //
                    // now report the packet
                    //
                    
                    INIT_SOCKET_NOTIFICATION( notification,
                                              sockObj,
                                              ( pendingPkt->dataInbound ) ? DldSocketFilterEventDataIn : DldSocketFilterEventDataOut );
                    
                    notification.eventData.inputoutput.dataIndex = pendingPkt->dataIndex;
                    notification.eventData.inputoutput.dataSize = mbuf_pkthdr_len( pendingPkt->data );
                    
                    //
                    // copy data to communication buffers
                    //
                    error = gSocketFilter->copyDataToBuffers( pendingPkt->data, notification.eventData.inputoutput.buffers );
                    if( 0x0 == error ){
                        
                        //
                        // notify the client
                        //
                        DldIOUserClient* userClient = gSocketFilter->getUserClient();
                        if( userClient ){
                            
                            error = userClient->socketFilterNotification( &notification );
                            gSocketFilter->releaseUserClient();
                            DLD_DBG_MAKE_POINTER_INVALID( userClient );
                            
                        } else {
                            
                            error = ENOENT;
                        }
                        
                        if( error ){
                            
                            //
                            // process an error,
                            // release all acquired buffers,
                            //
                            // there is a subtle moment - there will be a deadlock if releaseDataBuffersAndDeliverNotifications() is called
                            // with the socket object lock acquired for write and then try to reacquire the same lock,
                            // so postpone the releaseDataBuffers call
                            //
                            releaseBuffers = true;
                        }
                    } // end if( 0x0 == error )
                    
                    if( error ){
                        
                        //
                        // error, something went wrong or there was not enough buffers
                        //
                        assert( UINT8_MAX == notification.eventData.inputoutput.buffers[0] );
                    } // end if( error )
                    
                } // end if( pendingPkt->data )
                
                
                if( error ){
                    
                    //
                    // the packet reporting failed,
                    // the buffers pool has been depleted,
                    // reinsert in the list if the deadline has not been reached
                    //
                    if( pendingPkt->isDeadlineTimerExpired() ){
                        
                        //
                        // disable the packet delivery
                        //
                        assert( pendingPkt->deadlineTimerExpired );
                        pendingPkt->allowData = false;
                        
                    } else {
                        
                        reinsertInList = true;
                        break;
                    }

                } // end if( error )
                
                //
                // the packet was reported, decrease the waiting packet counter
                //
                pendingPkt->needToBeReported = false;
                OSDecrementAtomic( &sockObj->packetsWaitingForReporting );
                
                //
                // wake up a waiting thread in case of outbound data or restore
                // the receive windows size for incoming traffic
                //
                if( pendingPkt->waitEntry ){
                    
                    assert( ! pendingPkt->dataInbound );
                    assert( false == pendingPkt->waitEntry->waitSatisfied );
                    
                    pendingPkt->waitEntry->waitSatisfied = true;
                    wakeup( pendingPkt->waitEntry );
                    pendingPkt->waitEntry = NULL;
                } // end if( pendingPkt->waitEntry )
                
                //
                // set the socket receive buffer, this implicitly allows the TCP to move the right window boundary
                //
                if( pendingPkt->dataInbound ){
                    
                    errno_t sockErr;
                    
                    sockErr = sock_setsockopt( sockObj->socket,
                                               SOL_SOCKET,
                                               SO_RCVBUF,
                                               &sockObj->origReceiveBufferSize,
                                               sizeof(sockObj->origReceiveBufferSize));
                    assert( ! sockErr );
                    if( sockErr ){
                        
                        DBG_PRINT_ERROR(( "restoring the socket receive buffer has failed for so=0x%p, error=%d\n",
                                          sockObj->socket, sockErr));
                    }
                } // end if( pendingPkt->dataInbound )
                
#if DBG
                { // start of the test
                    //
                    // check that all packets that arrives before the last reported have also been reported
                    // and that all packets that have arrived after have not been reported
                    //
                    DldSocketObject::PendingPktQueueItem*	pendingPktTmp;
                    bool  needToBeReported = false;
                    
                    TAILQ_FOREACH( pendingPktTmp, &sockObj->pendingQueue, pendingQueueEntry)
                    {
                        
                        if( pendingPkt == pendingPktTmp ){
                            
                            needToBeReported = true;
                            continue;
                        }
                        
                        assert( needToBeReported == pendingPktTmp->needToBeReported );
                        assert( !( false == needToBeReported && NULL != pendingPktTmp->waitEntry ) );
                    } // end TAILQ_FOREACH_REVERSE
                } // end of the test
#endif // DBG
            } // end TAILQ_FOREACH
            
#if DBG
            sockObj->verifyPendingPacketsQueue( false );
#endif // DBG
        } // end of the lock
        sockObj->UnlockExclusive();
        
        //
        // release buffers before reinserting the socket object in the list - this breaks the infinite
        // recursion condition when releaseDataBuffersAndDeliverNotifications calls DeliverWaitingNotifications for the same
        // objects until a call stack is depleted, the watchdog thread should periodically call
        // DeliverWaitingNotifications to circumvent a situation when some buffers were freed before
        // the call to releaseDataBuffersAndDeliverNotifications is made here
        //
        if( releaseBuffers ){
            
            assert( error );
            
            gSocketFilter->releaseDataBuffersAndDeliverNotifications( notification.eventData.inputoutput.buffers );
            releaseBuffers = false;
        }
        
        if( reinsertInList ){
            
            IORWLockWrite( DldSocketObject::SocketsListToReportLock );
            { // start of the lock
                
                reinsertInList = (! sockObj->insertedInSocketsListToReport);
                
                if( reinsertInList ){
                    
                    //
                    // object already has a bumped reference as it was removed from the list
                    //
                    
                    //
                    // insert in the list's tail to give a chance to other socket objects
                    //
                    TAILQ_INSERT_TAIL( &DldSocketObject::SocketsListToReport, sockObj, socketsListToReportEntry );
                    sockObj->insertedInSocketsListToReport = true;
                } // end if( ! this->insertedInSocketsListToReport )
                
            } // end of the lock
            IORWLockUnlock( DldSocketObject::SocketsListToReportLock );
            
        } // end if( reinsertInList )
        
        //
        // as reinsertInList might be changed by the above code you cannot use "else" here
        //
        if( ! reinsertInList ){
            
            //
            // the object was referenced when it was inserted in the list
            //
            sockObj->release();
            DLD_DBG_MAKE_POINTER_INVALID( sockObj );
        } // end if( ! reinsertInList )
        
        //
        // if there was an erro then the buffers pool has been depleted, stop notification delivering
        //
        if( error )
            break;
        
    } // end while( ! TAILQ_EMPTY( &DldSocketObject::SocketsListToReport ) )
}

//--------------------------------------------------------------------

/*
    ReinjectDeferredData is used to scan the swallow_queue for packets to reinject in the stack.

    Note: there is a potential timing issue. If the user forces the kext to be unloaded in the middle of 
        a data transfer, the kext will not get the normal notify messages - sock_evt_disconnecting and
        sock_evt_disconnected. As such, the system will go straight to calling the detach_fn. It could be
        that the data_timer has just fired and the ReinjectDeferredData routine has managed to move 
        packets from the swallow_queue to the packets_to_inject. If the detach_fn is called at this point,
        it can clear the packets that might have been moved into the swallow queue via the FreeDeferredData call
        above, but packets in the packets_to_inject queue will not be freed and will cause the 
        sock_inject_data_in/out functions to be called using a potentially invalid socket reference.
        For this reason, there is code in the ReinjectDeferredData and in the detach_fn to check
        whether there are still packet to be processed. If this occurs, a semaphore like system using msleep and
        wakeup will allow the detach_fn to wait until ReinjectDeferredData has completed processing for
        data on the socket_t ref that is being detached.
 
    Note: the use of the packets_to_inject queue is to make it so that the queue lock does not need to
        held across the sock_inject_data_in/out calls. The queue lock is only held while moving packets
        from the pending queue to the local packets_to_inject queue. Then the lock is released. 
        A lock should never be held across system calls since one never knows whether the same lock will be accessed 
        within the system call.
 
 CAVEAT! Packets that are not reported will not be injected by this routine!
 
 the sock_inject_data_in routine acquires the socket mutex internally, so this it should not be called under
 any other lock acquired inside any callback that is called with a socket mutex held as this reverses the lock
 hierarchy( most of the callbacks are called with the socket mutex being held ) and will lead to a deadlock
 
 the following is an example of a deadlock if the injection is done with the socket detaching lock held
 
 thread A waits for the socket mutex release after acquiring the detaching lock
 
 #0  machine_switch_context (old=0xb244b7c, continuation=0, new=0x70f9000) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/pcb.c:869
 #1  0x00226e57 in thread_invoke (self=0xb244b7c, thread=0x70f9000, reason=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1628
 #2  0x002270f6 in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1863
 #3  0x00227184 in thread_block (continuation=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1880
 #4  0x0029d846 in lck_mtx_lock_wait_x86 (mutex=0xbbc8150) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/locks_i386.c:2021
 #5  0x00298328 in lck_mtx_lock () at cpu_data.h:384
 #6  0x003553a2 in tcp_lock (so=0x5dad40c, refcount=0, lr=0x352287) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_subr.c:2115
 #7  0x004ab120 in socket_lock (so=0x5dad40c, refcount=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:3925
 #8  0x00352287 in tcp_ip_output (so=0x5dad40c, tp=0x0, pkt=0x0, cnt=1, opt=0x0, flags=256, sack_in_progress=0, recwin=524280) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_output.c:1851
 #9  0x00353b56 in tcp_output (tp=0x5dad680) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_output.c:1623
 #10 0x004ae9a6 in sosend (so=0x5dad40c, addr=0x0, uio=0x0, top=0x2dc65a00, control=0x0, flags=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:1841
 #11 0x004c3bd5 in sock_inject_data_out (so=0x5dad40c, to=0x0, data=0x2dc65a00, control=0x0, flags=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:633
 #12 0x470c2ae2 in DldSocketObject::reinjectDeferredData (this=0xbbb2e00, injectType=DldSocketDataAll) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:1563
 #13 0x470c54b0 in DldSocketObject::InjectionThreadRoutine (context=0x0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:256
 
 thread B waits for the detaching lock after acquiring the socket mutex ( have you noticed a call to closef_locked? )
 
 #0  machine_switch_context (old=0x6696000, continuation=0, new=0xb1617a8) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/pcb.c:869
 #1  0x00226e57 in thread_invoke (self=0x6696000, thread=0xb1617a8, reason=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1628
 #2  0x002270f6 in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1863
 #3  0x00227184 in thread_block (continuation=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1880
 #4  0x004869c0 in _sleep (chan=0xbbb2e30 "\001", pri=50, wmsg=<value temporarily unavailable, due to optimizations>, abstime=1823462329845, continuation=0, mtx=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_synch.c:241
 #5  0x00486eef in msleep (chan=0xbbb2e30, mtx=0x0, pri=50, wmsg=0x470e2a68 "DldSocketObject::acquireDetachingLockForRemoval()", ts=0x31dcbd08) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_synch.c:335
 #6  0x470c125a in DldSocketObject::acquireDetachingLockForRemoval (this=0xbbb2e00) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:1937
 #7  0x470c1117 in DldSocketFilter::FltDetach (cookie=0xbbb2e00, so=0x5dad40c) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:415
 #8  0x470c117a in DldSocketFilter::FltDetachIPv4 (cookie=0xbbb2e00, so=0x5dad40c) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:432
 #9  0x004c3df0 in sflt_detach_private (entry=0x6580524, unregistering=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:381
 #10 0x004c402f in sflt_termsock (so=0x5dad40c) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:92
 #11 0x004ab889 in sofreelastref (so=0x5dad40c, dealloc=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:796
 #12 0x004aba19 in sofree (so=0x5dad40c) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:3993
 #13 0x004ad5fa in soclose_locked (so=0x5dad40c) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:1019
 #14 0x004ad646 in soclose (so=0x5dad40c) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:1033
 #15 0x004696fc in fo_close [inlined] () at :4869
 #16 0x004696fc in closef_finish [inlined] () at :4068
 #17 0x004696fc in closef_locked (fp=0x67e54b0, fg=0x67e0478, p=0xb093000) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:4167
 #18 0x0046b4ad in close_internal_locked (p=0xb093000, fd=48, fp=0x67e54b0, flags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:1950
 #19 0x0046b57e in close_nocancel (p=0xb093000, uap=0x6693e80, retval=0x6693ec4) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:1852
 #20 0x004ed78d in unix_syscall (state=0x5faa160) at /SourceCache/xnu/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:205 
 */

void
DldSocketObject::reinjectDeferredData(
    __in DldSocketObject::DldSocketDataDirectionType  injectType
    )
{
    assert( preemption_enabled() );
    
	PktPendingQueueHead		packetsToInject;
	
    //
	// init the queue which we use to place the packets we want to re-inject into the tcp stream
    //
	TAILQ_INIT( &packetsToInject );
    
    //
    // acquire the lock to be sure the socket will not perish
    //
    //
    // if( !this->acquireDetachingLock() ){
    //    
    //    DLD_COMM_LOG ( NET_PACKET_FLOW,( "acquireDetachingLock() failed for a socket 0x%p\n", this->so ));
    //    return;
    // }

    //
    // serialize the injection stream
    //
    IOLockLock( this->injectionMutex );
    { // start of the injection lock
        
        this->LockExclusive();
        { // start of the lock
            
            DldSocketObject::PendingPktQueueItem*	pendingPkt;
            DldSocketObject::PendingPktQueueItem*	pendingPktNext;
            
#if DBG
            this->verifyPendingPacketsQueue( false );
#endif // DBG
            
            //
            // iterate the queue looking for matching entries; if we find a match, 
            // remove it from queue and put it on packets_to_inject; because we're 
            // removing elements from tl_deferred_data, we can't use TAILQ_FOREACH
            //
            for( pendingPkt = TAILQ_FIRST( &this->pendingQueue ); pendingPkt != NULL; pendingPkt = pendingPktNext ){
                
                //
                // get the next element pointer before we potentially corrupt it
                //
                pendingPktNext = TAILQ_NEXT( pendingPkt, pendingQueueEntry );
                
                //
                // should the packet be injected or not according to the injection type?
                //
                if( DldSocketDataAll != injectType ){
                    
                    if( (true == pendingPkt->dataInbound && DldSocketDataInbound != injectType) ||
                        (false == pendingPkt->dataInbound && DldSocketDataOutbound != injectType))
                        continue;
                    
                } // end if( DldInjectAll != injectType )
               
                //
                // not reported packets are not injected,
                // all not reported packets are at the tail
                // so stop processing the current queue,
                // if the packet timer has expired it will be processed
                // by DeliverWaitingNotifications and then again
                // picked up here with needToBeReported set tot true
                //
                if( pendingPkt->needToBeReported )
                    break;
                
                //
                // if a packet's timer has expired then force processing
                // as most probably the packet has stuck and must be rejected, so the check for
                // reporting status and for response are not made for it, packets in the queue
                // deplenish the mbuf pool and if were not removed would stop network subsystem
                // from accepting or sending new data
                //
                if( ! pendingPkt->isDeadlineTimerExpired() ){
                    
                    //
                    // not reported packets are not injected,
                    // all not reported packets are at the tail
                    // so stop processing the current queue
                    //
                    if( ! pendingPkt->responseReceived )
                        break;
                    
                } else {
                    
                    //
                    // the packet's timer expired
                    //
                    assert( ! pendingPkt->needToBeReported );
                    this->deadlinedPackets += 0x1; // the counter is just for a debug purpose
                }
                
                //
                // TO DO -look for a match, if we find it, move it from the deferred 
                // data queue to our local queue
                //
                {
                    TAILQ_REMOVE( &this->pendingQueue, pendingPkt, pendingQueueEntry );
                    TAILQ_INSERT_TAIL( &packetsToInject, pendingPkt, pendingQueueEntry );
                } 
            } // end for
            // we're done with the global list, so release our lock on it
        } // end of the lock
        this->UnlockExclusive();
        
        //
        // now process the local list, injecting each packet we found
        //
        while( ! TAILQ_EMPTY( &packetsToInject ) ){
            
            errno_t error;
            DldSocketObject::PendingPktQueueItem*	pendingPkt;
            bool  freeMbufs = false;
            
            pendingPkt = TAILQ_FIRST( &packetsToInject );
            TAILQ_REMOVE( &packetsToInject, pendingPkt, pendingQueueEntry );
 
            //
            // if it happens that while the data being injected the socket moved to disconnecting state then
            // do not try to send the data, just purge it,
            // do the same for the packets with disabled data
            //
            if( 0x1 == this->flags.f_sock_evt_disconnecting || (! pendingPkt->allowData) ){
                
                freeMbufs = true;
                
            } else {
                
                //
                // inject the packet, in the right direction
                //
                if( pendingPkt->dataInbound ){
                    
                    //
                    // NOTE: for TCP connections, the "to" parameter is NULL. For a UDP connection, there will likely be a valid
                    // destination required. For the UDP case, you can use a pointer to local storage to pass the UDP sockaddr
                    // setting. Refer to the header doc for the sock_inject_data_out function
                    //
                    error = sock_inject_data_in( this->socket, NULL, pendingPkt->data, pendingPkt->control, pendingPkt->sflt_flags );
                    
                } else {
                    
                    //
                    // NOTE: for TCP connections, the "to" parameter is NULL. For a UDP connection, there will likely be a valid
                    // destination required. For the UDP case, you can use a pointer to local storage to pass the UDP sockaddr
                    // setting. Refer to the header dock for the sock_inject_data_out function
                    //
                    error = sock_inject_data_out( this->socket, NULL, pendingPkt->data, pendingPkt->control, pendingPkt->sflt_flags );
                }
                
            }
            
            //
            // if the inject failed, check whether the data is inbound or outbound. As per the
            // sock_inject_data_out description - The data and control values are always freed 
            // regardless of return value. However, for the sock_inject_data_in function -  If the 
            // function returns an error, the caller is responsible for freeing the mbuf.
            //
            if( 0x0 != error ){
                
                DLD_COMM_LOG ( NET_PACKET_FLOW, ("error calling sock_inject_data_in/out, so = 0x%p, error = %d, inbound = %d\n",
                                                 this->socket, error, (int)pendingPkt->dataInbound ));
                
                if( pendingPkt->dataInbound ){
                    
                    //
                    // only release mbufs for inbound injection failure
                    //
                    freeMbufs = true;
                }
            } // end if( 0x0 != error )
            
            if( freeMbufs ){
                
                mbuf_freem(pendingPkt->data);
                mbuf_freem(pendingPkt->control);
            } // end if( freeMbufs )
            
            if( pendingPkt->dataInbound ){
                
                OSDecrementAtomic( &this->numberOfPendingInPackets );		// decrement packet count
                OSAddAtomic( (-1)*pendingPkt->totalbytes, &this->totalPendingBytesIn );
                assert( this->numberOfPendingInPackets >= 0x0 );
                assert( this->totalPendingBytesIn >= 0x0 );
                
            } else {
                
                OSDecrementAtomic( &this->numberOfPendingOutPackets );		// decrement packet count
                OSAddAtomic( (-1)*pendingPkt->totalbytes, &this->totalPendingBytesOut );
                assert( this->numberOfPendingOutPackets >= 0x0 );
                assert( this->totalPendingBytesOut >= 0x0 );
            }
            
            //
            // free the queue entry
            //
            OSFree( pendingPkt, sizeof(*pendingPkt), gOSMallocTag);
            
        } // end while
        
        this->wakeupWaitingFotInjectionCompletion();
        
    } // end of the injection
    IOLockUnlock( this->injectionMutex );
    
    //this->releaseDetachingLock();
    
	// we don't need to do anything to tidy up packets_to_inject because 
	// a) the queue head is a local variable, and b) the queue elements 
	// are all gone (guaranteed by the while loop above).
}

//--------------------------------------------------------------------

void
DldSocketObject::setDeferredDataProperties(
    __in DldSocketDataProperty*  property
    )
{
    assert( preemption_enabled() );
    
    this->LockExclusive();
    { // start of the lock
        
        DldSocketObject::PendingPktQueueItem*	pendingPkt;
        
#if DBG
        //
        // if f_sock_evt_closing or sock_evt_shutdown is set the inbound( or both) packets queue has been purged
        //
        bool shouldBeApplied = ( DldSocketDataPropertyTypePermission == property->type &&
                                 0x0 == this->flags.f_sock_evt_pre_closing &&
                                 0x0 == this->flags.f_sock_evt_pre_shutdown );
        bool wasApplied = false;
        this->verifyPendingPacketsQueue( false );
#endif // DBG
        
        //
        // iterate the queue looking for matching entries
        //
        TAILQ_FOREACH( pendingPkt, &this->pendingQueue, pendingQueueEntry )
        {
            
            //
            // all not reported packets are at the tail
            // so stop processing the current queue
            //
            if( pendingPkt->needToBeReported ){
                
                assert( !shouldBeApplied || 0x0 != this->deadlinedPackets );
                break;
            }
            
            //
            // should the property be applyed to the packet 
            //
            if( pendingPkt->dataIndex != property->dataIndex )
                continue;
            
            assert( ! pendingPkt->needToBeReported );
            
#if DBG
            wasApplied = true;
#endif // DBG
            
            assert( DldSocketDataPropertyTypeUnknown != property->type );
            switch( property->type ){
                    
                case DldSocketDataPropertyTypePermission:
                    
                    pendingPkt->allowData = property->value.permission.allowData ? true : false;
                    pendingPkt->responseReceived = true;
                    break;
                    
                default:
                    
                    DBG_PRINT_ERROR(( "unknown property %d\n", (int)property->type  ));
                    break;
            } // end switch
            
            // we have finished
            break;
        } // end TAILQ_FOREACH
        
        assert( !( shouldBeApplied && !wasApplied) || 0x0 != this->deadlinedPackets );

    } // end of the lock
    this->UnlockExclusive();
}

//--------------------------------------------------------------------

void
DldSocketObject::verifyPendingPacketsQueue( __in bool lock )
//
// this is a debug mode function, it should not be used in a release build
//
{
    assert( preemption_enabled() );
    
    if( lock )
        this->LockExclusive();
    { // start of the lock
        
        bool    needToBeReportedWasFound = false;
        DldSocketObject::PendingPktQueueItem*	pendingPkt;
        
        //
        // iterate the queue looking for matching entries
        //
        TAILQ_FOREACH( pendingPkt, &this->pendingQueue, pendingQueueEntry )
        {
            
            //
            // all not reported packets are at the tail
            // so stop processing the current queue
            //            
            assert( !( needToBeReportedWasFound && !pendingPkt->needToBeReported ) );
            if( needToBeReportedWasFound && !pendingPkt->needToBeReported ){
                
                DBG_PRINT_ERROR(( "verifyPendingPacketsQueue failed the pending queue check for so=%p\n", this->socket ));
            }
            
            needToBeReportedWasFound = pendingPkt->needToBeReported;
            
        } // end TAILQ_FOREACH
        
    } // end of the lock
    if( lock )
        this->UnlockExclusive();
}

//--------------------------------------------------------------------

void
DldSocketObject::purgePendingQueue(
    __in DldSocketObject::DldSocketDataDirectionType purgeType
    )
{
    assert( preemption_enabled() );
    
    //
    // serialize with the injection stream
    //
    IOLockLock( this->injectionMutex );
    {
        this->LockExclusive();
        { // start of the lock
            
            DldSocketObject::PendingPktQueueItem*  pendingPkt;
            DldSocketObject::PendingPktQueueItem*  pendingPktNext;
            
            for( pendingPkt = TAILQ_FIRST(&this->pendingQueue); pendingPkt != NULL; pendingPkt = pendingPktNext )
            {
                //
                // get the next element pointer before we potentially corrupt it
                //
                pendingPktNext = TAILQ_NEXT( pendingPkt, pendingQueueEntry );
                
                //
                // should the packet be purged or not according to the type?
                //
                if( DldSocketDataAll != purgeType ){
                    
                    if( (true == pendingPkt->dataInbound && DldSocketDataInbound != purgeType) ||
                       (false == pendingPkt->dataInbound && DldSocketDataOutbound != purgeType))
                        continue;
                    
                } // end if( DldInjectAll != purgeType )
                
                //
                // look for a match, if we find it, move it from the deferred 
                // data queue to our local queue
                //
                
                if( pendingPkt->dataInbound ){
                    DLD_COMM_LOG ( NET_PACKET_FLOW, ("****INBOUND PACKET FREED FROM PENDING QUEUE socket_t is 0x%p, mbuf is 0x%p\n", this->socket, pendingPktNext->data));
                } else {
                    DLD_COMM_LOG ( NET_PACKET_FLOW, ("****OUTBOUND PACKET FREED FROM PENDING QUEUE socket_t is 0x%p, mbuf is 0x%p\n", this->socket, pendingPktNext->data));
                }
                
                //
                // wake up a waiting thread
                //
                if( pendingPkt->waitEntry ){
                    
                    assert( ! pendingPkt->dataInbound );
                    assert( false == pendingPkt->waitEntry->waitSatisfied );
                    
                    pendingPkt->waitEntry->waitSatisfied = true;
                    wakeup( pendingPkt->waitEntry );
                    pendingPkt->waitEntry = NULL;
                } // end if( pendingPkt->waitEntry )
                
                if( pendingPkt->needToBeReported ){
                    
                    //
                    // the packet will not be reported as data is being purged
                    //
                    assert( this->packetsWaitingForReporting > 0x0 );
                    OSDecrementAtomic( &this->packetsWaitingForReporting );
                    pendingPkt->needToBeReported = false;
                }
                
                if( pendingPkt->dataInbound ){
                    
                    OSDecrementAtomic( &this->numberOfPendingInPackets );		// decrement packet count
                    OSAddAtomic( (-1)*pendingPkt->totalbytes, &this->totalPendingBytesIn );
                    assert( this->numberOfPendingInPackets >= 0x0 );
                    assert( this->totalPendingBytesIn >= 0x0 );
                    
                } else {
                    
                    OSDecrementAtomic( &this->numberOfPendingOutPackets );		// decrement packet count
                    OSAddAtomic( (-1)*pendingPkt->totalbytes, &this->totalPendingBytesOut );
                    assert( this->numberOfPendingOutPackets >= 0x0 );
                    assert( this->totalPendingBytesOut >= 0x0 );
                }
                
                TAILQ_REMOVE( &this->pendingQueue, pendingPkt, pendingQueueEntry );
                my_mbuf_freem( &pendingPkt->data );
                my_mbuf_freem( &pendingPkt->control );
                OSFree( pendingPkt, sizeof(*pendingPkt), DldSocketObject::gOSMallocTag );
                DLD_DBG_MAKE_POINTER_INVALID( pendingPkt );
            } // end for
            
            this->wakeupWaitingFotInjectionCompletion();
            
        } // end of the lock
        this->UnlockExclusive();
        
    } // end of the synchronization
    IOLockUnlock( this->injectionMutex );
}

//--------------------------------------------------------------------

bool
DldSocketObject::checkForInjectionCompletion( __in DldSocketObject::DldSocketDataDirectionType  injectType, bool waitForCompletion )
{
    assert( !( waitForCompletion && (! preemption_enabled()) ) );
    
    //
    // we can't use injectionMutex here as this will violate the lock hierarchy if
    // the function is called from callbacks with held socket lock, see the comments for
    // reinjectDeferredData()
    //
    
    if( DldSocketDataOutbound == injectType || DldSocketDataAll == injectType ){
        
        if( waitForCompletion ){
            
            while( this->numberOfPendingOutPackets != 0 ) { // wait for injection completion
                
                struct timespec ts = { 1, 0 };              // one second
                (void)msleep( &this->numberOfPendingOutPackets,             // wait channel
                              NULL,                          // mutex
                              PUSER,                         // priority
                              "DldSocketObject::checkForInjectionCompletion()", // wait message
                              &ts );                         // sleep interval
                
                assert( this->numberOfPendingOutPackets >= 0x0 );
                
            } // end while
            
        } else {
            
            return false;
        }
        
    } // end if( DldSocketDataOutbound == injectType || ...
    
    
    if( DldSocketDataInbound == injectType || DldSocketDataAll == injectType ){
        
        if( waitForCompletion ){
            
            while( this->numberOfPendingInPackets != 0 ) { // wait for injection completion
                
                struct timespec ts = { 1, 0 };              // one second
                (void)msleep( &this->numberOfPendingInPackets,             // wait channel
                              NULL,                          // mutex
                              PUSER,                         // priority
                              "DldSocketObject::checkForInjectionCompletion()", // wait message
                              &ts );                         // sleep interval
                
                assert( this->numberOfPendingInPackets >= 0x0 );
                
            } // end while
            
        } else {
            
            return false;
        }
        
    } // end if( DldSocketDataInbound == injectType || ...
    
    return true;
}

//--------------------------------------------------------------------

void
DldSocketObject::wakeupWaitingFotInjectionCompletion()
{
    if( 0x0 == this->numberOfPendingInPackets ){
        
        wakeup( &this->numberOfPendingInPackets );
    }
    
    if( 0x0 == this->numberOfPendingOutPackets ){
        
        wakeup( &this->numberOfPendingOutPackets );
    }
}

//--------------------------------------------------------------------

/*
 checkTag - see if there is a tag associated with the mbuf_t with the matching bitmap bits set in the
    memory associated with the tag. Use global gidtag as id Tag to look for
 
 input
    m - pointer to mbuf_t variable on which to search for tag
    module_id - the tag_id obtained from the mbuf_tag_id_find call;
    tag_type - specific tagType to look for
    value - see if the tag_ref field has the expected value
    return 1 - success, the value in allocated memory associated with tag gidtag has a matching value
    return 0 - failure, either the mbuf_t is not tagged, or the allocated memory does not have the expected value
 
    Note that in this example, the value of tag_ref is used to store bitmap values. the allocated memory is
 process specific.  
 */

bool
DldSocketObject::checkTag(
    __in mbuf_t *m,
    __in mbuf_tag_id_t module_id,
    __in mbuf_tag_type_t tag_type,
    __in DLD_PACKETPROCFLAGS value
    )
{
    errno_t                 error;
    DLD_PACKETPROCFLAGS		*tag_ref;
    size_t                  len;
    
    // Check whether we have seen this packet before.
    error = mbuf_tag_find(*m, module_id, tag_type, &len, (void**)&tag_ref);
    if ((error == 0) && (len == sizeof(value)) && (*tag_ref == value) )
        return true;
    
    return false;
}

//--------------------------------------------------------------------

/*	
 - Set the tag associated with the mbuf_t with the bitmap bits set in bitmap
 The setTag calls makes a call to mbuf_tag_allocate with the MBUF_WAITOK flag set. Under OS X 10.4(+), waiting for the 
 memory allocation is ok from within a filter function.
  10//06 - for AFPoverIP IPv6 connections, there are some packets which are passed which do not have the 
 PKTHDR bit set in the mbug_flags field. This will cause the mbuf_tag_allocate function to fail with
 EINVAL error. 
 
 input
    m - mbuf_t pointer variable on which to search for tag
    module_id - the tag_id obtained from the mbuf_tag_id_find call;
    tag_type - specific tagType to look for
    value - value  to set in allocated memory
    return 0 - success, the tag has been allocated and for the mbuf_t and the value has been set in the
        allocated memory. 
        anything else - failure	
 */
errno_t
DldSocketObject::setTag(
    __in mbuf_t *data,
    __in mbuf_tag_id_t id_tag,
    __in mbuf_tag_type_t tag_type,
    __in DLD_PACKETPROCFLAGS value
    )
{	
	errno_t                 error;
	DLD_PACKETPROCFLAGS		*tag_ref = NULL;
	size_t                  len;
	
    assert( preemption_enabled() );
	assert(data);
    //
	// look for an existing tag
    //
	error = mbuf_tag_find(*data, id_tag, tag_type, &len, (void**)&tag_ref);
    //
	// allocate a tag if needed
    //
	if( KERN_SUCCESS != error ){
		
		error = mbuf_tag_allocate( *data, id_tag, tag_type, sizeof(value), MBUF_WAITOK, (void**)&tag_ref );
		if( KERN_SUCCESS == error ){
            
            //
            // set tag_ref
            //
			*tag_ref = value;
            
		} else if( EINVAL == error ){
			
			mbuf_flags_t	flags;
            
            //
			// check to see if the mbuf_tag_allocate failed because the mbuf_t has the M_PKTHDR flag bit not set
            //
			flags = mbuf_flags(*data);
			if( (flags & MBUF_PKTHDR) == 0 ){
                
				mbuf_t  m = *data;
				size_t  totalbytes = 0;
                
                //
				// the packet is missing the MBUF_PKTHDR bit. In order to use the mbuf_tag_allocate, function,
                // we need to prepend an mbuf to the mbuf which has the MBUF_PKTHDR bit set.
                // We cannot just set this bit in the flags field as there are assumptions about the internal
                // fields which there are no API's to access.
                //
				DLD_COMM_LOG( NET_PACKET_FLOW, ("mbuf_t = 0x%p missing MBUF_PKTHDR bit\n", m));
                
				while( NULL != m ){
                    
					totalbytes += mbuf_len(m);
					m = mbuf_next(m);	// look at the next mbuf
				}
                
				error = this->prependMbufHdr( data, totalbytes );
                assert( KERN_SUCCESS == error );
				if( KERN_SUCCESS == error ){
                    
					error = mbuf_tag_allocate( *data, id_tag, tag_type, sizeof(value), MBUF_WAITOK, (void**)&tag_ref );
					if( KERN_SUCCESS != error ){
                        
						DBG_PRINT_ERROR(("mbuf_tag_allocate failed a second time, mbuf = 0x%p, error was %d\n", *data, error));
					}
				} else {
                    
                    assert( !"prependMbufHdr failed" );
                    DBG_PRINT_ERROR(("prependMbufHdr failed, mbuf = 0x%p, error was %d\n", *data, error));
                }
			} else { // end if( (flags & MBUF_PKTHDR) == 0 )
                
                assert( !"mbuf_tag_allocate returned EINVAL and (flags & MBUF_PKTHDR) != 0" );
                DBG_PRINT_ERROR(("mbuf_tag_allocate returned EINVAL and (flags & MBUF_PKTHDR) != 0, mbuf = 0x%p, error was %d\n", *data, error));
            }
            
		} else {
         
            assert( !"unable to tag the mbuf, mbuf_tag_allocate failed" );
			DBG_PRINT_ERROR(("mbuf_tag_allocate failed, mbuf = 0x%p, error was %d\n", *data, error));
        }
	} // end if( KERN_SUCCESS == error )
    
	return error;
}

//--------------------------------------------------------------------

/*
    prependMbufHdr - used to prepend an mbuf_t init'd for PKTHDR so that an mbuf_tag_allocate
        call can be used to mark an mbuf. As per <rdar://problem/4786262>, on AFPoverIP IPv6 connections,
        very infrequently, the mbuf does not have the PKTHDR bit set and the mbuf_tag_allocate function fails. 
        A workaround solution is to prepend a PKTHDR mbuf to the front of the mbuf chain, so that the mbuf can be 
        "tagged"
 
    data - pointer to mbuf_t variable which has no PKTHDR bit set in the flags field
    len  - amount of data in the data mbuf chain
 
    return 0 (KERN_SUCCESS - success, the PKTHDR mbuf was successfully allocated and prepended to the front of the mbuf
        and data now points to the newly allocated mbuf_t.
 
    return any other value - failure, the PKTHDR mbuf_t failed to be allocated. 
 */
errno_t
DldSocketObject::prependMbufHdr( __inout mbuf_t *data, __in size_t pkt_len)
{
	mbuf_t      new_hdr;
	errno_t     error;
    
	error = mbuf_gethdr( MBUF_WAITOK, MBUF_TYPE_DATA, &new_hdr );
    assert( KERN_SUCCESS == error );
	if( KERN_SUCCESS == error ){
        
        //
        // we've created a replacement header, now we have to set things up
		// set the mbuf argument as the next mbuf in the chain
        //
		mbuf_setnext( new_hdr, *data );
		
        //
		// set the next packet attached to the mbuf argument in the pkt hdr
        //
		mbuf_setnextpkt(new_hdr, mbuf_nextpkt(*data));
        
        //
		// set the total chain len field in the pkt hdr
        //
		mbuf_pkthdr_setlen(new_hdr, pkt_len);
		mbuf_setlen(new_hdr, 0);
        
		mbuf_pkthdr_setrcvif(*data, NULL);
		
        //
		// now set the new mbuf_t as the new header mbuf_t
        //
		*data = new_hdr;
        
	} else {
        
        DBG_PRINT_ERROR(("mbuf_gethdr failed , mbuf = 0x%p, error was %d\n", *data, error));
    }
    
	return error;
}

//--------------------------------------------------------------------

bool
DldSocketObject::acquireDetachingLock()
{
    
    UInt32 oldCount = this->lockCount;
    
    while( 0x0 != oldCount && !OSCompareAndSwap( oldCount, oldCount+0x1, &this->lockCount ) ){
        
        oldCount = this->lockCount;
    } // end while
    
    return ( 0x0 != oldCount );
}

//--------------------------------------------------------------------

void
DldSocketObject::releaseDetachingLock()
{
    assert( this->lockCount > 0x0 );
    
    if( 0x1 == OSDecrementAtomic( &this->lockCount ) ){
        
        //
        // the socket is being detached and waiting for the last lock to go
        //
        assert( 0x0 == this->lockCount );
        wakeup( &this->lockCount );
    }
}

//--------------------------------------------------------------------

void
DldSocketObject::acquireDetachingLockForRemoval()
{
    //
    // this function can be called only once
    //
    
    assert( preemption_enabled() );
    assert( this->lockCount > 0x0 );
    
    OSDecrementAtomic( &this->lockCount );
    
    assert( this->lockCount >= 0x0 );
    
    do { // wait for any existing client invocations to return
        
        struct timespec ts = { 1, 0 };       // one second
        (void)msleep( &this->lockCount,      // wait channel
                      NULL,                          // mutex
                      PUSER,                         // priority
                      "DldSocketObject::acquireDetachingLockForRemoval()", // wait message
                      &ts );                         // sleep interval
        
        assert( this->lockCount >= 0x0 );
        
    } while( this->lockCount != 0 );
}

//--------------------------------------------------------------------

void
DldSocketObject::setRemoteAddress( __in const struct sockaddr *remoteAddress )
{
    assert( remoteAddress->sa_family == this->sa_family );
    
	if( AF_INET == remoteAddress->sa_family )
	{
		assert( sizeof( this->remoteAddress.addr4 ) >= remoteAddress->sa_len );
        
        //
		// save the remote address in the remote field
        //
		bcopy( remoteAddress, &(this->remoteAddress.addr4), remoteAddress->sa_len);
        
        //
		// ensure port is in host format
        //
		this->remoteAddress.addr4.sin_port = ntohs( this->remoteAddress.addr4.sin_port );
	}
	else if( AF_INET6 == remoteAddress->sa_family )
	{
		assert( sizeof( this->remoteAddress.addr6 ) >= remoteAddress->sa_len );
        
        //
		// save the remote address in the remote field
        //
		bcopy( remoteAddress, &(this->remoteAddress.addr6), remoteAddress->sa_len);
        
        //
		// ensure port is in host format
        //
		this->remoteAddress.addr6.sin6_port = ntohs( this->remoteAddress.addr6.sin6_port );
        
	} else {
        
        DBG_PRINT_ERROR(("unknown protocol family %d\n", (int)remoteAddress->sa_family ));
    }
}

//--------------------------------------------------------------------

void DldSocketObject::getRemoteAddress( __inout DldSocketObjectAddress* address )
{
    if( ! this->isRemoteAddressValid() ){
        
        bzero( address, sizeof( *address ) );
        return;
    }
    
    if( AF_INET == this->sa_family )
        bcopy( &(this->remoteAddress.addr4), address, this->remoteAddress.addr4.sin_len);
    else
        bcopy( &(this->remoteAddress.addr6), address, this->remoteAddress.addr6.sin6_len);
}

//--------------------------------------------------------------------

void
DldSocketObject::setLocalAddress( __in const struct sockaddr *localAddress )
{
    assert( localAddress->sa_family == this->sa_family );
    
	if( AF_INET == localAddress->sa_family )
	{
		assert( sizeof( this->localAddress.addr4 ) >= localAddress->sa_len );
        
        //
		// save the remote address in the local field
        //
		bcopy( localAddress, &(this->localAddress.addr4), localAddress->sa_len);
        
        //
		// ensure port is in host format
        //
		this->localAddress.addr4.sin_port = ntohs( this->localAddress.addr4.sin_port );
	}
	else if( AF_INET6 == localAddress->sa_family )
	{
		assert( sizeof( this->localAddress.addr6 ) >= localAddress->sa_len );
        
        //
		// save the remote address in the local field
        //
		bcopy( localAddress, &(this->localAddress.addr6), localAddress->sa_len);
        
        //
		// ensure port is in host format
        //
		this->localAddress.addr6.sin6_port = ntohs( this->localAddress.addr6.sin6_port );
        
	} else {
        
        DBG_PRINT_ERROR(("unknown protocol family %d\n", (int)localAddress->sa_family ));
    }
}

//--------------------------------------------------------------------

void DldSocketObject::getLocalAddress( __inout DldSocketObjectAddress* address )
{
    if( ! this->isLocalAddressValid() ){
        
        bzero( address, sizeof( *address ) );
        return;
    }
    
    if( AF_INET == this->sa_family )
        bcopy( &(this->localAddress.addr4), address, this->localAddress.addr4.sin_len);
    else
        bcopy( &(this->localAddress.addr6), address, this->localAddress.addr6.sin6_len);
}

//--------------------------------------------------------------------
