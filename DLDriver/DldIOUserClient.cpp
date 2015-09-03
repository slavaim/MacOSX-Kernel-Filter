/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOUserClient.h"
#include "DldIOLog.h"
#include "DldKernAuthorization.h"
#include "DldIOShadow.h"
#include "DldWhiteList.h"
#include "DldDiskCAWL.h"
#include "DldServiceProtection.h"
#include "DldAclWithProcsObject.h"
#include "DldServiceProtection.h"
#include "DldEncryptionProviders.h"
#include "DldIPCUserClient.h"
#ifdef _DLD_SOCKET_FILTER
#include "NKE/DldSocketFilter.h"
#endif // _DLD_SOCKET_FILTER

//--------------------------------------------------------------------

#define super IOUserClient

OSDefineMetaClassAndStructors( DldIOUserClient, IOUserClient )

//--------------------------------------------------------------------

//#define kAny ((IOByteCount) -1 )
/*
 a call stack for a client's function invokation
 #11 0x465020f8 in DldIOUserClient::setVidPidWhiteList (this=0xb530a00, vInBuffer=0x6f3d970, vOutBuffer=0x3189ac64, vInSize=0x24, vOutSizeP=0x3189aa70) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOUserClient.cpp:957
 #12 0x00647b7f in shim_io_connect_method_structureI_structureO (method=0x46549c30, object=0xb530a00, input=0x6f3d970 "", inputCount=36, output=0x3189ac64 "¼ÂTF¬1", outputCount=0x3189aa70) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:3592
 #13 0x0064c677 in IOUserClient::externalMethod (this=0xb530a00, selector=6, args=0x3189aaf0, dispatch=0x0, target=0x0, reference=0x0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:4199
 #14 0x0064a368 in is_io_connect_method (connection=0xb530a00, selector=6, scalar_input=0x6f3d96c, scalar_inputCnt=0, inband_input=0x6f3d970 "", inband_inputCnt=36, ool_input=0, ool_input_size=0, scalar_output=0xb5815c8, scalar_outputCnt=0xb5815c4, inband_output=0x3189ac64 "¼ÂTF¬1", inband_outputCnt=0x3189ac60, ool_output=0, ool_output_size=0x6f3d9b4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:2745
 #15 0x002c1261 in _Xio_connect_method (InHeadP=0x6f3d944, OutHeadP=0xb5815a0) at device/device_server.c:15466
 #16 0x00226d74 in ipc_kobject_server (request=0x6f3d900) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/ipc_kobject.c:339
 #17 0x002126b1 in ipc_kmsg_send (kmsg=0x6f3d900, option=0, send_timeout=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/ipc_kmsg.c:1371
 #18 0x0021e193 in mach_msg_overwrite_trap (args=0x3189bf60) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:505
 #19 0x0021e37d in mach_msg_trap (args=0x3189bf60) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:572
 #20 0x002d88fb in mach_call_munger (state=0x5c33d40) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/bsd_i386.c:697 
 */

static const IOExternalMethod sMethods[ kt_DldUserClientMethodsMax ] =
{
    // 0x0 kt_DldUserClientOpen
    {
        NULL,
        (IOMethod)&DldIOUserClient::open,
        kIOUCScalarIScalarO,
        0,
        0
    },
    // 0x1 kt_DldUserClientClose
    {
        NULL,
        (IOMethod)&DldIOUserClient::close,
        kIOUCScalarIScalarO,
        0,
        0
    },
    // 0x2 kt_DldUserClientSetQuirks
    {
        NULL,
        (IOMethod)&DldIOUserClient::setQuirks,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x3 kt_DldUserClientSetACL
    {
        NULL,
        (IOMethod)&DldIOUserClient::setACL,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x4 kt_DldUserClientSetShadowFile
    {
        NULL,
        (IOMethod)&DldIOUserClient::setShadowFile,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x5 kt_DldUserClientSetUserState
    {
        NULL,
        (IOMethod)&DldIOUserClient::setUserState,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x6 kt_DldUserClientSetVidPidWhiteList
    {
        NULL,
        (IOMethod)&DldIOUserClient::setVidPidWhiteList,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x7 kt_DldUserClientSetUIDWhiteList
    {
        NULL,
        (IOMethod)&DldIOUserClient::setUIDWhiteList,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x8 kt_DldUserClientSetDVDWhiteList
    {
        NULL,
        (IOMethod)&DldIOUserClient::setDVDWhiteList,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x9 kt_DldUserClientDiskCawlResponse
    {
        NULL,
        (IOMethod)&DldIOUserClient::processServiceDiskCawlResponse,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0xA kt_DldUserClientSocketFilterResponse
    {
        NULL,
        (IOMethod)&DldIOUserClient::processServiceSocketFilterResponse,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0xB kt_DldUserClientSetProcessSecurity
    {
        NULL,
        (IOMethod)&DldIOUserClient::setProcessSecurity,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0xC kt_DldUserClientSetFileSecurity
    {
        NULL,
        (IOMethod)&DldIOUserClient::setFileSecurity,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0xD kt_DldUserClientQueryDriverProperties
    {
        NULL,
        (IOMethod)&DldIOUserClient::getDriverProperties,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0xE kt_DldUserClientSetTempVidPidWhiteList
    {
        NULL,
        (IOMethod)&DldIOUserClient::setTempVidPidWhiteList,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0xF kt_DldUserClientSetTempUIDWhiteList
    {
        NULL,
        (IOMethod)&DldIOUserClient::setTempUIDWhiteList,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x10 kt_DldUserClientSetDLAdminSettings
    {
        NULL,
        (IOMethod)&DldIOUserClient::setDLAdminProperties,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x11 kt_DldUserClientSetEncryptionProvider
    {
        NULL,
        (IOMethod)&DldIOUserClient::setEncryptionProviderProperties,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
    // 0x12 kt_DldUserClientReportIPCCompletion
    {
        NULL,
        (IOMethod)&DldIOUserClient::reportIPCCompletion,
        kIOUCStructIStructO,
        kIOUCVariableStructureSize,
        kIOUCVariableStructureSize
    },
};

//--------------------------------------------------------------------

DldIOUserClient* DldIOUserClient::withTask( __in task_t owningTask )
{
    DldIOUserClient* client;
    
    DBG_PRINT(("DldIOUserClient::withTask( %p )\n", (void*)owningTask ));
    
    client = new DldIOUserClient();
    if( !client )
        return NULL;
    
    //
    // set an invalid PID
    //
    client->fClientPID = (-1);
        
    if (client->init() == false) {
        
        client->release();
        return NULL;
    }
    
    for( int type = 0x0; type < kt_DldNotifyTypeMax; ++type ){
        
        //
        // a default size is 512 Kb for each queue
        //
        client->fQueueSize[ type ] = 0x80000;
    }
    
#if defined(DBG)
    
    //
    // in the debug build allocate 1 MB
    //
    client->fQueueSize[ kt_DldNotifyTypeLog ] = 0x100000;
    client->fQueueSize[ kt_DldNotifyTypeShadow ] = 0x100000;
    
#else
    
    //
    // the log might be a memory hog, allocate 4 MB for it
    //
    client->fQueueSize[ kt_DldNotifyTypeLog ] = 4*0x100000;
    
    //
    // for shadowing allocate 4 MB
    //
    client->fQueueSize[ kt_DldNotifyTypeShadow ] = 4*0x100000;
#endif // DBG


#ifdef _DLD_SOCKET_FILTER
    
    //
    // the network data notification, the data send in separate mapped buffers
    //
    client->fQueueSize[ kt_DldNotifyTypeSocketFilter ] = 0x100000;
    
#endif // _DLD_SOCKET_FILTER
    

    client->fClient = owningTask;
    
    return client;
}

//--------------------------------------------------------------------

bool DldIOUserClient::start( __in IOService *provider )
{
    this->fProvider = OSDynamicCast( com_devicelock_driver_DeviceLockIOKitDriver, provider );
    assert( this->fProvider );
    if( !this->fProvider )
        return false;
    
    if( !super::start( provider ) ){
        
        DBG_PRINT_ERROR(("super::start(%p) failed\n", (void*)provider ));
        return false;
    }
    
    for( int type = 0x0; type < kt_DldNotifyTypeMax; ++type ){
        
        //
        // a fake type doesn't require any resourves
        //
        if( kt_DldNotifyTypeUnknown == type )
            continue;
        
        //
        // allocate a lock
        //
        this->fLock[ type ] = IOLockAlloc();
        assert( this->fLock[ type ] );
        if( !this->fLock[ type ] ){
            
            DBG_PRINT_ERROR(("this->fLock[ %u ]->IOLockAlloc() failed\n", type));
            
            super::stop( provider );
            return false;
        }        
        
        //
        // allocate a queue
        //
        this->fDataQueue[ type ] = IODataQueue::withCapacity( this->fQueueSize[ type ] );
        assert( this->fDataQueue[ type ] );
        while( !this->fDataQueue[ type ] && this->fQueueSize[ type ] > 0x8000){
            
            //
            // try to decrease the queue size until the low boundary of 32 Kb is reached
            //
            this->fQueueSize[ type ] = this->fQueueSize[ type ]/2;
            this->fDataQueue[ type ] = IODataQueue::withCapacity( this->fQueueSize[ type ]/2 );
        }
        
        assert( this->fDataQueue[ type ] );
        if( !this->fDataQueue[ type ] ){
            
            DBG_PRINT_ERROR(("this->fDataQueue[ %u ]->withCapacity( %u ) failed\n", type, (int)this->fQueueSize[ type ]));
            
            super::stop( provider );
            return false;
        }
        
        //
        // get the queue's memory descriptor
        //
        this->fSharedMemory[ type ] = this->fDataQueue[ type ]->getMemoryDescriptor();
        assert( this->fSharedMemory[ type ] );
        if( !this->fSharedMemory[ type ] ) {
            
            DBG_PRINT_ERROR(("this->fDataQueue[ %u ]->getMemoryDescriptor() failed\n", type));
            
            super::stop( provider );
            return false;
        }
        
    }// end for
    
    return true;
}

//--------------------------------------------------------------------

void DldIOUserClient::freeAllocatedResources()
{
    
    for( int type = 0x0; type < kt_DldNotifyTypeMax; ++type ){
        
        if( this->fDataQueue[ type ] ){
            
            //
            // send a termination notification to the user client
            //
            UInt32 message = kt_DldStopListeningToMessages;
            this->fDataQueue[ type ]->enqueue(&message, sizeof(message));
        }
        
        if( this->fSharedMemory[ type ] ) {
            
            this->fSharedMemory[ type ]->release();
            this->fSharedMemory[ type ] = NULL;
        }
        
        if( this->fDataQueue[ type ] ){
            
            this->fDataQueue[ type ]->release();
            this->fDataQueue[ type ] = NULL;
        }
        
        if( this->fLock[ type ] ){
            
            IOLockFree( this->fLock[ type ] );
            this->fLock[ type ] = NULL;
        }
        
    }// end for
    
}

//--------------------------------------------------------------------

void DldIOUserClient::free()
{
    this->freeAllocatedResources();
    
    super::free();
}

//--------------------------------------------------------------------

void DldIOUserClient::stop( __in IOService *provider )
{
    this->freeAllocatedResources();
    
    super::stop( provider );
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::open(void)
{
    if( this->isInactive() )
        return kIOReturnNotAttached;
    
    //
    // only one user client allowed
    //
    if( !fProvider->open(this) )
        return kIOReturnExclusiveAccess;
    
    this->fClientProc = current_proc();
    this->fClientPID  = proc_pid( current_proc() );
    
    //
    // forget about an old client behaviour,
    // as the new client has taken the security responsibility
    //
    gClientWasAbnormallyTerminated = false;
    
    return this->startLogging();
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::clientClose(void)
{
    if( !this->clientClosedItself ){
        
        //
        // looks like the client process was aborted
        //
        if( this->fProvider ){
            
            if( this->fProvider->isOpen(this) )
                this->fProvider->close(this);
            
            //
            // from that point the system is under a theft threat
            //
            gClientWasAbnormallyTerminated = true;
        }
        
        (void)this->close();
    }

    (void)this->terminate(0);
    
    this->fClient = NULL;
    this->fClientPID = (-1);
    this->fProvider = NULL;    
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::close(void)
{
    
    if( !this->fProvider )
        return kIOReturnNotAttached;
    
    if( this->fProvider->isOpen(this) )
        this->fProvider->close(this);
    
    //
    // remove the client from the list of protected process as it is supposed that
    // the connection for a protected process will be kept until the process termination,
    // should a protected process crashed this unconditioanl removing helps to preserve
    // system integrity
    //
    if( gServiceProtection )
        gServiceProtection->removeAllProcessesAndFilesSecurity();
    
    //
    // grant access to everyone and disable audit, logging and shadowing
    //
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( gArrayOfAclArrays ); ++i ){
        
        if( gArrayOfAclArrays[ i ] )
            gArrayOfAclArrays[ i ]->deleteAllACLs();
    }
    
    //
    // close all shadow files as presence of an opend vnode with
    // the non zero iocount prevents from VFS flushing on shutdown
    // and blocks the vlushing in vnode_drain forever
    /*
     #0  machine_switch_context (old=0x565f3d4, continuation=0, new=0x54a87a8) at /SourceCache/xnu/xnu-1456.1.25/osfmk/i386/pcb.c:869
     #1  0x002266fe in thread_invoke (self=0x565f3d4, thread=0x54a87a8, reason=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1623
     #2  0x0022699d in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1850
     #3  0x00226a2b in thread_block (continuation=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1867
     #4  0x002211ed in lck_mtx_sleep (lck=0x70bbb90, lck_sleep_action=0, event=0x70bbbcc, interruptible=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/locks.c:504
     #5  0x00486b15 in _sleep (chan=0x70bbbcc "\002", pri=<value temporarily unavailable, due to optimizations>, wmsg=<value temporarily unavailable, due to optimizations>, abstime=0, continuation=0, mtx=0x70bbb90) at /SourceCache/xnu/xnu-1456.1.25/bsd/kern/kern_synch.c:189
     #6  0x00487208 in msleep (chan=0x70bbbcc, mtx=0x70bbb90, pri=20, wmsg=0x59540e "vnode_drain", ts=0x0) at /SourceCache/xnu/xnu-1456.1.25/bsd/kern/kern_synch.c:335
     #7  0x002d9573 in vnode_drain [inlined] () at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_subr.c:3899
     #8  0x002d9573 in vnode_reclaim_internal (vp=0x70bbb90, locked=1, reuse=1, flags=0) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_subr.c:4012
     #9  0x002dec7e in vflush (mp=0x567ebd8, skipvp=0x0, flags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_subr.c:1903
     #10 0x002e9e9a in dounmount (mp=0x567ebd8, flags=524288, withref=0, ctx=0x564d364) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_syscalls.c:1184
     #11 0x002dab53 in vfs_unmountall () at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_subr.c:2752
     #12 0x004821b6 in boot (paniced=1, howto=0, command=0x31f03f18 "") at /SourceCache/xnu/xnu-1456.1.25/bsd/kern/kern_shutdown.c:175
     #13 0x0048dfcc in reboot (p=0x57d8d20, uap=0x54a5328, retval=0x564d2a4) at /SourceCache/xnu/xnu-1456.1.25/bsd/kern/kern_xxx.c:119
     #14 0x004ed85f in unix_syscall64 (state=0x54a5324) at /SourceCache/xnu/xnu-1456.1.25/bsd/dev/i386/systemcalls.c:433     
     */
    //
    if( gShadow )
        gShadow->releaseAllShadowFiles();
    
    //
    // fix the fact that the client in a normal way of detaching
    //
    this->clientClosedItself = true;
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

bool DldIOUserClient::terminate(IOOptionBits options)
{
    //
    // if somebody does a kextunload while a client is attached
    //
    if( this->fProvider && this->fProvider->isOpen(this) )
        this->fProvider->close(this);
    
    (void)stopLogging();
    
    return super::terminate( options );
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::startLogging(void)
{
    IOReturn   RC;
    
    assert( preemption_enabled() );
    assert( gLog );
    assert( gShadow );
    assert( gServiceProtection );
#ifdef _DLD_MACOSX_CAWL
    assert( gDiskCAWL );
#endif // _DLD_MACOSX_CAWL
#ifdef _DLD_SOCKET_FILTER
    assert( gSocketFilter );
#endif // _DLD_SOCKET_FILTER
    
    if(    !gLog
        || !gShadow
        || !gServiceProtection
#ifdef _DLD_MACOSX_CAWL
        || !gDiskCAWL
#endif // _DLD_MACOSX_CAWL
#ifdef _DLD_SOCKET_FILTER
        || !gSocketFilter
#endif // _DLD_SOCKET_FILTER
       ){
        
        DBG_PRINT_ERROR(("An attempt to connect a client to a driver with missing global objects\n"));
        return kIOReturnInternalError;
    }
    
    RC = gServiceUserClient.registerUserClient( this );
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("gServiceUserClient.registerUserClient( this ) failed\n"));
        goto __exit;
    }
    
    RC = gLog->registerUserClient( this );
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("gLog->registerUserClient( this ) failed\n"));
        goto __exit;
    }
    
    RC = gShadow->registerUserClient( this );
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("gShadow->registerUserClient( this ) failed\n"));
        goto __exit;
    }
    
    RC = gServiceProtection->registerUserClient( this );
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("gServiceProtection->registerUserClient( this ) failed\n"));
        goto __exit;
    }
    
#ifdef _DLD_MACOSX_CAWL
    RC = gDiskCAWL->registerUserClient( this );
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("gDiskCAWL->registerUserClient( this ) failed\n"));
        goto __exit;
    }
#endif // _DLD_MACOSX_CAWL
    
#ifdef _DLD_SOCKET_FILTER
    RC = gSocketFilter->registerUserClient( this );
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("gSocketFilter->registerUserClient( this ) failed\n"));
        goto __exit;
    }
#endif // _DLD_SOCKET_FILTER
    
__exit:
    
    if( kIOReturnSuccess != RC ){
        
#ifdef _DLD_MACOSX_CAWL
        gDiskCAWL->unregisterUserClient( this );
#endif // _DLD_MACOSX_CAWL
#ifdef _DLD_SOCKET_FILTER
        gSocketFilter->unregisterUserClient( this );
#endif // _DLD_SOCKET_FILTER
        gServiceProtection->unregisterUserClient( this );
        gShadow->unregisterUserClient( this );
        gLog->unregisterUserClient( this );
        gServiceUserClient.unregisterUserClient( this );
    }
    
    return RC;
}

//--------------------------------------------------------------------

/*
 a call stack example
 #0  machine_switch_context (old=0x7ae9b7c, continuation=0, new=0x59f0000) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/pcb.c:869
 #1  0x00226e57 in thread_invoke (self=0x7ae9b7c, thread=0x59f0000, reason=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1628
 #2  0x002270f6 in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1863
 #3  0x00227184 in thread_block (continuation=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1880
 #4  0x004869c0 in _sleep (chan=0x6382f8c "\001", pri=50, wmsg=<value temporarily unavailable, due to optimizations>, abstime=1084756243159, continuation=0, mtx=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_synch.c:241
 #5  0x00486eef in msleep (chan=0x6382f8c, mtx=0x0, pri=50, wmsg=0x471b3854 "DldSocketFilter::unregisterUserClient()", ts=0x32133d20) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_synch.c:335
 #6  0x4718fbc0 in DldSocketFilter::unregisterUserClient (this=0x6382f80, client=0x75d5f00) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/NKE/DldSocketFilter.cpp:877
 #7  0x470ba8fb in DldIOUserClient::stopLogging (this=0x75d5f00) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/DldIOUserClient.cpp:514
 #8  0x470b7cc0 in DldIOUserClient::terminate (this=0x75d5f00, options=0) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/DldIOUserClient.cpp:408
 #9  0x47034b92 in DldHookerCommonClass2<IOUserClientDldHook<(_DldInheritanceDepth)1>, IOUserClient>::terminate_hook (this=0x75d5f00, options=0) at DldHookerCommonClass2.h:1117
 #10 0x470b7b5c in DldIOUserClient::clientClose (this=0x75d5f00) at /work/DL_MacSvn/mac/dl-0.x.beta/mac/dl-0.x/DLDriver/DldIOUserClient.cpp:346
 #11 0x00561555 in is_io_service_close (connection=0x75d5f00) at /SourceCache/xnu/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:2443
 #12 0x002897d9 in _Xio_service_close (InHeadP=0x7135ea4, OutHeadP=0x5c6fb98) at device/device_server.c:3117
 #13 0x0021d7f7 in ipc_kobject_server (request=0x7135e00) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/ipc_kobject.c:339
 #14 0x00210983 in ipc_kmsg_send (kmsg=0x7135e00, option=0, send_timeout=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/ipc/ipc_kmsg.c:1371
 #15 0x00216be6 in mach_msg_overwrite_trap (args=0x716d708) at /SourceCache/xnu/xnu-1504.7.4/osfmk/ipc/mach_msg.c:505
 #16 0x00293eb4 in mach_call_munger64 (state=0x716d704) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/bsd_i386.c:830 
 */

IOReturn
DldIOUserClient::stopLogging(void)
{
    
    assert( gLog && gShadow && gServiceProtection );
    
    this->fNotificationPorts[ kt_DldNotifyTypeLog ]      = 0x0;
    this->fNotificationPorts[ kt_DldNotifyTypeShadow ]   = 0x0;
    this->fNotificationPorts[ kt_DldNotifyTypeDiskCAWL ] = 0x0;
    this->fNotificationPorts[ kt_DldNotifyTypeEvent ]    = 0x0;
#ifdef _DLD_SOCKET_FILTER
    this->fNotificationPorts[ kt_DldNotifyTypeSocketFilter ] = 0x0;
#endif // _DLD_SOCKET_FILTER
    
    if( gLog )
        gLog->unregisterUserClient( this );
    
    if( gShadow )
        gShadow->unregisterUserClient( this );
    
    if( gServiceProtection )
        gServiceProtection->unregisterUserClient( this );
    
#ifdef _DLD_MACOSX_CAWL
    if( gDiskCAWL )
        gDiskCAWL->unregisterUserClient( this );
#endif // _DLD_MACOSX_CAWL
    
#ifdef _DLD_SOCKET_FILTER
    if( gSocketFilter )
        gSocketFilter->unregisterUserClient( this );
#endif // _DLD_SOCKET_FILTER
    
    gServiceUserClient.unregisterUserClient( this );
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------


IOReturn
DldIOUserClient::registerNotificationPort( __in mach_port_t port, __in UInt32 type, __in UInt32 ref)
{    
    if ( (port == MACH_PORT_NULL) || 
         (UInt32)kt_DldNotifyTypeUnknown == type || type >= (UInt32)kt_DldNotifyTypeMax )
        return kIOReturnError;
    
    if( !fDataQueue[ type ] )
        return kIOReturnError;
        
    //
    // the order does matter ( may be a memory barrier is required )
    //
    this->fDataQueue[ type ]->setNotificationPort( port );
    this->fNotificationPorts[ type ] = port;
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::clientMemoryForType( __in UInt32 type, __in IOOptionBits *options,
                                      __in IOMemoryDescriptor **memory)
{
    *memory = NULL;
    *options = 0;
    
#ifdef _DLD_SOCKET_FILTER
    //
    // check for socket data notification type
    //
    if( type >= (UInt32)kt_DldAclTypeSocketDataBase && type < (UInt32)(kt_DldAclTypeSocketDataBase + kt_DldSocketBuffersNumber) ){
        
        if( ! gSocketFilter )
            return kIOReturnNoMemory;
        
        IOMemoryDescriptor* memoryDescr = gSocketFilter->getSocketBufferMemoryDescriptor( (UInt32)( type - kt_DldAclTypeSocketDataBase ) );
        if( NULL == memoryDescr ){
            
            //
            // in most of the cases this is not an error, the buffer for the index doesn't exist,
            // but we cannot tell a caller that the buffer just doesn't exist so return an error,
            // a caller should ignor the error and continue execution
            //
            return kIOReturnNoDevice;
        }
        
        *memory = memoryDescr;
        return kIOReturnSuccess;
    }
#endif // _DLD_SOCKET_FILTER
    
    //
    // check for shared circular queue memory type
    //
    if( (UInt32)kt_DldNotifyTypeUnknown != type && type < (UInt32)kt_DldNotifyTypeMax ){
        
        assert( this->fSharedMemory[ type ] );
        if (!this->fSharedMemory[ type ])
            return kIOReturnNoMemory;
        
        //
        // client will decrement this reference
        //
        this->fSharedMemory[ type ]->retain();
        *memory = this->fSharedMemory[ type ];
        
        return kIOReturnSuccess;
    }
    
    //
    // the type is out of range
    //
    DBG_PRINT_ERROR(("memory type %d is out of range\n", (int)type));
    return kIOReturnBadArgument;
}

//--------------------------------------------------------------------

IOExternalMethod*
DldIOUserClient::getTargetAndMethodForIndex( __in IOService **target, __in UInt32 index)
{
    if( index >= (UInt32)kt_DldUserClientMethodsMax )
        return NULL;
    
    *target = this; 
    return (IOExternalMethod *)&sMethods[index];
}

//--------------------------------------------------------------------

class IODataQueueWrapper: public IODataQueue
{
public:
    
    Boolean enqueueWithBarrier(void * data, UInt32 dataSize)
    {
        const UInt32       head      = dataQueue->head;  // volatile
        const UInt32       tail      = dataQueue->tail;
        const UInt32       entrySize = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
        IODataQueueEntry * entry;
        
        assert( preemption_enabled() );
        
        if ( tail >= head )
        {
            // Is there enough room at the end for the entry?
            if ( (tail + entrySize) <= dataQueue->queueSize )
            {
                entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);
                
                entry->size = dataSize;
                memcpy(&entry->data, data, dataSize);
                
                // The tail can be out of bound when the size of the new entry
                // exactly matches the available space at the end of the queue.
                // The tail can range from 0 to dataQueue->queueSize inclusive.
                DldMemoryBarrier();
                dataQueue->tail += entrySize;
            }
            else if ( head > entrySize ) 	// Is there enough room at the beginning?
            {
                // Wrap around to the beginning, but do not allow the tail to catch
                // up to the head.
                
                dataQueue->queue->size = dataSize;
                
                // We need to make sure that there is enough room to set the size before
                // doing this. The user client checks for this and will look for the size
                // at the beginning if there isn't room for it at the end.
                
                if ( ( dataQueue->queueSize - tail ) >= DATA_QUEUE_ENTRY_HEADER_SIZE )
                {
                    ((IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail))->size = dataSize;
                }
                
                memcpy(&dataQueue->queue->data, data, dataSize);
                DldMemoryBarrier();
                dataQueue->tail = entrySize;
            }
            else
            {
                return false;	// queue is full
            }
        }
        else
        {
            // Do not allow the tail to catch up to the head when the queue is full.
            // That's why the comparison uses a '>' rather than '>='.
            
            if ( (head - tail) > entrySize )
            {
                entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);
                
                entry->size = dataSize;
                memcpy(&entry->data, data, dataSize);
                DldMemoryBarrier();
                dataQueue->tail += entrySize;
            }
            else
            {
                return false;	// queue is full
            }
        }
        
        // Send notification (via mach message) that data is available.
        
        if ( ( head == tail )                /* queue was empty prior to enqueue() */
            ||   ( dataQueue->head == tail ) )   /* queue was emptied during enqueue() */
        {
            sendDataAvailableNotification();
        }
        
        return true;
    }
    
    void forceSendDataAvailableNotification( UInt32 msgSize )
    {
        sendDataAvailableNotification();
    };
};

static UInt32 gLogMessageCounter = 0x0;

IOReturn DldIOUserClient::logData( __in DldDriverDataLogInt* data )
{
    assert( preemption_enabled() );
    assert( this->fDataQueue[ kt_DldNotifyTypeLog ] );
    assert( this->fLock[ kt_DldNotifyTypeLog ] );
    assert( data->logDataSize <= sizeof( DldDriverDataLog ) );
    assert( DLD_LOG_TYPE_UNKNOWN != data->logData->Header.type );
    assert( data->logDataSize != 0x0 );
    
    if( 0x0 == data->logDataSize )// actually this is an error
        return kIOReturnSuccess;
    
    //
    // enqueue must be able to send a message to a client or else
    // the client will wait until the queue is full to receive
    // a message ( this is an internal enqueue logic )
    // 
    if( 0x0 == this->fNotificationPorts[ kt_DldNotifyTypeLog ] )
        return kIOReturnSuccess;
    
    bool enqueued;
    
    //
    // logData is called from an arbitrary context, so the access
    // serialization to the queue is required, the lock must not
    // disable the preemtion as IODataQueue::sendDataAvailableNotification
    // can block on the mutex
    /*
     #0  Debugger (message=0x8001003b <Address 0x8001003b out of bounds>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/i386/AT386/model_dep.c:866
     #1  0xffffff8000204ae6 in panic (str=0xffffff8000568e58 "\"thread_invoke: preemption_level %d, possible cause: %s\"@/SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1471") at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/debug.c:303
     #2  0xffffff80002091f4 in thread_invoke (self=0xffffff800804d588, thread=0xffffff8007c62750, reason=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1469
     #3  0xffffff80002097d8 in thread_block_reason (continuation=<value temporarily unavailable, due to optimizations>, parameter=<value temporarily unavailable, due to optimizations>, reason=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1850
     #4  0xffffff80002c32bc in lck_mtx_lock_wait_x86 (mutex=0xffffff80086ef348) at /SourceCache/xnu/xnu-1456.1.25/osfmk/i386/locks_i386.c:2021
     #5  0xffffff80002c11a4 in lck_mtx_lock () at pmap.h:215
     #6  0xffffff8000203649 in ipc_object_copyin_from_kernel (object=0xffffff80086ef340, msgt_name=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/ipc/ipc_object.c:591
     #7  0xffffff800020235f in ipc_kmsg_copyin_from_kernel (kmsg=0xffffff80087de000) at /SourceCache/xnu/xnu-1456.1.25/osfmk/ipc/ipc_kmsg.c:2533
     #8  0xffffff80002054f5 in mach_msg_send_from_kernel_proper (msg=<value temporarily unavailable, due to optimizations>, send_size=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/ipc_mig.c:152
     #9  0xffffff800054cad8 in IODataQueue::sendDataAvailableNotification (this=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IODataQueue.cpp:212
     #10 0xffffff800054cc30 in IODataQueue::enqueue (this=0xffffff8008334420, data=<value temporarily unavailable, due to optimizations>, dataSize=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IODataQueue.cpp:179
     #11 0xffffff7f815f33ec in DldIOUserClient::shadowNotification (this=0xffffff8006f7f000, data=0xffffff802d00be90) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOUserClient.cpp:504
     #12 0xffffff7f815fbeaa in DldIOShadow::shadowNotification (this=0xffffff80074c3900, notificationData=0xffffff802d00be90) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOShadow.cpp:560
     #13 0xffffff7f815fb052 in DldIOShadow::shadowOperationCompletionWQ (this=0xffffff80074c3900, compArgs=0xffffff802d00bef0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOShadow.cpp:1973
     #14 0xffffff7f815fc52f in DldIOShadow::shadowThreadMain (this_class=0xffffff80074c3900) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOShadow.cpp:301     
     */
    //
    IOLockLock( this->fLock[ kt_DldNotifyTypeLog ] );
    {// start of the lock
        
        assert( preemption_enabled() );
        data->logData->Header.logEntryNumber = ++gLogMessageCounter;
        
        enqueued = ((IODataQueueWrapper*)this->fDataQueue[ kt_DldNotifyTypeLog ])->enqueueWithBarrier( data->logData, data->logDataSize );
        //((IODataQueueWrapper*)this->fDataQueue[ kt_DldNotifyTypeLog ])->forceSendDataAvailableNotification();
#if defined( DBG )
        /*if( !enqueued ){
            
            __asm__ volatile( "int $0x3" );
        }*/
#endif//DBG
        
    }//end of the lock
    IOLockUnlock( this->fLock[ kt_DldNotifyTypeLog ] );
    
    //assert( enqueued );
    if( !enqueued ){
        
        DBG_PRINT_ERROR(("this->fDataQueue[ kt_DldNotifyTypeLog ]->enqueue failed for # %u\n", (int)gLogMessageCounter));
        
    }//end if( !enqueued )
    
    return enqueued? kIOReturnSuccess: kIOReturnNoMemory;
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::shadowNotification( __in DldDriverShadowNotificationInt* data )
{
    assert( preemption_enabled() );
    assert( this->fDataQueue[ kt_DldNotifyTypeShadow ] );
    assert( this->fLock[ kt_DldNotifyTypeShadow ] );
    assert( data->notifyDataSize == sizeof( DldDriverShadowNotification ) );
    
    //
    // enqueue must be able to send a message to a client or else
    // the client will wait until the queue is full to receive
    // a message ( this is an internal enqueue logic )
    // 
    if( 0x0 == this->fNotificationPorts[ kt_DldNotifyTypeShadow ] )
        return kIOReturnSuccess;
    
    bool enqueued;
    
    //
    // shadowNotification is called from a worker, so the access
    // serialization to the queue is not required, but is used
    // here as it doesn't harm and protects from the subtle bugs
    // if the semantics of access will be changed, the lock must not
    // disable the preemtion as IODataQueue::sendDataAvailableNotification
    // can block on the mutex
    /*
     #0  Debugger (message=0x8001003b <Address 0x8001003b out of bounds>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/i386/AT386/model_dep.c:866
     #1  0xffffff8000204ae6 in panic (str=0xffffff8000568e58 "\"thread_invoke: preemption_level %d, possible cause: %s\"@/SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1471") at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/debug.c:303
     #2  0xffffff80002091f4 in thread_invoke (self=0xffffff800804d588, thread=0xffffff8007c62750, reason=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1469
     #3  0xffffff80002097d8 in thread_block_reason (continuation=<value temporarily unavailable, due to optimizations>, parameter=<value temporarily unavailable, due to optimizations>, reason=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/sched_prim.c:1850
     #4  0xffffff80002c32bc in lck_mtx_lock_wait_x86 (mutex=0xffffff80086ef348) at /SourceCache/xnu/xnu-1456.1.25/osfmk/i386/locks_i386.c:2021
     #5  0xffffff80002c11a4 in lck_mtx_lock () at pmap.h:215
     #6  0xffffff8000203649 in ipc_object_copyin_from_kernel (object=0xffffff80086ef340, msgt_name=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/ipc/ipc_object.c:591
     #7  0xffffff800020235f in ipc_kmsg_copyin_from_kernel (kmsg=0xffffff80087de000) at /SourceCache/xnu/xnu-1456.1.25/osfmk/ipc/ipc_kmsg.c:2533
     #8  0xffffff80002054f5 in mach_msg_send_from_kernel_proper (msg=<value temporarily unavailable, due to optimizations>, send_size=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/ipc_mig.c:152
     #9  0xffffff800054cad8 in IODataQueue::sendDataAvailableNotification (this=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IODataQueue.cpp:212
     #10 0xffffff800054cc30 in IODataQueue::enqueue (this=0xffffff8008334420, data=<value temporarily unavailable, due to optimizations>, dataSize=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IODataQueue.cpp:179
     #11 0xffffff7f815f33ec in DldIOUserClient::shadowNotification (this=0xffffff8006f7f000, data=0xffffff802d00be90) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOUserClient.cpp:504
     #12 0xffffff7f815fbeaa in DldIOShadow::shadowNotification (this=0xffffff80074c3900, notificationData=0xffffff802d00be90) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOShadow.cpp:560
     #13 0xffffff7f815fb052 in DldIOShadow::shadowOperationCompletionWQ (this=0xffffff80074c3900, compArgs=0xffffff802d00bef0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOShadow.cpp:1973
     #14 0xffffff7f815fc52f in DldIOShadow::shadowThreadMain (this_class=0xffffff80074c3900) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOShadow.cpp:301     
     */
    //
    IOLockLock( this->fLock[ kt_DldNotifyTypeShadow ] );
    {// start of the lock
        
        assert( preemption_enabled() );
        enqueued = ((IODataQueueWrapper*)this->fDataQueue[ kt_DldNotifyTypeShadow ])->enqueueWithBarrier( data->notifyData, data->notifyDataSize );
    }//end of the lock
    IOLockUnlock( this->fLock[ kt_DldNotifyTypeShadow ] );
    //assert( enqueued );
    if( !enqueued ){
        
        DBG_PRINT_ERROR(("this->fDataQueue[ kt_DldNotifyTypeShadow ]->enqueue failed\n"));
        
    }//end if( !enqueued )
    
    return enqueued? kIOReturnSuccess: kIOReturnNoMemory;
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::eventNotification( __in DldDriverEventData* data )
{
    assert( preemption_enabled() );
    assert( this->fDataQueue[ kt_DldNotifyTypeEvent ] );
    assert( this->fLock[ kt_DldNotifyTypeEvent ] );
    assert( data->Header.size >= sizeof( DldDriverEventData ) );
    
    //
    // enqueue must be able to send a message to a client or else
    // the client will wait until the queue is full to receive
    // a message ( this is an internal enqueue logic )
    // 
    if( 0x0 == this->fNotificationPorts[ kt_DldNotifyTypeEvent ] )
        return kIOReturnSuccess;
    
    if( data->Header.size > DLD_MAX_EVENT_SIZE ){
        
        DBG_PRINT_ERROR(("data->Header.size(%d) > DLD_MAX_EVENT_SIZE, the event type is %d\n",
                          data->Header.size, data->Header.type));
        return kIOReturnBadArgument;
    }
    
    bool enqueued;
    
    IOLockLock( this->fLock[ kt_DldNotifyTypeEvent ] );
    {// start of the lock
        
        assert( preemption_enabled() );
        enqueued = ((IODataQueueWrapper*)this->fDataQueue[ kt_DldNotifyTypeEvent ])->enqueueWithBarrier( data, data->Header.size );
    }//end of the lock
    IOLockUnlock( this->fLock[ kt_DldNotifyTypeEvent ] );
    //assert( enqueued );
    if( !enqueued ){
        
        DBG_PRINT_ERROR(("this->fDataQueue[ kt_DldNotifyTypeEvent ]->enqueue failed\n"));
        
    }//end if( !enqueued )
    
    return enqueued? kIOReturnSuccess: kIOReturnNoMemory;
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::diskCawlNotification( __in DldDriverDiskCAWLNotificationInt* data )
{
    assert( preemption_enabled() );
    assert( this->fDataQueue[ kt_DldNotifyTypeDiskCAWL ] );
    assert( this->fLock[ kt_DldNotifyTypeDiskCAWL ] );
    assert( data->notifyDataSize >= sizeof( *data->notifyData ) );
    
    if( 0x0 == data->notifyDataSize )// actually this is an error
        return kIOReturnSuccess;
    
    //
    // enqueue must be able to send a message to a client or else
    // the client will wait until the queue is full to receive
    // a message ( this is an internal enqueue logic )
    // 
    if( 0x0 == this->fNotificationPorts[ kt_DldNotifyTypeDiskCAWL ] )
        return kIOReturnSuccess;
    
    bool enqueued;
    
    //
    // the function is called from an arbitrary context, so the access
    // serialization to the queue is required, the lock must not
    // disable the preemtion as IODataQueue::sendDataAvailableNotification
    // can block on the mutex
    //
    IOLockLock( this->fLock[ kt_DldNotifyTypeDiskCAWL ] );
    {// start of the lock
        
        assert( preemption_enabled() );
        enqueued = ((IODataQueueWrapper*)this->fDataQueue[ kt_DldNotifyTypeDiskCAWL ])->enqueueWithBarrier( data->notifyData, data->notifyDataSize );
#if defined( DBG )
        /*if( !enqueued ){
         
         __asm__ volatile( "int $0x3" );
         }*/
#endif//DBG
        
    }//end of the lock
    IOLockUnlock( this->fLock[ kt_DldNotifyTypeDiskCAWL ] );
    
    //assert( enqueued );
    if( !enqueued ){
        
        DBG_PRINT_ERROR(("this->fDataQueue[ kt_DldNotifyTypeDiskCAWL ]->enqueue failed\n"));
        
    }//end if( !enqueued )
    
    return enqueued? kIOReturnSuccess: kIOReturnNoMemory;
}

//--------------------------------------------------------------------

IOReturn DldIOUserClient::socketFilterNotification( __in DldSocketFilterNotification* data )
{
#ifdef _DLD_SOCKET_FILTER
    assert( preemption_enabled() );
    assert( this->fDataQueue[ kt_DldNotifyTypeSocketFilter ] );
    assert( this->fLock[ kt_DldNotifyTypeSocketFilter ] );
    
    //
    // enqueue must be able to send a message to a client or else
    // the client will wait until the queue is full to receive
    // a message ( this is an internal enqueue logic ), the return
    // status for the socket notification is not a successful one 
    // as opposite to other notifications as returning a success will result
    // in failing to release data buffers
    // 
#ifndef DBG
    if( 0x0 == this->fNotificationPorts[ kt_DldNotifyTypeSocketFilter ] )
        return kIOReturnError;
#endif
    
    bool enqueued;
    
    //
    // the function is called from an arbitrary context, so the access
    // serialization to the queue is required, the lock must not
    // disable the preemtion as IODataQueue::sendDataAvailableNotification
    // can block on the mutex
    //
    IOLockLock( this->fLock[ kt_DldNotifyTypeSocketFilter ] );
    {// start of the lock
        
        assert( preemption_enabled() );
        enqueued = ((IODataQueueWrapper*)this->fDataQueue[ kt_DldNotifyTypeSocketFilter ])->enqueueWithBarrier( data, data->size );
#if defined( DBG )
        /*if( !enqueued ){
         
         __asm__ volatile( "int $0x3" );
         }*/
#endif//DBG
        
    }//end of the lock
    IOLockUnlock( this->fLock[ kt_DldNotifyTypeSocketFilter ] );
    
    //assert( enqueued );
    if( !enqueued ){
        
        DBG_PRINT_ERROR(("this->fDataQueue[ kt_DldNotifyTypeSocketFilter ]->enqueue failed\n"));
        
    }//end if( !enqueued )
    
    return enqueued? kIOReturnSuccess: kIOReturnNoMemory;
#else // _DLD_SOCKET_FILTER
    return kIOReturnUnsupported;
#endif // _DLD_SOCKET_FILTER
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setQuirks(
    __in  void *vInBuffer,//DldAclDescriptorHeader
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    DldQuirksOfSecuritySettings*  quirks = (DldQuirksOfSecuritySettings*)vInBuffer;
    vm_size_t                     inSize = (vm_size_t)vInSize;
    
    if( inSize < sizeof( *quirks ) )
        return kIOReturnBadArgument;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    //
    // copy the quirks
    //
    gSecuritySettings.securitySettings = *quirks;
    
    DLD_COMM_LOG( QUIRKS,( "setQuirks : controlUsbHid=%d, controlUsbMassStorage=%d, controlUsbBluetooth=%d, controlUsbNetwork=%d, "
                           "controlIEE1394Storage=%d, controlBluetoothHid=%d, disableOnShadowErrors=%d, disableAccessOnCawlError=%d, "
                           "disableAccessOnServiceCrash=%d\n",
                           gSecuritySettings.securitySettings.controlUsbHid,
                           gSecuritySettings.securitySettings.controlUsbMassStorage,
                           gSecuritySettings.securitySettings.controlUsbBluetooth,
                           gSecuritySettings.securitySettings.controlUsbNetwork,
                           gSecuritySettings.securitySettings.controlIEE1394Storage,
                           gSecuritySettings.securitySettings.controlBluetoothHid,
                           gSecuritySettings.securitySettings.disableOnShadowErrors,
                           gSecuritySettings.securitySettings.disableAccessOnCawlError,
                           gSecuritySettings.securitySettings.disableAccessOnServiceCrash ) );
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setACL(
    __in  void *vInBuffer,//DldAclDescriptorHeader
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    bool                     set = false;
    DldAclDescriptorHeader*  AclDscrHdr = (DldAclDescriptorHeader*)vInBuffer;
    vm_size_t                inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    //
    // check the header validity
    //
    if( inSize < (sizeof( *AclDscrHdr )+offsetof( kauth_filesec, fsec_acl) + KAUTH_ACL_SIZE(0)) ||
        inSize < AclDscrHdr->size ||
        AclDscrHdr->deviceType.type.major >= DLD_DEVICE_TYPE_MAX ||
        AclDscrHdr->aclType >= kDldAclTypeMax ){
        
        DBG_PRINT_ERROR(("invalid input data on check phase 1\n"));
        return kIOReturnBadArgument;
        
    }// end if
    
    //
    // up to know we checked the header for integrity
    //
    struct kauth_filesec *filesec = (struct kauth_filesec *)(AclDscrHdr+1);
    
    //
    // check the filesec validity
    //
    if( filesec->fsec_magic != KAUTH_FILESEC_MAGIC ||
        (__offsetof(struct kauth_filesec, fsec_acl) + KAUTH_ACL_SIZE( filesec->fsec_acl.acl_entrycount )) < ( inSize-sizeof( *AclDscrHdr ) ) ){
        
        DBG_PRINT_ERROR(("invalid input data on check phase 2\n"));
        return kIOReturnBadArgument;
        
    }// end if
    
    if( NULL == gArrayOfAclArrays[ AclDscrHdr->aclType ] ){
        
        DBG_PRINT_ERROR(("NULL == gArrayOfAclArrays[ AclDscrHdr->aclType ]\n"));
        return kIOReturnNoResources;
    }
    
    if( KAUTH_FILESEC_NOACL == filesec->fsec_acl.acl_entrycount ){
        
        //
        // a case of NULL ACL, that is there is no ACL at all
        //
        set = gArrayOfAclArrays[ AclDscrHdr->aclType ]->setNullAcl( AclDscrHdr->deviceType );
        assert( set );
        
        return set? kIOReturnSuccess: kIOReturnError;
    }
    
    DldAclObject* aclObject = DldAclObject::withAcl( &filesec->fsec_acl, AclDscrHdr->deviceType );
    assert( aclObject );
    if( !aclObject )
        return kIOReturnNoMemory;
    
    assert( AclDscrHdr->aclType < kDldAclTypeMax && gArrayOfAclArrays[ AclDscrHdr->aclType ] );
    
    //
    // the aclObject object will be retained by setAcl
    //
    set = gArrayOfAclArrays[ AclDscrHdr->aclType ]->setAcl( aclObject );
    assert( set );
    
    //
    // the object must be released in any case
    //
    aclObject->release();
    
    return set? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

DldAclObject*
DldIOUserClient::userModeFilesecToKernelACL(
    __in mach_vm_address_t filesecUserMode,//kauth_filesec
    __in mach_vm_size_t    filesecSize,
    __out bool*            isNullACL
    )
{
    struct kauth_filesec* capturedFilesec;
    
    assert( preemption_enabled() );
    assert( false == *isNullACL );
    
    *isNullACL = false; // just for safety
    
    //
    // protect from DoS attack initiated by a malicious or hacked application
    //
    if( filesecSize > (1000*PAGE_SIZE) ){
        
        DBG_PRINT_ERROR(( "filesecSize > 1000*PAGE_SIZE\n" ));
        return NULL;
    }
    
    //
    // check for a minimum size - the descriptor with zero ACLs
    //
    if( filesecSize < (__offsetof(struct kauth_filesec, fsec_acl) + KAUTH_ACL_SIZE( 0x0 ) ) ){
        
        DBG_PRINT_ERROR(( "filesecSize < (__offsetof(struct kauth_filesec, fsec_acl) + KAUTH_ACL_SIZE( 0x0 ) )\n" ));
        return NULL;
    }
    
    capturedFilesec = (struct kauth_filesec*)IOMalloc( filesecSize );
    assert( capturedFilesec );
    if( !capturedFilesec ){
        
        DBG_PRINT_ERROR(( "IOMalloc( filesecSize ) failed \n"  ));
        return NULL;
    }
    
    int error;
    
    error = copyin( filesecUserMode , capturedFilesec, filesecSize );
    assert( !error );
    if( error ){
        
        DBG_PRINT_ERROR(( "copyin( filesecUserMode , filesecUserMode, filesecSize ) failed with the 0x%x error\n", error ));
        IOFree( capturedFilesec, filesecSize );
        return NULL;
    }
    
    //
    // check for the structure validity
    //
    if( capturedFilesec->fsec_magic != KAUTH_FILESEC_MAGIC ||
       (__offsetof(struct kauth_filesec, fsec_acl) + KAUTH_ACL_SIZE( capturedFilesec->fsec_acl.acl_entrycount )) < filesecSize ){
        
        DBG_PRINT_ERROR(("invalid input data for kauth_filesec\n"));
        IOFree( capturedFilesec, filesecSize );
        return NULL;
    }// end if
    
    if( KAUTH_FILESEC_NOACL == capturedFilesec->fsec_acl.acl_entrycount ){
        
        //
        // this is a case of NULL ACL
        //
        *isNullACL = true;
        IOFree( capturedFilesec, filesecSize );
        return NULL;
    } // end if( KAUTH_FILESEC_NOACL 
    
    DldDeviceType  type;
    bzero( &type, sizeof( type ) );// Unknown type
    
    DldAclObject* aclObject = DldAclObject::withAcl( &capturedFilesec->fsec_acl, type );
    assert( aclObject );
    if( !aclObject ){
        
        DBG_PRINT_ERROR(("DldAclObject::withAcl failed\n"));
        IOFree( capturedFilesec, filesecSize );
        return NULL;
    }
    
    IOFree( capturedFilesec, filesecSize );
    return aclObject;
}

//--------------------------------------------------------------------

//
// alocates a memory in the kernel mode and copies a content of a user mode memory, the allocated memory is of
// the same size as provided by the second parameter, the memory is allocated by a call to IOMalloc, a caller must free
//
void* DldIOUserClient::userModeMemoryToKernelMode(
    __in mach_vm_address_t  userAddress,
    __in mach_vm_size_t bufferSize
    )
{
    if( 0x0 == bufferSize || bufferSize > 1000*PAGE_SIZE )
        return NULL;
    
    void* kernelBuffer = IOMalloc( bufferSize );
    assert( kernelBuffer );
    if( !kernelBuffer ){
        
        DBG_PRINT_ERROR(("kernelBuffer = IOMalloc( bufferSize ) failed\n"));
        return NULL;
    }
    
    IOReturn error = copyin( userAddress , kernelBuffer, bufferSize );
    assert( !error );
    if( error ){
        
        DBG_PRINT_ERROR(( "copyin( userAddress , kernelBuffer, bufferSize ) failed with the 0x%x error\n", error ));
        IOFree( kernelBuffer, bufferSize );
        return NULL;
    }
    
    return kernelBuffer;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setShadowFile(
    __in  void *vInBuffer,//DldShadowFileDescriptorHeader
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    IOReturn                        RC = kIOReturnSuccess;
    DldShadowFileDescriptorHeader*  ShadowDscrHdr = (DldShadowFileDescriptorHeader*)vInBuffer;
    vm_size_t                       inSize = (vm_size_t)vInSize;
    
    //__asm__ volatile( "int $0x3" );
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    //
    // check the header validity
    //
    if( !gShadow ||
        inSize < (offsetof(DldShadowFileDescriptorHeader,name) + sizeof( "/X" ) ) ||
        inSize < ShadowDscrHdr->size ||
        ShadowDscrHdr->nameLength < sizeof( "/X" ) || // check for the shortest name
        ShadowDscrHdr->size < (vm_size_t)( &(((DldShadowFileDescriptorHeader*)0)->name[0]) + ShadowDscrHdr->nameLength ) || // check for integrity
        inSize < ShadowDscrHdr->size || // is the buffer big enough for the header and data
        L'\0' != ShadowDscrHdr->name[ ShadowDscrHdr->nameLength - 0x1 ] ||
        ( ShadowDscrHdr->maxFileSize < 0x10000 && DLD_IGNR_FSIZE != ShadowDscrHdr->maxFileSize ) // do not create files which are too small
        ){
        
        DBG_PRINT_ERROR(("invalid input data on check phase 1\n"));
        return kIOReturnBadArgument;
        
    }// end if
    
    DldIOShadowFile* shadowFile = DldIOShadowFile::withFileName( ShadowDscrHdr->name, ShadowDscrHdr->nameLength );
    if( !shadowFile ){
        
        DBG_PRINT_ERROR(("DldIOShadowFile::withFileName( %s, %u) failed\n", ShadowDscrHdr->name, (int)ShadowDscrHdr->nameLength));
        return kIOReturnError;
    }
    
    shadowFile->setFileSizes( ShadowDscrHdr->maxFileSize, ShadowDscrHdr->switchFileSize );
    shadowFile->setID( ShadowDscrHdr->shadowFileID );
    
    if( !gShadow->addShadowFile( shadowFile ) )
        RC = kIOReturnError;
    
    //
    // must be released in any case as addShadowFile takes a refernce in case of success
    //
    shadowFile->release();
    
    return RC;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setUserState(
    __in  void *vInBuffer,//DldLoggedUserInfo
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    DldLoggedUserInfo*  userState = (DldLoggedUserInfo*)vInBuffer;
    vm_size_t           inSize = (vm_size_t)vInSize;
    bool                set;
    
    assert( gCredsArray );
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    //
    // check the structure validity
    //
    if( inSize < ( sizeof( *userState ) ) ||
        kDldUserSessionUnknown == userState->sessionType || kDldUserSessionMax <= userState->sessionType ||
        kDldUserLoginStateUnknown == userState->loginState || kDldUserLoginStateMax <= userState->loginState ||
        kDldUserSessionActivityUnknown == userState->activityState || kDldUserSessionActivityMax <= userState->activityState ){
        
        DBG_PRINT_ERROR(("invalid input data on check phase 1\n"));
        return kIOReturnBadArgument;
        
    }// end if
    
    if( NULL == gCredsArray ){
        
        DBG_PRINT_ERROR(("gCredsArray is NULL\n"));
        return kIOReturnNoResources;
    }
    
    set = gCredsArray->setUserState( userState );
    
    return set? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setVidPidWhiteListEx(
    __in  void *vInBuffer,//DldDeviceVidPidDscr
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    __in DldWhiteListType  listType
    )
{
    DldDeviceVidPidDscr*  whiteList = (DldDeviceVidPidDscr*)vInBuffer;
    vm_size_t             inSize = (vm_size_t)vInSize;
    bool                  set;
    
    assert( gWhiteList );
    assert( kDldWhiteListUSBVidPid == listType || kDldTempWhiteListUSBVidPid == listType );
    
    if( ! ( kDldWhiteListUSBVidPid == listType || kDldTempWhiteListUSBVidPid == listType ) ){
        
        DBG_PRINT_ERROR(("an invalid list type %d\n", (int)listType));
        return kIOReturnBadArgument;
    }
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( NULL == gWhiteList ){
        
        DBG_PRINT_ERROR(("gWhiteList is NULL\n"));
        return kIOReturnNoResources;
    }
    
    if( inSize < sizeof( whiteList->common ) ){
        
        DBG_PRINT_ERROR(("InSize < sizeof( whiteList->common )\n"));
        return kIOReturnBadArgument;
    }
    
    //
    // get an ACL that defines to whom the white list is applied
    //
    DldAclObject* aclObject = NULL;
    
    if( 0x0 != whiteList->common.filesec ){
        
        bool isNullACL = false;
        
        aclObject = this->userModeFilesecToKernelACL( whiteList->common.filesec, whiteList->common.filesecSize, &isNullACL );
        if( (! aclObject) && (! isNullACL) ){
            
            DBG_PRINT_ERROR(("userModeFilesecToKernelACL() failed, usually because of a bad pointer\n"));
            return kIOReturnBadArgument;
        }
        
    }// end if( 0x0 != whiteList->common.filesec )
    
    //
    // the input buffer validity is checked by setWhiteListWithCopy()
    //
    set = gWhiteList->setWhiteListWithCopy( listType,
                                            whiteList,
                                            inSize,
                                            aclObject? aclObject->getAcl(): NULL );
    
    if( aclObject )
        aclObject->release();
    
    return set? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setVidPidWhiteList(
    __in  void *vInBuffer,//DldDeviceVidPidDscr
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    return this->setVidPidWhiteListEx( vInBuffer,
                                       vOutBuffer,
                                       vInSize,
                                       vOutSizeP,
                                       kDldWhiteListUSBVidPid );
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setTempVidPidWhiteList(
    __in  void *vInBuffer,//DldDeviceVidPidDscr
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    return this->setVidPidWhiteListEx( vInBuffer,
                                      vOutBuffer,
                                      vInSize,
                                      vOutSizeP,
                                      kDldTempWhiteListUSBVidPid );
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setUIDWhiteListEx(
    __in  void *vInBuffer,//DldDeviceUIDDscr
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    __in DldWhiteListType  listType
    )
{
    DldDeviceUIDDscr*  whiteList = (DldDeviceUIDDscr*)vInBuffer;
    vm_size_t          inSize = (vm_size_t)vInSize;
    bool               set;
    
    assert( gWhiteList );
    assert( kDldWhiteListUSBUid == listType || kDldTempWhiteListUSBUid == listType );
    
    if( ! ( kDldWhiteListUSBUid == listType || kDldTempWhiteListUSBUid == listType ) ){
        
        DBG_PRINT_ERROR(("an invalid list type %d\n", (int)listType));
        return kIOReturnBadArgument;
    }
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( NULL == gWhiteList ){
        
        DBG_PRINT_ERROR(("gWhiteList is NULL\n"));
        return kIOReturnNoResources;
    }
    
    if( inSize < sizeof( whiteList->common ) ){
        
        DBG_PRINT_ERROR(("InSize < sizeof( whiteList->common )\n"));
        return kIOReturnBadArgument;
    }
    
    //
    // get an ACL that defines to whom the white list is applied
    //
    DldAclObject* aclObject = NULL;
    
    if( 0x0 != whiteList->common.filesec ){
        
        bool isNullACL = false;
        aclObject = this->userModeFilesecToKernelACL( whiteList->common.filesec, whiteList->common.filesecSize, &isNullACL );
        if( (! aclObject) && (! isNullACL) ){
            
            DBG_PRINT_ERROR(("userModeFilesecToKernelACL() failed, usually because of a bad pointer\n"));
            return kIOReturnBadArgument;
        }
        
    }// end if( 0x0 != whiteList->common.filesec )
    
    //
    // the input buffer validity is checked by setWhiteListWithCopy()
    //
    set = gWhiteList->setWhiteListWithCopy( listType,
                                            whiteList,
                                            inSize,
                                            aclObject? aclObject->getAcl(): NULL );
    
    if( aclObject )
        aclObject->release();
    
    return set? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setUIDWhiteList(
                                 __in  void *vInBuffer,//DldDeviceUIDDscr
                                 __out void *vOutBuffer,
                                 __in  void *vInSize,
                                 __in  void *vOutSizeP,
                                 void *, void *)
{
    return this->setUIDWhiteListEx( vInBuffer,
                                    vOutBuffer,
                                    vInSize,
                                    vOutSizeP,
                                    kDldWhiteListUSBUid );
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setTempUIDWhiteList(
                                 __in  void *vInBuffer,//DldDeviceUIDDscr
                                 __out void *vOutBuffer,
                                 __in  void *vInSize,
                                 __in  void *vOutSizeP,
                                 void *, void *)
{
    return this->setUIDWhiteListEx( vInBuffer,
                                    vOutBuffer,
                                    vInSize,
                                    vOutSizeP,
                                    kDldTempWhiteListUSBUid );
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setDVDWhiteList(
    __in  void *vInBuffer,//DldDeviceUIDDscr
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
    DldDeviceUIDDscr*  whiteList = (DldDeviceUIDDscr*)vInBuffer;
    vm_size_t          inSize = (vm_size_t)vInSize;
    bool               set;
    
    assert( gWhiteList );
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( NULL == gWhiteList ){
        
        DBG_PRINT_ERROR(("gWhiteList is NULL\n"));
        return kIOReturnNoResources;
    }
    
    if( inSize < sizeof( whiteList->common ) ){
        
        DBG_PRINT_ERROR(("InSize < sizeof( whiteList->common )\n"));
        return kIOReturnBadArgument;
    }
    
    //
    // get an ACL that defines to whom the white list is applied
    //
    DldAclObject* aclObject = NULL;
    
    if( 0x0 != whiteList->common.filesec ){
        
        bool isNullACL = false;
        aclObject = this->userModeFilesecToKernelACL( whiteList->common.filesec, whiteList->common.filesecSize, &isNullACL );
        if( (! aclObject) && (! isNullACL) ){
            
            DBG_PRINT_ERROR(("userModeFilesecToKernelACL() failed, usually because of a bad pointer\n"));
            return kIOReturnBadArgument;
        }
        
    }// end if( 0x0 != whiteList->common.filesec )
    
    //
    // the input buffer validity is checked by setWhiteListWithCopy()
    //
    set = gWhiteList->setWhiteListWithCopy( kDldWhiteListDVDUid,
                                            whiteList,
                                            inSize,
                                            aclObject? aclObject->getAcl(): NULL );
    
    if( aclObject )
        aclObject->release();
    
    return set? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::processServiceDiskCawlResponse(
    __in  void *vInBuffer, // DldServiceDiskCawlResponse
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
#ifdef _DLD_MACOSX_CAWL
    DldServiceDiskCawlResponse*  diskCawlResponse = (DldServiceDiskCawlResponse*)vInBuffer;
    vm_size_t             inSize = (vm_size_t)vInSize;
    
    assert( gDiskCAWL );
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( NULL == gDiskCAWL ){
        
        DBG_PRINT_ERROR(("gDiskCAWL is NULL\n"));
        return kIOReturnNoResources;
    }
    
    if( inSize < sizeof( *diskCawlResponse ) ){
        
        DBG_PRINT_ERROR(("InSize < sizeof( *diskCawlResponse )\n"));
        return kIOReturnBadArgument;
    }
    
    return gDiskCAWL->processServiceResponse( diskCawlResponse );
#else
    return kIOReturnUnsupported;
#endif
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::processServiceSocketFilterResponse(
    __in  void *vInBuffer, // DldSocketFilterServiceResponse
    __out void *vOutBuffer,
    __in  void *vInSize,
    __in  void *vOutSizeP,
    void *, void *)
{
#ifdef _DLD_SOCKET_FILTER
    DldSocketFilterServiceResponse*  serviceSocketFilterResponse = (DldSocketFilterServiceResponse*)vInBuffer;
    vm_size_t             inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( inSize < sizeof( *serviceSocketFilterResponse ) ){
        
        DBG_PRINT_ERROR(("inSize < sizeof(*serviceResponse)\n"));
        return kIOReturnBadArgument;
    }
    
    if( ! gSocketFilter ){
        
        DBG_PRINT_ERROR(("gSocketFilter is NULL\n"));
        return kIOReturnBadArgument;
    }
    
    return gSocketFilter->processServiceResponse( serviceSocketFilterResponse );
#else // _DLD_SOCKET_FILTER
    return kIOReturnUnsupported;
#endif // _DLD_SOCKET_FILTER
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setProcessSecurity( __in  void *vInBuffer, // DldProcessSecurity
                                     __out void *vOutBuffer,
                                     __in  void *vInSize,
                                     __in  void *vOutSizeP,
                                     void *, void *)
{
    DldProcessSecurity*  processSecurity = (DldProcessSecurity*)vInBuffer;
    vm_size_t            inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( inSize < sizeof( *processSecurity ) ){
        
        DBG_PRINT_ERROR(("inSize < sizeof(DldProcessSecurity)\n"));
        return kIOReturnBadArgument;
    }
    
    if( ! gServiceProtection ){
        
        DBG_PRINT_ERROR(("gServiceProtection is NULL\n"));
        return kIOReturnBadArgument;
    }
    
    //
    // get acl as acl object
    //
    DldAclObject* aclObject = NULL;
    
    if( 0x0 != processSecurity->acl ){
        
        bool isNullACL = false;
        aclObject = this->userModeFilesecToKernelACL( processSecurity->acl, processSecurity->aclSize, &isNullACL );
        if( (! aclObject) && (! isNullACL) ){
            
            DBG_PRINT_ERROR(("this->userModeFilesecToKernelACL() failed or an ACL is incorrect\n"));
            return kIOReturnNoMemory;
        }
    } // if( 0x0 != processSecurity->acl )
    
    IOReturn err = gServiceProtection->setProcessSecurity( processSecurity->pid , aclObject );
    
    if( aclObject )
        aclObject->release();
    
    return err;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setFileSecurity( __in  void *vInBuffer, // DldFileSecurity
                                  __out void *vOutBuffer,
                                  __in  void *vInSize,
                                  __in  void *vOutSizeP,
                                  void *, void *)
{
    //userModeMemoryToKernelMode
    DldFileSecurity*     fileSecurity = (DldFileSecurity*)vInBuffer;
    vm_size_t            inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( inSize < sizeof( DldFileSecurity ) ){
        
        DBG_PRINT_ERROR(("inSize < sizeof(DldFileSecurity)\n"));
        return kIOReturnBadArgument;
    }
    
    if( ! gServiceProtection )
        return kIOReturnBadArgument;
    
    IOReturn                err;
    DldAclObject*           usersAclObject = NULL;
    DldAclWithProcs*        procsAcl = NULL;
    DldAclWithProcsObject*  procsAclObj = NULL;
    OSString*               pathOSStr = NULL;
    char*                   pathCStr = NULL;
    
    if( 0x0 != fileSecurity->usersAclSize ){
        
        bool isNullACL = false;
        usersAclObject = this->userModeFilesecToKernelACL( fileSecurity->usersAcl, fileSecurity->usersAclSize, &isNullACL );
        if( (! usersAclObject) && (! isNullACL) ){
            
            DBG_PRINT_ERROR(("this->userModeFilesecToKernelACL() failed or an ACL is incorrect\n"));
            err = kIOReturnNoMemory;
            goto __exit;
        }
    } // end if( 0x0 != fileSecurity->usersAclSize )
    
    if( 0x0 != fileSecurity->processesAclSize ){
        
        procsAcl = (DldAclWithProcs*)this->userModeMemoryToKernelMode( fileSecurity->processesAcl, fileSecurity->processesAclSize );
        if( ! procsAcl ){
            
            DBG_PRINT_ERROR(("this->userModeFilesecToKernelACL() failed or an ACL is incorrect\n"));
            err = kIOReturnNoMemory;
            goto __exit;
        }
        
        if( procsAcl ){
            //
            // check the validity
            //
            if( DldAclWithProcsSize( procsAcl ) > fileSecurity->processesAclSize ){
                
                //
                // a wrong input data
                //
                
                DBG_PRINT_ERROR(("DldAclWithProcsSize( fileProcAcl ) > fileSecurity->processesAclSize\n"));
                err = kIOReturnBadArgument;
                goto __exit;
            }
            
            procsAclObj = DldAclWithProcsObject::withAcl( procsAcl );
            assert( !procsAclObj );
            if( !procsAclObj ){
                
                DBG_PRINT_ERROR(("DldAclWithProcsObject::withAcl( procsAcl ) failed\n"));
                err = kIOReturnNoMemory;
                goto __exit;
            }
        } // end if( procsAcl )
        
    } // end if( 0x0 != fileSecurity->processesAclSize )
    
    pathCStr = (char*)this->userModeMemoryToKernelMode( fileSecurity->path, fileSecurity->pathSize );
    assert( pathCStr );
    if( !pathCStr ){
        
        DBG_PRINT_ERROR(("this->userModeMemoryToKernelMode( fileSecurity->path, fileSecurity->pathSize ) failed\n"));
        err = (0x0 != fileSecurity->pathSize ) ? kIOReturnNoMemory : kIOReturnBadArgument;
        goto __exit;
    }
    
    //
    // check the validity
    //
    if( pathCStr[ fileSecurity->pathSize - 0x1 ] != '\0' ){
        
        DBG_PRINT_ERROR(("no terminating zero\n"));
        err = kIOReturnBadArgument;
        goto __exit;
    }
    
    pathOSStr = OSString::withCString( pathCStr );
    assert( pathOSStr );
    if( !pathOSStr ){
        
        DBG_PRINT_ERROR((" OSString::withCString( pathCStr ) failed\n"));
        err = kIOReturnNoMemory;
        goto __exit;
    }
    
    err = gServiceProtection->setFileSecurity( pathOSStr, usersAclObject, procsAclObj );
    
__exit:
    
    if( pathCStr )
        IOFree( pathCStr, fileSecurity->pathSize );
    
    if( pathOSStr )
        pathOSStr->release();
    
    if( usersAclObject )
        usersAclObject->release();
    
    if( procsAclObj )
        procsAclObj->release();
    
    //
    // a memory has been allocated for a copy of the data in the fileProcAclObj object
    //
    if( procsAcl )
        IOFree( procsAcl, fileSecurity->usersAclSize );
    
    return err;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setDLAdminProperties( __in  void *vInBuffer,
                                       __out void *vOutBuffer,
                                       __in  void *vInSize,
                                       __in  void *vOutSizeP,
                                       void *, void *)

{
    DldDLAdminsSettings*     adminSettings = (DldDLAdminsSettings*)vInBuffer;
    vm_size_t                inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( inSize < sizeof( DldDLAdminsSettings ) ){
        
        DBG_PRINT_ERROR(("inSize < sizeof(DldDLAdminsSettings)\n"));
        return kIOReturnBadArgument;
    }
    
    gServiceProtection->setDLAdminsSettings( adminSettings );
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::setEncryptionProviderProperties( __in  void *vInBuffer,
                                                  __out void *vOutBuffer,
                                                  __in  void *vInSize,
                                                  __in  void *vOutSizeP,
                                                  void *, void *)
{
    DldEncryptionProviderProperties*     providerSettings = (DldEncryptionProviderProperties*)vInBuffer;
    vm_size_t                            inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( inSize < sizeof( *providerSettings ) || providerSettings->provider < 0 || providerSettings->provider >= DLD_STATIC_ARRAY_SIZE(gEncryptionProvider)){
        
        DBG_PRINT_ERROR(("an invalid structure, size %d, provider %d\n", inSize, (unsigned int)providerSettings->provider));
        return kIOReturnBadArgument;
    }
    
    if( ProviderUnknown == providerSettings->provider ){
        
        //
        // the unknown provider settings are not writable
        //
        return kIOReturnNotPrivileged;
    }
    
    gEncryptionProvider[ providerSettings->provider ].enabled = (0x0 != providerSettings->enabled);
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::reportIPCCompletion( __in  void *vInBuffer,
                                      __out void *vOutBuffer,
                                      __in  void *vInSize,
                                      __in  void *vOutSizeP,
                                      void *, void *)
{
    DldDriverEventData*     eventData = (DldDriverEventData*)vInBuffer;
    vm_size_t               inSize = (vm_size_t)vInSize;
    
    //
    // there is no output data
    //
    *(UInt32*)vOutSizeP = 0x0;
    
    if( inSize < sizeof( *eventData ) ){
        
        DBG_PRINT_ERROR(("an invalid structure, size %d\n", inSize));
        return kIOReturnBadArgument;
    }
    
    return DldIPCUserClient::ProcessResponse( eventData );
}

//--------------------------------------------------------------------

IOReturn
DldIOUserClient::getDriverProperties( __in  void *vInBuffer,
                                      __out void *vOutBuffer,
                                      __in  void *vInSize,
                                      __in  void *vOutSizeP,
                                      void *, void *)
{
    assert( 0x0 != gDriverProperties.size );
    
    //
    // the driver must be able to change the size field
    //
    if( NULL == vOutBuffer || *(UInt32*)vOutSizeP < (offsetof( DldDriverProperties, size ) + sizeof(gDriverProperties.size) ) )
        return kIOReturnBadArgument;
    
    UInt32  size = min( gDriverProperties.size, *(UInt32*)vOutSizeP );
    
    *(UInt32*)vOutSizeP = size;
    bcopy( &gDriverProperties, vOutBuffer, size );
    ((DldDriverProperties*)(vOutBuffer))->size = size;
    
    return ( size < gDriverProperties.size ) ? kIOReturnOverrun : kIOReturnSuccess;
}


//--------------------------------------------------------------------

IOReturn
DldIOUserClientRef::registerUserClient( __in DldIOUserClient* client )
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

IOReturn
DldIOUserClientRef::unregisterUserClient( __in DldIOUserClient* client )
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
                     "DldSocketFilter::unregisterUserClient()", // wait message
                     &ts );                         // sleep interval
        
    } while( this->clientInvocations != 0 );
    
    currentClient->release();
    this->pendingUnregistration = false;
    
    return unregistered? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

bool
DldIOUserClientRef::isUserClientPresent()
{
    return ( NULL != this->userClient );
}

//--------------------------------------------------------------------

//
// if non NULL value is returned a caller must call releaseUserClient()
// when it finishes with the returned client object
//
DldIOUserClient*
DldIOUserClientRef::getUserClient()
{
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
        
        return NULL;
    }
    
    return currentClient;
}

//--------------------------------------------------------------------

void
DldIOUserClientRef::releaseUserClient()
{
    //
    // do not exchange or add any condition before OSDecrementAtomic as it must be always done!
    //
    if( 0x1 == OSDecrementAtomic( &this->clientInvocations ) && NULL == this->userClient ){
        
        //
        // this was the last invocation
        //
        wakeup( &this->clientInvocations );
    }
}

//--------------------------------------------------------------------

pid_t
DldIOUserClientRef::getUserClientPid()
{
    DldIOUserClient* client = this->getUserClient();
    if( ! client )
        return (-1);
    
    pid_t pid = client->getPid();
    this->releaseUserClient();
    
    return pid;
}

//--------------------------------------------------------------------
