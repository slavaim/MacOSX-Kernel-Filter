/* 
 * Copyright (c) 2012 Slava Imameev. All rights reserved.
 */

#ifndef _DLDSOCKETOBJECT_H
#define _DLDSOCKETOBJECT_H

#include <sys/kpi_socket.h>
#include <sys/kpi_mbuf.h>
#include <sys/kpi_socket.h>
#include <sys/kpi_socketfilter.h>
#include <netinet/in.h>

#include <libkern/OSMalloc.h>

#include "DldCommon.h"
#include "DldIOService.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#define INVALID_SOCKET_HANDLE       0x0
#define SO_EVENTS_LOG_SIZE          20
#define SOCKET_OBJECT_SIGNATURE     0xABCD2345
#define DLD_SOCKTAG_ID_TYPE         0x1

//
// values to use with the memory allocated by the tag function, to indicate which processing has been
// performed already
//
typedef enum DLD_PACKETPROCFLAGS{
	DLD_INBOUND_DONE = 1,
	DLD_OUTBOUND_DONE
} DLD_PACKETPROCFLAGS;

typedef enum DldSocketDataDirection{
    DldSocketDataDirectionOut = 1,
    DldSocketDataDirectionIn
};

//--------------------------------------------------------------------

//
// IMPORTANT
// the lock hierarchy, in order of acquiring
//   - the socket object lock ( rwLock ) is acquired first
//   - the socket list lock ( SocketsListLock )
//

class DldSocketObject: public OSObject{
    
    OSDeclareDefaultStructors( DldSocketObject );
    
private:
    
    //
    // a number of alocated objects
    //
    static SInt32   SocketObjectsCounter;
    
    //
    // just for a debug
    //
    static bool     Initialized;
    
    //
    // all socket objects are anchored in the double linked list, protected by SocketsListLock,
    // all objects in the list are retained, the list is protected by SocketsListLock
    //
    static TAILQ_HEAD( DldSocketsListHead, DldSocketObject ) SocketsList;
    TAILQ_ENTRY(DldSocketObject)   socketListEntry;
    
    //
    // a RW lock to protect SocketsList
    //
    static IORWLock*       SocketsListLock;
    
    //
    // used to temporary link objects by InjectionThreadRoutine
    //
    TAILQ_ENTRY(DldSocketObject)   injectionSocketListEntry;
    
    //
    // used to temporary link objects that have buffers to be reported,
    // a reference is hold for all objects in the list, when the object
    // is removed from the list the reference is also removed
    //
    static TAILQ_HEAD( DldSocketsListToReportHead, DldSocketObject ) SocketsListToReport;
    TAILQ_ENTRY(DldSocketObject)   socketsListToReportEntry;
    
    //
    // a RW lock to protect SocketsListToReport
    //
    static IORWLock*       SocketsListToReportLock;
    
    //
    // tag for use with OSMalloc calls which is used to associate memory
    // allocations made with this kext. Preferred to using MALLOC and FREE
    //
    static OSMallocTag		gOSMallocTag;
    
    //
    // a sequence ID generator, used to distinguish two objects when one is a closed socket and another is a new one
    // that reuses a memory freed by the closed socket, because of asynchronous processing the parser might reply
    // with the packets sendt to a closed socket, without a sequence ID they would be applied to a new socket
    //
    static SInt32           gSocketSequence;
    
private:
    
    //
    // a thread that injects deferred data
    //
    static void InjectionThreadRoutine( void* context );
    
private:
    
    //
    // used to generate a unique ( per socket ) data index
    //
    SInt32   pendingDataNextIndex;
    
    //
    // a number of packets that were rejected because of time deadline,
    // used only for a debug purposses, might wrap around because of overflow
    //
    SInt32   deadlinedPackets;
    
    //
    // a kernel socket related to the socket object
    //
    socket_t      socket;
    
    //
    // a socket ID for communication with the service
    //
    DldSocketID   socketId;
    
    //
    // a tag to mark mbufs
    //
    mbuf_tag_id_t gidtag;
    
    //
    // socket's receive buffer initial size
    //
    u_int32_t     origReceiveBufferSize;
    
    //
    // when the socket is valid the count is greate then 0x0,
    // when the count drops to 0x0 the socket is invalid
    //
    UInt32  lockCount;
    
    //
    // set to true if the socket has been disconnected
    //
    bool    disconnected;

    //
    // the flags are protected by the rwLock, if a flag is being changed the lock must be held
    //
    struct {
        
        //
        // 0x1 if the socket object has been inserted in the global socket objects list
        //
        unsigned int    insertedInSocketsList:0x1;
        
        //
        // socket state flags are mostly for information purposses, do not use them for state transitions,
        // the flags are set after the event has been processed
        //
        unsigned int    f_sock_evt_connecting:0x1;
        unsigned int    f_sock_evt_connected:0x1;
        unsigned int    f_sock_evt_disconnecting:0x1;
        unsigned int    f_sock_evt_disconnected:0x1;
        unsigned int    f_sock_evt_flush_read:0x1;
        unsigned int    f_sock_evt_shutdown:0x1;
        unsigned int    f_sock_evt_cantrecvmore:0x1;
        unsigned int    f_sock_evt_cantsendmore:0x1;
        unsigned int    f_sock_evt_closing:0x1;
        unsigned int    f_sock_evt_bound:0x1;
        
        //
        // the following flags are set before starting the event processing
        //
        unsigned int    f_sock_evt_pre_connecting:0x1;
        unsigned int    f_sock_evt_pre_connected:0x1;
        unsigned int    f_sock_evt_pre_disconnecting:0x1;
        unsigned int    f_sock_evt_pre_disconnected:0x1;
        unsigned int    f_sock_evt_pre_flush_read:0x1;
        unsigned int    f_sock_evt_pre_shutdown:0x1;
        unsigned int    f_sock_evt_pre_cantrecvmore:0x1;
        unsigned int    f_sock_evt_pre_cantsendmore:0x1;
        unsigned int    f_sock_evt_pre_closing:0x1;
        unsigned int    f_sock_evt_pre_bound:0x1;
        
    } flags;
    
#if DBG
    unsigned int nextEventsLogPosition;
    sflt_event_t eventsLog[ SO_EVENTS_LOG_SIZE ];
#endif //DBG
    
    //
    // connection specific information
    //
    sa_family_t sa_family;
    DldSocketObjectAddress localAddress;
	DldSocketObjectAddress remoteAddress;
    
    //
    // some statistics
    //
    SInt32 totalPendingBytesOut;
    SInt32 numberOfPendingOutPackets;
    
    SInt32 totalPendingBytesIn;
    SInt32 numberOfPendingInPackets;
    
    typedef struct _WaitEntry{
        bool waitSatisfied;
    } WaitEntry;
    
    //
    // the PendingPktQueueItem record is used to store packet information when a packet is made pending.
    // The item is queued to the pendingQueue regardless of direction. The dataInbound flag determines
    // which direction the pending packet will be processed. In a more complicated case it might be useful
    // to implement separate queues
    //
    typedef struct _PendingPktQueueItem {
        TAILQ_ENTRY(_PendingPktQueueItem) pendingQueueEntry; /* link to next and prev queued entry or NULL */
        SInt32                  dataIndex; // an index that is used by the service-driver communication for data transfer
        SInt32                  totalbytes; // this is (mbuf_pkthdr_len( data ) + mbuf_pkthdr_len( control ))
        uint64_t                allocationTime; // when the packet was allocated
        mbuf_t					data;
        mbuf_t					control;
        bool                    allowData; // if false the packet is not being injected
        bool                    dataInbound;
        bool                    needToBeReported; // true if the data has to be reported, increases packetsWaitingForReporting counter
        bool                    responseReceived; // true if the service has made a decision for this packet
        bool                    deadlineTimerExpired; // true if the packet has not been processed in a reasonable time interval ( like 60 secs )
        sflt_data_flag_t		sflt_flags;
        struct{
            UInt32              errorWhileAllocatingBuffers: 0x1; // a debug info, do not use it for control transfer
            UInt32              notReportedEntryInFront: 0x1;     // a debug info, do not use it for control transfer
            UInt32              waitWasAsserted: 0x1;             // a debug info, do not use it for control transfer
        }                       flags;
        WaitEntry*              waitEntry; // might be NULL if there is no waiting thread, set to a signal state after the data is reported
        
        //
        // the timeout is set to 60 secs
        //
        bool isDeadlineTimerExpired() {
            
            if( this->deadlineTimerExpired )
                return true;
            
            this->deadlineTimerExpired = ( mach_absolute_time() > (this->allocationTime + 60LL*1000000000LL) );
            
            return this->deadlineTimerExpired;
        };
        
    } PendingPktQueueItem;
    
    //
    // a head for the list of mbuffer items, protected by rwLock
    //
    TAILQ_HEAD( PktPendingQueueHead, _PendingPktQueueItem );
    PktPendingQueueHead         pendingQueue;
    
    //
    // a counter for deferred packets waiting for being reported, i.e. with needToBeReported set to true
    //
    SInt32                      packetsWaitingForReporting;
    
    //
    // a RW lock to protect pendingQueue and other fields
    //
    IORWLock*                   rwLock;
    
    //
    // an injection serialization mutex
    //
    IOLock*                     injectionMutex;

#if defined( DBG )
    thread_t                    rwLockExclusiveThread;
#endif//DBG
    
    //
    // true if the object has been inserted in SocketsListToReport,
    // protected by SocketsListToReportLock
    //
    bool                        insertedInSocketsListToReport;
    
    //
    // capturing mode for the socket
    //
    DldCapturingMode            capturingMode;
    
private:
    
    //
    // the returned packet has all fields set to default value and a refrence count of 0x1,
    // a caller must use dereferncePkt() to delete the packet
    //
    PendingPktQueueItem* allocatePkt(__in mbuf_t mbufData,
                                     __in mbuf_t mbufControl,
                                     __in sflt_data_flag_t flags,
                                     __in bool   isInboundData );
    
    void verifyPendingPacketsQueue( __in bool lock );
    
public:
    
    typedef enum _DldSocketDataDirectionType{
        DldSocketDataAll = 0x0,
        DldSocketDataInbound,
        DldSocketDataOutbound
    } DldSocketDataDirectionType;
    
    void purgePendingQueue( __in DldSocketDataDirectionType purgeType );
    
private:
    
    void LockShared();
    void UnlockShared();
    void LockExclusive();
    void UnlockExclusive();
    
private:
    
    bool checkTag( __in mbuf_t *m,
                   __in mbuf_tag_id_t module_id,
                   __in mbuf_tag_type_t tag_type,
                   __in DLD_PACKETPROCFLAGS value );
    
    errno_t setTag( __in mbuf_t *data,
                    __in mbuf_tag_id_t id_tag,
                    __in mbuf_tag_type_t tag_type,
                    __in DLD_PACKETPROCFLAGS value );
    
    errno_t prependMbufHdr( __inout mbuf_t *data, __in size_t pkt_len);
    
    errno_t	FltData( __in socket_t so,
                     __in const struct sockaddr *addr, // a peer's address
                     __inout mbuf_t *data,
                     __inout mbuf_t *control,
                     __in sflt_data_flag_t flags,
                    __in DldSocketDataDirection  direction );
    
protected:
    
    virtual bool init();
    virtual void free();
    
public:
    
#if defined( DBG )
    SInt32          signature;
#endif // DBG
    
    //
    // must be called once when the NKE is being initialized
    //
    static errno_t InitSocketObjectsSubsystem();
    static void RemoveSocketObjectsSubsystem();
    
    static DldSocketObject* withSocket( __in socket_t so, __in mbuf_tag_id_t gidtag, __in sa_family_t sa_family );
    
    //
    // insert an object in the list and takes a reference
    //
    void insertInSocketsList();
    
    //
    // undoes insertInSocketsList
    //
    void removeFromSocketsList();
    
    socket_t toSocket(){ return this->socket; };
    
    static DldSocketObject* GetSocketObjectRef( __in socket_t so );
    
    void logSocketPreEvent( __in sflt_event_t event );
    void logSocketPostEvent( __in sflt_event_t event );
    
    errno_t	FltDataOut( __in socket_t so,
                        __in const struct sockaddr *to,
                        __inout mbuf_t *data,
                        __inout mbuf_t *control,
                        __in sflt_data_flag_t flags );
    
    errno_t	FltDataIn( __in socket_t so,
                       __in const struct sockaddr *from,
                       __inout mbuf_t *data,
                       __inout mbuf_t *control,
                       __in sflt_data_flag_t flags );
    
    void reinjectDeferredData( __in DldSocketDataDirectionType  injectType );
    
    bool checkForInjectionCompletion( __in DldSocketDataDirectionType  injectType, __in bool waitForCompletion );
    void wakeupWaitingFotInjectionCompletion();
    
    static void DeliverWaitingNotifications();
    
    //
    // if false is returned by acquireDetachingLock the socket is invalid
    //
    bool acquireDetachingLock();
    void releaseDetachingLock();
    void acquireDetachingLockForRemoval();
    
    void setDeferredDataProperties( __in DldSocketDataProperty*  property );
    
    void setRemoteAddress( __in const struct sockaddr *remote );
    void getRemoteAddress( __inout DldSocketObjectAddress* address );
    
    void setLocalAddress( __in const struct sockaddr *local );
    void getLocalAddress( __inout DldSocketObjectAddress* address );
    
    bool isLocalAddressValid() { return ( AF_INET == this->sa_family ) ? 
                                            ( 0x0 != this->localAddress.addr4.sin_len ) :
                                            ( 0x0 != this->localAddress.addr6.sin6_len ); }
    
    bool isRemoteAddressValid() { return ( AF_INET == this->sa_family ) ? 
                                            ( 0x0 != this->remoteAddress.addr4.sin_len ) :
                                            ( 0x0 != this->remoteAddress.addr6.sin6_len ); }
    
    sa_family_t getProtocolFamily() { return this->sa_family; }
    
    void getSocketId( __inout DldSocketID* outSocketId ){ *outSocketId = this->socketId; };
    UInt64 getSocketSequence() { return this->socketId.socketSequence; };
    
    void markAsDisconnected() { this->disconnected = true; };
    bool isDisconnected() { return this->disconnected; };
};

//--------------------------------------------------------------------

inline
bool 
DldIsSocketObjectInList( __in socket_t so )
{
    DldSocketObject* soObj;
    soObj = DldSocketObject::GetSocketObjectRef( so );
    if( soObj )
        soObj->release();
    
    return ( NULL != soObj );
}

//--------------------------------------------------------------------

//
// the returned object is not referenced
//
inline
DldSocketObject* DldCookieToSocketObject( __in void* cookie )
{
    DldSocketObject*   sockObj = (DldSocketObject*)cookie;
    
    assert( SOCKET_OBJECT_SIGNATURE == sockObj->signature );
    
    return sockObj;
}

//--------------------------------------------------------------------

inline
void* DldSocketObjectToCookie( __in DldSocketObject* sockObj )
{    
    assert( SOCKET_OBJECT_SIGNATURE == sockObj->signature );
    
    return (void*)sockObj;
}

//--------------------------------------------------------------------

#endif // _DLDSOCKETOBJECT_H