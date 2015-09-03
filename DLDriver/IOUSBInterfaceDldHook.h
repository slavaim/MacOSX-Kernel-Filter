/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOUSBINTERFACEDLDHOOK_H
#define _IOUSBINTERFACEDLDHOOK_H

#include <IOKit/usb/IOUSBMassStorageClass.h>
#include <IOKit/assert.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

#if 0
//--------------------------------------------------------------------

class IOUSBInterfaceDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOUSBInterfaceDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOUSBInterfaceDldHook, OSObject, IOUSBInterface )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOUSBInterfaceDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOUSBInterfaceDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOUSBInterfaceDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOUSBInterfaceDldHook, IOUSBInterface )
    DldDeclarePureVirtualHelperClassEnd( IOUSBInterfaceDldHook, IOUSBInterface )
    
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

#endif//_IOUSBINTERFACEDLDHOOK_H