/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#ifdef _DLD_MACOSX_CAWL

#ifndef _DLDDISKCAWL_H
#define _DLDDISKCAWL_H

#include <IOKit/IOService.h>
#include <IOKIt/IOLocks.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include <sys/wait.h>
#include "DldCommon.h"
#include "DldIOVnode.h"
#include "DldIOUserClient.h"
#include "DldUserToKernel.h"

//--------------------------------------------------------------------

typedef struct _DldDiskCAWLObjectInitializer{
    
    //
    // reserved
    //
    int reserved;
    
} DldDiskCAWLObjectInitializer;

//--------------------------------------------------------------------

class DldDiskCAWL: public OSObject
{
    OSDeclareDefaultStructors( DldDiskCAWL )
    
private:
    
    //
    // used to generate the CAWL notification's ID
    //
    volatile SInt64      IdGenerator;
    
    IORWLock*            rwLock;
    
    //
    // the user client's state variables
    //
    bool                 pendingUnregistration;
    UInt32               clientInvocations;
    
    //
    // a user client for the kernel-to-user communication
    // the object is retained
    //
    volatile DldIOUserClient* userClient;
    
#if defined( DBG )
    thread_t             exclusiveThread;
#endif//DBG
    
public:
    
    typedef union _NotificationData{
        
        typedef struct _FileOpen{
            struct _DldAccessCheckParam* accessParam;
        }FileOpen;
        
        FileOpen accessRequest;
        FileOpen open;
        
        struct{
            UInt64          timeStamp;
            off_t           offset;
            user_ssize_t    size;
            unsigned char   backingSparseFileID[16];
        } write;
        
    } NotificationData;
    
public:
    
    //
    // create a new object
    //
    static DldDiskCAWL* withInitializer( __in DldDiskCAWLObjectInitializer* initializer);
    
    virtual bool isUserClientPresent();
    virtual IOReturn registerUserClient( __in DldIOUserClient* client );
    virtual IOReturn unregisterUserClient( __in DldIOUserClient* client );
    
    //
    // process a response from the service
    //
    virtual errno_t processServiceResponse( __in DldServiceDiskCawlResponse* serviceResponse );
    
    //
    // send a CAWL notification to the service
    //
    virtual errno_t diskCawlNotification( __in     DldIOVnode* dldVnode,
                                          __in     DldCawlOpcode opCode,
                                          __in_opt vfs_context_t vfsContext, // valid for kDldCawlOpAccessRequest only
                                          __in_opt  DldDiskCAWL::NotificationData* data );
    
protected:
    
    virtual void free();
    
private:
    
    bool initWithInitializer( __in DldDiskCAWLObjectInitializer* initializer );
    
    void LockShared();
    void UnLockShared();
    
    void LockExclusive();
    void UnLockExclusive();
};

//--------------------------------------------------------------------

extern DldDiskCAWL*  gDiskCAWL;

//--------------------------------------------------------------------

#endif // _DLDDISKCAWL_H

#endif//#ifdef _DLD_MACOSX_CAWL
