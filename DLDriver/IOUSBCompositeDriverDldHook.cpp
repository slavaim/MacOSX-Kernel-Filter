/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "IOUSBCompositeDriverDldHook.h"

//--------------------------------------------------------------------

#if 0

#define super OSObject

OSDefineMetaClassAndStructors( IOUSBCompositeDriverDldHook, OSObject )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOUSBCompositeDriverDldHook,OSObject, IOUSBCompositeDriver )
DldDefineCommonIOServiceHook_HookObjectInt( IOUSBCompositeDriverDldHook, OSObject, IOUSBCompositeDriver )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOUSBCompositeDriverDldHook, OSObject, IOUSBCompositeDriver )
DldDefineCommonIOServiceHook_InitMembers( IOUSBCompositeDriverDldHook, OSObject, IOUSBCompositeDriver )

#endif//0