/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "IOUSBDeviceDldHook.h"

//--------------------------------------------------------------------
#if 0

#define super OSObject

OSDefineMetaClassAndStructors( IOUSBDeviceDldHook, OSObject )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOUSBDeviceDldHook, OSObject, IOUSBDevice )
DldDefineCommonIOServiceHook_HookObjectInt( IOUSBDeviceDldHook, OSObject, IOUSBDevice )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOUSBDeviceDldHook, OSObject, IOUSBDevice )
DldDefineCommonIOServiceHook_InitMembers( IOUSBDeviceDldHook, OSObject, IOUSBDevice )

#endif//0