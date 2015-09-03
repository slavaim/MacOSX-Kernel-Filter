/* 
 * Copyright (c) 2012 Slava Imameev. All rights reserved.
 */

/*
   Theory of operation:
  
   At init time, we add ourselve to the list of socket filters for TCP.
   For each new connection (active or passive), we log the endpoint
   addresses, keep track of the stats of the conection and log the
   results and close time.
  
   As packet swallowing is enabled, for both the data_in 
   and data_out functions, all unprocessed packets are tagged and swallowed - 
   that is, these routines return the EJUSTRETURN result which tells the system
   to stop processing of the packet.
   
   When swallowing a packet, save the mbuf_t value, not the reference value passed to the
   sf_data_in_func/sf_data_out_func. The reference value to the mbuf_t parameter is no 
   longer valid upon return from the sf_data_in_func/sf_data_out_func.
 
   In the datain/out functions, as the mbuf_t is passed by reference, the NKE can split the
	contents of the mbuf_t, allowing the lead portion of the kext to be processed, and swallowing
    the tail portion. Other modifications can also be made to the mbuf_t.
   
   This sample also implements the use of the "Fine Grain Locking" api's provided in
   locks.h, as well as the OSMalloc call defined in <libkern/OSMalloc.h>.
   
   Note the following 
   a. When calling OSMalloc, the WaitOK version can be used since memory allocation will
		not block packet processing.
   b. With fine grain locking support now present in the kernel, the Network funnel is no
		longer needed to serialize network access. The "Fine grain locking" calls are used to
		serialize access to the various queues defined to log socket information,
		control connection information, and to stored data_inbound and outbound swallowed packets.
		A call to lck_mtx_lock blocks until the mutex parameter is freed with a call to 
		lck_mtx_unlock.
		
 */

//--------------------------------------------------------------------

#include "DldSocketFilter.h"
#include "DldSocketObject.h"

#define DLD_SOCK_FLT_HANDLE_IP4  0xFEBCD987
#define DLD_SOCK_FLT_HANDLE_IP6  0xFEBCD789

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldSocketFilter, OSObject )

DldSocketFilter*  DldSocketFilter::CurrentFilter = NULL;

mbuf_tag_id_t	DldSocketFilter::gidtag;

//--------------------------------------------------------------------

errno_t DldSocketFilter::InitSocketFilterSubsystem()
{
    errno_t  error;
    
    // set up the tag value associated with this NKE in preparation for swallowing packets and re-injecting them
	error = mbuf_tag_id_find( "DLDriverNKE" , &DldSocketFilter::gidtag );
    assert( KERN_SUCCESS == error );
	if( KERN_SUCCESS != error ){
        
		DBG_PRINT_ERROR(("mbuf_tag_id_find returned error %d\n", error));
		return error;
	}
    
    error = DldSocketObject::InitSocketObjectsSubsystem();
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error ){
        
        DBG_PRINT_ERROR(("InitSocketObjectsSubsystem() failed with error = %d\n", error));
        return error;
    }
    
    return error;
}

//--------------------------------------------------------------------

DldSocketFilter* DldSocketFilter::withDefault()
{
    DldSocketFilter*  newFilter;
    
    newFilter = new DldSocketFilter();
    assert( newFilter );
    if( ! newFilter ){
        
        DBG_PRINT_ERROR(("new DldSocketFilter() failed\n"));
        return NULL;
    }
    
    if( ! newFilter->init() ){
        
        DBG_PRINT_ERROR(("newFilter->init() failed\n"));
        newFilter->release();
        return NULL;
    }
    
    //
    // create an empty array for buffer objects
    //
    newFilter->dataBuffers = OSArray::withCapacity( kt_DldSocketBuffersNumber );
    assert( newFilter->dataBuffers );
    if( ! newFilter->dataBuffers ){
        
        DBG_PRINT_ERROR(( "OSArray::withCapacity() fauled\n" ));
        newFilter->release();
        return NULL;
    }
    
    //
    // set a free buffer list
    //
    newFilter->freeBuffersHead = UINT8_MAX;
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( newFilter->freeBuffers ); ++i ){
        
        newFilter->freeBuffers[ i ] = UINT8_MAX;
    } // end for
    
    //
    // create the buffers
    //
    for( int i = 0x0; i < newFilter->dataBuffers->getCapacity(); ++i ){
        
        //
        // create a buffer, in case of 40 buffers each one will be of 64 KB size
        //
        DldDataBuffer*  dataBuffer = DldDataBuffer::withSize( 0x280000/kt_DldSocketBuffersNumber, (UInt32)i );
        assert( dataBuffer );
        if( ! dataBuffer ){
            
            DBG_PRINT_ERROR(( "DldDataBuffer::withSize() failed\n" ));
            break;
        }
        
        //
        // insert in the list of free buffers
        //
        newFilter->freeBuffers[ i ] = newFilter->freeBuffersHead;
        newFilter->freeBuffersHead = i;
        
        //
        // add the buffer in the array
        //
        newFilter->dataBuffers->setObject( dataBuffer );
        
        //
        // the object is retained by the array
        //
        dataBuffer->release();
    } // end for
    
    return newFilter;
}

//--------------------------------------------------------------------

void DldSocketFilter::free()
{
    if( this->dataBuffers ){
        
        this->dataBuffers->flushCollection();
        this->dataBuffers->release();
    }
    
    super::free();
}

//--------------------------------------------------------------------

/* Dispatch vector for IPv4 socket functions */
struct sflt_filter DldSocketFilter::SfltIPv4 = {
	DLD_SOCK_FLT_HANDLE_IP4,/* sflt_handle - use a registered creator type - <http://developer.apple.com/datatype/> */
	SFLT_GLOBAL,			/* sf_flags */
	"DLDriverNKE",		    /* sf_name - cannot be nil else param err results */
	FltUnregisteredIPv4,	/* sf_unregistered_func */
	FltAttachIPv4,          /* sf_attach_func - cannot be nil else param err results */			
	FltDetachIPv4,			/* sf_detach_func - cannot be nil else param err results */
	FltNotify,              /* sf_notify_func */
	NULL,					/* sf_getpeername_func */
	NULL,					/* sf_getsockname_func */
	FltDataIn,              /* sf_data_in_func */
	FltDataOut,             /* sf_data_out_func */
	FltConnectIn,           /* sf_connect_in_func */
	FltConnectOut,          /* sf_connect_out_func */
	FltBind,				/* sf_bind_func */
	FltSetoption,           /* sf_setoption_func */
	FltGetoption,           /* sf_getoption_func */
	FltListen,              /* sf_listen_func */
	NULL					/* sf_ioctl_func */
};

/* Dispatch vector for IPv6 socket functions */
struct sflt_filter DldSocketFilter::SFltIPv6 = {
	DLD_SOCK_FLT_HANDLE_IP6,/* sflt_handle - use a registered creator type - <http://developer.apple.com/datatype/> */
	SFLT_GLOBAL,			/* sf_flags */
	"DLDriverNKE",		    /* sf_name - cannot be nil else param err results */
	FltUnregisteredIPv6,	/* sf_unregistered_func */
	FltAttachIPv6,          /* sf_attach_func - cannot be nil else param err results */			
	FltDetachIPv6,			/* sf_detach_func - cannot be nil else param err results */
	FltNotify,              /* sf_notify_func */
	NULL,					/* sf_getpeername_func */
	NULL,					/* sf_getsockname_func */
	FltDataIn,              /* sf_data_in_func */
	FltDataOut,             /* sf_data_out_func */
	FltConnectIn,           /* sf_connect_in_func */
	FltConnectOut,          /* sf_connect_out_func */
	FltBind,				/* sf_bind_func */
	FltSetoption,           /* sf_setoption_func */
	FltGetoption,           /* sf_getoption_func */
	FltListen,              /* sf_listen_func */
	NULL					/* sf_ioctl_func */
};

//--------------------------------------------------------------------

errno_t DldSocketFilter::startFilter()
{
    errno_t   status = KERN_SUCCESS;
    
    //
    // only one attached filter at time
    //
    if( ! OSCompareAndSwapPtr( NULL, this, &DldSocketFilter::CurrentFilter ) ){
        
        assert( !"an attempt to register more than one socket filter" );
        DBG_PRINT_ERROR(("an attempt to register more than one socket filter\n"));
        
        return EBUSY;
    }
    
    //
    // retain the current filter
    //
    this->retain();
    
    /* Register the NKE */
	// register the filter with AF_INET domain, SOCK_STREAM type, TCP protocol and set the global flag
	status = sflt_register( &DldSocketFilter::SfltIPv4, PF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( KERN_SUCCESS == status ){
        
		this->filterForIPv4IsRegistered = true;
        DLD_COMM_LOG(COMMON,("TCP/IPv4 filter has been registered\n"));
        
	} else {
        
		DBG_PRINT_ERROR(("sflt_register( &DldSocketFilter::SfltIPv4, PF_INET, SOCK_STREAM, IPPROTO_TCP ) fauled with a status = %d\n",status));
    }
	
	status = sflt_register( &DldSocketFilter::SFltIPv6, PF_INET6, SOCK_STREAM, IPPROTO_TCP );
	if( KERN_SUCCESS == status ){
        
		this->filterForIPv6IsRegistered = true;
        DLD_COMM_LOG(COMMON,("TCP/IPv6 filter has been registered\n"));
        
    } else {
        
        DBG_PRINT_ERROR(("sflt_register( &DldSocketFilter::SFltIPv6, PF_INET, SOCK_STREAM, IPPROTO_TCP ) fauled with a status = %d\n",status));
    }
    
    return status;
}

//--------------------------------------------------------------------

errno_t DldSocketFilter::stopFilter()
{
    errno_t    error = KERN_SUCCESS;
    
    if( this != DldSocketFilter::CurrentFilter ){
        
        assert( !"an attempt ot unregister socket filter by an object that has not been registered a sa filter" );
        DBG_PRINT_ERROR(("an attempt ot unregister socket filter by an object that has not been registered a sa filter\n"));
        
        return EPERM;
    }
    
    if( this->filterForIPv4IsRegistered ){
        
        error = sflt_unregister( DLD_SOCK_FLT_HANDLE_IP4 );
        if( KERN_SUCCESS != error ){
            
            DBG_PRINT_ERROR(("sflt_unregister( DLD_SOCK_FLT_HANDLE_IP4 ) failed with a statue = %d", error));
            return error;
        }
        
        this->filterForIPv4IsRegistered = false;
        DLD_COMM_LOG(COMMON,("TCP/IPv4 filter has been unregistered\n"));
        
    } // end if( this->filterForIPv4IsRegistered )
    
    if( this->filterForIPv6IsRegistered ){
        
        error = sflt_unregister( DLD_SOCK_FLT_HANDLE_IP4 );
        if( KERN_SUCCESS != error ){
            
            DBG_PRINT_ERROR(("sflt_unregister( DLD_SOCK_FLT_HANDLE_IP4 ) failed with a statue = %d", error));
            return error;
        }
        
        this->filterForIPv6IsRegistered = false;
        DLD_COMM_LOG(COMMON,("TCP/IPv6 filter has been unregistered\n"));
        
    } // end if( if( this->filterForIPv6IsRegistered ){
    
    //
    // the object was retained when it was registered as a socket filter
    //
    this->release();
    
    DldSocketFilter::CurrentFilter = NULL;
    
    assert( KERN_SUCCESS == error );
    return error;
}

//--------------------------------------------------------------------

void
DldSocketFilter::FltUnregisteredIPv4(sflt_handle handle)
{
}

//--------------------------------------------------------------------

void
DldSocketFilter::FltUnregisteredIPv6(sflt_handle handle)
{
}

//--------------------------------------------------------------------

errno_t
DldSocketFilter::FltAttach(void **cookie, socket_t so, sa_family_t sa_family)
{
    //assert( ! DldIsSocketObjectInList( so ) );
    
    //
    // do not attach if the socket filter has not been initialized
    //
    if( ! gSocketFilter )
        return ENOENT;
    
    //
    // it happened that sometimes the detach callback was not called, the reasons are still not known
    //
    if( DldIsSocketObjectInList( so ) ){
        
        DldSocketObject* oldSoObj;
        oldSoObj = DldSocketObject::GetSocketObjectRef( so );
        if( oldSoObj ){
            
            DldSocketFilter::FltDetach( DldSocketObjectToCookie( oldSoObj ), so );
            oldSoObj->release();
            DLD_DBG_MAKE_POINTER_INVALID( oldSoObj );
        }
    }
    
    DldSocketObject* soObj;
        
    soObj = DldSocketObject::withSocket( so, DldSocketFilter::gidtag, sa_family );
    assert( soObj );
    if( soObj ){
        
        soObj->insertInSocketsList();
        
        //
        // release the object, the sockets list takes its own reference
        //
        soObj->release();
        
    } else {
        
        DBG_PRINT_ERROR(("DldSocketObject::withSocket failed\n"));
        return ENOMEM;
    }
    
    *cookie = DldSocketObjectToCookie( soObj );
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

errno_t
DldSocketFilter::FltAttachIPv4(void **cookie, socket_t so)
{
    return FltAttach( cookie, so, AF_INET );
}

//--------------------------------------------------------------------

errno_t
DldSocketFilter::FltAttachIPv6(void **cookie, socket_t so)
{

    return FltAttach( cookie, so, AF_INET6 );
}

//--------------------------------------------------------------------

void	
DldSocketFilter::FltDetach(void *cookie, socket_t so)
{
    DldSocketObject* soObj = DldCookieToSocketObject( cookie );
    
    assert( DldIsSocketObjectInList( so ) && soObj->toSocket() == so );
    assert( preemption_enabled() );
    
    //
    // removing from the queue releases the object,
    // so take the reference on the object
    //
    soObj->retain();
    {
        soObj->removeFromSocketsList();
        
        //
        // wait for the last user completion,
        // this prevents the further lock acquiring
        //
        soObj->acquireDetachingLockForRemoval();
        
        //
        // remove all stalled data
        //
        soObj->purgePendingQueue( DldSocketObject::DldSocketDataAll );
        
        //
        // wait for all pending and purging operations completion
        //
        soObj->checkForInjectionCompletion( DldSocketObject::DldSocketDataAll, true );
    }
    soObj->release();
    DLD_DBG_MAKE_POINTER_INVALID( soObj );
    // soObj is invalid after it was dereferenced
}

//--------------------------------------------------------------------

void	
DldSocketFilter::FltDetachIPv4(void *cookie, socket_t so)
{
    FltDetach( cookie, so );
}

//--------------------------------------------------------------------

void	
DldSocketFilter::FltDetachIPv6(void *cookie, socket_t so)
{
    FltDetach( cookie, so );
}

//--------------------------------------------------------------------

void	
DldSocketFilter::FltNotify(void *cookie, socket_t so, sflt_event_t event, void *param)
{
    /*
     an example of the events sequence for a socket being destroyed
     flags = {
     insertedInSocketsList = 0, 
     f_sock_evt_connecting = 1, 
     f_sock_evt_connected = 1, 
     f_sock_evt_disconnecting = 1, 
     f_sock_evt_disconnected = 1, 
     f_sock_evt_flush_read = 0, 
     f_sock_evt_shutdown = 0, 
     f_sock_evt_cantrecvmore = 0, 
     f_sock_evt_cantsendmore = 0, 
     f_sock_evt_closing = 1, 
     f_sock_evt_bound = 1
     }, 
     nextEventsLogPosition = 7, 
     eventsLog = {10, 1, 2, 9, 3, 4, 3, 0 <repeats 13 times>}, 
     */
    
    /*
     a possible(one of many) stack for disconnect event callback ( sock_evt_disconnected )
     #0  DldSocketObject::logSocketEvent (this=0x8796200, event=4) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketObject.cpp:300
     #1  0x47183cf8 in DldSocketFilter::FltNotify (cookie=0x0, so=0x8cdf000, event=4, param=0x0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:268
     #2  0x004c44dc in sflt_notify (so=0x8cdf000, event=4, param=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:140
     #3  0x004b1ac1 in soisdisconnected (so=0x8cdf000) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket2.c:219
     #4  0x00355f63 in tcp_close (tp=0x8cdf274) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_subr.c:965
     #5  0x003513dd in tcp_input (m=0x2dcafb00, off0=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_input.c:2767
     #6  0x003473f6 in ip_proto_dispatch_in (m=0x2dcafb00, hlen=20, proto=6 '\006', inject_ipfref=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:613
     #7  0x00348a95 in ip_input (m=0x2dcafb00) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:1372
     #8  0x00348bbc in ip_proto_input (protocol=2, packet_list=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:519
     #9  0x0032f121 in proto_input (protocol=2, packet_list=0x2dcafb00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/kpi_protocol.c:298
     #10 0x0031b7c1 in ether_inet_input (ifp=0x5ebf204, protocol_family=2, m_list=0x2dcafb00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/ether_inet_pr_module.c:215
     #11 0x00317a3c in dlil_ifproto_input (ifproto=0x60d06c4, m=0x2dcafb00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1079
     #12 0x0031a1b4 in dlil_input_packet_list (ifp_param=0x0, m=0x2dcafb00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1197
     #13 0x0031a422 in dlil_input_thread_func (inputthread=0x5def184) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:878         
     */
    
    /*
     a stack for closing event callback ( sock_evt_closing )
     #2  0x468e9ce3 in DldSocketFilter::FltNotify (cookie=0x0, so=0x5215990, event=9, param=0x0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:268
     #3  0x004c44dc in sflt_notify (so=0x5215990, event=9, param=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:140
     #4  0x004ad2f6 in soclose_locked (so=0x5215990) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:892
     #5  0x004ad646 in soclose (so=0x5215990) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket.c:1033
     #6  0x004696fc in fo_close [inlined] () at :4869
     #7  0x004696fc in closef_finish [inlined] () at :4068
     #8  0x004696fc in closef_locked (fp=0x560d2c0, fg=0x52691a0, p=0x67d8a80) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:4167
     #9  0x0046b4ad in close_internal_locked (p=0x67d8a80, fd=4, fp=0x560d2c0, flags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:1950
     #10 0x0046b57e in close_nocancel (p=0x67d8a80, uap=0x67eb170, retval=0x67eb1b4) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_descrip.c:1852
     #11 0x004ed78d in unix_syscall (state=0x670c0e0) at /SourceCache/xnu/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:205         
     */
    
    DldSocketObject* soObj = DldSocketObject::GetSocketObjectRef( so );
    if( soObj ){
        
        soObj->logSocketPreEvent( event );
        
        DldIOUserClient* userClient = gSocketFilter->getUserClient();
        
        switch( event ){
                // a comment from the kernel sources describing scenarious for sock_evt_connecting
                // and sock_evt_connected events
                /*
                 * Procedures to manipulate state flags of socket
                 * and do appropriate wakeups.  Normal sequence from the
                 * active (originating) side is that soisconnecting() is
                 * called during processing of connect() call,
                 * resulting in an eventual call to soisconnected() if/when the
                 * connection is established.  When the connection is torn down
                 * soisdisconnecting() is called during processing of disconnect() call,
                 * and soisdisconnected() is called when the connection to the peer
                 * is totally severed.  The semantics of these routines are such that
                 * connectionless protocols can call soisconnected() and soisdisconnected()
                 * only, bypassing the in-progress calls when setting up a ``connection''
                 * takes no time.
                 *
                 * From the passive side, a socket is created with
                 * two queues of sockets: so_incomp for connections in progress
                 * and so_comp for connections already made and awaiting user acceptance.
                 * As a protocol is preparing incoming connections, it creates a socket
                 * structure queued on so_incomp by calling sonewconn().  When the connection
                 * is established, soisconnected() is called, and transfers the
                 * socket structure to so_comp, making it available to accept().
                 *
                 * If a socket is closed with sockets on either
                 * so_incomp or so_comp, these sockets are dropped.
                 *
                 * If higher level protocols are implemented in
                 * the kernel, the wakeups done here will sometimes
                 * cause software-interrupt process scheduling.
                 */
            case sock_evt_connecting:
            {
                //
                // set the local address if it was not set by bind()
                //
                if( ! soObj->isLocalAddressValid() ){
                    
                    DldSocketObjectAddress  localAddress;
                    errno_t  error;
                    
                    error = sock_getsockname(so, (struct sockaddr*)&localAddress, sizeof(localAddress));
                    assert( 0x0 == error );
                    if( 0x0 == error ){
                        
                        soObj->setLocalAddress((const struct sockaddr*)&localAddress);
                    } else {
                        
                        DBG_PRINT_ERROR(("sock_getsockname() failed with an error = %d\n", error));
                    }
                    
                } // end if( ! soObj->isLocalAddressValid() )
                break;
            }
                
            case sock_evt_connected:
            {
                //
                // this is the place where we have the valid local and remote addresses
                // and notify a client about a new socket
                //
                /*
                 #0  DldSocketFilter::FltNotify (cookie=0x8d50300, so=0x5e08060, event=2, param=0x0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:599
                 #1  0x004c44dc in sflt_notify (so=0x5e08060, event=2, param=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:140
                 #2  0x004b1bc2 in soisconnected (so=0x5e08060) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/uipc_socket2.c:176
                 #3  0x00350a09 in tcp_input (m=0x2ddade00, off0=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_input.c:2312
                 #4  0x003473f6 in ip_proto_dispatch_in (m=0x2ddade00, hlen=20, proto=6 '\006', inject_ipfref=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:613
                 #5  0x00348a95 in ip_input (m=0x2ddade00) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:1372
                 #6  0x00348bbc in ip_proto_input (protocol=2, packet_list=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:519
                 #7  0x0032f121 in proto_input (protocol=2, packet_list=0x2ddade00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/kpi_protocol.c:298
                 #8  0x0031be3c in lo_input (ifp=0x5a61004, protocol_family=2, m=0x2ddade00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/if_loop.c:296
                 #9  0x00317a3c in dlil_ifproto_input (ifproto=0x5bf5ec4, m=0x2ddade00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1079
                 #10 0x0031a1b4 in dlil_input_packet_list (ifp_param=0x5a61004, m=0x2ddade00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1197
                 #11 0x0031a3f5 in dlil_input_thread_func (inputthread=0x875000) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:868                 
                 */
                
                //
                // set the local address
                //
                if( ! soObj->isLocalAddressValid() ){
                    
                    DldSocketObjectAddress  localAddress;
                    errno_t  error;
                    
                    error = sock_getsockname(so, (struct sockaddr*)&localAddress, sizeof(localAddress));
                    assert( 0x0 == error );
                    if( 0x0 == error ){
                        
                        soObj->setLocalAddress((const struct sockaddr*)&localAddress);
                    } else {
                        
                        DBG_PRINT_ERROR(("sock_getsockname() failed with an error = %d\n", error));
                    }
                    
                } // end if( ! soObj->isLocalAddressValid() )
                
                //
                // set the remote address
                //
                if( ! soObj->isRemoteAddressValid() ){
                    
                    DldSocketObjectAddress  remoteAddress;
                    errno_t  error;
                    
                    error = sock_getpeername(so, (struct sockaddr*)&remoteAddress, sizeof(remoteAddress));
                    assert( 0x0 == error );
                    if( 0x0 == error ){
                        
                        soObj->setRemoteAddress((const struct sockaddr*)&remoteAddress);
                    } else {
                        
                        DBG_PRINT_ERROR(("sock_getpeername() failed with an error = %d\n", error));
                    }
                    
                } // end if( ! soObj->isLocalAddressValid() )
                
                //
                // send a notification to a client
                //
                if( userClient ){
                    
                    DldSocketFilterNotification  notification;
                    IOReturn  RC;
                    
                    INIT_SOCKET_NOTIFICATION( notification, soObj, DldSocketFilterEventConnected );
                    
                    soObj->getLocalAddress( &notification.eventData.connected.localAddress );
                    soObj->getRemoteAddress( &notification.eventData.connected.remoteAddress );
                    notification.eventData.connected.sa_family = soObj->getProtocolFamily();
                    
                    RC = userClient->socketFilterNotification( &notification );
                    assert( kIOReturnSuccess == RC );
                } // end if( userClient )
                
                break;
            }
                
            case sock_evt_closing:
            {
                //
                // there is no point to inject deferred inbound data
                // as the socket is being closed by an application and there will be no read calls,
                // do not call reinjectDeferredData here as the closing event is sent with
                // an acquired socket lock
                //
                soObj->purgePendingQueue( DldSocketObject::DldSocketDataInbound );
                
                //
                // send a notification to a client
                //
                if( userClient ){
                    
                    DldSocketFilterNotification  notification;
                    IOReturn  RC;
                    
                    INIT_SOCKET_NOTIFICATION( notification, soObj, DldSocketFilterEventClosing );
                    
                    RC = userClient->socketFilterNotification( &notification );
                    assert( kIOReturnSuccess == RC );
                } // end if( userClient )
                
                break;
            }
                
            case sock_evt_disconnecting:
            {
                //
                // sock_evt_disconnecting is normally called without a sockte lock held
                // so it is safe to call reinjectDeferredData(), see the accompanying comments
                //
                
                //
                // send all deferred outbound data, the last chance, actually in most cases the inbound
                // data was purged while processing sock_evt_closing
                //
                soObj->reinjectDeferredData( DldSocketObject::DldSocketDataAll );
                soObj->checkForInjectionCompletion( DldSocketObject::DldSocketDataAll, true );
                
                break;
            } // end case sock_evt_closing & sock_evt_disconnecting
                
            case sock_evt_disconnected:
            {
                
                //
                // send a notification to a client
                //
                if( userClient ){
                    
                    DldSocketFilterNotification  notification;
                    IOReturn  RC;
                    
                    INIT_SOCKET_NOTIFICATION( notification, soObj, DldSocketFilterEventDisconnected );
                    
                    //
                    // mark the socket as disconnected after notification initialization
                    // to not pass the disconnect flag with the notification as the service
                    // might ignore notifications with disconnect flag being set
                    //
                    soObj->markAsDisconnected();
                    
                    RC = userClient->socketFilterNotification( &notification );
                    assert( kIOReturnSuccess == RC );
                    
                } else {
                    
                    soObj->markAsDisconnected();
                    
                }// end else for if( userClient )
                break;
            }
                
            case sock_evt_shutdown:
            {
                int how = *(int*)param;
                
                if( SHUT_RDWR == how ){
                    
                    //
                    // the both halves of the connection are being closed, send outbound deferred data only
                    // and discard the left inbound data
                    //
                    soObj->reinjectDeferredData( DldSocketObject::DldSocketDataOutbound );
                    soObj->purgePendingQueue( DldSocketObject::DldSocketDataInbound );
                    
                } else if( SHUT_WR == how ){
                    
                    //
                    // the write half of the connection are being closed, send outbound deferred data only
                    // and left the inbound data intact
                    //
                    soObj->reinjectDeferredData( DldSocketObject::DldSocketDataOutbound );
                    
                } else if( SHUT_RD == how ){
                    
                    //
                    // the read half of the connection are being closed, discard all inbound data
                    //
                    soObj->purgePendingQueue( DldSocketObject::DldSocketDataInbound );
                    
                } else {
                    
                    assert( !"an unknown shutdown howto paramter" );
                    DBG_PRINT_ERROR(("an unknown shutdown howto paramter %d\n", how));
                }
                
                break;
            } // end case sock_evt_shutdown
                
            case sock_evt_cantrecvmore:
            {
                //
                // a peer has sent a FIN signal, this end of the connection will stop accepting data when read returns 0,
                // flush the inbound data - the data will be delivered on next read(s) followed by zero bytes returned
                // by the read, this stops this half of the connection to accept any data. Actually we are exploiting the
                // following code from the soreceive()
                //
                // if (so->so_state & SS_CANTRCVMORE) {
                //    if (m)
                //        goto dontblock;
                //    else
                //        goto release;
                // }
                //
                soObj->reinjectDeferredData( DldSocketObject::DldSocketDataInbound );
                
                break;
            } // end case sock_evt_cantrecvmore
        }
        
        soObj->logSocketPostEvent( event );
        soObj->release();
        DLD_DBG_MAKE_POINTER_INVALID( soObj );
        
        if( userClient ){
            
            gSocketFilter->releaseUserClient();
            DLD_DBG_MAKE_POINTER_INVALID( userClient );
        }
        
    } // end if( soObj )
}

//--------------------------------------------------------------------

/*
 a call stack for data in callback
 #0  DldSocketFilter::FltDataIn (cookie=0x0, so=0x4f560f0, from=0x0, data=0x2a723b14, control=0x0, flags=0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:246
 #1  0x00520273 in sflt_data_in (so=0x4f560f0, from=0x0, data=0x2a723b14, control=0x0, flags=0, filtered=0x2a723a9c) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kpi_socketfilter.c:176
 #2  0x0050c988 in sbappendstream (sb=0x4f56134, m=0x2c28fc00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_socket2.c:724
 #3  0x003abf88 in tcp_input (m=0x2c28fc00, off0=<value temporarily unavailable, due to optimizations>) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/tcp_input.c:2910
 #4  0x003a19e0 in ip_proto_dispatch_in (m=0x2c28fc00, hlen=20, proto=6 '\006', inject_ipfref=0x0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/ip_input.c:613
 #5  0x003a30f8 in ip_input (m=0x2c28fc00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/ip_input.c:1372
 #6  0x003a3228 in ip_proto_input (protocol=2, packet_list=0x0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/ip_input.c:519
 #7  0x00362d7a in proto_input (protocol=2, packet_list=0x2c28fc00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/kpi_protocol.c:298
 #8  0x0034fa9b in lo_input (ifp=0x494c644, protocol_family=2, m=0x2c28fc00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/if_loop.c:296
 #9  0x0034b676 in dlil_ifproto_input (ifproto=0x4d4aa64, m=0x2c28fc00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/dlil.c:1079
 #10 0x0034de13 in dlil_input_packet_list (ifp_param=0x494c644, m=0x2c28fc00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/dlil.c:1197
 #11 0x0034e054 in dlil_input_thread_func (inputthread=0x886900) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/dlil.c:868 
 */
errno_t	
DldSocketFilter::FltDataIn(void *cookie, socket_t so, const struct sockaddr *from,
          mbuf_t *data, mbuf_t *control, sflt_data_flag_t flags)
{
    errno_t  error;
    DldSocketObject* soObj = DldCookieToSocketObject( cookie );
    
    assert( DldIsSocketObjectInList( so ) && soObj->toSocket() == so );
    assert( preemption_enabled() );
    
    error = soObj->FltDataIn( so, from, data, control, flags );
    
	return error;
}

//--------------------------------------------------------------------

/*
 a call stack for data out callback
 #0  DldSocketFilter::FltDataOut (cookie=0x0, so=0x4f5650c, to=0x0, data=0x30cbbe0c, control=0x30cbbe10, flags=0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:253
 #1  0x0050a7c5 in sosend (so=0x4f5650c, addr=0x0, uio=0x30cbbe74, top=0x2c2e7b00, control=0x0, flags=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_socket.c:1802
 #2  0x004f247d in soo_write (fp=0x5af4470, uio=0x30cbbe74, flags=0, ctx=0x30cbbf04) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/sys_socket.c:177
 #3  0x004f017b in dofilewrite (ctx=0x30cbbf04, fp=0x5af4470, bufp=4303596544, nbyte=72, offset=-1, flags=0, retval=0x7ba9c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/sys_generic.c:568
 #4  0x004f02d9 in write_nocancel (p=0xa9c9820, uap=0x7c3feb8, retval=0x7ba9c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/sys_generic.c:458
 #5  0x0054e9fd in unix_syscall64 (state=0x7c3feb4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365 
 
    @typedef DldSocketFilter::FltDataOut
 
    @discussion sf_data_out_func is called to filter outbound data. If
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
errno_t	
DldSocketFilter::FltDataOut(
    __in void *cookie,
    __in socket_t so,
    __in const struct sockaddr *to,
    __inout mbuf_t *data,
    __inout mbuf_t *control,
    __in sflt_data_flag_t flags
    )
{
    errno_t  error;
    DldSocketObject* soObj = DldCookieToSocketObject( cookie );
    
    assert( DldIsSocketObjectInList( so ) && soObj->toSocket() == so );
    assert( preemption_enabled() );
    
    error = soObj->FltDataOut( so, to, data, control, flags );
    
	return error;
}

//--------------------------------------------------------------------

errno_t	
DldSocketFilter::FltConnect(
    __in void *cookie,
    __in socket_t so,
    __in_opt const struct sockaddr *to,
    __in_opt const struct sockaddr *from
    )
{
    //
    // verify that the address is AF_INET/AF_INET6
    //
    assert( ( from && ((from->sa_family == AF_INET) || (from->sa_family == AF_INET6))) ||
            ( to && ((to->sa_family == AF_INET) || (to->sa_family == AF_INET6))) );
    
    DldSocketObject* soObj = DldCookieToSocketObject( cookie );
    
    assert( DldIsSocketObjectInList( so ) && soObj->toSocket() == so );
    assert( preemption_enabled() );
    
    soObj->setRemoteAddress( to ? to : from );
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

/*
 a call stack for connect in callback
 #0  DldSocketFilter::FltConnectIn (cookie=0x0, so=0x6100160, from=0x31c73b68) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:259
 #1  0x0050d61c in sonewconn (head=0x6100160, connstatus=0, from=0x31c73b68) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_socket2.c:379
 #2  0x003a962c in tcp_input (m=0x2dc47f00, off0=<value temporarily unavailable, due to optimizations>) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/tcp_input.c:1165
 #3  0x003a19e0 in ip_proto_dispatch_in (m=0x2dc47f00, hlen=20, proto=6 '\006', inject_ipfref=0x0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/ip_input.c:613
 #4  0x003a30f8 in ip_input (m=0x2dc47f00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/ip_input.c:1372
 #5  0x003a3228 in ip_proto_input (protocol=2, packet_list=0x0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/netinet/ip_input.c:519
 #6  0x00362d7a in proto_input (protocol=2, packet_list=0x2dc47f00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/kpi_protocol.c:298
 #7  0x0034f420 in ether_inet_input (ifp=0x61f0014, protocol_family=2, m_list=0x2dc47f00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/ether_inet_pr_module.c:205
 #8  0x0034b676 in dlil_ifproto_input (ifproto=0x64b74c4, m=0x2dc47f00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/dlil.c:1079
 #9  0x0034de13 in dlil_input_packet_list (ifp_param=0x0, m=0x2dc47f00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/dlil.c:1197
 #10 0x0034e081 in dlil_input_thread_func (inputthread=0x62d4d94) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/net/dlil.c:878
 */
errno_t	
DldSocketFilter::FltConnectIn(void *cookie, socket_t so, const struct sockaddr *from)
{
    return FltConnect( cookie, so, NULL, from );
}

//--------------------------------------------------------------------

/*
 a call stack for connect out callback
 #0  DldSocketFilter::FltConnectOut (cookie=0x0, so=0xc40c1d0, to=0x2c17bea8) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:265
 #1  0x00509038 in soconnectlock (so=0xc40c1d0, nam=0x2c17bea8, dolock=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_socket.c:1252
 #2  0x005115f1 in connect_nocancel (p=0x6d27010, uap=0xb7f1c10, retval=0xb7f1c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_syscalls.c:632
 #3  0x0054e52a in unix_syscall (state=0xb61cf30) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:192 
 */
errno_t	
DldSocketFilter::FltConnectOut(void *cookie, socket_t so, const struct sockaddr *to)
{
    return FltConnect( cookie, so, to, NULL );
}

//--------------------------------------------------------------------

/*
 a call stack for bind callback
 #0  DldSocketFilter::FltBind (cookie=0x0, so=0x6100160, to=0x322d3e88) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:271
 #1  0x00509914 in sobind (so=0x6100160, nam=0x322d3e88) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_socket.c:642
 #2  0x00511f8a in bind (p=0xb78f820, uap=0xc314828, retval=0xbad6054) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_syscalls.c:256
 #3  0x0054e9fd in unix_syscall64 (state=0xc314824) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365 
 */
errno_t	
DldSocketFilter::FltBind(void *cookie, socket_t so, const struct sockaddr *to)
{
    DldSocketObject* soObj = DldCookieToSocketObject( cookie );
    
    assert( DldIsSocketObjectInList( so ) && soObj->toSocket() == so );
    assert( preemption_enabled() );
    
    soObj->setLocalAddress( to );
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

errno_t	
DldSocketFilter::FltSetoption(void *cookie, socket_t so, sockopt_t opt)
{
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

errno_t
DldSocketFilter::FltGetoption(void *cookie, socket_t so, sockopt_t opt)
{
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

/*
 a call stack for listen callback
 #0  DldSocketFilter::FltListen (cookie=0x0, so=0x6100160) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:289
 #1  0x005097c1 in solisten (so=0x6100160, backlog=1000) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_socket.c:744
 #2  0x00511dff in listen (p=0xb78f820, uap=0xc314828, retval=0xbad6054) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/uipc_syscalls.c:293
 #3  0x0054e9fd in unix_syscall64 (state=0xc314824) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365 
 */
errno_t	
DldSocketFilter::FltListen(void *cookie, socket_t so)
{
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

IOReturn DldSocketFilter::registerUserClient( __in DldIOUserClient* client )
{
    return userClient.registerUserClient( client );
}

//--------------------------------------------------------------------

IOReturn DldSocketFilter::unregisterUserClient( __in DldIOUserClient* client )
{
    return userClient.unregisterUserClient( client );
}

//--------------------------------------------------------------------

bool DldSocketFilter::isUserClientPresent()
{
    return userClient.isUserClientPresent();
}

//--------------------------------------------------------------------

//
// if non NULL value is returned a caller must call releaseUserClient()
// when it finishes with the returned client object
//
DldIOUserClient* DldSocketFilter::getUserClient()
{
    return userClient.getUserClient();
}

//--------------------------------------------------------------------

void DldSocketFilter::releaseUserClient()
{
    userClient.releaseUserClient();
}

//--------------------------------------------------------------------

IOMemoryDescriptor* DldSocketFilter::getSocketBufferMemoryDescriptor( __in UInt32 index )
{
    assert( index < kt_DldSocketBuffersNumber );
    
    if( index >= this->dataBuffers->getCount() )
        return NULL;
    
    DldDataBuffer*  dataBuffer;
    
    dataBuffer = OSDynamicCast( DldDataBuffer, this->dataBuffers->getObject( index ) );
    assert( dataBuffer );
    
    return dataBuffer->getMemoryDescriptor();
}

//--------------------------------------------------------------------

//
// returns a referenced buffer object or NULL,
// a caller must release the object
//
DldDataBuffer* DldSocketFilter::acquireSocketDataBufferForIO()
{
    //
    // get a first free index
    //
    UInt8  freeIndex;
    
    //
    // we do not suffer from ABA problem here as an array is used
    //
    do{
        
        freeIndex = this->freeBuffersHead;
        assert( UINT8_MAX == freeIndex || freeIndex < DLD_STATIC_ARRAY_SIZE( this->freeBuffers ) );
        
    } while( UINT8_MAX != freeIndex && (! OSCompareAndSwap( freeIndex, this->freeBuffers[ freeIndex ], &this->freeBuffersHead )) );
    
    if( UINT8_MAX == freeIndex ){
        
        //
        // there are no free buffers left
        //
        return NULL;
    }
    
    //
    // mark as removed from the list
    //
    this->freeBuffers[ freeIndex ] = UINT8_MAX;
    
    DldDataBuffer*  dataBuffer;
    
    dataBuffer = OSDynamicCast( DldDataBuffer, this->dataBuffers->getObject( freeIndex ) );
    assert( dataBuffer );
    
    dataBuffer->retain();
    
#if DBG
    assert( dataBuffer->acquireForIO() );
#endif
    
    return dataBuffer;
}

//--------------------------------------------------------------------

//
// releases the buffer from IO thus allow it acquiring for another IO,
// a caller must use the index returned by buffer's getIndex() method
//
void DldSocketFilter::releaseSocketDataBufferFromIO( __in UInt32 index )
{
#if DBG
    UInt8 nextFreeBuffer = this->freeBuffers[ index ];
    assert( index < kt_DldSocketBuffersNumber && UINT8_MAX == nextFreeBuffer );
#endif
    
    if( index >= kt_DldSocketBuffersNumber || UINT8_MAX != this->freeBuffers[ index ] ){
        
        //
        // actually this is an error, an attempt to free an already freed buffer,
        // but as the data comes from a user mode application we want some protection
        //
        DBG_PRINT_ERROR(("the index %d is out of range or an attempt to free an already freed network data buffer\n", (int)index));
        return;
    }
    
#if DBG
    DldDataBuffer*  dataBuffer;
    
    dataBuffer = OSDynamicCast( DldDataBuffer, this->dataBuffers->getObject( index ) );
    assert( dataBuffer );
    dataBuffer->releaseFromIO();
#endif
    
    //
    // we do not suffer from ABA problem here as an array is used
    //
    do{
        
        this->freeBuffers[ index ] = this->freeBuffersHead;
        
    } while( ! OSCompareAndSwap( this->freeBuffers[ index ], index, &this->freeBuffersHead ) );
}

//--------------------------------------------------------------------

//
// a caller must eventually release all buffers by calling releaseSocketDataBufferFromIO(),
// this function might be called with a held socket lock
//
errno_t
DldSocketFilter::copyDataToBuffers(
    __in const mbuf_t mbuf,
    __inout UInt8*    bufferIndices // an array bufferIndices[ kt_DldSocketBuffersNumber ]
    )
{
    assert( preemption_enabled() );
    
    bufferIndices[ 0 ] = UINT8_MAX;
    
    //
    // if there is no mbuf then nothing to do
    //
    if( ! mbuf )
        return KERN_SUCCESS;
    
    errno_t  error = KERN_SUCCESS;
    size_t   totalbytes = mbuf_pkthdr_len( mbuf );
    size_t   residual = totalbytes;
    size_t   offsetInMbuf = 0x0;
    int      i = 0x0;
    
    while( 0x0 != residual ){
        
        assert( i < kt_DldSocketBuffersNumber );
        
        //
        // write the data into the communication buffers and then notify the protocol dissectors and CAWL
        //
        DldDataBuffer*  buffer = this->acquireSocketDataBufferForIO();
        if( ! buffer ){
            
            error = ENOMEM;
            break;
        }
        
        size_t bytesCopied;
        
        error = buffer->copyDataMbuf( 0x0, residual, offsetInMbuf, mbuf, &bytesCopied );
        assert( 0x0 == error );
        if( error ){
            
            DBG_PRINT_ERROR(( "copyDataMbuf returned an error = %d\n", error ));
            this->releaseSocketDataBufferFromIO( buffer->getIndex() );
            buffer->release();
            break;
        }
        
        assert( residual >= bytesCopied );
        offsetInMbuf = offsetInMbuf + bytesCopied;
        residual = residual - bytesCopied;
        
        bufferIndices[ i ] = buffer->getIndex();
        i += 0x1;
        
        buffer->release();
        
    } // end while( 0x0 != residual )
    
    //
    // set a terminator, if the entire array has been used the terminator is not required
    //
    if( i < kt_DldSocketBuffersNumber )
        bufferIndices[ i ] = UINT8_MAX;
    
    if( error ){
        
        //
        // in case of error release all acquired communication buffers,
        // releaseDataBuffersAndDeliverNotifications() can't be called here as it calls
        // calls DldSocketObject::DeliverWaitingNotifications() that
        // might try to acquire a socket lock held by a caller
        //
        this->releaseDataBuffers( bufferIndices );
        
    } // end if( error )
    
    return error;
}

//--------------------------------------------------------------------

void
DldSocketFilter::releaseDataBuffers(
    __inout UInt8*    bufferIndices // an array bufferIndices[ kt_DldSocketBuffersNumber ]
    )
{
    for( int i = 0x0; i < kt_DldSocketBuffersNumber; ++i ){
        
        //
        // check for the terminating value
        //
        if( UINT8_MAX == bufferIndices[ i ] )
            break;
        
        this->releaseSocketDataBufferFromIO( bufferIndices[ i ] );
    } // end for
}

//--------------------------------------------------------------------

void
DldSocketFilter::releaseDataBuffersAndDeliverNotifications(
    __inout UInt8*    bufferIndices // an array bufferIndices[ kt_DldSocketBuffersNumber ]
    )
{
    this->releaseDataBuffers( bufferIndices );
    
    DldSocketObject::DeliverWaitingNotifications();
}

//--------------------------------------------------------------------

IOReturn
DldSocketFilter::processServiceResponse(
    __in  DldSocketFilterServiceResponse* response
    )
{
    assert( preemption_enabled() );
    
    //
    // at first release buffers
    //
    assert( DLD_STATIC_ARRAY_SIZE( response->buffersToRelease ) == kt_DldSocketBuffersNumber );
    gSocketFilter->releaseDataBuffersAndDeliverNotifications( response->buffersToRelease );
    
    //
    // process deferred data properties, there might be no any property as a request can only release buffers
    //
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( response->property ); ++i ){
        
        DldSocketDataProperty*   property = &response->property[ i ];
        
        if( DldSocketDataPropertyTypeUnknown == property->type )
            break;
        
        assert( 0x0 != property->socketId.socketSequence );
        
        //
        // TO DO - optimize gere by remebering the last found object and deferring reinjection
        // untill the object changes
        //
        DldSocketObject* soObj = DldSocketObject::GetSocketObjectRef( (socket_t)property->socketId.socket );
        if( soObj ){
            
            //
            // check that the request is not for a closed socket, i.e. a socket structure has been reused
            //
            if( soObj->getSocketSequence() == property->socketId.socketSequence ){
                
                soObj->setDeferredDataProperties( property );
                soObj->reinjectDeferredData( DldSocketObject::DldSocketDataAll );
            }
            soObj->release();
            DLD_DBG_MAKE_POINTER_INVALID( soObj );
        }
    } // end for
    
    return kIOReturnSuccess;
}    
//--------------------------------------------------------------------

