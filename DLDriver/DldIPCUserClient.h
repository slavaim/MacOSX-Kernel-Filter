/*
 *  IPCUserClient.h
 *  DeviceLock
 *
 *  Created by Slava on 7/01/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */

#ifndef _IPCUSERCLIENT_H
#define _IPCUSERCLIENT_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include "DldCommon.h"

//--------------------------------------------------------------------

class DldIPCUserClient : public IOUserClient
{
    OSDeclareDefaultStructors( DldIPCUserClient )
    
private:
    
    typedef struct _WaitBlock{
        volatile UInt32     inUse;     // not zero if in use
        volatile SInt32     eventID;   // a related event
        bool                completed; // true on completion
        IOReturn            operationCompletionStatus; // operation completion status
    } WaitBlock;
    
    task_t                           fClient;
    proc_t                           fClientProc;
    pid_t                            fClientPID;
    uid_t                            fClientUID;
    bool                             trustedClient;
    
    const static unsigned int        WaitBlocksNumber = 32;
    static WaitBlock                 WaitBlocks[WaitBlocksNumber];
    static IOSimpleLock*             WaitLock;
    
public:
    
    static DldIPCUserClient* withTask( __in task_t owningTask, __in bool trustedClient, uid_t proc_uid );
    static bool InitIPCUserClientStaticPart();
    
    virtual IOExternalMethod *getTargetAndMethodForIndex(IOService **target,
                                                         UInt32 index);
    
    virtual IOReturn invalidRequest(void);
    virtual IOReturn ipcRequest(   __in  void *vInBuffer,
                                   __out void *vOutBuffer,
                                   __in  void *vInSize,
                                   __in  void *vOutSizeP,
                                   void *, void *);
    
    bool acquireWaitBlock( __out UInt32* waitBlockIndex, __in SInt32 eventID );
    void releaseWaitBlock( __in UInt32 waitBlockIndex );
    IOReturn waitForCompletion( __in unsigned int waitBlockIndex );
    static IOReturn ProcessResponse( __in DldDriverEventData* response );
};

//--------------------------------------------------------------------

#endif // _IPCUSERCLIENT_H
