/*
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "IOBlockStorageServicesDldHook.h"

//--------------------------------------------------------------------

#if 0

#define super OSObject

OSDefineMetaClassAndStructors( IOBlockStorageServicesDldHook, OSObject )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOBlockStorageServicesDldHook, OSObject, IOBlockStorageServices )
DldDefineCommonIOServiceHook_HookObjectInt( IOBlockStorageServicesDldHook, OSObject, IOBlockStorageServices )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOBlockStorageServicesDldHook, OSObject, IOBlockStorageServices )
//DldDefineCommonIOServiceHook_InitMembers( IOBlockStorageServicesDldHook, IOService, IOBlockStorageServices )

//--------------------------------------------------------------------

bool
IOBlockStorageServicesDldHook::InitMembers()
{
    DldInitMembers_Enter( IOBlockStorageServicesDldHook, OSObject, IOBlockStorageServices )
    
    DldInitMembers_AddCommonHookedVtableFunctionsInfo( IOBlockStorageServicesDldHook, OSObject, IOBlockStorageServices )
    
#ifndef __LP64__
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( IOBlockStorageServicesDldHook,
                                                           doAsyncReadWrite,
                                                           doAsyncReadWrite1_hook,
                                                           OSObject,
                                                           IOBlockStorageServices,
                                                           1,
                                                           IOReturn,
                                                           (IOMemoryDescriptor *buffer,
                                                            UInt32 block, UInt32 nblks,
                                                            IOStorageCompletion completion) )
    
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( IOBlockStorageServicesDldHook,
                                                           doSyncReadWrite,
                                                           doSyncReadWrite1_hook,
                                                           OSObject,
                                                           IOBlockStorageServices,
                                                           1,
                                                           IOReturn,
                                                           (IOMemoryDescriptor *buffer,
                                                            UInt32 block,UInt32 nblks) )
#endif /* !__LP64__ */
    
    
#ifndef __LP64__
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( IOBlockStorageServicesDldHook,
                                                           doAsyncReadWrite,
                                                           doAsyncReadWrite2_hook,
                                                           OSObject,
                                                           IOBlockStorageServices,
                                                           2,
                                                           IOReturn,
                                                           (IOMemoryDescriptor *buffer,
                                                            UInt64 block, UInt64 nblks,
                                                            IOStorageCompletion completion) )
#endif /* !__LP64__ */
    
    
    /* 10.5.0 */
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( IOBlockStorageServicesDldHook,
                                                           doAsyncReadWrite,
                                                           doAsyncReadWrite3_hook,
                                                           OSObject,
                                                           IOBlockStorageServices,
                                                           3,
                                                           IOReturn,
                                                           (IOMemoryDescriptor *buffer,
                                                            UInt64 block, UInt64 nblks,
                                                            IOStorageAttributes *attributes,
                                                            IOStorageCompletion *completion) )
    
    DldInitMembers_Exit( IOBlockStorageServicesDldHook, IOService, IOBlockStorageServices )
    
    return true;
}

//--------------------------------------------------------------------

#ifndef __LP64__

IOReturn
IOBlockStorageServicesDldHook::doAsyncReadWrite1_hook(
                                                    IOMemoryDescriptor *buffer,
                                                    UInt32 block, UInt32 nblks,
                                                    IOStorageCompletion completion)
{
    
    IOBlockStorageServicesDldHook* HookObject = IOBlockStorageServicesDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return false;
    
    typedef IOReturn (*doAsyncReadWrite1Func)( IOBlockStorageServices*  __this,
                                              IOMemoryDescriptor *buffer,
                                              UInt32 block, UInt32 nblks,
                                              IOStorageCompletion completion);
    
    int indx = DldVirtualFunctionEnumValue( IOBlockStorageServicesDldHook, doAsyncReadWrite1 );
    doAsyncReadWrite1Func  Original = (doAsyncReadWrite1Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );
    
    assert( Original );
    
    return Original( reinterpret_cast<IOBlockStorageServices*>(this), buffer, block, nblks, completion );
    
}


IOReturn
IOBlockStorageServicesDldHook::doSyncReadWrite1_hook(
                                                   IOMemoryDescriptor *buffer,
                                                   UInt32 block,UInt32 nblks)
{
    
    IOBlockStorageServicesDldHook* HookObject = IOBlockStorageServicesDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return false;
    
    typedef IOReturn (*doSyncReadWrite1Func)( IOBlockStorageServices*  __this,
                                             IOMemoryDescriptor *buffer,
                                             UInt32 block,UInt32 nblks);
    
    int indx = DldVirtualFunctionEnumValue( IOBlockStorageServicesDldHook, doSyncReadWrite1 );
    doSyncReadWrite1Func  Original = (doSyncReadWrite1Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );
    
    assert( Original );
    
    return Original( reinterpret_cast<IOBlockStorageServices*>(this), buffer, block, nblks );
}

#endif /* !__LP64__ */

//--------------------------------------------------------------------

#ifndef __LP64__

IOReturn
IOBlockStorageServicesDldHook::doAsyncReadWrite2_hook(
                                                    IOMemoryDescriptor *buffer,
                                                    UInt64 block, UInt64 nblks,
                                                    IOStorageCompletion completion)
{
    
    IOBlockStorageServicesDldHook* HookObject = IOBlockStorageServicesDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return false;
    
    typedef IOReturn (*doAsyncReadWrite2Func)( IOBlockStorageServices*  __this,
                                              IOMemoryDescriptor *buffer,
                                              UInt64 block, UInt64 nblks,
                                              IOStorageCompletion completion);
    
    
    int indx = DldVirtualFunctionEnumValue( IOBlockStorageServicesDldHook, doAsyncReadWrite2 );
    doAsyncReadWrite2Func  Original = (doAsyncReadWrite2Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );
    
    assert( Original );
    
    return Original( reinterpret_cast<IOBlockStorageServices*>(this), buffer, block, nblks, completion );
}

#endif /* !__LP64__ */

//--------------------------------------------------------------------

/* 10.5.0 */
IOReturn
IOBlockStorageServicesDldHook::doAsyncReadWrite3_hook(
                                                    IOMemoryDescriptor *buffer,
                                                    UInt64 block, UInt64 nblks,
                                                    IOStorageAttributes *attributes,
                                                    IOStorageCompletion *completion)
{
    
    IOBlockStorageServicesDldHook* HookObject = IOBlockStorageServicesDldHook::GetStaticClassInstance();
    assert( HookObject );
    if( NULL == HookObject )
        return false;
    
    typedef IOReturn (*doAsyncReadWrite3Func)( IOBlockStorageServices*  __this,
                                              IOMemoryDescriptor *buffer,
                                              UInt64 block, UInt64 nblks,
                                              IOStorageAttributes *attributes,
                                              IOStorageCompletion *completion);
    
    
    int indx = DldVirtualFunctionEnumValue( IOBlockStorageServicesDldHook, doAsyncReadWrite3 );
    doAsyncReadWrite3Func  Original = (doAsyncReadWrite3Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );
    
    assert( Original );
    
    return Original( reinterpret_cast<IOBlockStorageServices*>(this), buffer, block, nblks, attributes, completion );
    
}

#endif//0
//--------------------------------------------------------------------


