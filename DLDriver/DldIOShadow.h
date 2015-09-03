/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOSHADOW_H
#define _DLDIOSHADOW_H

#include <IOKit/IOService.h>
#include <IOKIt/IOLocks.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include <sys/wait.h>
#include "DldCommon.h"
#include "DldIODataQueue.h"
#include "DldIOVnode.h"
#include "DldIOShadowFile.h"
#include "DldIOUserClient.h"
#include "DldSCSITask.h"

//--------------------------------------------------------------------

typedef struct _DldShadowObjectInitializer{
    
    //
    // size of the queue in bytes
    //
    UInt32  queueSize;
    
} DldShadowObjectInitializer;

//--------------------------------------------------------------------

typedef struct _DldCommonShadowParams{
    
    //
    // an operation ID which is guaranted to be unique until
    // its completion
    //
    __in UInt64    operationID;
    
    //
    // an event on which the writing thread will wait for shadow completion,
    // the event values ( i.e. *completionEvent )  are set atomicaly acording to
    // the state transitions
    // 0 - initialized,
    // 1 - preparing for waiting ( before entering a wq )
    // 2 - ready for waiting ( after entering in a wq )
    // 3 - shadow completed,
    // use DldInitNotificationEvent(), DldSetNotificationEvent()
    // and DldWaitForNotificationEvent() for the event management,
    // the event is optional for some type of the request and is
    // mandatory for the others, curently the event is mandatory
    // for VFS write and pageout operation
    //
    __in UInt32* completionEvent;
    
    //
    // an optional member,
    // if not NULL then on return the *shadowCompletionRC value is valid only if
    // - success was returned by shadowing function ( which generally
    //   means that the request has been sent to a worker thread )
    // - shadow has completed ( which generally means that a worker
    //   thread has written data to a shadow file )
    //
    __inout __opt IOReturn*  shadowCompletionRC;
    
} DldCommonShadowParams;

//--------------------------------------------------------------------

typedef struct _DldShadowMsgHdr{
    
#if defined( DBG )
    #define DLD_SHADOW_HDR_SIGNATURE   0xABCD3215
    UInt32  signature;
#endif//DBG
    
    DldShadowType  type;
    
} DldShadowMsgHdr;

//--------------------------------------------------------------------

typedef struct _DldShadowOperationCompletion{
    
    //
    // an ID of the completed operation, the same as was used for shadowing
    //
    UInt64    operationID;
    
    //
    // a returned status for shadowed operation
    //
    int       retval;
    
    //
    // a return value for the shadowing operation
    // if the value is not KERN_SUCCESS then the shadowing
    // failed
    //
    int       shadowingRetval;
    
    //
    // some additional data depending of the request's nature,
    // actually must be a copy of DldShadowDataHeader::operationData::completion::additionalData
    //
    union{
        
        //
        // bytes written by FSD in a file( a shadowed file! ) when processing
        // a write request, used for:
        // DldShadowTypeFileWrite
        // DldShadowTypeFilePageout
        //
        user_ssize_t   bytesWritten;
        
        //
        // a completion status for a SCSI operation, DldShadowTypeSCSICommand
        //
        DldSCSITaskResults   scsiOperationStatus;
        
    } additionalData;
    
} DldShadowOperationCompletion;

//--------------------------------------------------------------------

typedef struct _DldShadowOperationCompletionArgs{
    
#if defined( DBG )
#define DLD_SHADOW_COMP_SIGNATURE   0xABCD3217
    UInt32                        signature;
#endif//DBG
    
    //
    // a referenced DldIOVnode, an optional parameter
    //
    __opt  DldIOVnode*             dldVnode;
    
    DldShadowOperationCompletion   comp;
    
} DldShadowOperationCompletionArgs;

//--------------------------------------------------------------------

typedef struct _DldShadowCreateCloseVnode{
    
#if defined( DBG )
#define DLD_SHADOW_OCR_SIGNATURE   0xABCD3218
    UInt32  signature;
#endif//DBG
    
    //
    // DldShadowTypeFileOpen
    // DldShadowTypeFileClose
    // DldShadowTypeFileReclaim
    //
    DldShadowType          type; 
    
    //
    // a common parameters to return status and set an event
    //
    DldCommonShadowParams  commonParams;
    
    //
    // a referenced DldIOVnode
    //
    DldIOVnode*      dldVnode;
    
    //
    // valid only for open operation
    //
    __opt DldDriverDataLogInt*  logData;
    
} DldShadowCreateCloseVnode;

//--------------------------------------------------------------------

typedef struct _DldRangeDescriptor{
    
    //
    // offset of the range in the backing file,
    // i.e. in case of pageout operation 
    // from where the vnode pager paged in the range,
    //
    off_t        fileOffset;
    
    //
    // the size of the range
    //
    user_size_t  rangeSize;
    
} DldRangeDescriptor;

//--------------------------------------------------------------------

typedef union _DldWriteVnode_ap{
    
    struct vnop_write_args*    ap_write;   //DldShadowTypeFileWrite
    struct vnop_pageout_args*  ap_pageout; //DldShadowTypeFilePageout
    
} DldWriteVnode_ap;

//--------------------------------------------------------------------

typedef struct _DldShadowWriteVnodeArgs{
    
#if defined( DBG )
#define DLD_SHADOW_WRVN_SIGNATURE   0xABCD3216
    UInt32  signature;
#endif//DBG
    
    DldShadowType   type;
    
    //
    // a copy of the arguments passed for a write vnode or pageout function
    //
    union{
        vnop_write_args   ap_write;
        vnop_pageout_args ap_pageout;
    } ap;
    
    //
    // in case of the pageout V2 the upl is created by FSD, so we
    // create our own upl and free it on completion
    //
    upl_t          upl;
    
    //
    // an operation ID, a unique value until the operation completion
    //
    UInt64         operationID;
    
    //
    // a referenced DldIOVnode, must be derefrenced on completion
    //
    DldIOVnode*    dldVnode;
    
    //
    // a common parameters to return status and set an event
    //
    DldCommonShadowParams   commonParams;
    
    //
    // if true the all dIOMemoryDescriptors have been wired ( i.e. prepared )
    // and mut be unwired on completion
    //
    bool          pagesAreWired;
    
    //
    // an array of the IOMemoryDescriptor* for ap.a_uio vectors,
    // released when it is no longer needed by a recepient,
    // each entry describes exactly a single contiguous range
    // of memory
    //
    OSArray*       memDscArray;
    
    //
    // an array for the DldRangeDescriptors related with
    // the memory descriptors in the memDscArray, saved
    // as OSData binary objects
    //
    OSArray*      rangeDscArray;
    
} DldShadowWriteVnodeArgs;

//--------------------------------------------------------------------

typedef struct _DldShadowSCSIArgs{
    
#if defined( DBG )
#define DLD_SHADOW_SCSI_SIGNATURE   0xABCD3219
    UInt32                        signature;
#endif//DBG
    
    //
    // a non referenced dldSCSITask, the object is retained through
    // the completion callback hook which releases the object
    // when the request is completed
    //
    DldSCSITask*            dldSCSITask;
    
    //
    // a common parameters to return status and set an event
    //
    DldCommonShadowParams   commonParams;
    
} DldShadowSCSIArgs;

//--------------------------------------------------------------------

class DldIOShadow: public OSObject
{
    OSDeclareDefaultStructors( DldIOShadow )
    
private:
    
    //
    // a communication queue for the kernel-to-kernel communication
    //
    DldIODataQueue* dataQueue;
    
    //
    // a shadow thread
    //
    thread_t       thread;
    
    //
    // set to true if the stop has been called
    //
    bool           stopped;
    
    //
    // a thread termination notification, 0x0 if the thread is running
    //
    UInt32         terminationEvent;
    
    //
    // the first entry( i.e. with the index is 0x0 ) contains
    // a current active file, when the file is removed the next
    // entry becomes active and occupies the first slot
    //
    OSArray*       shadowFilesArray;
    
    //
    // a rw-lock to protect internal data
    //
    IORWLock*    rwLock;
    
#if defined( DBG )
    thread_t     exclusiveThread;
#endif//DBG
    
    //
    // the user client's state variables
    //
    bool             pendingUnregistration;
    UInt32           clientInvocations;
    
    //
    // a value used to generate operation IDs
    //
    UInt32           operationCounter;
    
    //
    // a user client for the kernel-to-user communication
    // the object is retained
    //
    volatile DldIOUserClient* userClient;
    
    //
    // set to true if the quota was exceeded at least once,
    //
    bool quotaExceeded;
    
    //
    // a thread routine
    //
    static void shadowThreadMain( void * this_class );
    
    //
    // returns a referenced object or NULL, the caller must dereference,
    // the sizeToBeWritten value advises on a number of bytes being written
    // may be 0x0 if no write will follow
    //
    DldIOShadowFile* getCurrenShadowFileReserveSpace( __in_opt off_t sizeToWrite = 0x0,
                                                      __out_opt off_t*  reservedOffset_t = NULL );
    
    //
    // a counterpart worker routine for shadowFileWrite
    //
    void processFileWriteWQ( __in DldShadowWriteVnodeArgs*   args );
    
    //
    // a counterpart worker routine for shadowOperationCompletion
    //
    void shadowOperationCompletionWQ( __in DldShadowOperationCompletionArgs*  compArgs );
    
    //
    // a counterpart worker routine for shadowFileOpen, shadowFileClose, shadowFileReclaim
    //
    void shadowCreateCloseWQ( __in DldShadowCreateCloseVnode*  args );
    
    //
    // a counterpart worker routine for shadowSCSIOperation
    //
    void processSCSIOperationWQ( __in DldShadowSCSIArgs*   scsiArgs );
    
    void LockShared();
    void UnLockShared();
    
    void LockExclusive();
    void UnLockExclusive();
    
protected:
    
    virtual void free();
    virtual bool initWithInitializer( __in DldShadowObjectInitializer* initializer );
    
    virtual bool shadowFileWriteInt( __in  DldShadowType type,
                                     __in  DldIOVnode* dldVnode,
                                     __in  DldWriteVnode_ap ap,
                                     __inout DldCommonShadowParams* commonParams );
    
    virtual bool shadowFileOpenCloseInt( __in          DldShadowType  type,
                                         __in          DldIOVnode* dldVnode,
                                         __in __opt    DldDriverDataLogInt*  logData,
                                         __inout __opt DldCommonShadowParams* commonParams
                                       );
    
    static void shadowCSITaskCompletionCallbackHook( __in SCSITaskIdentifier request );
    
public:
    
    static DldIOShadow* withInitializer( __in DldShadowObjectInitializer* initializer );
    
    virtual void stop();
    
    //
    // generates a unique ID for operation, valid until the counter wraps around
    //
    UInt64 generateUniqueID(){ return (UInt64)(OSIncrementAtomic( &this->operationCounter )+0x1); }
    
    //
    // *shadowCompletionStatus valid only after shadow completion and if shadowFileWrite returned true,
    // if the false value is returned the completionEvent won't be set in a signal state
    // and shadowCompletionStatus will not have any valid value
    //
    virtual bool shadowFileWrite( __in  DldIOVnode* dldVnode,
                                  __in  struct vnop_write_args *ap,
                                  __inout DldCommonShadowParams* commonParams );
    
    //
    // if false is returned the *commonParams->shadowCompletionRC value
    // is invalid, if true is returned it contains a valid value
    // after commonParams->completionEvent has been set to a signal state,
    //
    virtual bool shadowFilePageout( __in DldIOVnode* dldVnode,
                                    __in struct vnop_pageout_args *ap,
                                    __inout DldCommonShadowParams* commonParams );
    
    virtual bool shadowOperationCompletion( __in_opt DldIOVnode* dldVnode,
                                            __in DldShadowOperationCompletion* comp );
    
    virtual bool shadowFileOpen( __in DldIOVnode* dldVnode,
                                 __in DldDriverDataLogInt*  logData,
                                 __inout DldCommonShadowParams* commonParams );
    
    virtual bool shadowFileClose( __in DldIOVnode* dldVnode,
                                  __inout DldCommonShadowParams* commonParams );
    
    virtual bool shadowFileReclaim( __in DldIOVnode* dldVnode );
    
    //
    // if false is returned the *commonParams->shadowCompletionRC value
    // is invalid, if true is returned it contains a valid value
    // after commonParams->completionEvent has been set to a signal state,
    //
    virtual bool shadowSCSIOperation( __in DldIOService*      dldService,
                                      __in SCSITaskIdentifier request,
                                      __inout DldCommonShadowParams* commonParams );
    
    //
    // saves a shadow file object and takes a reference to the object
    //
    virtual bool addShadowFile( __in DldIOShadowFile* shadowFile );
    
    //
    // releases all saved shadow files, there will be no registered shadow files
    // but there might be referenced shadow files on which the IO is ongoing,
    // they will be released when IO completed
    //
    virtual void releaseAllShadowFiles();
    
    //
    // returns true if all resources ( a shadow file and a client ) are present
    //
    virtual bool isShadowResourcesAllocated();
    
    //
    // returns true if the quota has been exceed at least once, this
    // doen't mean that the quota is currently exceeded
    //
    virtual bool quotaWasExceededAtLeastOnce();
    
    //
    // called when the quota is exceeded
    //
    virtual void quotaExceedingDetected();
    
    virtual bool isUserClientPresent();
    
    
    virtual IOReturn registerUserClient( __in DldIOUserClient* client );
    virtual IOReturn unregisterUserClient( __in DldIOUserClient* client );
    virtual IOReturn shadowNotification( __in DldDriverShadowNotificationInt*  intData );
    virtual IOReturn shadowFileRemovedNotification( __in DldIOShadowFile* shadowFile );
};

//--------------------------------------------------------------------

extern DldIOShadow*   gShadow;

//--------------------------------------------------------------------

#endif//_DLDIOSHADOW_H