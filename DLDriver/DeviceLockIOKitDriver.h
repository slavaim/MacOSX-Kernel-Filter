/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */
#ifndef _DEVICELOCKIOKITDRIVER_H
#define _DEVICELOCKIOKITDRIVER_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODataQueue.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldIOKitHookEngine.h"
#include "DldKAuthVnodeGate.h"
#include "DldIOLog.h"

//
// the I/O Kit driver class
//
class com_devicelock_driver_DeviceLockIOKitDriver : public IOService
{
    OSDeclareDefaultStructors(com_devicelock_driver_DeviceLockIOKitDriver)
    
private:
    
    UInt32 initializationCompletionEvent;
    bool   initializationWasSuccessful;
    
private:
    bool getBoolByNameFromDict( __in OSDictionary* dictionairy, __in const char* name );
    IOReturn  fillLogFlags( __inout DldLogFlags*  logFlags, __in OSDictionary*  logFlagDictionairy );
    IOReturn  processKmodParameters();
    
public:
    virtual bool start(IOService *provider);
    virtual void stop( IOService * provider );
    
    virtual IOReturn newUserClient( __in task_t owningTask,
                                    __in void*,
                                    __in UInt32 type,
                                    __in IOUserClient **handler );
    
protected:
    static void			GetDeviceTreeContinuation( void * this_class );
    IOReturn            DumpTree( void );
    IOReturn            IOPrintPlane( const IORegistryPlane * plane );
    
protected:
    
    virtual bool init();
    virtual void free();
    
private:
    
};

//
// the user client class
//
/*
class com_osxbook_driver_DeviceLockIOKitDriver : public IOUserClient
{
    OSDeclareDefaultStructors(com_osxbook_driver_VnodeWatcherUserClient)
    
private:
    task_t                           fClient;
    com_osxbook_driver_VnodeWatcher *fProvider;
    IODataQueue                     *fDataQueue;
    IOMemoryDescriptor              *fSharedMemory;
    kauth_listener_t                 fListener;
    
public:
    virtual bool     start(IOService *provider);
    virtual void     stop(IOService *provider);
    virtual IOReturn open(void);
    virtual IOReturn clientClose(void);
    virtual IOReturn close(void);
    virtual bool     terminate(IOOptionBits options);
    virtual IOReturn startLogging(void);
    virtual IOReturn stopLogging(void);
    
    virtual bool     initWithTask(
                                  task_t owningTask, void *securityID, UInt32 type);
    virtual IOReturn registerNotificationPort(
                                              mach_port_t port, UInt32 type, UInt32 refCon);
    
    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                         IOMemoryDescriptor **memory);
    virtual IOExternalMethod *getTargetAndMethodForIndex(IOService **target,
                                                         UInt32 index);
};
 */

#endif//_DEVICELOCKIOKITDRIVER_H
