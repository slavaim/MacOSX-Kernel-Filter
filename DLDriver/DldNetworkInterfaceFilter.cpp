/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#include <sys/types.h>
#include <sys/vm.h>
#include <sys/proc.h>
#include "DldNetworkInterfaceFilter.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldNetworkInterfaceFilter, OSObject )

//--------------------------------------------------------------------

bool DldNetworkInterfaceFilter::init()
{
    assert( preemption_enabled() );
    
    if( !super::init() ){
        
        DBG_PRINT_ERROR(("super::init() failed\n"));
    }
    
    /*!
     @struct iff_filter
     @discussion This structure is used to define an interface filter for
     use with the iflt_attach function.
     @field iff_cookie A kext defined cookie that will be passed to all
     filter functions.
     @field iff_name A filter name used for debugging purposes.
     @field iff_protocol The protocol of the packets this filter is
     interested in. If you specify zero, packets from all protocols
     will be passed to the filter.
     @field iff_input The filter function to handle inbound packets, may
     be NULL.
     @field iff_output The filter function to handle outbound packets,
     may be NULL.
     @field iff_event The filter function to handle interface events, may
     be null.
     @field iff_ioctl The filter function to handle interface ioctls, may
     be null.
     @field iff_detached The filter function used to notify the filter that
     it has been detached.
     */
    
    this->iffFilter.iff_cookie = (void*)this;
    this->iffFilter.iff_name = "DldNetInterfaceFilter";
    this->iffFilter.iff_protocol = 0x0;
    
    this->iffFilter.iff_input    = dld_iff_input_func;
    this->iffFilter.iff_output   = dld_iff_output_func;
    this->iffFilter.iff_event    = NULL; // dld_iff_event_func;
    this->iffFilter.iff_ioctl    = NULL; // dld_iff_ioctl_func;
    this->iffFilter.iff_detached = dld_iff_detached_func;
    
    return true;
}

//--------------------------------------------------------------------

void DldNetworkInterfaceFilter::free()
{    
    if( this->interfaceFilter )
        iflt_detach( this->interfaceFilter );
    
    if( this->bsdInterface )
        ifnet_release( this->bsdInterface );
    
    super::free();
}

//--------------------------------------------------------------------

DldNetworkInterfaceFilter* DldNetworkInterfaceFilter::withNetworkInterface(__in DldIOService*   networkInterface)
{
    DldNetworkInterfaceFilter*  filter = NULL;
    OSString*                   interfaceNameRef = NULL;
    errno_t                     error;
    
    assert( preemption_enabled() );
    
    filter = new DldNetworkInterfaceFilter();
    assert( filter );
    if( !filter )
        return NULL;
    
    if( !filter->init() ){
        
        filter->release();
        return NULL;
    }
    
    filter->interfaceObject = networkInterface->getSystemService();
    assert( filter->interfaceObject );
    
    networkInterface->LockShared();
    { // start of the lock
    
        //
        // the get property can fail as the network stack attaches (in the notification callback)
        // its object to not started network interface objects, like in the following call stack,
        // the result is that when the DlDriver sees an attaching it supposes that it has missed
        // a start call for a provider and initiates a start procedure processing for a not start
        // object with missing BSD property, this processing will fail and the next time the start
        // processing will be called after a genuine start request
        /*
         #0  Debugger (message=0x64e760 "panic") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/AT386/model_dep.c:845
         #1  0x0022ff83 in panic (str=0x5ef1b3 "%s:%d Assertion failed: %s") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/debug.c:305
         #2  0x00230079 in Assert (file=0x470b6538 "/work/DL_MacSvn/mac/dl-0.x/DLDriver/DldNetworkInterfaceFilter.cpp", line=103, expression=0x470b6638 "interfaceNameRef") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/debug.c:160
         #3  0x4708c64f in DldNetworkInterfaceFilter::withNetworkInterface (networkInterface=0xaa76ef0) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldNetworkInterfaceFilter.cpp:103
         #4  0x46fe9a9c in DldNetIOInterfacePropertyData::fillPropertyData (this=0xaa6fe24, dldService=0xaa76ef0, forceUpdate=true) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldObjectProperty.cpp:2286
         #5  0x46fec975 in DldObjectPropertyEntry::updateDescriptor (this=0xaa9f6d0, service=0x0, dldService=0xaa76ef0, forceUpdate=true) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldObjectProperty.cpp:367
         #6  0x46fc65f7 in DldIOService::start (this=0xaa76ef0, service=0x0, provider=0x0) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldIOService.cpp:457
         #7  0x46fc6d89 in DldIOService::attach (this=0x80beef0, provider=0xaa76ef0) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldIOService.cpp:584
         #8  0x46f21294 in DldHookerCommonClass::attach (this=0x8178a68, serviceObject=0x5d421c0, provider=0xaa3d010) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldHookerCommonClass.cpp:931
         #9  0x46f213c0 in DldHookerCommonClass::attachToChild (this=0x8178a68, serviceObject=0xaa3d010, child=0x5d421c0, plane=0x549dd30) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldHookerCommonClass.cpp:976
         #10 0x46f480b4 in DldHookerCommonClass2<IOServiceDldHook2<(_DldInheritanceDepth)2>, IOService>::attachToChild_hook (this=0xaa3d010, child=0x5d421c0, plane=0x549dd30) at DldHookerCommonClass2.h:1239
         #11 0x0059359c in IORegistryEntry::attachToParent (this=0x5d421c0, parent=0xaa3d010, plane=0x549dd30) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IORegistryEntry.cpp:1648
         #12 0x00595d64 in IOService::attach (this=0x5d421c0, provider=0xaa3d010) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:436
         #13 0x46f48944 in DldHookerCommonClass2<IOServiceDldHook2<(_DldInheritanceDepth)1>, IOService>::attach_hook (this=0x5d421c0, provider=0xaa3d010) at DldHookerCommonClass2.h:1208
         #14 0x3178ee12 in ?? ()
         #15 0x00593d56 in _IOServiceMatchingNotificationHandler (target=0x5d421c0, refCon=0x0, newService=0xaa3d010, notifier=0x5d51d30) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:3550
         #16 0x00596414 in IOService::invokeNotifer (this=0xaa3d010, notify=0xaa3d010) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:2500
         #17 0x005965be in IOService::deliverNotification (this=0xaa3d010, type=0x549c640, orNewState=8, andNewState=8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:3867
         #18 0x005991d6 in IOService::doServiceMatch (this=0xaa3d010, options=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:3062
         #19 0x0059afa9 in _IOConfigThread::main (arg=0xaa85890, result=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:3350
         */
        interfaceNameRef = OSDynamicCast( OSString, networkInterface->getProperty("BSD Name") );
        //assert( interfaceNameRef );
        if( interfaceNameRef )
            interfaceNameRef->retain();
        
    } // end of the lock
    networkInterface->UnLockShared();
    
    if( !interfaceNameRef ){
        
        DBG_PRINT_ERROR(("unable to get a BSD net interface name for %p\n", networkInterface));
        filter->release();
        filter = NULL;
        goto __exit;
    }
    
    DLD_COMM_LOG(COMMON,("preparing a filter for %p(%s)\n", networkInterface, interfaceNameRef->getCStringNoCopy()));
    
    //
    // get the corresponding BSD network interface, the returned object is referenced
    //
    error = ifnet_find_by_name( interfaceNameRef->getCStringNoCopy(), &filter->bsdInterface );
    assert( !error );
    if( KERN_SUCCESS != error ){
        
        DBG_PRINT_ERROR(( "ifnet_find_by_name() failed for %p(%s)\n", networkInterface, interfaceNameRef->getCStringNoCopy() ));
        filter->release();
        filter = NULL;
        goto __exit;
    }
    
__exit:
    
    if( interfaceNameRef )
        interfaceNameRef->release();
    
    return filter;
}

//--------------------------------------------------------------------

errno_t
DldNetworkInterfaceFilter::attachToNetworkInterface()
{
    assert( preemption_enabled() );
    assert( this->bsdInterface );
    
    return iflt_attach( this->bsdInterface, &this->iffFilter, &this->interfaceFilter );
}

//--------------------------------------------------------------------

/*
 an example of a call stack
 #0  DldNetworkInterfaceFilter::dld_iff_input_func (cookie=0x823b100, interface=0x6108e04, protocol=2, data=0x2c3cbf74, frame_ptr=0x2c3cbf48) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldNetworkInterfaceFilter.cpp:160
 #1  0x0031a00c in dlil_interface_filters_input [inlined] () at :1035
 #2  0x0031a00c in dlil_input_packet_list (ifp_param=0x0, m=0x2de42c00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1136
 #3  0x0031a422 in dlil_input_thread_func (inputthread=0x5cbea04) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:878
 */
errno_t
DldNetworkInterfaceFilter::dld_iff_input_func(
    void *cookie,
    ifnet_t interface,
    protocol_family_t protocol,
    mbuf_t *data,
    char **frame_ptr )
{
    DldNetworkInterfaceFilter*  filter = (DldNetworkInterfaceFilter*)cookie;
    errno_t error;
    
    assert( preemption_enabled() );
    assert( interface == filter->bsdInterface );
    
    DldRequestedAccess requestedAccess = { 0x0 };
    requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
    
    error = filter->commonHooker.checkAndLogUserClientAccess( filter->interfaceObject,
                                                              proc_pid( current_proc() ),
                                                              NULL, // use a current user
                                                              &requestedAccess );
    
    assert( EJUSTRETURN != error ); // a special code, a caller will not free a buffer
    
    return error;
}

//--------------------------------------------------------------------

/*
 an example of a call stack
 #0  DldNetworkInterfaceFilter::dld_iff_output_func (cookie=0x823b100, interface=0x6108e04, protocol=2, data=0x2c3cb72c) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldNetworkInterfaceFilter.cpp:188
 #1  0x0031887a in dlil_output (ifp=0x6108e04, proto_family=2, packetlist=0x0, route=0x5d68fa0, dest=0x79a04a4, raw=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1598
 #2  0x0032deef in ifnet_output (interface=0x6108e04, protocol_family=2, m=0x2deeca00, route=0x5d68fa0, dest=0x79a04a4) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/kpi_interface.c:589
 #3  0x0034b4bb in ip_output_list (m0=0x0, packetchain=1, opt=0x0, ro=0x2c3cb944, flags=<value temporarily unavailable, due to optimizations>, imo=0x0, ipoa=0x2c3cb95c) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_output.c:1529
 #4  0x0035224d in tcp_ip_output (so=0x602f818, tp=<value temporarily unavailable, due to optimizations>, pkt=0x2deeca00, cnt=1, opt=0x0, flags=256, sack_in_progress=0, recwin=524280) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_output.c:1836
 #5  0x00353b56 in tcp_output (tp=0x602fa8c) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_output.c:1623
 #6  0x00351abc in tcp_input (m=0x2de56e00, off0=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/tcp_input.c:3042
 #7  0x003473f6 in ip_proto_dispatch_in (m=0x2de56e00, hlen=20, proto=6 '\006', inject_ipfref=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:613
 #8  0x00348a95 in ip_input (m=0x2de56e00) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:1372
 #9  0x00348bbc in ip_proto_input (protocol=2, packet_list=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/netinet/ip_input.c:519
 #10 0x0032f121 in proto_input (protocol=2, packet_list=0x2de5b400) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/kpi_protocol.c:298
 #11 0x0031b7c1 in ether_inet_input (ifp=0x6108e04, protocol_family=2, m_list=0x2de5b400) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/ether_inet_pr_module.c:215
 #12 0x00317a3c in dlil_ifproto_input (ifproto=0x666cac4, m=0x2de5b400) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1079
 #13 0x0031a1b4 in dlil_input_packet_list (ifp_param=0x0, m=0x2de56e00) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:1197
 #14 0x0031a422 in dlil_input_thread_func (inputthread=0x5cbea04) at /SourceCache/xnu/xnu-1504.7.4/bsd/net/dlil.c:878 
 */
errno_t
DldNetworkInterfaceFilter::dld_iff_output_func(
    void *cookie,
    ifnet_t interface,
    protocol_family_t protocol,
    mbuf_t *data )
{
    DldNetworkInterfaceFilter*  filter = (DldNetworkInterfaceFilter*)cookie;
    errno_t error;
    
    assert( preemption_enabled() );
    assert( interface == filter->bsdInterface );
    
    DldRequestedAccess requestedAccess = { 0x0 };
    requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
    
    error = filter->commonHooker.checkAndLogUserClientAccess( filter->interfaceObject,
                                                              proc_pid( current_proc() ),
                                                              NULL, // use a current user
                                                              &requestedAccess );
    
    assert( EJUSTRETURN != error ); // a special code, a caller will not free a buffer
    
    return error;
}

//--------------------------------------------------------------------

void DldNetworkInterfaceFilter::dld_iff_detached_func(void *cookie, ifnet_t interface)
{
    DLD_COMM_LOG( COMMON, ("a net filter for %p has been detached, cookie = %p\n", (void*)interface, cookie));
}

//--------------------------------------------------------------------

