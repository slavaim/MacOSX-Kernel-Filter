/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */


#include "AppleUSBOHCIDldHook.h"

//--------------------------------------------------------------------

#if 0

#define super DldIOUSBControllerV3

OSDefineMetaClassAndStructors( AppleUSBOHCIDldHook, DldIOUSBControllerV3 )
DldDefineCommonIOServiceHookFunctionsAndStructors( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )
DldDefineCommonIOServiceHook_HookObjectInt( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )
DldDefineCommonIOServiceHook_UnHookObjectInt( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )

//--------------------------------------------------------------------

IOReturn
AppleUSBOHCIDldHook::Read_hook(
    IOMemoryDescriptor *buffer,
    USBDeviceAddress address,
    IOUSBController::Endpoint *endpoint,
    IOUSBCompletion *completion,
    UInt32 noDataTimeout,
    UInt32 completionTimeout,
    IOByteCount reqCount)
/*
 this is a hook, so "this" is an object of the AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    AppleUSBOHCIDldHook* HookObject = AppleUSBOHCIDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return kIOReturnError;
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = HookObject->Read( controller, buffer, address, endpoint, completion,
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
    
    int indx = DldVirtualFunctionEnumValue( AppleUSBOHCIDldHook, Read );
    
    assert( NULL != HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction );
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    ReadFunc  OrigRead = (ReadFunc)HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction;
    return OrigRead( reinterpret_cast<IOUSBControllerV3*>(this), buffer, address, endpoint, completion,
                    noDataTimeout, completionTimeout, reqCount);
}

//--------------------------------------------------------------------

IOReturn
AppleUSBOHCIDldHook::Write_hook(
    IOMemoryDescriptor *buffer,
    USBDeviceAddress address,
    IOUSBController::Endpoint *endpoint,
    IOUSBCompletion *completion,
    UInt32 noDataTimeout,
    UInt32 completionTimeout,
    IOByteCount reqCount)
/*
 this is a hook, so "this" is an object of the AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    AppleUSBOHCIDldHook* HookObject = AppleUSBOHCIDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return kIOReturnError;
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = HookObject->Write( controller, buffer, address, endpoint, completion,
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
    
    int indx = DldVirtualFunctionEnumValue( AppleUSBOHCIDldHook, Write );
    
    assert( NULL != HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction );
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) );
    
    WriteFunc  OrigWrite = (WriteFunc)HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction;
    return OrigWrite( reinterpret_cast<IOUSBControllerV3*>(this), buffer, address, endpoint, completion,
                     noDataTimeout, completionTimeout, reqCount);
}

//--------------------------------------------------------------------

IOReturn
AppleUSBOHCIDldHook::IsocIO1_hook(IOMemoryDescriptor *buffer, UInt64 frameStart,
                                  UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                  IOUSBIsocCompletion *completion )
/*
 this is a hook, so "this" is an object of the AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    AppleUSBOHCIDldHook* HookObject = AppleUSBOHCIDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return kIOReturnError;
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = HookObject->IsocIO( controller, buffer, frameStart, numFrames, frameList,
                       address, endpoint, completion );
    if( kIOReturnSuccess != RC )
        return RC;   
    
    typedef IOReturn(*IsocIO1Func)(IOUSBControllerV3*  __this,
                                   IOMemoryDescriptor *buffer, UInt64 frameStart,
                                   UInt32 numFrames, IOUSBIsocFrame *frameList,
                                   USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                   IOUSBIsocCompletion *completion );
    
    int indx = DldVirtualFunctionEnumValue( AppleUSBOHCIDldHook, IsocIO1 );
    
    assert( NULL != HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction );
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) );
    
    IsocIO1Func  OrigIsocIO1 = (IsocIO1Func)HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction;
    return OrigIsocIO1( reinterpret_cast<IOUSBControllerV3*>(this), buffer, frameStart, numFrames, frameList,
                       address, endpoint, completion);
}

//--------------------------------------------------------------------

IOReturn
AppleUSBOHCIDldHook::IsocIO2_hook(IOMemoryDescriptor *buffer, UInt64 frameStart,
                                  UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList,
                                  USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                  IOUSBLowLatencyIsocCompletion *completion,
                                  UInt32 updateFrequency )
/*
 this is a hook, so "this" is an object of the AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    AppleUSBOHCIDldHook* HookObject = AppleUSBOHCIDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return kIOReturnError;
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = HookObject->IsocIO( controller, buffer, frameStart, numFrames, frameList,
                       address, endpoint, completion, updateFrequency);
    if( kIOReturnSuccess != RC )
        return RC;    
    
    typedef IOReturn(*IsocIO2Func)(IOUSBControllerV3*  __this,
                                   IOMemoryDescriptor *buffer, UInt64 frameStart,
                                   UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList,
                                   USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                   IOUSBLowLatencyIsocCompletion *completion,
                                   UInt32 updateFrequency );
    
    int indx = DldVirtualFunctionEnumValue( AppleUSBOHCIDldHook, IsocIO2 );
    
    assert( NULL != HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction );
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) );
    
    IsocIO2Func  OrigIsocIO2 = (IsocIO2Func)HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction;
    return OrigIsocIO2( reinterpret_cast<IOUSBControllerV3*>(this), buffer, frameStart, numFrames, frameList,
                       address, endpoint, completion, updateFrequency);
}

//--------------------------------------------------------------------

IOReturn
AppleUSBOHCIDldHook::ReadV2_hook(
    IOMemoryDescriptor *buffer,
    USBDeviceAddress address,
    IOUSBController::Endpoint *endpoint,
    IOUSBCompletionWithTimeStamp *completion,
    UInt32 noDataTimeout,
    UInt32 completionTimeout,
    IOByteCount reqCount)
/*
 this is a hook, so "this" is an object of the AppleUSBOHCIDldHook class( IOUSBControllerV3 is a parent )
 */
{
    
    AppleUSBOHCIDldHook* HookObject = AppleUSBOHCIDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return kIOReturnError;
    
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) ); 
    
    IOUSBControllerV3*  controller = (IOUSBControllerV3*)this;
    IOReturn            RC;
    
    RC = HookObject->ReadV2( controller, buffer, address, endpoint, completion,
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
    
    int indx = DldVirtualFunctionEnumValue( AppleUSBOHCIDldHook, ReadV2 );
    
    assert( NULL != HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction );
    assert( ((OSObject*)this)->metaCast( "IOUSBControllerV3" ) );
    
    ReadV2Func  OrigReadV2 = (ReadV2Func)HookObject->HookedVtableFunctionsInfo[ indx ].OriginalFunction;
    return OrigReadV2( reinterpret_cast<IOUSBControllerV3*>(this), buffer, address, endpoint, completion,
                      noDataTimeout, completionTimeout, reqCount);
}

//--------------------------------------------------------------------

bool
AppleUSBOHCIDldHook::InitMembers()
{
    
    DldInitMembers_Enter( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )
    
    DldInitMembers_AddCommonHookedVtableFunctionsInfo( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )
    DldInitMembers_AddFunctionInfoForHookedClass( AppleUSBOHCIDldHook, Read,
                                                 Read_hook, DldIOUSBControllerV3, IOUSBControllerV3 )
    
    DldInitMembers_AddFunctionInfoForHookedClass( AppleUSBOHCIDldHook, Write,
                                                 Write_hook, DldIOUSBControllerV3, IOUSBControllerV3 )
    
    
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( AppleUSBOHCIDldHook, IsocIO, IsocIO1_hook, DldIOUSBControllerV3,
                                                           IOUSBControllerV3, 1, IOReturn, 
                                                           (IOMemoryDescriptor *buffer, UInt64 frameStart,
                                                            UInt32 numFrames, IOUSBIsocFrame *frameList,
                                                            USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                                            IOUSBIsocCompletion *completion ) )
    
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( AppleUSBOHCIDldHook, IsocIO, IsocIO2_hook, DldIOUSBControllerV3,
                                                           IOUSBControllerV3, 2, IOReturn,
                                                           (IOMemoryDescriptor *buffer, UInt64 frameStart,
                                                            UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList,
                                                            USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                                            IOUSBLowLatencyIsocCompletion *completion,
                                                            UInt32 updateFrequency ) )
    
    DldInitMembers_AddFunctionInfoForHookedClass( AppleUSBOHCIDldHook, ReadV2,
                                                 ReadV2_hook, DldIOUSBControllerV3, IOUSBControllerV3 )
    
    DldInitMembers_Exit( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )
    
    return true;
}

#endif//0
//--------------------------------------------------------------------
