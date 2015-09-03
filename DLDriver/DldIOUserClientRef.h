/*
 *  DldIOUserClientRef.h
 *  DeviceLock
 *
 *  Created by Slava on 2/01/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */

#ifndef _DLDIOUSERCLIENTREF_H
#define _DLDIOUSERCLIENTREF_H

#include <IOKit/IOTypes.h>

//--------------------------------------------------------------------

class DldIOUserClient;

class DldIOUserClientRef{
    
protected:
    //
    // the user client's state variables
    //
    bool    pendingUnregistration;
    UInt32  clientInvocations;
    
    //
    // a user client for the kernel-to-user communication
    // the object is retained
    //
    volatile class DldIOUserClient* userClient;
    
public:
    
    DldIOUserClientRef(): pendingUnregistration(false), clientInvocations(0), userClient(NULL){;}
    virtual ~DldIOUserClientRef(){ assert( NULL == userClient );}
    
    virtual bool isUserClientPresent();
    virtual IOReturn registerUserClient( __in DldIOUserClient* client );
    virtual IOReturn unregisterUserClient( __in DldIOUserClient* client );
    
    //
    // returns (-1) if there is no client
    //
    virtual pid_t getUserClientPid();
    
    //
    // a caller must call releaseUserClient() for each successfull call to getUserClient()
    //
    DldIOUserClient* getUserClient();
    void releaseUserClient();
    
};

//--------------------------------------------------------------------

//
// a user client for the kernel-to-user communication
//
extern DldIOUserClientRef       gServiceUserClient;

//--------------------------------------------------------------------
#endif // _DLDIOUSERCLIENTREF_H
