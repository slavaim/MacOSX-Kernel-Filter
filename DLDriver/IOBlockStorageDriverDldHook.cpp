/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "IOBlockStorageDriverDldHook.h"

//--------------------------------------------------------------------

#if 0

#define super OSObject

OSDefineMetaClassAndStructors( IOBlockStorageDriverDldHook, OSObject )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
DldDefineCommonIOServiceHook_HookObjectInt( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
//DldDefineCommonIOServiceHook_InitMembers( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )

//--------------------------------------------------------------------

bool
IOBlockStorageDriverDldHook::InitMembers()
{
    DldInitMembers_Enter( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
    
    DldInitMembers_AddCommonHookedVtableFunctionsInfo( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
    
    DldInitMembers_AddFunctionInfoForHookedClass( IOBlockStorageDriverDldHook,
                                                  read,
                                                  read_hook,
                                                  OSObject,
                                                  IOBlockStorageDriver )
    
    DldInitMembers_AddFunctionInfoForHookedClass( IOBlockStorageDriverDldHook,
                                                  write,
                                                  write_hook,
                                                  OSObject,
                                                  IOBlockStorageDriver )
    
    DldInitMembers_Exit( IOBlockStorageDriverDldHook, OSObject, IOBlockStorageDriver )
    
    return true;
}

//--------------------------------------------------------------------

void
IOBlockStorageDriverDldHook::read_hook(
    IOService *           client,
    UInt64                byteStart,
    IOMemoryDescriptor *  buffer,
    IOStorageAttributes * attributes,
    IOStorageCompletion * completion )
{
    
    IOBlockStorageDriverDldHook* HookObject = IOBlockStorageDriverDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return;

    typedef void (*readFunc)( IOBlockStorageDriver*  __this,
                              IOService *           client,
                              UInt64                byteStart,
                              IOMemoryDescriptor *  buffer,
                              IOStorageAttributes * attributes,
                              IOStorageCompletion * completion );
    
    int indx = DldVirtualFunctionEnumValue( IOBlockStorageDriverDldHook, read );
    readFunc  Original = (readFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );

    assert( Original );
    
    return Original( reinterpret_cast<IOBlockStorageDriver*>(this), client, byteStart, buffer, attributes, completion );
    
}

//--------------------------------------------------------------------

void
IOBlockStorageDriverDldHook::write_hook(
    IOService *           client,
    UInt64                byteStart,
    IOMemoryDescriptor *  buffer,
    IOStorageAttributes * attributes,
    IOStorageCompletion * completion )
{
    
    IOBlockStorageDriverDldHook* HookObject = IOBlockStorageDriverDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return;
    
    typedef void (*writeFunc)( IOBlockStorageDriver*  __this,
                               IOService *           client,
                               UInt64                byteStart,
                               IOMemoryDescriptor *  buffer,
                               IOStorageAttributes * attributes,
                               IOStorageCompletion * completion );
    
    int indx = DldVirtualFunctionEnumValue( IOBlockStorageDriverDldHook, write );
    writeFunc  Original = (writeFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );
    
    assert( Original );
    
    return Original( reinterpret_cast<IOBlockStorageDriver*>(this), client, byteStart, buffer, attributes, completion );
    
}

#endif//0
//--------------------------------------------------------------------


