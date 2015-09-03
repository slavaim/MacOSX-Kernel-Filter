/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOSCSIPERIPHERALDEVICENUB_H
#define _IOSCSIPERIPHERALDEVICENUB_H

#include <IOKit/scsi/IOSCSIPeripheralDeviceNub.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

#if 0
//--------------------------------------------------------------------

class IOSCSIPeripheralDeviceNubDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOSCSIPeripheralDeviceNubDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOSCSIPeripheralDeviceNubDldHook, OSObject, IOSCSIPeripheralDeviceNub )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOSCSIPeripheralDeviceNubDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOSCSIPeripheralDeviceNubDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOSCSIPeripheralDeviceNubDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOSCSIPeripheralDeviceNubDldHook, IOSCSIPeripheralDeviceNub )
    DldDeclarePureVirtualHelperClassEnd( IOSCSIPeripheralDeviceNubDldHook, IOSCSIPeripheralDeviceNub )
    
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

#endif//_IOSCSIPERIPHERALDEVICENUB_H