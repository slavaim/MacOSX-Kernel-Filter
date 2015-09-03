/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOUSBControllerV3.h"
#include "DldKernAuthorization.h"
#include "DldIOLog.h"
#include <sys/proc.h>
#include <sys/vm.h>

//--------------------------------------------------------------------

/*
 FYI - a call stack for a USB mouse access check
 
 #0  DldKauthAclEvaluate (cred=0x5952c24, eval=0x32133548) at /work/DL_MacSvn/mac/dl-0.x.beta/beta2/dl-0.x/DLDriver/DldKernAuthorization.cpp:327
 #1  0x473bafb6 in DldKauthAclEvaluate (dldEval=0x321336b4, acl=0x5d002e0) at /work/DL_MacSvn/mac/dl-0.x.beta/beta2/dl-0.x/DLDriver/DldKernAuthorization.cpp:806
 #2  0x473bbb19 in DldTypePermissionsArray::isAccessAllowed (this=0x7664680, dldEval=0x321336b4) at /work/DL_MacSvn/mac/dl-0.x.beta/beta2/dl-0.x/DLDriver/DldKernAuthorization.cpp:853
 #3  0x473bc8d2 in isAccessAllowed (param=0x321338e0) at /work/DL_MacSvn/mac/dl-0.x.beta/beta2/dl-0.x/DLDriver/DldKernAuthorization.cpp:1909
 #4  0x473aa6a4 in DldUsbAllowAccess (controller=0x59e3800, address=3, endpoint=0x619924c) at /work/DL_MacSvn/mac/dl-0.x.beta/beta2/dl-0.x/DLDriver/DldIOUSBControllerV3.cpp:228
 #5  0x473aad15 in DldIOUSBControllerV3::ReadV2 (this=0x84ed214, usbController=0x59e3800, buffer=0x7988f00, address=3, endpoint=0x619924c, completion=0x797ef94, noDataTimeout=0, completionTimeout=0, reqCount=6) at /work/DL_MacSvn/mac/dl-0.x.beta/beta2/dl-0.x/DLDriver/DldIOUSBControllerV3.cpp:428
 #6  0x4734ac7b in AppleUSBHCIDldHook<(_DldInheritanceDepth)0, (_DldUsbHciType)2>::ReadV2_hook () at AppleUSBEHCIDldHook.h:501
 #7  0x01376559 in ?? ()
 #8  0x0139cbc5 in ?? ()
 #9  0x0139e497 in ?? ()
 #10 0x0022fb84 in thread_call_thread (group=0x59e3800) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/thread_call.c:847 
 */

//--------------------------------------------------------------------

#define super DldHookerBaseInterface

//--------------------------------------------------------------------

#define DldUsbError   (kIOReturnNoBandwidth)

static bool DldUsbAllowAccess(
    __in IOUSBControllerV3* controller,
    __in USBDeviceAddress address,
    __in IOUSBController::Endpoint* endpoint
    )
{
    bool                  disable = false;
    DldAccessCheckParam   param;
    DldDeviceType         deviceType;
    DldIOService*         dldController = NULL;
    DldIOService*         dldUsbDevice = NULL;
    Boolean               checkSecurity = false;
    UInt32                numberOfInterfaces = 0x0;
    
    assert( preemption_enabled() );
    /*
    if ( 0x2 != address )
        return false;
     */
    
    dldController = DldIOService::RetrieveDldIOServiceForIOService( controller );
    assert( dldController );
    if( !dldController ){
        
        DBG_PRINT_ERROR(("DldIOService::RetrieveDldIOServiceForIOService( 0x%p, 0x%X )\n", controller, (int)address ));
        return true;
    }
    
    //
    // remember there is no guarantee that a returned IOService
    // will be valid, to be valid the corresponding DldIOService
    // must be found - this is done by isAccessAllowed
    //
    IOService*  usbDeviceService = NULL;
    
    //
    // we are not interested in the controller as it represent the root of the USB bus,
    // find a corresponding IOUSBDevice attached to the controller using the device's
    // address
    //
    dldController->LockShared();
    {// start of the lock
        
        const OSArray* childrenProps;
        
        childrenProps = dldController->getChildProperties();
        assert( childrenProps );
        if( childrenProps ){
            
            int count;
            
            count = childrenProps->getCount();
            for( int i = 0x0; i < count; ++i ){
                
                const OSObject*           object;
                DldObjectPropertyEntry*   property;
                
                object = childrenProps->getObject(i);
                assert( OSDynamicCast( DldObjectPropertyEntry, object ) );
                
                property = (DldObjectPropertyEntry*)object;
                if( DldObjectPopertyType_UsbDevice != property->dataU.property->typeDsc.type )
                    continue;
                
                if( address != property->dataU.usbDeviceProperty->usbDeviceAddress )
                    continue;
                
                usbDeviceService = property->dataU.property->service;
                assert( NULL != usbDeviceService );
                
                checkSecurity = property->dataU.property->checkSecurity;
                
                numberOfInterfaces = property->dataU.usbDeviceProperty->numberOfInterfaces;
                
                break;
            }// end for
            
        }// end if( childrenProps )
        
    }// end of the lock
    dldController->UnLockShared();
    
    
    if( !usbDeviceService ){
        
        //
        // we can't continue without knowledge about the device
        //
        assert( !disable );
        goto __exit;
    }
    
    //
    // make a second check if the device supports multiple interfaces
    // as one of the interface might the one that should not be checked
    // and this one issued the request
    //
    if( ( numberOfInterfaces > 0x1 ) && checkSecurity ){
        
        assert( usbDeviceService );
        
        dldUsbDevice = DldIOService::RetrieveDldIOServiceForIOService( usbDeviceService );
        assert( dldUsbDevice );
        
        if( dldUsbDevice ){
            
            dldUsbDevice->LockShared();
            {// start of the lock
                
                const OSArray* childrenProps;
                
                //
                // interfaces are among the children
                //
                childrenProps = dldUsbDevice->getChildProperties();
                assert( childrenProps );
                if( childrenProps ){
                    
                    int count;
                    
                    count = childrenProps->getCount();
                    for( int i = 0x0; i < count; ++i ){
                        
                        const OSObject*           object;
                        DldObjectPropertyEntry*   property;
                        
                        object = childrenProps->getObject(i);
                        assert( OSDynamicCast( DldObjectPropertyEntry, object ) );
                        
                        property = (DldObjectPropertyEntry*)object;
                        if( DldObjectPopertyType_UsbInterface != property->dataU.property->typeDsc.type )
                            continue;
                        
                        property->dataU.usbInterfaceProperty->LockShared();
                        {// start of the lock
                            
                            int                        endpointsCount;
                            IOUSBController::Endpoint* endpoints;
                            
                            endpointsCount = property->dataU.usbInterfaceProperty->endpointsArrayValidEntriesCount;
                            endpoints = property->dataU.usbInterfaceProperty->endpoints;
                            
                            for( int i = 0x0; i < endpointsCount; ++i ){
                                
                                //
                                // Each endpoint has a unique address within a configuration,
                                // the endpoint's number plus its direction, for more information
                                // see http://www.freebsd.org/doc/en/books/arch-handbook/usb-dev.html
                                //
                                if( endpoints[ i ].number != endpoint->number ||
                                    endpoints[ i ].direction != endpoint->direction ||
                                    endpoints[ i ].transferType != endpoint->transferType )
                                    continue;
                                
                                #if defined( DBG )
                                // __asm__ volatile( "int $0x3" );
                                #endif
                                //
                                // the interface descriptor for the endpoint is found
                                //
                                checkSecurity = property->dataU.property->checkSecurity;
                                break;
                            }// end for
                            
                        }// end of the lock
                        property->dataU.usbInterfaceProperty->UnLockShared();
                        
                    }// end for
                    
                }// end if( childrenProps )
                
            }// end of the lock
            dldUsbDevice->UnLockShared();
            
        }//end if( dldUsbDevice )
            
    }// end if( numberOfInterfaces > 0x1 )
    
    assert( usbDeviceService );
    
    if( !checkSecurity ){
        
        assert( !disable );
        goto __exit;
    }
    
    deviceType.combined   = 0x0;
    deviceType.type.major = DLD_DEVICE_TYPE_USB;
    
    bzero( &param, sizeof( param ) );
    
    param.userSelectionFlavor = kActiveUserSelectionFlavor;
    param.aclType             = kDldAclTypeSecurity;
    param.checkParentType     = true;
    param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
    param.credential          = NULL;
    param.service             = usbDeviceService;// optional as dldIOService is not NULL
    param.dldIOService        = dldUsbDevice;
    param.deviceType          = deviceType;// optional as dldIOService is not NULL
#if defined(LOG_ACCESS)
    param.sourceFunction      = __PRETTY_FUNCTION__;
    param.sourceFile          = __FILE__;
    param.sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
    
    /*if( dldIOService->getObjectProperty() &&
     DLD_DEVICE_TYPE_REMOVABLE == dldIOService->getObjectProperty()->dataU.property->deviceType.type.major ){}*/
    
    ::DldAcquireResources( &param );
    ::isAccessAllowed( &param );
    //
    // DldReleaseResources will be called later as the user context is required for logging
    //
    
    disable = ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                param.output.access.result[ DldMajorTypeFlavor ].disable || 
                param.output.access.result[ DldParentTypeFlavor ].disable );
    
    //
    // USB is the lowest bus for the security check
    //
    assert( !param.output.access.result[ DldParentTypeFlavor ].disable );
    
    //
    // log the action
    //
    if( param.output.access.result[ DldFullTypeFlavor ].log ||
        param.output.access.result[ DldMajorTypeFlavor ].log ){

        assert( 0x0 != param.dldRequestedAccess.winRequestedAccess );
        assert( param.decisionKauthCredEntry && param.decisionKauthCredEntry->getCred() );
        
        if( NULL == dldUsbDevice )
            dldUsbDevice = DldIOService::RetrieveDldIOServiceForIOService( usbDeviceService );
        assert( dldUsbDevice );
        
        if( dldUsbDevice ){
            
            kauth_cred_t   credential;
            
            credential = (param.decisionKauthCredEntry)?param.decisionKauthCredEntry->getCredWithRef(): kauth_cred_get_with_ref();
            assert( credential );
            
            if( credential ){
                
                DldDriverDataLogInt intData;
                DldDriverDataLog    data;
                bool                logDataValid;
                
                intData.logDataSize = sizeof( data );
                intData.logData = &data;
                
                logDataValid = dldUsbDevice->initDeviceLogData( &param.dldRequestedAccess,
                                                                DldFileOperationUnknown,
                                                                proc_pid( current_proc() ),
                                                                credential,
                                                                ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                                                                  param.output.access.result[ DldMajorTypeFlavor ].disable ),
                                                                &intData );
                
                assert( logDataValid );
                if( logDataValid ){
                    
                    gLog->logData( &intData );
                    
                } else {
                    
                    DBG_PRINT_ERROR(("USB device log data is invalid\n"));
                }
                
                kauth_cred_unref( &credential );
                
            } else {
                
                DBG_PRINT_ERROR(("unable to retrieve credentials\n"));
                
            }// end  for else if( credential )
            
        } // end if( dldUsbDevice )
        
    }// end if( param.output.access.result[ DldFullTypeFlavor ].log ||
    
    ::DldReleaseResources( &param );
    
__exit:
    
    if( dldUsbDevice )
        dldUsbDevice->release();
    
    if( dldController )
        dldController->release();
    
#if defined( DBG )
    /*if( disable ){
        __asm__ volatile( "int $0x3" );
    }*/
#endif
    
    return (!disable);
}

//--------------------------------------------------------------------

//
// IOUSBControllerV3 method
//
IOReturn DldIOUSBControllerV3::Read(
    IOUSBControllerV3*  usbController,
    IOMemoryDescriptor* buffer,
    USBDeviceAddress    address,
    IOUSBController::Endpoint* endpoint,
    IOUSBCompletion*    completion,
    UInt32              noDataTimeout,
    UInt32              completionTimeout,
    IOByteCount         reqCount
    )
{
    if( !DldUsbAllowAccess( usbController, address, endpoint ) ){
        return DldUsbError;
    }
    
    return kIOReturnSuccess;
}
//kUSBControl
//--------------------------------------------------------------------

//
// IOUSBControllerV3 method
//
IOReturn DldIOUSBControllerV3::Write(
    IOUSBControllerV3*   usbController,
    IOMemoryDescriptor*  buffer,
    USBDeviceAddress     address,
    IOUSBController::Endpoint* endpoint,
    IOUSBCompletion*     completion,
    UInt32               noDataTimeout,
    UInt32               completionTimeout,
    IOByteCount          reqCount
    )
{
    if( !DldUsbAllowAccess( usbController, address, endpoint ) ){
        return DldUsbError;
    }
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

//
// IOUSBControllerV3 method
//
IOReturn DldIOUSBControllerV3::IsocIO(
    IOUSBControllerV3*   usbController,
    IOMemoryDescriptor*  buffer,
    UInt64               frameStart,
    UInt32               numFrames,
    IOUSBIsocFrame*      frameList,
    USBDeviceAddress     address,
    IOUSBController::Endpoint* endpoint,
    IOUSBIsocCompletion* completion
    )
{
    if( !DldUsbAllowAccess( usbController, address, endpoint ) ){
        return DldUsbError;
    }
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

//
// IOUSBControllerV3 method
//
IOReturn DldIOUSBControllerV3::IsocIO(
    IOUSBControllerV3*   usbController,
    IOMemoryDescriptor*  buffer,
    UInt64               frameStart,
    UInt32               numFrames,
    IOUSBLowLatencyIsocFrame* frameList,
    USBDeviceAddress     address,
    IOUSBController::Endpoint* endpoint,
    IOUSBLowLatencyIsocCompletion *completion,
    UInt32               updateFrequency
    )
{
    if( !DldUsbAllowAccess( usbController, address, endpoint ) ){
        return DldUsbError;
    }
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

//
// IOUSBControllerV2 methods
//
IOReturn DldIOUSBControllerV3::ReadV2(
    IOUSBControllerV3*   usbController,
    IOMemoryDescriptor*  buffer,
    USBDeviceAddress	 address,
    IOUSBController::Endpoint* endpoint,
    IOUSBCompletionWithTimeStamp *completion,
    UInt32               noDataTimeout,
    UInt32	             completionTimeout,
    IOByteCount          reqCount
    )
{
    if( !DldUsbAllowAccess( usbController, address, endpoint ) ){
        return DldUsbError;
    }
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------
