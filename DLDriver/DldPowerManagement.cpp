/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/IOService.h>
#include <IOKit/IONotifier.h>
#include "DldPowerManagement.h"
#include "DldInternalLog.h"


IONotifier *gPMNotifier;

extern "C" {
    
    //--------------------------------------------------------------------
    
    /*
     the normal calling stack for a power down event is
     #0  DldPowerManagementHandler (target=0x5640a00, refCon=0x0, messageType=3758096976, provider=0x559e600, messageArgument=0x318a3d00, argSize=0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldPowerManagement.cpp:35
     #1  0x0060873d in IOService::messageClient (this=0x559e600, type=3758096976, client=0x5fb4480, argument=0x318a3d00, argSize=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1357
     #2  0x0065cca7 in platformHaltRestartApplier (object=0x5fb4480, context=0x318a3dbc) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOPMrootDomain.cpp:3133
     #3  0x00606947 in applyToInterestNotifiers (target=0x559e600, typeOfInterest=0x54e99c0, applier=0x65cc12 <platformHaltRestartApplier(OSObject*, void*)>, context=0x318a3dbc) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1408
     #4  0x006069de in IOService::applyToInterested (this=0x559e600, typeOfInterest=0x54e99c0, applier=0x65cc12 <platformHaltRestartApplier(OSObject*, void*)>, context=0x318a3dbc) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1419
     #5  0x0065ebcf in IOPMrootDomain::handlePlatformHaltRestart (this=0x559e600, pe_type=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOPMrootDomain.cpp:3184
     #6  0x0063d2fc in PEHaltRestart (type=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOPlatformExpert.cpp:804
     #7  0x002fb47a in halt_all_cpus (reboot=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/AT386/model_dep.c:696
     #8  0x0022d221 in host_reboot (host_priv=0x844860, options=8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/machine.c:152
     #9  0x0053034f in boot (paniced=1, howto=8, command=0x318a3f08 "") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_shutdown.c:211
     #10 0x0053fd02 in reboot (p=0x58a1d20, uap=0x54dad88, retval=0x569a8f4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_xxx.c:119
     #11 0x005b19b5 in unix_syscall64 (state=0x54dad84) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365
     
     for a sleep event is
     #0  DldPowerManagementHandler (target=0x5a1ac80, refCon=0x0, messageType=3758097024, provider=0x559e600, messageArgument=0x31b43cb4, argSize=0) at /work/DeviceLockProject/DeviceLoc IOKitDriver/DldPowerManagement.cpp:35
     #1  0x0060873d in IOService::messageClient (this=0x559e600, type=3758097024, client=0x5a16800, argument=0x31b43cb4, argSize=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Ke nel/IOService.cpp:1357
     #2  0x00614983 in IOService::pmTellClientWithResponse (object=0x5a16800, arg=0x31b43d98) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOServicePM.cpp:4912
     #3  0x00606947 in applyToInterestNotifiers (target=0x559e600, typeOfInterest=0x54e99c0, applier=0x614740 <IOService::pmTellClientWithResponse(OSObject*, void*)>, context=0x31b43d98  at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1408
     #4  0x006069de in IOService::applyToInterested (this=0x559e600, typeOfInterest=0x54e99c0, applier=0x614740 <IOService::pmTellClientWithResponse(OSObject*, void*)>, context=0x31b43d 8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1419
     #5  0x00615001 in IOService::tellClientsWithResponse (this=0x559e600, messageType=-536870272, filter=0x654a77 <clientMessageFilter(OSObject*, void*)>) at /work/Mac_OS_X_kernel/10_6 4/xnu-1504.7.4/iokit/Kernel/IOServicePM.cpp:4765
     #6  0x006588b2 in IOPMrootDomain::tellChangeDown (this=0x559e600, stateNum=2) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOPMrootDomain.cpp:3350
     #7  0x00610805 in IOService::tellChangeDown2 (this=0x559e600, stateNum=2) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOServicePM.cpp:4611
     #8  0x00610835 in IOService::ParentDownTellPriorityClientsPowerDown (this=0x559e600) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOServicePM.cpp:3911
     #9  0x0061d5f4 in IOService::servicePMRequest (this=0x559e600, request=0x7b9d300, queue=0x55cf3c0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOServicePM.cpp:5774
     #10 0x0061142f in IOPMWorkQueue::checkForWork (this=0x55cf3c0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOServicePM.cpp:6400
     #11 0x00622c58 in IOWorkLoop::runEventSources (this=0x553b140) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOWorkLoop.cpp:318
     #12 0x00622f2b in IOWorkLoop::threadMain (this=0x553b140) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOWorkLoop.cpp:342     
     */
    IOReturn
    DldPowerManagementHandler(
                   void * target,
                   void * refCon,
                   UInt32 messageType,
                   IOService * provider,
                   void * messageArgument,
                   vm_size_t argSize
                   )
    {
        //__asm__ volatile( "int $0x3" );
        
        //
        // use the ASL as the internal log might not be able to flush data on power off( as the ASL too )
        //
        DLD_COMM_LOG_TO_ASL( POWER_MANAGEMENT, ("Got a power management notification. A message type was %d\n", (int)messageType));
        
        switch( messageType ){
            case kIOMessageSystemWillPowerOff:
            case kIOMessageSystemWillRestart:
                if( gInternalLog )
                    gInternalLog->prepareForPowerOff();
                break;
                
            case kIOMessageSystemWillNotPowerOff:
            case kIOMessageSystemHasPoweredOn:
            case kIOMessageSystemWillPowerOn:
                break;
        }
        
        acknowledgeSleepWakeNotification( refCon );
        
        return 0;
    }
    
    //--------------------------------------------------------------------

    kern_return_t
    DldPowerManagementStart( void* object )
    { 
        gPMNotifier = registerPrioritySleepWakeInterest( &DldPowerManagementHandler, object, NULL);
        assert( NULL != gPMNotifier );
        gDldDbgData.gPMNotifier = (void*)gPMNotifier;
        return KERN_SUCCESS;
    }
    
    //--------------------------------------------------------------------

    kern_return_t
    DldPowerManagementStop()
    {
        if( gPMNotifier )
            gPMNotifier->remove();
        
        return KERN_SUCCESS;
    }
    
    //--------------------------------------------------------------------

} // extern "C"