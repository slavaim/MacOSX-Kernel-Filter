/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOUSBDEVICEDLDHOOK_H
#define _IOUSBDEVICEDLDHOOK_H

#include <IOKit/usb/IOUSBDevice.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0
class IOUSBDeviceDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOUSBDeviceDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOUSBDeviceDldHook, OSObject, IOUSBDevice )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOUSBDeviceDldHook )
       DldAddCommonVirtualFunctionsEnumDeclaration( IOUSBDeviceDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOUSBDeviceDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOUSBDeviceDldHook, IOUSBDevice )
    DldDeclarePureVirtualHelperClassEnd( IOUSBDeviceDldHook, IOUSBDevice )
    
public:
    
    ////////////////////////////////////////////////////////
    //
    // hooking function declarations
    //
    /////////////////////////////////////////////////////////
    
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
};
#endif//0

#endif//_IOUSBDEVICEDLDHOOK_H