/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef IOBSDSYSTEM_H
#define IOBSDSYSTEM_H

#include <libkern/c++/OSData.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSIterator.h>
#include <libkern/c++/OSDictionary.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/IORegistryEntry.h>
#include "DldCommon.h"
#include "DldIOService.h"
#include "DldIOVnode.h"

extern
bool
DldInitBSD();

extern
void
DldUninitBSD();

extern
IORegistryEntry *
DldGetBsdMediaObjectFromName(
    __in const char * ioBSDNamePtr
    );

extern 
IORegistryEntry*
DldGetBsdMediaObjectFromMount(
    __in mount_t mountPtr
    );

extern
DldIOService*
DldGetReferencedDldIOServiceFoMediaByVnode(
    __in vnode_t vnode
    );

extern
void
DldRemoveMediaEntriesFromMountCache(
    __in IORegistryEntry*  ioMedia
    );

extern
DldIOService*
DldGetReferencedDldIOServiceForSerialDeviceVnode(
    __in DldIOVnode* dldVnode
    );

extern
Boolean
DldIsBootMedia(
    __in IOService*  service
    );

#endif//IOBSDSYSTEM_H