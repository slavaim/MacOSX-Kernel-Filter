/* 
 * Copyright (c) 2012 Slava Imameev. All rights reserved.
 */

#ifndef _DLDSOCKETFILTER_H
#define _DLDSOCKETFILTER_H

#include <mach/vm_types.h>
#include <mach/kmod.h>
#include <sys/socket.h>
#include <sys/kpi_socket.h>
#include <sys/kpi_mbuf.h>
#include <sys/kpi_socket.h>
#include <sys/kpi_socketfilter.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <netinet/in.h>
#include <kern/locks.h>
#include <kern/assert.h>
#include <kern/debug.h>

#include <libkern/OSMalloc.h>
#include <libkern/OSAtomic.h>
#include <sys/kern_control.h>
#include <sys/kauth.h>
#include <sys/time.h>
#include <stdarg.h>

#include "DldCommon.h"
#include "DldIOService.h"
#include "DldHookerCommonClass.h"
#include "DldIOUserClient.h"
#include "DldDataBuffer.h"

class DldSocketFilter: public OSObject{
    
    OSDeclareDefaultStructors( DldSocketFilter )
    
private:
    
    //
    // Dispatch vectors for IPv4 and IPv6 socket functions
    //
    static struct sflt_filter SfltIPv4;
    static struct sflt_filter SFltIPv6;
    
    //
    // tag associated with this kext for use in marking packets that have been previously processed.
    //
    static mbuf_tag_id_t gidtag; 
    
    bool    filterForIPv4IsRegistered;
    bool    filterForIPv6IsRegistered;
    
    //
    // a user client for the kernel-to-user communication
    //
    DldIOUserClientRef userClient;
    
    //
    // an array of DldDataBuffer objects that are used for passing data to protocol dissectors and CAWL
    //
    OSArray*  dataBuffers;
    
    //
    // an array that is used to create a free buffers list, UINT8_MAX means the end of the list,
    // the interlocked functions can't be used with this array as the array antry is 8 bit value while
    // all interlocked functions require 32 or 64 bit value
    //
    UInt8    freeBuffers[ kt_DldSocketBuffersNumber ];
    
    //
    // an index of the first free buffer in the freeBuffers array, UINT_MAX is an empty list,
    // it is important to note that this value must be of 32 bit size as it is used by interlocked
    // functions that accept a pointer to 32 bit value
    //
    UInt32    freeBuffersHead;
    
public:
    
    //
    // a caller must call releaseUserClient() for each successfull call to getUserClient()
    //
    DldIOUserClient* getUserClient();
    void releaseUserClient();
    
protected:
    
    /*!
        @typedef FltUnregisteredIPv4
     
        @discussion sf_unregistered_func is called to notify the filter it
     has been unregistered. This is the last function the stack will
     call and this function will only be called once all other
     function calls in to your filter have completed. Once this
     function has been called, your kext may safely unload.
        @param handle The socket filter handle used to identify this filter.
     */
    static void
    FltUnregisteredIPv4(sflt_handle handle);
    
    static void
    FltUnregisteredIPv6(sflt_handle handle);
    
    /*!
        @typedef FltAttachIPv4
     
        @discussion sf_attach_func is called to notify the filter it has
     been attached to a new TCP socket. The filter may allocate memory for
     this attachment and use the cookie to track it. This filter is
     called in one of two cases:
     1) You've installed a global filter and a new socket was created.
     2) Your non-global socket filter is being attached using the SO_NKE
     socket option.
        @param cookie Used to allow the socket filter to set the cookie for
     this attachment.
        @param so The socket the filter is being attached to.
        @result If you return a non-zero value, your filter will not be
     attached to this socket.
     */
    
    static errno_t
    FltAttachIPv4(void **cookie, socket_t so);
    
    static errno_t
    FltAttachIPv6(void **cookie, socket_t so);
    
    static errno_t
    FltAttach(void **cookie, socket_t so, sa_family_t sa_family);
    
    /*
        @typedef FltDetachIPv4
     
        @discussion sf_detach_func is called to notify the filter it has
     been detached from a socket. If the filter allocated any memory
     for this attachment, it should be freed. This function will
     be called when the socket is disposed of.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @result If you return a non-zero value, your filter will not be
     attached to this socket.
     */
    static void	
    FltDetachIPv4(void *cookie, socket_t so);
    
    static void	
    FltDetachIPv6(void *cookie, socket_t so);
    
    static void	
    FltDetach(void *cookie, socket_t so);
    
    /*
        @typedef FltNotify
     
        @discussion FltNotify is called to notify the filter of various
     state changes and other events occuring on the socket.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param event The type of event that has occurred.
        @param param Additional information about the event.
     */
    static void	
    FltNotify(void *cookie, socket_t so, sflt_event_t event, void *param);
    
    /* 
     Notes for both the FltDataIn and the FltDataOut implementations.
     For these functions, the mbuf_t parameter is passed by reference. The kext can 
     manipulate the mbuf such as prepending an mbuf_t, splitting the mbuf and saving some
     tail portion of data, etc. As a reminder, you are responsible to ensure that
     data is processed in the correct order that is is received. If the kext splits the 
     mbuf_t, and returns the lead portion of data, then return KERN_SUCCESS, even though
     the nke swallows the tail portion.
     */
    /*!
        @typedef FltDataIn
     
        @discussion FltDataIn is called to filter incoming data. If
     your filter intercepts data for later reinjection, it must queue
     all incoming data to preserve the order of the data. Use
     sock_inject_data_in to later reinject this data if you return
     EJUSTRETURN. Warning: This filter is on the data path, do not
     block or spend excessive time.
        @param cookie Cookie value specified when the filter attach was
     called.
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
    static errno_t	
    FltDataIn(void *cookie, socket_t so, const struct sockaddr *from,
              mbuf_t *data, mbuf_t *control, sflt_data_flag_t flags);
    
    
    /*!
        @typedef FltDataOut
     
        @discussion FltDataOut is called to filter outbound data. If
     your filter intercepts data for later reinjection, it must queue
     all outbound data to preserve the order of the data when
     reinjecting. Use sock_inject_data_out to later reinject this
     data. Warning: This filter is on the data path, do not block or
     spend excessive time.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param from The address the data is from, may be NULL if the socket
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
    static errno_t	
    FltDataOut(void *cookie, socket_t so, const struct sockaddr *to, mbuf_t *data,
                   mbuf_t *control, sflt_data_flag_t flags);
    
    /*
        @typedef FltConnectIn
     
        @discussion FltConnectIn is called to filter data_inbound
     connections. A protocol will call this before accepting an
     incoming connection and placing it on the queue of completed
     connections. Warning: This filter is on the data path, do not
     block or spend excesive time.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param from The address the incoming connection is from.
        @result Return:
     0 - The caller will continue with normal processing of the connection.
     Anything Else - The caller will rejecting the incoming connection.
     */
    static errno_t	
    FltConnectIn(void *cookie, socket_t so, const struct sockaddr *from);
    
    /*!
        @typedef FltConnectOut
     
        @discussion FltConnectOut is called to filter outbound
     connections. A protocol will call this before initiating an
     outbound connection. Warning: This filter is on the data path,
     do not block or spend excesive time.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param to The remote address of the outbound connection.
        @result Return:
     0 - The caller will continue with normal processing of the connection.
     Anything Else - The caller will rejecting the outbound connection.
     */
    static errno_t	
    FltConnectOut(void *cookie, socket_t so, const struct sockaddr *to);
    
    /*!
        @typedef FltBind
     
        @discussion FltBind is called before performing a bind
     operation on a socket.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param to The local address of the socket will be bound to.
        @result Return:
     0 - The caller will continue with normal processing of the bind.
     Anything Else - The caller will rejecting the bind.
     */
    static errno_t	
    FltBind(void *cookie, socket_t so, const struct sockaddr *to);
    
    /*!
        @typedef sf_setoption_func
     
        @discussion sf_setoption_func is called before performing setsockopt
     on a socket.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param opt The socket option to set.
        @result Return:
     0 - The caller will continue with normal processing of the setsockopt.
     Anything Else - The caller will stop processing and return this error.
     */
    static errno_t	
    FltSetoption(void *cookie, socket_t so, sockopt_t opt);
    
    /*!
        @typedef FltGetoption
     
        @discussion FltGetoption is called before performing getsockopt
     on a socket.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @param opt The socket option to get.
        @result Return:
     0 - The caller will continue with normal processing of the getsockopt.
     Anything Else - The caller will stop processing and return this error.
     */
    static errno_t
    FltGetoption(void *cookie, socket_t so, sockopt_t opt);
    
    /*!
        @typedef FltListen
     
        @discussion FltListen is called before performing listen
     on a socket.
        @param cookie Cookie value specified when the filter attach was
     called.
        @param so The socket the filter is attached to.
        @result Return:
     0 - The caller will continue with normal processing of listen.
     Anything Else - The caller will stop processing and return this error.
     */
    static errno_t	
    FltListen(void *cookie, socket_t so);
    
    static errno_t
    FltConnect( __in void *cookie,
                __in socket_t so,
                __in_opt const struct sockaddr *to,
                __in_opt const struct sockaddr *from );
    
private:
    
    static DldSocketFilter*  CurrentFilter;
    
protected:
    
    virtual void free();
    
public:
    
    virtual errno_t startFilter();
    virtual errno_t stopFilter();
    
    static DldSocketFilter* withDefault();
    static errno_t InitSocketFilterSubsystem();
    
    virtual bool isUserClientPresent();
    virtual IOReturn registerUserClient( __in DldIOUserClient* client );
    virtual IOReturn unregisterUserClient( __in DldIOUserClient* client );
    
    //
    // returns a referenced object or NULL
    //
    virtual IOMemoryDescriptor* getSocketBufferMemoryDescriptor( __in UInt32 index );
    
    //
    // returns a referenced buffer object or NULL,
    // a caller must release the object
    //
    virtual DldDataBuffer* acquireSocketDataBufferForIO();
    
    //
    // releases the buffer from IO thus allow it acquiring for another IO,
    // a caller must use the index returned by buffer's getIndex() method
    //
    virtual void releaseSocketDataBufferFromIO( __in UInt32 index );
    
    //
    // a caller must eventually release all buffers by calling releaseSocketDataBufferFromIO()
    //
    errno_t copyDataToBuffers( __in const mbuf_t mbuf,
                               __inout UInt8*    bufferIndices // an array bufferIndices[ kt_DldSocketBuffersNumber ]
                             );
    
    //
    // releases buffers acquired by copyDataToBuffers
    //
    void releaseDataBuffers( __inout UInt8*    bufferIndices // an array bufferIndices[ kt_DldSocketBuffersNumber ]
                            );
    void releaseDataBuffersAndDeliverNotifications( __inout UInt8*    bufferIndices // an array bufferIndices[ kt_DldSocketBuffersNumber ]
                            );
    IOReturn processServiceResponse( __in DldSocketFilterServiceResponse*  response );
    
};

extern DldSocketFilter*     gSocketFilter;

//--------------------------------------------------------------------

#define  INIT_SOCKET_NOTIFICATION( _notification, _socketObj, _event ) \
    bzero( &_notification, sizeof( _notification ) ); \
    _notification.size = sizeof( _notification ); \
    _notification.event = _event; \
    _notification.flags.separated.notificationForDisconnectedSocket = ( _socketObj->isDisconnected() ) ? 0x1 : 0x0; \
    _socketObj->getSocketId( &_notification.socketId ); \
    if( DldSocketFilterEventDataIn == _event || DldSocketFilterEventDataOut == _event ){ \
        notification.eventData.inputoutput.buffers[ 0 ] = UINT8_MAX;\
    }

//--------------------------------------------------------------------

#endif // _DLDSOCKETFILTER_H