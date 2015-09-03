/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _IOSERIALSTREAMSYNCDLDHOOK_H
#define _IOSERIALSTREAMSYNCDLDHOOK_H

#include <sys/types.h>
#include <sys/vm.h>
#include <sys/proc.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"
#include "DldHookerCommonClass2.h"
#include "DldKernAuthorization.h"
#include <IOKit/serial/IOSerialStreamSync.h>

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
class IOSerialStreamSyncDldHook : public DldHookerBaseInterface
{
    
    /////////////////////////////////////////////
    //
    // start of the required declarations
    //
    /////////////////////////////////////////////
public:
    enum{
        kDld_requestEvent_hook = 0x0,
        kDld_enqueueEvent_hook,
        kDld_dequeueEvent_hook,
        kDld_enqueueData_hook,
        kDld_dequeueData_hook,
        kDld_NumberOfAddedHooks
    };
    
    friend class DldIOKitHookEngine;
    friend class DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>;
    
    static const char* fGetHookedClassName(){ return "IOSerialStreamSync"; };
    DldDeclareGetClassNameFunction();
    
protected:
    
    static DldInheritanceDepth fGetInheritanceDepth(){ return Depth; };
    DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>*   mHookerCommon2;
    
protected:
    static IOSerialStreamSyncDldHook<Depth>* newWithDefaultSettings();
    virtual bool init();
    virtual void free();
    
    /////////////////////////////////////////////
    //
    // end of the required declarations
    //
    /////////////////////////////////////////////
    
    
    ////////////////////////////////////////////////////////
    //
    // hooking functions declaration
    //
    /////////////////////////////////////////////////////////
    
protected:
    
    
    /* requestEvent processes the specified event as an immediate request and
     *returns the results in data.  This is primarily used for getting link
     *status information and verifying baud rate and such.
     */
    virtual IOReturn requestEvent_hook(UInt32 event, UInt32 *data);
    
    /* enqueueEvent will place the specified event into the TX queue.  The
     *sleep argument allows the caller to specify the enqueueEvent's
     *behaviour when the TX queue is full.  If sleep is true, then this
     *method will sleep until the event is enqueued.  If sleep is false,
     *then enqueueEvent will immediatly return kIOReturnNoResources.
     */
    virtual IOReturn enqueueEvent_hook(UInt32 event, UInt32 data, bool sleep);
    
    /* dequeueEvent will remove the oldest event from the RX queue and return
     *it in event & data.  The sleep argument defines the behaviour if the RX
     *queue is empty.  If sleep is true, then this method will sleep until an
     *event is available.  If sleep is false, then an EOQ event will be
     *returned.  In either case kIOReturnSuccess is returned.
     */
    virtual IOReturn dequeueEvent_hook(UInt32 *event, UInt32 *data, bool sleep);
    
    /* enqueueData will attempt to copy data from the specified buffer to the
     *TX queue as a sequence of VALID_DATA events.  The argument bufferSize
     *specifies the number of bytes to be sent.  The actual number of bytes
     *transferred is returned in transferCount.  If sleep is true, then this
     *method will sleep until all bytes can be transferred.  If sleep is
     *false, then as many bytes as possible will be copied to the TX queue.
     */
    virtual IOReturn enqueueData_hook(UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep );
    
    /* dequeueData will attempt to copy data from the RX queue to the specified
     *buffer.  No more than bufferSize VALID_DATA events will be transferred.
     *In other words, copying will continue until either a non-data event is
     *encountered or the transfer buffer is full.  The actual number of bytes
     *transferred is returned in transferCount.
     *
     *The sleep semantics of this method are slightly more complicated than
     *other methods in this API:  Basically, this method will continue to
     *sleep until either minCount characters have been received or a non
     *data event is next in the RX queue.  If minCount is zero, then this
     *method never sleeps and will return immediatly if the queue is empty.
     *
     *The latency parameter specifies the maximum amount of time that should
     *pass after the first character is available before the routine returns.
     *This allows the caller to specify a 'packet' timeout.  The unit of the
     *latency parameter is microseconds, though the exact delay may vary
     *depending on the granularity of the timeout services available to the
     *driver.
     */
    virtual IOReturn dequeueData_hook(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
    bool isAccessAllowed( __in kauth_ace_rights_t requestedAccess, __in IOSerialStreamSync* service );
};

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOSerialStreamSyncDldHook<Depth>*
IOSerialStreamSyncDldHook<Depth>::newWithDefaultSettings()
{
    IOSerialStreamSyncDldHook<Depth>*  newObject;
    
    newObject = new IOSerialStreamSyncDldHook<Depth>();
    if( !newObject )
        return NULL;
    
    newObject->mHookerCommon2 = new DldHookerCommonClass2< IOSerialStreamSyncDldHook<Depth> ,IOSerialStreamSync >;
    assert( newObject->mHookerCommon2 );
    if( !newObject->mHookerCommon2 ){
        
        delete newObject;
        return NULL;
    }
    
    if( !newObject->init() ){
        
        assert( !"newObject->init() failed" );
        DBG_PRINT_ERROR(("newObject->init() failed\n"));
        
        newObject->free();
        delete newObject->mHookerCommon2;
        delete newObject;
        
        return NULL;
    }
    
    return newObject;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
bool
IOSerialStreamSyncDldHook<Depth>::init()
{
    // super::init()
    
    assert( this->mHookerCommon2 );
    
    if( this->mHookerCommon2->init( this ) ){
        
        //
        // add new hooking functions specific for the class
        //
        this->mHookerCommon2->fAddHookingFunctionExternal(
           IOSerialStreamSyncDldHook<Depth>::kDld_requestEvent_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOSerialStreamSync::requestEvent ),
           DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::_ptmf2ptf( 
               this, (void (DldHookerBaseInterface::*)(void)) &IOSerialStreamSyncDldHook<Depth>::requestEvent_hook ) );
        
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
           IOSerialStreamSyncDldHook<Depth>::kDld_enqueueEvent_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOSerialStreamSync::enqueueEvent ),
           DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::_ptmf2ptf( 
               this, (void (DldHookerBaseInterface::*)(void)) &IOSerialStreamSyncDldHook<Depth>::enqueueEvent_hook ) );
        
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
           IOSerialStreamSyncDldHook<Depth>::kDld_dequeueEvent_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOSerialStreamSync::dequeueEvent ),
           DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::_ptmf2ptf( 
              this, (void (DldHookerBaseInterface::*)(void)) &IOSerialStreamSyncDldHook<Depth>::dequeueEvent_hook ) );
        
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
           IOSerialStreamSyncDldHook<Depth>::kDld_enqueueData_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOSerialStreamSync::enqueueData ),
           DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::_ptmf2ptf( 
              this, (void (DldHookerBaseInterface::*)(void)) &IOSerialStreamSyncDldHook<Depth>::enqueueData_hook ) );
        
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
           IOSerialStreamSyncDldHook<Depth>::kDld_dequeueData_hook,
           DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOSerialStreamSync::dequeueData ),
           DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::_ptmf2ptf( 
              this, (void (DldHookerBaseInterface::*)(void)) &IOSerialStreamSyncDldHook<Depth>::dequeueData_hook ) );
        
        
    } else {
        
        DBG_PRINT_ERROR(("this->mHookerCommon2.init( this ) failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
void
IOSerialStreamSyncDldHook<Depth>::free()
{
    if( this->mHookerCommon2 )
        this->mHookerCommon2->free();
    //super::free();
}

//--------------------------------------------------------------------

/* requestEvent processes the specified event as an immediate request and
 *returns the results in data.  This is primarily used for getting link
 *status information and verifying baud rate and such.
 */
template<DldInheritanceDepth Depth>
IOReturn
IOSerialStreamSyncDldHook<Depth>::requestEvent_hook(UInt32 event, UInt32 *data)
/*
 this is a hook, so "this" is an object of the IOSerialStreamSync class
 */
{
    DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnError;
    
    {
        /*
        DldAccessCheckParam param;
        
        bzero( &param, sizeof( param ) );
        
        param.userSelectionFlavor = kDefaultUserSelectionFlavor;// change to logged user
        param.aclType             = kDldAclTypeSecurity;
        param.checkParentType     = true;
        param.service             = OSDynamicCast( IOService, (OSObject*)this );// this is for the HOOKED object's class
        assert( param.service );
        param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
#if defined(LOG_ACCESS)
        param.sourceFunction      = __PRETTY_FUNCTION__;
        param.sourceFile          = __FILE__;
        param.sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
        
        ::DldAcquireResources( &param );
        ::isAccessAllowed( &param );
        ::DldReleaseResources( &param );
        
        if( param.output.access.result[ DldFullTypeFlavor ].disable ||
            param.output.access.result[ DldMajorTypeFlavor ].disable ||
            param.output.access.result[ DldParentTypeFlavor ].disable )
            return kIOReturnError;
         */
        errno_t             error;
        DldRequestedAccess  requestedAccess = { 0x0 };
        
        requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
        
        assert( OSDynamicCast( IOService, (OSObject*)this ) );
        error = commonHooker2->fBaseCommonHooker()->checkAndLogUserClientAccess( OSDynamicCast( IOService, (OSObject*)this ),
                                                                                 proc_pid( current_proc() ),
                                                                                 NULL, // use a current user
                                                                                 &requestedAccess );
        
        if( kIOReturnSuccess != error )
            return error;
    }
    
    typedef bool(*requestEventFunc)( IOSerialStreamSync*, UInt32 event, UInt32 *data);
    
    requestEventFunc   Original;
    Original = (requestEventFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOSerialStreamSyncDldHook<Depth>::kDld_requestEvent_hook );
    
    assert( Original );
    if( !Original )
        return kIOReturnError;
    
    return Original( reinterpret_cast<IOSerialStreamSync*>(this), event, data );
}

//--------------------------------------------------------------------

/* enqueueEvent will place the specified event into the TX queue.  The
 *sleep argument allows the caller to specify the enqueueEvent's
 *behaviour when the TX queue is full.  If sleep is true, then this
 *method will sleep until the event is enqueued.  If sleep is false,
 *then enqueueEvent will immediatly return kIOReturnNoResources.
 */
template<DldInheritanceDepth Depth>
IOReturn
IOSerialStreamSyncDldHook<Depth>::enqueueEvent_hook(UInt32 event, UInt32 data, bool sleep)
{
    DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnError;
    
    {
        /*
        DldAccessCheckParam param;
        
        bzero( &param, sizeof( param ) );
        
        param.userSelectionFlavor = kDefaultUserSelectionFlavor;// change to logged user
        param.aclType             = kDldAclTypeSecurity;
        param.checkParentType     = true;
        param.service             = OSDynamicCast( IOService, (OSObject*)this );// this is for the HOOKED object's class
        assert( param.service );
        param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
#if defined(LOG_ACCESS)
        param.sourceFunction      = __PRETTY_FUNCTION__;
        param.sourceFile          = __FILE__;
        param.sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
        
        ::DldAcquireResources( &param );
        ::isAccessAllowed( &param );
        ::DldReleaseResources( &param );
        
        if( param.output.access.result[ DldFullTypeFlavor ].disable ||
           param.output.access.result[ DldMajorTypeFlavor ].disable ||
           param.output.access.result[ DldParentTypeFlavor ].disable )
            return kIOReturnError;
         */
        errno_t             error;
        DldRequestedAccess  requestedAccess = { 0x0 };
        
        requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
        
        assert( OSDynamicCast( IOService, (OSObject*)this ) );
        error = commonHooker2->fBaseCommonHooker()->checkAndLogUserClientAccess( OSDynamicCast( IOService, (OSObject*)this ),
                                                                                 proc_pid( current_proc() ),
                                                                                 NULL, // use a current user
                                                                                 &requestedAccess );
        
        if( kIOReturnSuccess != error )
            return error;
    }
    
    typedef bool(*enqueueEventFunc)( IOSerialStreamSync*, UInt32 event, UInt32 data, bool sleep);
    
    enqueueEventFunc    Original;
    
    Original = (enqueueEventFunc)commonHooker2->fGetOriginalFunctionExternal(
       (OSObject*)this,
       IOSerialStreamSyncDldHook<Depth>::kDld_enqueueEvent_hook );
    
    assert( Original );
    if( !Original )
        return kIOReturnError;
    
    return Original( reinterpret_cast<IOSerialStreamSync*>(this), event, data, sleep );
}

//--------------------------------------------------------------------

/* dequeueEvent will remove the oldest event from the RX queue and return
 *it in event & data.  The sleep argument defines the behaviour if the RX
 *queue is empty.  If sleep is true, then this method will sleep until an
 *event is available.  If sleep is false, then an EOQ event will be
 *returned.  In either case kIOReturnSuccess is returned.
 */
template<DldInheritanceDepth Depth>
IOReturn
IOSerialStreamSyncDldHook<Depth>::dequeueEvent_hook(UInt32 *event, UInt32 *data, bool sleep)
{
    DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnError;    
    
    {
        /*
        DldAccessCheckParam param;
        
        bzero( &param, sizeof( param ) );
        
        param.userSelectionFlavor = kDefaultUserSelectionFlavor;// change to logged user
        param.aclType             = kDldAclTypeSecurity;
        param.checkParentType     = true;
        param.service             = OSDynamicCast( IOService, (OSObject*)this );// this is for the HOOKED object's class
        assert( param.service );
        param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
#if defined(LOG_ACCESS)
        param.sourceFunction      = __PRETTY_FUNCTION__;
        param.sourceFile          = __FILE__;
        param.sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
        
        ::DldAcquireResources( &param );
        ::isAccessAllowed( &param );
        ::DldReleaseResources( &param );
        
        if( param.output.access.result[ DldFullTypeFlavor ].disable ||
           param.output.access.result[ DldMajorTypeFlavor ].disable ||
           param.output.access.result[ DldParentTypeFlavor ].disable )
            return kIOReturnError;
         */
        errno_t             error;
        DldRequestedAccess  requestedAccess = { 0x0 };
        
        requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
        
        assert( OSDynamicCast( IOService, (OSObject*)this ) );
        error = commonHooker2->fBaseCommonHooker()->checkAndLogUserClientAccess( OSDynamicCast( IOService, (OSObject*)this ),
                                                                                 proc_pid( current_proc() ),
                                                                                 NULL, // use a current user
                                                                                 &requestedAccess );
        
        if( kIOReturnSuccess != error )
            return error;
    }
    
    typedef bool(*dequeueEventFunc)( IOSerialStreamSync*, UInt32 *event, UInt32 *data, bool sleep);
    
    dequeueEventFunc Original = (dequeueEventFunc)commonHooker2->fGetOriginalFunctionExternal(
       (OSObject*)this,
       IOSerialStreamSyncDldHook<Depth>::kDld_dequeueEvent_hook );
    
    assert( Original );
    if( !Original )
        return kIOReturnError;
    
    return Original( reinterpret_cast<IOSerialStreamSync*>(this), event, data, sleep );
}

//--------------------------------------------------------------------

/* enqueueData will attempt to copy data from the specified buffer to the
 *TX queue as a sequence of VALID_DATA events.  The argument bufferSize
 *specifies the number of bytes to be sent.  The actual number of bytes
 *transferred is returned in transferCount.  If sleep is true, then this
 *method will sleep until all bytes can be transferred.  If sleep is
 *false, then as many bytes as possible will be copied to the TX queue.
 */
template<DldInheritanceDepth Depth>
IOReturn
IOSerialStreamSyncDldHook<Depth>::enqueueData_hook(UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep )
{
    DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnError;
    
    {
        /*
        DldAccessCheckParam param;
        
        bzero( &param, sizeof( param ) );
        
        param.userSelectionFlavor = kDefaultUserSelectionFlavor;// change to logged user
        param.aclType             = kDldAclTypeSecurity;
        param.checkParentType     = true;
        param.service             = OSDynamicCast( IOService, (OSObject*)this );// this is for the HOOKED object's class
        assert( param.service );
        param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
#if defined(LOG_ACCESS)
        param.sourceFunction      = __PRETTY_FUNCTION__;
        param.sourceFile          = __FILE__;
        param.sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
        
        ::DldAcquireResources( &param );
        ::isAccessAllowed( &param );
        ::DldReleaseResources( &param );
        
        if( param.output.access.result[ DldFullTypeFlavor ].disable ||
            param.output.access.result[ DldMajorTypeFlavor ].disable ||
            param.output.access.result[ DldParentTypeFlavor ].disable )
                return kIOReturnError;
         */
        
        errno_t             error;
        DldRequestedAccess  requestedAccess = { 0x0 };
        
        requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
        
        assert( OSDynamicCast( IOService, (OSObject*)this ) );
        error = commonHooker2->fBaseCommonHooker()->checkAndLogUserClientAccess( OSDynamicCast( IOService, (OSObject*)this ),
                                                                                 proc_pid( current_proc() ),
                                                                                 NULL, // use a current user
                                                                                 &requestedAccess );
        
        if( kIOReturnSuccess != error )
            return error;
    }
    
    typedef bool(*enqueueDataFunc)( IOSerialStreamSync*, UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep);
    
    enqueueDataFunc Original = (enqueueDataFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOSerialStreamSyncDldHook<Depth>::kDld_enqueueData_hook );
    
    assert( Original );
    if( !Original )
        return kIOReturnError;
    
    return Original( reinterpret_cast<IOSerialStreamSync*>(this), buffer, size, count, sleep );
}

//--------------------------------------------------------------------

/* dequeueData will attempt to copy data from the RX queue to the specified
 *buffer.  No more than bufferSize VALID_DATA events will be transferred.
 *In other words, copying will continue until either a non-data event is
 *encountered or the transfer buffer is full.  The actual number of bytes
 *transferred is returned in transferCount.
 *
 *The sleep semantics of this method are slightly more complicated than
 *other methods in this API:  Basically, this method will continue to
 *sleep until either minCount characters have been received or a non
 *data event is next in the RX queue.  If minCount is zero, then this
 *method never sleeps and will return immediatly if the queue is empty.
 *
 *The latency parameter specifies the maximum amount of time that should
 *pass after the first character is available before the routine returns.
 *This allows the caller to specify a 'packet' timeout.  The unit of the
 *latency parameter is microseconds, though the exact delay may vary
 *depending on the granularity of the timeout services available to the
 *driver.
 */
template<DldInheritanceDepth Depth>
IOReturn
IOSerialStreamSyncDldHook<Depth>::dequeueData_hook(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
    DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOSerialStreamSyncDldHook<Depth>,IOSerialStreamSync>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnError;
    
    {
        /*
        DldAccessCheckParam param;
        
        bzero( &param, sizeof( param ) );
        
        param.userSelectionFlavor = kDefaultUserSelectionFlavor;// change to logged user
        param.aclType             = kDldAclTypeSecurity;
        param.checkParentType     = true;
        param.service             = OSDynamicCast( IOService, (OSObject*)this );// this is for the HOOKED object's class
        assert( param.service );
        param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
#if defined(LOG_ACCESS)
        param.sourceFunction      = __PRETTY_FUNCTION__;
        param.sourceFile          = __FILE__;
        param.sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
        
        ::DldAcquireResources( &param );
        ::isAccessAllowed( &param );
        ::DldReleaseResources( &param );
        
        if( param.output.access.result[ DldFullTypeFlavor ].disable ||
           param.output.access.result[ DldMajorTypeFlavor ].disable ||
           param.output.access.result[ DldParentTypeFlavor ].disable )
            return kIOReturnError;
        */
        errno_t             error;
        DldRequestedAccess  requestedAccess = { 0x0 };
        
        requestedAccess.winRequestedAccess = DEVICE_READ | DEVICE_WRITE;
        
        assert( OSDynamicCast( IOService, (OSObject*)this ) );
        error = commonHooker2->fBaseCommonHooker()->checkAndLogUserClientAccess( OSDynamicCast( IOService, (OSObject*)this ),
                                                                                 proc_pid( current_proc() ),
                                                                                 NULL, // use a current user
                                                                                 &requestedAccess );
        
        if( kIOReturnSuccess != error )
            return error;
    }
    
    typedef bool(*dequeueDataFunc)(IOSerialStreamSync*, UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);
    
    dequeueDataFunc Original = (dequeueDataFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOSerialStreamSyncDldHook<Depth>::kDld_dequeueData_hook );
    
    assert( Original );
    if( !Original )
        return kIOReturnError;
    
    return Original( reinterpret_cast<IOSerialStreamSync*>(this), buffer, size, count, min );
}

//--------------------------------------------------------------------

#endif /* !_IOSERIALSTREAMSYNCDLDHOOK_H */