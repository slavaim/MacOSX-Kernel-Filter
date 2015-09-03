/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _IOMEDIABSDCLIENTDLDHOOK_H
#define _IOMEDIABSDCLIENTDLDHOOK_H

#include <sys/types.h>                       // (miscfs/devfs/devfs.h, ...)

#include <miscfs/devfs/devfs.h>              // (devfs_make_node, ...)
#include <sys/buf.h>                         // (buf_t, ...)
#include <sys/fcntl.h>                       // (FWRITE, ...)
#include <sys/ioccom.h>                      // (IOCGROUP, ...)
#include <sys/proc.h>                        // (proc_is64bit, ...)
#include <sys/stat.h>                        // (S_ISBLK, ...)
#include <sys/systm.h>                       // (DEV_BSIZE, ...)
#include <IOKit/assert.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
//#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOMediaBSDClient.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#if 0

class IOMediaBSDClientDldHook : public OSObject
{
    OSDeclareDefaultStructors( IOMediaBSDClientDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOMediaBSDClientDldHook, OSObject, IOMediaBSDClient )
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOMediaBSDClientDldHook )
        DldAddCommonVirtualFunctionsEnumDeclaration( IOMediaBSDClientDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOMediaBSDClientDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOMediaBSDClientDldHook, IOMediaBSDClient )
    DldDeclarePureVirtualHelperClassEnd( IOMediaBSDClientDldHook, IOMediaBSDClient )
    
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

#endif//_IOMEDIABSDCLIENTDLDHOOK_H