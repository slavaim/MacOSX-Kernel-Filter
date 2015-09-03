/*
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _IOBLOCKSTORAGESERVICESDLDHOOK_H
#define _IOBLOCKSTORAGESERVICESDLDHOOK_H

#include <IOKit/scsi/IOBlockStorageServices.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0

class IOBlockStorageServicesDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOBlockStorageServicesDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOBlockStorageServicesDldHook, OSObject, IOBlockStorageServices )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOBlockStorageServicesDldHook )
    DldAddCommonVirtualFunctionsEnumDeclaration( IOBlockStorageServicesDldHook )
    
#ifndef __LP64__
    DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageServicesDldHook, doAsyncReadWrite1 )
    DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageServicesDldHook, doSyncReadWrite1 )
#endif /* !__LP64__ */
    
#ifndef __LP64__
    DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageServicesDldHook, doAsyncReadWrite2 )
#endif /* !__LP64__ */
    
#ifdef __LP64__
    DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageServicesDldHook, doAsyncReadWrite3 )
#else /* !__LP64__ */
    DldAddVirtualFunctionInEnumDeclaration( IOBlockStorageServicesDldHook, doAsyncReadWrite3 )
#endif /* !__LP64__ */
    
    DldVirtualFunctionsEnumDeclarationEnd( IOBlockStorageServicesDldHook )
    
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOBlockStorageServicesDldHook, IOBlockStorageServices )
    
public:
#ifndef __LP64__
    //doAsyncReadWrite1_hook
    virtual IOReturn	doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                         UInt32 block, UInt32 nblks,
                                         IOStorageCompletion completion) __attribute__ ((deprecated));
    //doSyncReadWrite1_hook
    virtual IOReturn	doSyncReadWrite(IOMemoryDescriptor *buffer,
                                        UInt32 block,UInt32 nblks) __attribute__ ((deprecated));
#endif /* !__LP64__ */
    
#ifndef __LP64__
    //doAsyncReadWrite2_hook
    virtual IOReturn	doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                         UInt64 block, UInt64 nblks,
                                         IOStorageCompletion completion) __attribute__ ((deprecated));
#endif /* !__LP64__ */
    
#ifdef __LP64__
    //doAsyncReadWrite3_hook
    virtual IOReturn	doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                         UInt64 block, UInt64 nblks,
                                         IOStorageAttributes *attributes,
                                         IOStorageCompletion *completion)	= 0;
#else /* !__LP64__ */
    //doAsyncReadWrite3_hook
    virtual IOReturn	doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                         UInt64 block, UInt64 nblks,
                                         IOStorageAttributes *attributes,
                                         IOStorageCompletion *completion); /* 10.5.0 */
#endif /* !__LP64__ */
    
    DldDeclarePureVirtualHelperClassEnd( IOBlockStorageDriverDldHook, IOBlockStorageDriver )
    
public:
    
    ////////////////////////////////////////////////////////
    //
    // hooking function declarations
    //
    /////////////////////////////////////////////////////////
    
#ifndef __LP64__
    virtual IOReturn	doAsyncReadWrite1_hook(IOMemoryDescriptor *buffer,
                                               UInt32 block, UInt32 nblks,
                                               IOStorageCompletion completion) __attribute__ ((deprecated));
    
    virtual IOReturn	doSyncReadWrite1_hook(IOMemoryDescriptor *buffer,
                                              UInt32 block,UInt32 nblks) __attribute__ ((deprecated));
#endif /* !__LP64__ */
    
    
#ifndef __LP64__
    virtual IOReturn	doAsyncReadWrite2_hook(IOMemoryDescriptor *buffer,
                                               UInt64 block, UInt64 nblks,
                                               IOStorageCompletion completion) __attribute__ ((deprecated));
#endif /* !__LP64__ */
    
    
    virtual IOReturn	doAsyncReadWrite3_hook(IOMemoryDescriptor *buffer,
                                               UInt64 block, UInt64 nblks,
                                               IOStorageAttributes *attributes,
                                               IOStorageCompletion *completion);/* 10.5.0 */
    
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
};

#endif//0

#endif//_IOBLOCKSTORAGESERVICESDLDHOOK_H

