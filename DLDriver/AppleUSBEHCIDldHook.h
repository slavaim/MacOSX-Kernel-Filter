/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef APPLEUSBEHCIDLDHOOK_H
#define APPLEUSBEHCIDLDHOOK_H

#include "DldHookerCommonClass2.h"
#include "DldIOUSBControllerV3.h"

//--------------------------------------------------------------------

typedef enum _DldUsbHciType{
    kDldUsbHciTypeUnknown = 0x0,
    kDldUsbHciTypeEHCI,
    kDldUsbHciTypeOHCI,
    kDldUsbHciTypeUHCI,
    kDldUsbHciTypeXHCI, // Intel USB 3
}DldUsbHciType;

//AppleUSBEHCIDldHook

//
// a hooke for the AppleUSBEHCI( or OHCI ) is defined throught its parent IOUSBControllerV3
// as the AppleUSBEHCI( or OHCI ) class is a proprietor Apple class
//
template<DldInheritanceDepth Depth, DldUsbHciType Type>
class AppleUSBHCIDldHook: public DldIOUSBControllerV3{
  
    
    /////////////////////////////////////////////
    //
    // start of the required declarations
    //
    /////////////////////////////////////////////
    
public:
    enum{
        kDld_Read_hook = 0x0,
        kDld_Write_hook,
        kDld_IsocIO1_hook,
        kDld_IsocIO2_hook,
        kDld_ReadV2_hook,
        kDld_NumberOfAddedHooks
    };
    
    friend class DldIOKitHookEngine;
    friend class DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>, IOUSBControllerV3>;
    
    //
    // fGetHookedClassName defines the actual class which is being hooked
    //
    static const char* fGetHookedClassName(){
        
        assert( kDldUsbHciTypeEHCI == Type ||
                kDldUsbHciTypeOHCI == Type ||
                kDldUsbHciTypeUHCI == Type ||
                kDldUsbHciTypeXHCI == Type );
        
        switch( Type ){
            case kDldUsbHciTypeEHCI:
                return "AppleUSBEHCI";
            case kDldUsbHciTypeOHCI:
                return "AppleUSBOHCI";
            case kDldUsbHciTypeUHCI:
                return "AppleUSBUHCI";
            case kDldUsbHciTypeXHCI:
                return "AppleUSBXHCI";
            default:
                return "AppleUSBUnknownHCI";
        }
    };
    
    DldDeclareGetClassNameFunction();
    
protected:
    
    static DldInheritanceDepth fGetInheritanceDepth(){ return Depth; };
    DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>, IOUSBControllerV3>*   mHookerCommon2;
    
protected:
    static AppleUSBHCIDldHook<Depth,Type>* newWithDefaultSettings();
    virtual bool init();
    virtual void free();
    
    /////////////////////////////////////////////
    //
    // end of the required declarations
    //
    /////////////////////////////////////////////
    
    
    ////////////////////////////////////////////////////////
    //
    // hooking functions declaration
    //
    /////////////////////////////////////////////////////////
    
protected:
    virtual IOReturn 		Read_hook(IOMemoryDescriptor *buffer, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount);
    virtual IOReturn 		Write_hook(IOMemoryDescriptor *buffer, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount);
    virtual IOReturn		IsocIO1_hook(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBIsocCompletion *completion );
    virtual IOReturn		IsocIO2_hook(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBLowLatencyIsocCompletion *completion, UInt32 updateFrequency );	
    
    // IOUSBControllerV2 methods
    // we override these to deal with methods attempting to go through the workloop while we are in sleep
    virtual IOReturn		ReadV2_hook(IOMemoryDescriptor *buffer, USBDeviceAddress	address, IOUSBController::Endpoint *endpoint, IOUSBCompletionWithTimeStamp *completion, UInt32 noDataTimeout, UInt32	completionTimeout, IOByteCount reqCount);
    
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
};

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
AppleUSBHCIDldHook<Depth,Type>*
AppleUSBHCIDldHook<Depth,Type>::newWithDefaultSettings()
{
    AppleUSBHCIDldHook<Depth,Type>*  newObject;
    
    newObject = new AppleUSBHCIDldHook<Depth,Type>();
    if( !newObject )
        return NULL;
    
    newObject->mHookerCommon2 = new DldHookerCommonClass2< AppleUSBHCIDldHook<Depth,Type> ,IOUSBControllerV3 >;
    assert( newObject->mHookerCommon2 );
    if( !newObject->mHookerCommon2 ){
        
        delete newObject;
        return NULL;
    }
    
    if( !newObject->init() ){
        
        assert( !"newObject->init() failed" );
        DBG_PRINT_ERROR(("newObject->init() failed"));
        
        newObject->free();
        delete newObject->mHookerCommon2;
        delete newObject;
        
        return NULL;
    }
    
    return newObject;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
bool
AppleUSBHCIDldHook<Depth,Type>::init()
{
    // super::init()
    
    assert( this->mHookerCommon2 );
    
    //
    // a virtual class is needed to get the indices as the required
    // function are protected and the compiler doesn't resolve any reference to it
    // from non-friend or non-derived classes
    //
    class PureVirtualClass: public IOUSBControllerV3{
        
        friend class AppleUSBHCIDldHook<Depth,Type>;
        
    protected:
        virtual IOReturn 		Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount);
        virtual IOReturn 		Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount);
        virtual IOReturn		IsocIO(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBIsocCompletion *completion );
        virtual IOReturn		IsocIO(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBLowLatencyIsocCompletion *completion, UInt32 updateFrequency );	
        
        // IOUSBControllerV2 methods
        // we override these to deal with methods attempting to go through the workloop while we are in sleep
        virtual IOReturn		ReadV2(IOMemoryDescriptor *buffer, USBDeviceAddress	address, IOUSBController::Endpoint *endpoint, IOUSBCompletionWithTimeStamp *completion, UInt32 noDataTimeout, UInt32	completionTimeout, IOByteCount reqCount);
        
        virtual void PureVirtual()=0;
    };
    
    if( this->mHookerCommon2->init( this ) ){
        
        //
        // add new hooking functions specific for the class
        //
        this->mHookerCommon2->fAddHookingFunctionExternal(
           AppleUSBHCIDldHook<Depth, Type>::kDld_Read_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &PureVirtualClass::Read ),
           DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::_ptmf2ptf( 
             this,
             (void (DldHookerBaseInterface::*)(void)) &AppleUSBHCIDldHook<Depth,Type>::Read_hook ) );
        
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
           AppleUSBHCIDldHook<Depth, Type>::kDld_Write_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &PureVirtualClass::Write ),
           DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::_ptmf2ptf( 
             this,
             (void (DldHookerBaseInterface::*)(void)) &AppleUSBHCIDldHook<Depth,Type>::Write_hook ) );
        
        typedef IOReturn(PureVirtualClass::* IsocIO1Func)(
                                       IOMemoryDescriptor *buffer, UInt64 frameStart,
                                       UInt32 numFrames, IOUSBIsocFrame *frameList,
                                       USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                       IOUSBIsocCompletion *completion );
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
           AppleUSBHCIDldHook<Depth, Type>::kDld_IsocIO1_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) (IsocIO1Func)&PureVirtualClass::IsocIO ),
           DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::_ptmf2ptf( 
              this,
              (void (DldHookerBaseInterface::*)(void)) &AppleUSBHCIDldHook<Depth,Type>::IsocIO1_hook ) );
        
        typedef IOReturn(PureVirtualClass::* IsocIO2Func)(
                                       IOMemoryDescriptor *buffer, UInt64 frameStart,
                                       UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList,
                                       USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                       IOUSBLowLatencyIsocCompletion *completion,
                                       UInt32 updateFrequency );
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
            AppleUSBHCIDldHook<Depth, Type>::kDld_IsocIO2_hook,
            DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) (IsocIO2Func)&PureVirtualClass::IsocIO ),
            DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::_ptmf2ptf( 
               this,
               (void (DldHookerBaseInterface::*)(void)) &AppleUSBHCIDldHook<Depth,Type>::IsocIO2_hook ) );
        
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
            AppleUSBHCIDldHook<Depth, Type>::kDld_ReadV2_hook,
            DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &PureVirtualClass::ReadV2 ),
            DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::_ptmf2ptf( 
                this,
                (void (DldHookerBaseInterface::*)(void)) &AppleUSBHCIDldHook<Depth,Type>::ReadV2_hook ) );
        
    } else {
        
        DBG_PRINT_ERROR(("this->mHookerCommon2.init( this ) failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
void
AppleUSBHCIDldHook<Depth,Type>::free()
{
    if( this->mHookerCommon2 )
        this->mHookerCommon2->free();
    //super::free();
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
IOReturn
AppleUSBHCIDldHook<Depth,Type>::Read_hook(
                               IOMemoryDescriptor *buffer,
                               USBDeviceAddress address,
                               IOUSBController::Endpoint *endpoint,
                               IOUSBCompletion *completion,
                               UInt32 noDataTimeout,
                               UInt32 completionTimeout,
                               IOByteCount reqCount)
/*
 this is a hook, so "this" is an object of the AppleUSBEHCIDldHook or AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{

    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 ){
        
        DBG_PRINT_ERROR(("fCommonHooker2() failed\n"));
        return kIOReturnError;
    }
    
    AppleUSBHCIDldHook<Depth,Type>*  hookingObject = commonHooker2->fGetContainingClassObject();
    assert( hookingObject );
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = hookingObject->Read( controller, buffer, address, endpoint, completion,
                              noDataTimeout, completionTimeout, reqCount);
    if( kIOReturnSuccess != RC )
        return RC;
    
    typedef IOReturn(*ReadFunc)( IOUSBControllerV3*  __this,
                                IOMemoryDescriptor *buffer,
                                USBDeviceAddress address,
                                IOUSBController::Endpoint *endpoint,
                                IOUSBCompletion *completion,
                                UInt32 noDataTimeout,
                                UInt32 completionTimeout,
                                IOByteCount reqCount);
    
    ReadFunc  OrigRead = (ReadFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        AppleUSBHCIDldHook<Depth,Type>::kDld_Read_hook );
    
    assert( OrigRead );
    
    return OrigRead( reinterpret_cast<IOUSBControllerV3*>(this), buffer, address, endpoint, completion,
                    noDataTimeout, completionTimeout, reqCount);
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
IOReturn
AppleUSBHCIDldHook<Depth,Type>::Write_hook(
                                           IOMemoryDescriptor *buffer,
                                           USBDeviceAddress address,
                                           IOUSBController::Endpoint *endpoint,
                                           IOUSBCompletion *completion,
                                           UInt32 noDataTimeout,
                                           UInt32 completionTimeout,
                                           IOByteCount reqCount)
/*
 this is a hook, so "this" is an object of the AppleUSBEHCIDldHook or AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 ){
        
        DBG_PRINT_ERROR(("fCommonHooker2() failed\n"));
        return kIOReturnError;
    }
    
    AppleUSBHCIDldHook<Depth,Type>*  hookingObject = commonHooker2->fGetContainingClassObject();
    assert( hookingObject );
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = hookingObject->Write( controller, buffer, address, endpoint, completion,
                              noDataTimeout, completionTimeout, reqCount);
    if( kIOReturnSuccess != RC )
        return RC;
    
    typedef IOReturn(*WriteFunc)( IOUSBControllerV3*  __this,
                                 IOMemoryDescriptor *buffer,
                                 USBDeviceAddress address,
                                 IOUSBController::Endpoint *endpoint,
                                 IOUSBCompletion *completion,
                                 UInt32 noDataTimeout,
                                 UInt32 completionTimeout,
                                 IOByteCount reqCount);
    
    WriteFunc  OrigWrite = (WriteFunc)commonHooker2->fGetOriginalFunctionExternal(
         (OSObject*)this,
         AppleUSBHCIDldHook<Depth,Type>::kDld_Write_hook );
    
    assert( OrigWrite );
    
    return OrigWrite( reinterpret_cast<IOUSBControllerV3*>(this), buffer, address, endpoint, completion,
                     noDataTimeout, completionTimeout, reqCount);
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
IOReturn
AppleUSBHCIDldHook<Depth,Type>::IsocIO1_hook(IOMemoryDescriptor *buffer, UInt64 frameStart,
                                             UInt32 numFrames, IOUSBIsocFrame *frameList,
                                             USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                             IOUSBIsocCompletion *completion )
/*
 this is a hook, so "this" is an object of the AppleUSBEHCIDldHook or AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 ){
        
        DBG_PRINT_ERROR(("fCommonHooker2() failed\n"));
        return kIOReturnError;
    }
    
    AppleUSBHCIDldHook<Depth,Type>*  hookingObject = commonHooker2->fGetContainingClassObject();
    assert( hookingObject );
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = hookingObject->IsocIO( controller, buffer, frameStart, numFrames, frameList,
                                address, endpoint, completion);
    if( kIOReturnSuccess != RC )
        return RC;
    
    typedef IOReturn(*IsocIO1Func)(IOUSBControllerV3*  __this,
                                   IOMemoryDescriptor *buffer, UInt64 frameStart,
                                   UInt32 numFrames, IOUSBIsocFrame *frameList,
                                   USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                   IOUSBIsocCompletion *completion );
    
    IsocIO1Func  OrigIsocIO1 = (IsocIO1Func)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        AppleUSBHCIDldHook<Depth,Type>::kDld_IsocIO1_hook );
    
    assert( OrigIsocIO1 );
    
    return OrigIsocIO1( reinterpret_cast<IOUSBControllerV3*>(this), buffer, frameStart, numFrames, frameList,
                        address, endpoint, completion);
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
IOReturn
AppleUSBHCIDldHook<Depth,Type>::IsocIO2_hook(IOMemoryDescriptor *buffer, UInt64 frameStart,
                                             UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList,
                                             USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                             IOUSBLowLatencyIsocCompletion *completion,
                                             UInt32 updateFrequency )
/*
 this is a hook, so "this" is an object of the AppleUSBEHCIDldHook or AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 ){
        
        DBG_PRINT_ERROR(("fCommonHooker2() failed\n"));
        return kIOReturnError;
    }
    
    AppleUSBHCIDldHook<Depth,Type>*  hookingObject = commonHooker2->fGetContainingClassObject();
    assert( hookingObject );
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = hookingObject->IsocIO( controller, buffer, frameStart, numFrames, frameList,
                                address, endpoint, completion, updateFrequency);
    if( kIOReturnSuccess != RC )
        return RC;
    
    typedef IOReturn(*IsocIO2Func)(IOUSBControllerV3*  __this,
                                   IOMemoryDescriptor *buffer, UInt64 frameStart,
                                   UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList,
                                   USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                   IOUSBLowLatencyIsocCompletion *completion,
                                   UInt32 updateFrequency );
    
    IsocIO2Func  OrigIsocIO2 = (IsocIO2Func)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        AppleUSBHCIDldHook<Depth,Type>::kDld_IsocIO2_hook );
    
    assert( OrigIsocIO2 );
    
    return OrigIsocIO2( reinterpret_cast<IOUSBControllerV3*>(this), buffer, frameStart, numFrames, frameList,
                        address, endpoint, completion, updateFrequency);
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth, DldUsbHciType Type>
IOReturn
AppleUSBHCIDldHook<Depth,Type>::ReadV2_hook(
                                            IOMemoryDescriptor *buffer,
                                            USBDeviceAddress address,
                                            IOUSBController::Endpoint *endpoint,
                                            IOUSBCompletionWithTimeStamp *completion,
                                            UInt32 noDataTimeout,
                                            UInt32 completionTimeout,
                                            IOByteCount reqCount )
/*
 this is a hook, so "this" is an object of the AppleUSBEHCIDldHook or AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<AppleUSBHCIDldHook<Depth,Type>,IOUSBControllerV3>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 ){
        
        DBG_PRINT_ERROR(("fCommonHooker2() failed\n"));
        return kIOReturnError;
    }
    
    AppleUSBHCIDldHook<Depth,Type>*  hookingObject = commonHooker2->fGetContainingClassObject();
    assert( hookingObject );
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = hookingObject->ReadV2( controller, buffer, address, endpoint, completion,
                                noDataTimeout, completionTimeout, reqCount);
    if( kIOReturnSuccess != RC )
        return RC;
    
    typedef IOReturn(*ReadV2Func)( IOUSBControllerV3*  __this,
                                  IOMemoryDescriptor *buffer,
                                  USBDeviceAddress address,
                                  IOUSBController::Endpoint *endpoint,
                                  IOUSBCompletionWithTimeStamp *completion,
                                  UInt32 noDataTimeout,
                                  UInt32 completionTimeout,
                                  IOByteCount reqCount);
    
    ReadV2Func  OrigReadV2 = (ReadV2Func)commonHooker2->fGetOriginalFunctionExternal(
       (OSObject*)this,
       AppleUSBHCIDldHook<Depth,Type>::kDld_ReadV2_hook );
    
    assert( OrigReadV2 );
    
    return OrigReadV2( reinterpret_cast<IOUSBControllerV3*>(this), buffer, address, endpoint, completion,
                       noDataTimeout, completionTimeout, reqCount);
}

//--------------------------------------------------------------------


#endif//APPLEUSBEHCIDLDHOOK_H

