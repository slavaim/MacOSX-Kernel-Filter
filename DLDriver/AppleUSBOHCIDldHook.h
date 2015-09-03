/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef APPLEUSBOHCIDLDHOOK_H
#define APPLEUSBOHCIDLDHOOK_H

#include "DldIOUSBControllerV3.h"

//--------------------------------------------------------------------

#if 0

//
// a hooke for the AppleUSBOHCI is defined throught its parent IOUSBControllerV3
//as the AppleUSBEHCI class is a proprietor Apple class
//
class AppleUSBOHCIDldHook: public DldIOUSBControllerV3{
    
    OSDeclareDefaultStructors( AppleUSBOHCIDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( AppleUSBOHCIDldHook, DldIOUSBControllerV3, IOUSBControllerV3 )
    
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( AppleUSBOHCIDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( AppleUSBOHCIDldHook )
        DldAddVirtualFunctionInEnumDeclaration( AppleUSBOHCIDldHook, Read )
        DldAddVirtualFunctionInEnumDeclaration( AppleUSBOHCIDldHook, Write )
        DldAddVirtualFunctionInEnumDeclaration( AppleUSBOHCIDldHook, IsocIO1 )
        DldAddVirtualFunctionInEnumDeclaration( AppleUSBOHCIDldHook, IsocIO2 )
        DldAddVirtualFunctionInEnumDeclaration( AppleUSBOHCIDldHook, ReadV2 )
    DldVirtualFunctionsEnumDeclarationEnd( AppleUSBOHCIDldHook )
    
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    
    //
    // so we will use a parent class as the leaf class is a private Apple class
    //
    DldDeclarePureVirtualHelperClassStart( AppleUSBOHCIDldHook, IOUSBControllerV3 )
    
protected:
        virtual IOReturn 		Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount);
        virtual IOReturn 		Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount);
        virtual IOReturn		IsocIO(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBIsocCompletion *completion );
        virtual IOReturn		IsocIO(IOMemoryDescriptor *buffer, UInt64 frameStart, UInt32 numFrames, IOUSBLowLatencyIsocFrame *frameList, USBDeviceAddress address, IOUSBController::Endpoint *endpoint, IOUSBLowLatencyIsocCompletion *completion, UInt32 updateFrequency );	
    
        // IOUSBControllerV2 methods
        // we override these to deal with methods attempting to go through the workloop while we are in sleep
        virtual IOReturn		ReadV2(IOMemoryDescriptor *buffer, USBDeviceAddress	address, IOUSBController::Endpoint *endpoint, IOUSBCompletionWithTimeStamp *completion, UInt32 noDataTimeout, UInt32	completionTimeout, IOByteCount reqCount);
    
    DldDeclarePureVirtualHelperClassEnd( AppleUSBOHCIDldHook, IOUSBMassStorageClass )
    
    
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

#endif//0
//--------------------------------------------------------------------


#endif//APPLEUSBOHCIDLDHOOK_H


