/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */
#ifndef _DLDIOLOG_H
#define _DLDIOLOG_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODataQueue.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DeviceLockIOKitDriver.h"
#include "DldIOUserClient.h"

//--------------------------------------------------------------------

//
// a forward declaration
//
class DldIOUserClient;

//--------------------------------------------------------------------

class DldIOLog: public OSObject
{
    OSDeclareDefaultStructors( DldIOLog )
    
private:
    
    bool             pendingUnregistration;
    UInt32           clientInvocations;
    
    //
    // the object is retained
    //
    volatile DldIOUserClient* userClient;
    
public:
    
    static DldIOLog* newLog();
    
    bool     isUserClientPresent();
    IOReturn registerUserClient( __in DldIOUserClient* client );
    IOReturn unregisterUserClient( __in DldIOUserClient* client );
    IOReturn logData( __in DldDriverDataLogInt*  intData );
};

//--------------------------------------------------------------------

extern DldIOLog*             gLog;

//--------------------------------------------------------------------
#endif//_DLDIOLOG_H