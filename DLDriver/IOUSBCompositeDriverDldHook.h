/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOUSBCOMPOSITEDRIVERDLDHOOK_H
#define _IOUSBCOMPOSITEDRIVERDLDHOOK_H

#include <IOKit/usb/IOUSBCompositeDriver.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0

class IOUSBCompositeDriverDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOUSBCompositeDriverDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOUSBCompositeDriverDldHook, OSObject, IOUSBCompositeDriver )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOUSBCompositeDriverDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOUSBCompositeDriverDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOUSBCompositeDriverDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOUSBCompositeDriverDldHook, IOUSBCompositeDriver )
    DldDeclarePureVirtualHelperClassEnd( IOUSBCompositeDriverDldHook, IOUSBCompositeDriver )
    
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

#endif//_IOUSBCOMPOSITEDRIVERDLDHOOK_H