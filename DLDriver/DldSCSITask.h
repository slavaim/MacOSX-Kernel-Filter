/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDSCSITASK_H
#define DLDSCSITASK_H

//
// Apple decided to not export SCSITask class or functions to retrieve
// information from a SCSITask object as it is supposed that a developer uses
// a class derived from IOSCSIPrimaryCommandsDevice to create its layer
// an thus have functions to manipulate with a SCSITask class object,
// nevertheless the SCSITask class functions are exported by 
// com.apple.iokit.IOSCSIArchitectureModelFamily module and can be extracted
// from it, currently to simplify the development the implicit linking
// is used thus the drver declares a dependence from com.apple.iokit.IOSCSIArchitectureModelFamily,
// in the future this dependence must be removed as the system might just do not have
// a required module as a user removed it deliberately thus preventing the DldDriver
// from loading, a solution might be to find the required functions
// using a full names ( like __ZN8SCSITask25GetCommandDescriptorBlockEPA16_h
// for GetCommandDescriptorBlock ) exported by IOSCSIArchitectureModelFamily.kext
// and provide them to the driver ( caveat - this can't be done in a kernel mode
// as the MachO header is not full there, it lacks the LC_SYMTAB segment, in user mode
// there is a danger that a file on a file system and loaded module differ, so the check
// of the offsets must be done by the byte code checking ), the offset for a loaded module is 
// calculated as - base+code_offset+func_offset where the base is a base of the loaded
// module ( as returned by kextstat ), code_offset is a kernel_segment_command_t->fileoff offset as in 
// LC_SEGMENT_KERNEL segmnet without name ( see the DldRetrieveModuleGlobalSymbolAddress function ),
// func_offset is an offset as reported by "nm -m ./IOSCSIArchitectureModelFamily | grep GetCommandDescriptorBlock"
//

#include "DldCommon.h"
#include "DldHookerCommonClass.h"
#include "DldSupportingCode.h"

#include <IOKit/scsi/IOSCSIProtocolInterface.h>
#include <IOKit/scsi/SCSITask.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSICommandDefinitions.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>
#include <IOKit/scsi/IOSCSIProtocolInterface.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>

//--------------------------------------------------------------------

void
DldLogSCSITask(
               __in SCSITaskIdentifier   request
               );

//
// This will always return a 16 Byte CDB
//
bool  DldSCSITaskGetCommandDescriptorBlock( __in    SCSITaskIdentifier   request,
                                            __inout SCSICommandDescriptorBlock * cdbData );


bool  DldSCSITaskSetServiceResponse( __inout SCSITaskIdentifier   request,
                                     __in SCSIServiceResponse serviceResponse );

bool  DldSCSITaskSetTaskStatus( __inout SCSITaskIdentifier request,
                                __in    SCSITaskStatus newTaskStatus );

void DldSCSITaskCompletedNotification ( __inout SCSITaskIdentifier request );

void DldSCSITaskCompleteAccessDenied( __inout SCSITaskIdentifier request );

UInt8 DldSCSITaskGetCommandDescriptorBlockSize ( __in SCSITaskIdentifier request );

bool DldSCSITaskIsMediaEjectionRequest( __in SCSITaskIdentifier request );

dld_classic_rights_t 
DldSCSITaskGetCdbRequestedAccess( __in SCSITaskIdentifier request );

//--------------------------------------------------------------------

class DldSCSITask: public OSObject{
    
    OSDeclareDefaultStructors( DldSCSITask )
    
private:
    
    //
    // a pointer to a reasl system allocated SCSITask,
    // the object is REFERENCED to retain it valid until
    // we completed with its processing, but it is a good
    // behaviour to complete all our tasks before a user
    // client gets a chance to receive a completion notification
    // from a callback
    //
    SCSITaskIdentifier request;
    
    //
    // an original completion callback for the request, 
    // the value is set when the completion is being hooked
    //
    SCSITaskCompletion originalCompletionCallback;
    
    bool completionHooked;
    
    //
    // counts how many times the buffer has been prepared
    // by calling prepare(), i.e. has been wired
    //
    UInt32 bufferPreparedCount;
    
    //
    // a REFERENCED memory descriptor for the data buffer
    //
    IOMemoryDescriptor*  dataBuffer;
    
    //
    // a referenced DldIOService for a DVD drive to which the task is sent
    //
    DldIOService*        dldService;
    
    //
    // an ID of the shadow operation for this request,
    // the same as DldCommonShadowParams::operationID
    //
    UInt64               shadowOperationID;
    
    //
    // a shadow operation status
    // the same as *DldCommonShadowParams::shadowCompletionRC
    //
    IOReturn             shadowStatus;
    
    //
    // shadow completion event, used to synchronize an asynchronous
    // shadowing opertion completion with the SCSI completion callback
    // hook which reports the shadow status to the service
    //
    UInt32               shadowCompletionEvent;
    
    //
    // used to queue objects
    //
    CIRCLEQ_ENTRY( DldSCSITask )  listEntry;
    
    //
    // an offset to the fCompletionCallback field in the system's SCSITask objects
    //
    static vm_offset_t  gCallbackOffset;
    
    //
    // structure declaration
    //
    CIRCLEQ_HEAD( DldSCSITaskListHead, DldSCSITask );
    
    //
    // all DldSCSITask objects are linked in the list
    //
    static DldSCSITaskListHead gSCSITaskListHead;
    
    //
    // true if all static members are in the state which allows
    // to safely create DldSCSITask objects
    //
    static bool gStaticInit;
    
    //
    // a lock to protect the list access
    //
    static IORWLock*    rwLock;
    
#if defined( DBG )
    static thread_t exclusiveThread;
#endif//DBG
    
protected:
    
    virtual void free();
    
public:
    
    static IOReturn DldInitSCSITaskSubsystem();
    static void     DldFreeSCSITaskSubsystem();
    
    static DldSCSITask*  withSCSITaskIdentifier( __in SCSITaskIdentifier   request, __in DldIOService*   dldService );
    
    static void LockShared();
    static void UnlockShared();
    
    static void LockExclusive();
    static void UnlockExclusive();
    
    //
    // an example for a completion hook, not used
    //
    static void CommonSCSITaskHook( __in SCSITaskIdentifier   request );
    
    //
    // returns a refernced task for the request, the task is
    // returned only if has been found in the list
    //
    static DldSCSITask* GetReferencedSCSITask( __in SCSITaskIdentifier   request );
    
    //
    // returns THE current completion routine!
    //
    SCSITaskCompletion getCompletionRoutine()
    {
        assert( (vm_offset_t)(-1) != gCallbackOffset );
        return *(SCSITaskCompletion*)( (vm_offset_t)request+gCallbackOffset );
    }
    
    //
    // returns a DVD service object, the object is not refernced as retained by the DldSCSITask
    //
    DldIOService* getDldIOService(){ return this->dldService; };
    
    void setShadowOperationID( __in UInt32 shadowID ){ this->shadowOperationID = shadowID; };
    UInt32 getShadowOperationID(){ return this->shadowOperationID; };
    
    void setShadowStatus( __in IOReturn status ){ this->shadowStatus = status; };
    IOReturn getShadowStatus(){ return this->shadowStatus; };
    
    //
    // calling this routine for not shadowed operation results in an infinite waiting
    //
    void waitForShadowCompletion();
    void setShadowCompletionEvent(){ DldSetNotificationEvent( &this->shadowCompletionEvent ); };
    
    //
    // hooks a completion routine, only one hook is supported
    //
    bool SetTaskCompletionCallbackHook( __in SCSITaskCompletion completionHook );
    
    //
    // restores the original completion callback value
    //
    bool RestoreOriginalTaskCompletionCallback();
    
    //
    // calls the original completion if it exists,
    // must be called only from a completion hook
    //
    void CallOriginalCompletion();
    
    //
    // wires the dataBuffer, no need to "complete" explicitly
    // as this will be done implicitly by free()
    //
    IOReturn prepareDataBuffer();
    
    //
    // completes the request, can't be called inside callback!
    //
    void CompleteAccessDenied();
    
    dld_classic_rights_t GetCdbRequestedAccess();
    
    //
    // for the following functions definitions see the Apple's SCSITask definition
    // in the DLDSCSITask.cpp, these functions is a one to one mapping of the functions
    // from Apple's SCSITask
    //
    bool                GetCommandDescriptorBlock( __inout SCSICommandDescriptorBlock * cdbData );
    bool                SetServiceResponse( __in SCSIServiceResponse serviceResponse );
    bool                SetTaskStatus ( __in SCSITaskStatus newTaskStatus );
    UInt8               GetCommandDescriptorBlockSize();
    SCSIServiceResponse GetServiceResponse();
    SCSITaskStatus      GetTaskStatus();
    UInt64              GetRealizedDataTransferCount();
    UInt64              GetRequestedDataTransferCount();
    UInt64              GetDataBufferOffset();
    void                TaskCompletedNotification();
    IOMemoryDescriptor* GetDataBuffer();
};

//--------------------------------------------------------------------

#endif//DLDSCSITASK_H

