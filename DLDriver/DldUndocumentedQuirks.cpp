/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldUndocumentedQuirks.h"
#include "DldVmPmap.h"
#include "DldFakeFSD.h"
#include <sys/lock.h>
#include <sys/proc.h>

//--------------------------------------------------------------------

//
// an offset to mach task pointer in the BSD proc structure,
// used for a conversion from BSD process to mach task
//
static vm_offset_t  gTaskOffset = (vm_offset_t)(-1);

//
// an offset to the task's bsd_info field, used to convert the
// mach task to a bsd proc if the last exists( there might
// be a mach task without a corresponding BSD process ), the bsd_info
// field is zeroed on process exit
//
static vm_offset_t  gProcOffset = (vm_offset_t)(-1);;

//--------------------------------------------------------------------

task_t DldBsdProcToTask( __in proc_t proc )
{
    assert( (vm_offset_t)(-1) != gTaskOffset );
    
    return *(task_t*)( (vm_offset_t)proc + gTaskOffset );
}

//--------------------------------------------------------------------

// use get_bsdtask_info() instead!
proc_t DldTaskToBsdProc( __in task_t task )
{
    assert( (vm_offset_t)(-1) != gProcOffset );
    
    proc_t  proc;
    
    proc = *(proc_t*)( (vm_offset_t)task + gProcOffset );
    
    // like get_bsdtask_info() does
    if( !proc )
        proc = kernproc;
    
    return proc;
}

//--------------------------------------------------------------------

vm_offset_t
DldGetTaskOffsetInBsdProc(
    __in proc_t  bsdProc,
    __in task_t  machTask
)
{
    task_t*  task_p = (task_t*)bsdProc;
    
    //
    // it is unlikely that the proc structure size will be greater than a page size
    //
    while( task_p < (task_t*)( (vm_offset_t)bsdProc + PAGE_SIZE ) ){
        
        if( (vm_offset_t)task_p == page_aligned( (vm_offset_t)task_p ) ){
            
            //
            // page boundary crossing, check for validity
            //
            if( 0x0 == DldVirtToPhys( (vm_offset_t)task_p ) )
                break;
            
        }// end if
        
        if( *task_p == machTask )
            return ( (vm_offset_t)task_p - (vm_offset_t)bsdProc );
        
        ++task_p;
        
    }// end while
    
    //
    // failed to find an offset
    //
    return (vm_offset_t)(-1);
}

//--------------------------------------------------------------------

vm_offset_t
DldGetBsdProcOffsetInTask(
    __in proc_t  bsdProc,
    __in task_t  machTask
    )
/*
 returns the task_t's bsd_info field offset,
 the kernel has an internal function
 void  *get_bsdtask_info(task_t t)
 {
    return(t->bsd_info);
 }
 which is unavailable for third party kernel extensions
 */
{
    proc_t*  proc_p = (proc_t*)machTask;
    
    //
    // it is unlikely that the task structure size will be greater than a page size
    //
    while( proc_p < (proc_t*)( (vm_offset_t)machTask + PAGE_SIZE ) ){
        
        if( (vm_offset_t)proc_p == page_aligned( (vm_offset_t)proc_p ) ){
            
            //
            // page boundary crossing, check for validity
            //
            if( 0x0 == DldVirtToPhys( (vm_offset_t)proc_p ) )
                break;
            
        }// end if
        
        if( *proc_p == bsdProc )
            return ( (vm_offset_t)proc_p - (vm_offset_t)machTask );
        
        ++proc_p;
        
    }// end while
    
    //
    // failed to find an offset
    //
    return (vm_offset_t)(-1);
}

//--------------------------------------------------------------------

//
// it is hardly that we will need more than CPUs in the system,
// but we need a reasonable prime value for a good dustribution
//
static IOSimpleLock*    gPreemptionLocks[ 19 ];

bool
DldAllocateInitPreemtionLocks()
{
    
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( gPreemptionLocks ); ++i ){
        
        gPreemptionLocks[ i ] = IOSimpleLockAlloc();
        assert( gPreemptionLocks[ i ] );
        if( !gPreemptionLocks[ i ] )
            return false;
        
    }
    
    return true;
}

//--------------------------------------------------------------------

void
DldFreePreemtionLocks()
{
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( gPreemptionLocks ); ++i ){
        
        if( gPreemptionLocks[ i ] )
            IOSimpleLockFree( gPreemptionLocks[ i ] );
        
    }
}

//--------------------------------------------------------------------

#define DldTaskToPreemptionLockIndx( _t_ ) (int)( ((vm_offset_t)(_t_)>>5)%DLD_STATIC_ARRAY_SIZE( gPreemptionLocks ) )

//
// MAC OS X kernel doesn't export disable_preemtion() and does not
// allow a spin lock allocaton on the stack! Marvelous, first time
// I saw such a reckless design! All of the above are allowed by
// Windows kernel as IRQL rising to DISPATCH_LEVEL and KSPIN_LOCK.
//

//
// returns a cookie for DldEnablePreemption
//
int
DldDisablePreemption()
{
    int indx;
    
    //
    // check for a recursion or already disabled preemption,
    // we support a limited recursive functionality - the
    // premption must not be enabled by calling enable_preemption()
    // while was disabled by this function
    //
    if( !preemption_enabled() )
        return (int)(-1);
    
    indx = DldTaskToPreemptionLockIndx( current_task() );
    assert( indx < DLD_STATIC_ARRAY_SIZE( gPreemptionLocks ) );
    
    //
    // acquiring a spin lock have a side effect of preemption disabling,
    // so try to find any free slot for this thread
    //
    while( !IOSimpleLockTryLock( gPreemptionLocks[ indx ] ) ){
        
        indx = (indx+1)%DLD_STATIC_ARRAY_SIZE( gPreemptionLocks );
        
    }// end while
    
    assert( !preemption_enabled() );
    
    return indx;
}

//--------------------------------------------------------------------

//
// accepts a value returned by DldDisablePreemption
//
void
DldEnablePreemption( __in int cookie )
{
    assert( !preemption_enabled() );
    
    //
    // check whether a call to DldDisablePreemption was a void one
    //
    if( (int)(-1) == cookie )
        return;
    
    assert( cookie < DLD_STATIC_ARRAY_SIZE( gPreemptionLocks ) );
    
    //
    // release the lock thus enabling preemption
    //
    IOSimpleLockUnlock( gPreemptionLocks[ cookie ] );
    
    assert( preemption_enabled() );
}

//--------------------------------------------------------------------

bool
DldInitUndocumentedQuirks()
{
    IOReturn  RC;
    
    assert( preemption_enabled() );
    assert( current_task() == kernel_task );
    
    if( !DldAllocateInitPreemtionLocks() ){
        
        DBG_PRINT_ERROR(("DldAllocateInitPreemtionLocks() failed\n"));
        return false;
    }
    
    if( ! gGlobalSettings.doNotHookVFS ){
        
        //
        // get a vnode layout
        //
        RC = DldGetVnodeLayout();
        assert( kIOReturnSuccess == RC );
        if( kIOReturnSuccess != RC ){
            
            DBG_PRINT_ERROR(("DldGetVnodeLayout() failed\n"));
            return false;
        }
        
    } else {
        
        DBG_PRINT(("gGlobalSettings.doNotHookVFS is TRUE, the VFS hooking will not be applied"));
    }
    
    //
    // get a mach task field offset in BSD proc
    //
    gTaskOffset = DldGetTaskOffsetInBsdProc( current_proc(), current_task() );
    assert( ((vm_offset_t)(-1)) != gTaskOffset );
    if( ((vm_offset_t)(-1)) == gTaskOffset ){
        
        DBG_PRINT_ERROR(("DldGetTaskOffsetInBsdProc() failed\n"));
        return false;
    }
    
    gDldDbgData.gTaskOffset = gTaskOffset;
    
    gProcOffset = DldGetBsdProcOffsetInTask( current_proc(), current_task() );
    assert( ((vm_offset_t)(-1)) != gProcOffset );
    if( ((vm_offset_t)(-1)) == gProcOffset ){
        
        DBG_PRINT_ERROR(("DldGetBsdProcOffsetInTask() failed\n"));
        return false;
    }
    
    gDldDbgData.gProcOffset = gProcOffset;
    
    return true;
}

//--------------------------------------------------------------------

void
DldFreeUndocumentedQuirks()
{
    DldFreePreemtionLocks();
}

//--------------------------------------------------------------------
/*
#include <libkern/c++/OSKext.h>

vm_address_t   gKernStart;

bool
DldGetKernelKextInfo()
{
    OSKext*    kernelKext;
    OSString*  kernelName;
    
    //
    // it is  possible that the old kernels used __kernel__ name instead ( see Singh's book p 1254 )
    //
    kernelName = OSString::withCStringNoCopy( "mach_kernel" );
    assert( kernelName );
    if( !kernelName ){
        
        DBG_PRINT_ERROR(("OSString::withCStringNoCopy(\"mach_kernel\") failed\n"));
        return false;
    }
    
    kernelKext = OSKext::lookupKextWithIdentifier( kernelName );
    assert( kernelKext );
    kernelName->release();
    if( !kernelKext ){
        
        DBG_PRINT_ERROR(("OSKext::lookupKextWithIdentifier( kernelName ) returned NULL, the kernel can't be located\n"));
        return false;
    }
    
    vm_address_t   kernelAddress;
    kernelAddress = trunc_page( (vm_address_t)preemption_enabled );
    while( kernelAddress ){
        
        OSKext*    kext;
        
        //
        // check the address one page below for a KEXT presence
        //
        kext = OSKext::lookupKextWithAddress( kernelAddress - PAGE_SIZE );
        if( kernelKext != kext ){
            //
            // so the kernelAddress is the start of the kernel
            // as either there is no extension before it
            // or there is another extension before the kernel
            //
            if( kext )
                kext->release();
            
            break;
        }
        
        kext->release();
        kernelAddress = kernelAddress - PAGE_SIZE;
    }// end while
    
    gKernStart = kernelAddress;
    
    kernelKext->release();
    return true;
}

//--------------------------------------------------------------------


typedef void (*kdp_send_t)(void * pkt, unsigned int pkt_len);
typedef void (*kdp_receive_t)(void * pkt, unsigned int * pkt_len, 
                              unsigned int timeout);
void 
kdp_register_send_receive(kdp_send_t send, kdp_receive_t receive);

void
kdp_unregister_send_receive(kdp_send_t send, kdp_receive_t receive);

void
DldKdpRegister()
{
    boolean_t    keepsyms = FALSE;
    
    DldGetKernelKextInfo();
    
    //
    // the __LINKEDIT section's data for the kernel's Mach-O header
    // is not jettisoned only if the keepsyms boot argument's value is 1,
    // for reference see OSKext::removeKextBootstrap(void) in the
    // xnu/libkern/c++/OSKext.cpp source file, the data is jettisoned
    // regardles of the kextd demon's '-j' parameter ( this is contary
    // to what is written in the Singh's book on p 1267 )
    //
    PE_parse_boot_argn("keepsyms", &keepsyms, sizeof(keepsyms));
    if( !keepsyms )
        return;
    
    DLD_COMM_LOG(("the keepsyms boot argument's value is 1\n"));
    
    //kdp_register_send_receive( NULL, NULL );
}
//--------------------------------------------------------------------
*/
