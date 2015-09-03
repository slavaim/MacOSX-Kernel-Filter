/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOBLOCKSTORAGEDRIVERDLDHOOK_H
#define _IOBLOCKSTORAGEDRIVERDLDHOOK_H

#include <IOKit/storage/IOBlockStorageDriver.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0

class IOBlockStorageDriverDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOBlockStorageDriverDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOBlockStorageDriverDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOBlockStorageDriverDldHook )
        DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageDriverDldHook, read )
        DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageDriverDldHook, write )
    DldVirtualFunctionsEnumDeclarationEnd( IOBlockStorageDriverDldHook )
    
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOBlockStorageDriverDldHook, IOBlockStorageDriver )
    
        public:
        virtual void read(IOService *           client,
                          UInt64                byteStart,
                          IOMemoryDescriptor *  buffer,
                          IOStorageAttributes * attributes,
                          IOStorageCompletion * completion);
    
        virtual void write(IOService *           client,
                           UInt64                byteStart,
                           IOMemoryDescriptor *  buffer,
                           IOStorageAttributes * attributes,
                           IOStorageCompletion * completion);
    
    DldDeclarePureVirtualHelperClassEnd( IOBlockStorageDriverDldHook, IOBlockStorageDriver )
    
public:
    
    ////////////////////////////////////////////////////////
    //
    // hooking function declarations
    //
    /////////////////////////////////////////////////////////
    
    virtual void read_hook(IOService *           client,
                           UInt64                byteStart,
                           IOMemoryDescriptor *  buffer,
                           IOStorageAttributes * attributes,
                           IOStorageCompletion * completion);
    
    virtual void write_hook(IOService *           client,
                            UInt64                byteStart,
                            IOMemoryDescriptor *  buffer,
                            IOStorageAttributes * attributes,
                            IOStorageCompletion * completion);
    
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
};

#endif//0

#endif//_IOBLOCKSTORAGEDRIVERDLDHOOK_H