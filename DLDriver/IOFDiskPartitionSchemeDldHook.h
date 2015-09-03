/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOFDISKPARTITIONSCHEMEDLDHOOK_H
#define _IOFDISKPARTITIONSCHEMEDLDHOOK_H

#include <IOKit/storage/IOFDiskPartitionScheme.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0

class IOFDiskPartitionSchemeDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOFDiskPartitionSchemeDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOFDiskPartitionSchemeDldHook, OSObject, IOFDiskPartitionScheme )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOFDiskPartitionSchemeDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOFDiskPartitionSchemeDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOFDiskPartitionSchemeDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOFDiskPartitionSchemeDldHook, IOFDiskPartitionScheme )
    DldDeclarePureVirtualHelperClassEnd( IOFDiskPartitionSchemeDldHook, IOFDiskPartitionScheme )
    
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

#endif//_IOFDISKPARTITIONSCHEMEDLDHOOK_H
