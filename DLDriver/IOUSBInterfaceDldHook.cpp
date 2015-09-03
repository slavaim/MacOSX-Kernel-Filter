/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "IOUSBInterfaceDldHook.h"

//--------------------------------------------------------------------

#if 0
#define super DldHookerBaseInterface

DldDefineCommonIOServiceHookFunctionsAndStructors( IOUSBInterfaceDldHook, OSObject, IOUSBInterface )
DldDefineCommonIOServiceHook_HookObjectInt( IOUSBInterfaceDldHook, OSObject, IOUSBInterface )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOUSBInterfaceDldHook, OSObject, IOUSBInterface )
DldDefineCommonIOServiceHook_InitMembers( IOUSBInterfaceDldHook, OSObject, IOUSBInterface )

#endif//0