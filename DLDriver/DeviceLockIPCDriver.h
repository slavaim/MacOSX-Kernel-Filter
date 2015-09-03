/*
 *  DeviceLockIPCDriver.h
 *  DeviceLock
 *
 *  Created by Slava on 6/01/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */

#ifndef _DEVICELOCKIPCDRIVER_H
#define _DEVICELOCKIPCDRIVER_H

#include <IOKit/IOService.h>
#include "DldCommon.h"

//--------------------------------------------------------------------

//
// the IPC driver class
//
class com_devicelock_driver_DeviceLockIPCDriver : public IOService
{
    OSDeclareDefaultStructors(com_devicelock_driver_DeviceLockIPCDriver)
    
public:
    virtual bool start(IOService *provider);
    virtual void stop( IOService * provider );
    
protected:
    virtual bool init();
    
     virtual IOReturn newUserClient( __in task_t owningTask,
                                     __in void*,
                                     __in UInt32 type,
                                     __in IOUserClient **handler );
};

//--------------------------------------------------------------------

#endif // _DEVICELOCKIPCDRIVER_H