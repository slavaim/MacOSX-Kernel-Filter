/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOMEDIADLDHOOK_H
#define _IOMEDIADLDHOOK_H

#include <IOKit/storage/IOMedia.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0

class IOMediaDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOMediaDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOMediaDldHook, OSObject, IOMedia )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOMediaDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOMediaDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOMediaDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOMediaDldHook, IOMedia )
    DldDeclarePureVirtualHelperClassEnd( IOMediaDldHook, IOMedia )
    
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

#endif//_IOMEDIADLDHOOK_H