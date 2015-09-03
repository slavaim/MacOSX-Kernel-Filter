/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "IOMediaDldHook.h"

//--------------------------------------------------------------------

#if 0

#define super OSObject

OSDefineMetaClassAndStructors( IOMediaDldHook, OSObject )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOMediaDldHook, OSObject, IOMedia )
DldDefineCommonIOServiceHook_HookObjectInt( IOMediaDldHook, OSObject, IOMedia )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOMediaDldHook, OSObject, IOMedia )
DldDefineCommonIOServiceHook_InitMembers( IOMediaDldHook, OSObject, IOMedia )

#endif//0

