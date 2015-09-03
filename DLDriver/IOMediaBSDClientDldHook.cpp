/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "IOMediaBSDClientDldHook.h"

//--------------------------------------------------------------------

#if 0

#define super OSObject

OSDefineMetaClassAndStructors( IOMediaBSDClientDldHook, OSObject )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOMediaBSDClientDldHook, OSObject, IOMediaBSDClient )
DldDefineCommonIOServiceHook_HookObjectInt( IOMediaBSDClientDldHook, OSObject, IOMediaBSDClient )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOMediaBSDClientDldHook, OSObject, IOMediaBSDClient )
DldDefineCommonIOServiceHook_InitMembers( IOMediaBSDClientDldHook, OSObject, IOMediaBSDClient )

#endif//0
