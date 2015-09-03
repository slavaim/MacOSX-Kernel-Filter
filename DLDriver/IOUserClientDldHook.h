/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */


#ifndef IOUSERCLIENTDLDHOOK_H
#define IOUSERCLIENTDLDHOOK_H

#include "DldCommon.h"
#include "DldHookerCommonClass.h"
#include "DldHookerCommonClass2.h"
#include "DldKernAuthorization.h"


template<DldInheritanceDepth Depth>
class IOUserClientDldHook : public DldHookerBaseInterface
{
    /////////////////////////////////////////////
    //
    // start of the required declarations
    //
    /////////////////////////////////////////////
public:
    enum{
        kDld_getExternalMethodForIndex_hook = 0x0,
        kDld_getExternalAsyncMethodForIndex_hook,
        kDld_getTargetAndMethodForIndex_hook,
        kDld_getAsyncTargetAndMethodForIndex_hook,
        kDld_getExternalTrapForIndex_hook,
        kDld_getTargetAndTrapForIndex_hook,
        kDld_externalMethod_hook,
        kDld_clientMemoryForType_hook,
        kDld_registerNotificationPort1_hook,//(mach_port_t port, UInt32 type, io_user_reference_t refCon)
        kDld_registerNotificationPort2_hook,//(mach_port_t port, UInt32 type, UInt32 refCon )
        kDld_getNotificationSemaphore_hook,
        kDld_NumberOfAddedHooks
    };
    
    friend class DldIOKitHookEngine;
    friend class DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>;
    
    static const char* fGetHookedClassName(){ return "IOUserClient"; };
    DldDeclareGetClassNameFunction();
    
protected:
    
    static DldInheritanceDepth fGetInheritanceDepth(){ return Depth; };
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*   mHookerCommon2;
    
protected:
    static IOUserClientDldHook<Depth>* newWithDefaultSettings();
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

    // Old methods for accessing method vector backward compatiblility only
    virtual IOExternalMethod * getExternalMethodForIndex_hook( UInt32 index );
    
    virtual IOExternalAsyncMethod * getExternalAsyncMethodForIndex_hook( UInt32 index );
    
    // Methods for accessing method vector.
    virtual IOExternalMethod * getTargetAndMethodForIndex_hook( IOService ** targetP, UInt32 index );
    
    virtual IOExternalAsyncMethod * getAsyncTargetAndMethodForIndex_hook( IOService ** targetP, UInt32 index );
    
    // Methods for accessing trap vector - old and new style
    virtual IOExternalTrap * getExternalTrapForIndex_hook( UInt32 index );
    
    virtual IOExternalTrap * getTargetAndTrapForIndex_hook( IOService **targetP, UInt32 index );
    
    virtual IOReturn externalMethod_hook( uint32_t selector, IOExternalMethodArguments * arguments,
                                    IOExternalMethodDispatch * dispatch = 0, OSObject * target = 0, void * reference = 0 );
    
    virtual IOReturn registerNotificationPort1_hook(mach_port_t port, UInt32 type, io_user_reference_t refCon);
    
    virtual IOReturn registerNotificationPort2_hook(mach_port_t port, UInt32 type, UInt32 refCon );
    
    virtual IOReturn clientMemoryForType_hook( UInt32 type,
                                               IOOptionBits * options,
                                               IOMemoryDescriptor ** memory );
    
    virtual IOReturn getNotificationSemaphore_hook( UInt32 notification_type,
                                                    semaphore_t * semaphore );
    
    ////////////////////////////////////////////////////////
    //
    // end of hooking functions declaration
    //
    /////////////////////////////////////////////////////////
    
private:
    IOReturn  checkAndLogUserClientAccess( __in DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2 );
    
};

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOUserClientDldHook<Depth>*
IOUserClientDldHook<Depth>::newWithDefaultSettings()
{
    IOUserClientDldHook<Depth>*  newObject;
    
    newObject = new IOUserClientDldHook<Depth>();
    if( !newObject )
        return NULL;
    
    newObject->mHookerCommon2 = new DldHookerCommonClass2< IOUserClientDldHook<Depth> ,IOUserClient >;
    assert( newObject->mHookerCommon2 );
    if( !newObject->mHookerCommon2 ){
        
        delete newObject;
        return NULL;
    }
    
    if( !newObject->init() ){
        
        assert( !"newObject->init() failed" );
        DBG_PRINT_ERROR(("newObject->init() failed"));
        
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
IOUserClientDldHook<Depth>::init()
{
    // super::init()
    
    assert( this->mHookerCommon2 );
    
    if( this->mHookerCommon2->init( this ) ){
        
        //
        // add new hooking functions specific for the class
        //
        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getExternalMethodForIndex_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getExternalMethodForIndex ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getExternalMethodForIndex_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getExternalAsyncMethodForIndex_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getExternalAsyncMethodForIndex ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getExternalAsyncMethodForIndex_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getTargetAndMethodForIndex_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getTargetAndMethodForIndex ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getTargetAndMethodForIndex_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getAsyncTargetAndMethodForIndex_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getAsyncTargetAndMethodForIndex ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getAsyncTargetAndMethodForIndex_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getExternalTrapForIndex_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getExternalTrapForIndex ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getExternalTrapForIndex_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getTargetAndTrapForIndex_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getTargetAndTrapForIndex ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getTargetAndTrapForIndex_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_externalMethod_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::externalMethod ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::externalMethod_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_registerNotificationPort1_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( IOReturn ( IOUserClient::* )(mach_port_t port, UInt32 type, io_user_reference_t refCon) ) &IOUserClient::registerNotificationPort ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::registerNotificationPort1_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_registerNotificationPort2_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( IOReturn ( IOUserClient::* )(mach_port_t port, UInt32 type, UInt32 refCon ) ) &IOUserClient::registerNotificationPort ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::registerNotificationPort2_hook ) );
        
        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_clientMemoryForType_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::clientMemoryForType ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::clientMemoryForType_hook ) );
        

        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOUserClientDldHook<Depth>::kDld_getNotificationSemaphore_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOUserClient::getNotificationSemaphore ),
                                                          DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUserClientDldHook<Depth>::getNotificationSemaphore_hook ) );
        
    } else {
        
        DBG_PRINT_ERROR(("this->mHookerCommon2.init( this ) failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
void
IOUserClientDldHook<Depth>::free()
{
    if( this->mHookerCommon2 )
        this->mHookerCommon2->free();
    //super::free();
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOReturn
IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(
    __in DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2
    )
{
    DldRequestedAccess requestedAccess = {0x0};
    requestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
    
    //
    // get the parent in the IOService tree, it happened that there might be orphan client objects in the system
    //
    IORegistryEntry*   parent;
    parent = (OSDynamicCast( IOUserClient, reinterpret_cast<OSObject*>(this) ))->getParentEntry( gIOServicePlane );
    //assert( parent );
    if( parent ){
        
        assert( OSDynamicCast( IOService, parent ) );
        if( NULL == OSDynamicCast( IOService, parent ) )
            return kIOReturnNotPermitted;
        
        //
        // check and log the access
        //
        if( kIOReturnSuccess != commonHooker2->fBaseCommonHooker()->checkAndLogUserClientAccess( OSDynamicCast( IOService, parent ), current_task(), &requestedAccess ) ){
            
            //
            // disable access
            //
            return kIOReturnNotPermitted;
        }
        
    }// end if( parent )
    
    //
    // allow access
    //
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

// Old methods for accessing method vector backward compatiblility only
template<DldInheritanceDepth Depth>
IOExternalMethod*
IOUserClientDldHook<Depth>::getExternalMethodForIndex_hook( UInt32 index )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return NULL;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return NULL;
    
    typedef IOExternalMethod* (*getExternalMethodForIndexFunc)( IOUserClient*, UInt32 index );
    
    getExternalMethodForIndexFunc Original = (getExternalMethodForIndexFunc)commonHooker2->fGetOriginalFunctionExternal(
                                                      (OSObject*)this,
                                                      IOUserClientDldHook<Depth>::kDld_getExternalMethodForIndex_hook );
    assert( Original );
    if( !Original )
        return NULL;
    
    return Original( reinterpret_cast<IOUserClient*>(this), index );
}

//--------------------------------------------------------------------

// Methods for accessing method vector
/* FYI an example of a call stack
 #0  IOUserClient::getTargetAndMethodForIndex (this=0x12bb0200, targetP=0x56d82c7c, index=2) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOUserClient.cpp:1063
 #1  0x71aa2fef in getTargetAndMethodForIndex_hook (this=0x12bb0200, targetP=0x56d82c7c, index=2) at IOUserClientDldHook.h:341
 #2  0x00562a44 in IOUserClient::externalMethod (this=0x12bb0200, selector=2, args=0x56d82cd0, dispatch=0x0, target=0x0, reference=0x0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOUserClient.cpp:4125
 #3  0x005633be in is_io_connect_method (connection=0x12bb0200, selector=2, scalar_input=0x9706290, scalar_inputCnt=0, inband_input=0x9706294 "", inband_inputCnt=0, ool_input=0, ool_input_size=0, scalar_output=0x12b165c8, scalar_outputCnt=0x12b165c4, inband_output=0x56d82e3c "\003", inband_outputCnt=0x56d83e3c, ool_output=0, ool_output_size=0x97062b4) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOUserClient.cpp:2716
 #4  0x0028421d in _Xio_connect_method (InHeadP=0x9706268, OutHeadP=0x12b165a0) at device/device_server.c:15466
 #5  0x0021d09c in ipc_kobject_server (request=0x9706200) at /SourceCache/xnu/xnu-1456.1.25/osfmk/kern/ipc_kobject.c:338
 #6  0x00210234 in ipc_kmsg_send (kmsg=0x9706200, option=0, send_timeout=0) at /SourceCache/xnu/xnu-1456.1.25/osfmk/ipc/ipc_kmsg.c:1354
 #7  0x00216497 in mach_msg_overwrite_trap (args=0x94ce1c8) at /SourceCache/xnu/xnu-1456.1.25/osfmk/ipc/mach_msg.c:505
 #8  0x002922ba in mach_call_munger64 (state=0x94ce1c4) at /SourceCache/xnu/xnu-1456.1.25/osfmk/i386/bsd_i386.c:830 
 */
template<DldInheritanceDepth Depth>
IOExternalMethod*
IOUserClientDldHook<Depth>::getTargetAndMethodForIndex_hook( IOService ** targetP, UInt32 index )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return NULL;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return NULL;
    
    typedef IOExternalMethod* (*getTargetAndMethodForIndexFunc)( IOUserClient*, IOService ** targetP, UInt32 index );
    
    getTargetAndMethodForIndexFunc Original = (getTargetAndMethodForIndexFunc)commonHooker2->fGetOriginalFunctionExternal(
                                                      (OSObject*)this,
                                                      IOUserClientDldHook<Depth>::kDld_getTargetAndMethodForIndex_hook );
    assert( Original );
    if( !Original )
        return NULL;
    
    return Original( reinterpret_cast<IOUserClient*>(this), targetP, index );
}


//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOExternalAsyncMethod*
IOUserClientDldHook<Depth>::getExternalAsyncMethodForIndex_hook( UInt32 index )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return NULL;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return NULL;
    
    typedef IOExternalAsyncMethod* (*getExternalAsyncMethodForIndexFunc)( IOUserClient*, UInt32 index );
    
    getExternalAsyncMethodForIndexFunc Original = (getExternalAsyncMethodForIndexFunc)commonHooker2->fGetOriginalFunctionExternal(
             (OSObject*)this,
             IOUserClientDldHook<Depth>::kDld_getExternalAsyncMethodForIndex_hook );
    assert( Original );
    if( !Original )
        return NULL;
    
    return Original( reinterpret_cast<IOUserClient*>(this), index );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOExternalAsyncMethod*
IOUserClientDldHook<Depth>::getAsyncTargetAndMethodForIndex_hook( IOService ** targetP, UInt32 index )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return NULL;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return NULL;
    
    typedef IOExternalAsyncMethod* (*getAsyncTargetAndMethodForIndexFunc)( IOUserClient*, IOService ** targetP, UInt32 index );
    
    getAsyncTargetAndMethodForIndexFunc Original = (getAsyncTargetAndMethodForIndexFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOUserClientDldHook<Depth>::kDld_getAsyncTargetAndMethodForIndex_hook );
    assert( Original );
    if( !Original )
        return NULL;
    
    return Original( reinterpret_cast<IOUserClient*>(this), targetP, index );
}

//--------------------------------------------------------------------

// Methods for accessing trap vector - old and new style
template<DldInheritanceDepth Depth>
IOExternalTrap*
IOUserClientDldHook<Depth>::getExternalTrapForIndex_hook( UInt32 index )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return NULL;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return NULL;
    
    typedef IOExternalTrap* (*getExternalTrapForIndexFunc)( IOUserClient*, UInt32 index );
    
    getExternalTrapForIndexFunc Original = (getExternalTrapForIndexFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOUserClientDldHook<Depth>::kDld_getExternalTrapForIndex_hook );
    assert( Original );
    if( !Original )
        return NULL;
    
    return Original( reinterpret_cast<IOUserClient*>(this), index );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOExternalTrap*
IOUserClientDldHook<Depth>::getTargetAndTrapForIndex_hook( IOService **targetP, UInt32 index )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return NULL;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return NULL;
    
    typedef IOExternalTrap* (*getTargetAndTrapForIndexFunc)( IOUserClient*, IOService ** targetP, UInt32 index );
    
    getTargetAndTrapForIndexFunc Original = (getTargetAndTrapForIndexFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOUserClientDldHook<Depth>::kDld_getTargetAndTrapForIndex_hook );
    assert( Original );
    if( !Original )
        return NULL;
    
    return Original( reinterpret_cast<IOUserClient*>(this), targetP, index );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOReturn
IOUserClientDldHook<Depth>::externalMethod_hook( uint32_t selector, IOExternalMethodArguments * arguments,
    IOExternalMethodDispatch * dispatch, OSObject * target, void * reference)
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnUnsupported;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return kIOReturnUnsupported;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return kIOReturnNotPermitted;
    
    typedef IOReturn (*externalMethodFunc)( IOUserClient*, uint32_t selector, IOExternalMethodArguments * arguments,
                                             IOExternalMethodDispatch * dispatch, OSObject * target, void * reference);
    
    externalMethodFunc Original = (externalMethodFunc)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOUserClientDldHook<Depth>::kDld_externalMethod_hook );
    assert( Original );
    if( !Original )
        return kIOReturnUnsupported;
    
    return Original( reinterpret_cast<IOUserClient*>(this), selector, arguments, dispatch, target, reference );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOReturn
IOUserClientDldHook<Depth>::registerNotificationPort1_hook(mach_port_t port, UInt32 type, io_user_reference_t refCon)
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnUnsupported;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return kIOReturnUnsupported;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return kIOReturnNotPermitted;
    
    typedef IOReturn (*registerNotificationPort1Func)( IOUserClient*, mach_port_t port, UInt32 type, io_user_reference_t refCon );
    
    registerNotificationPort1Func Original = (registerNotificationPort1Func)commonHooker2->fGetOriginalFunctionExternal(
        (OSObject*)this,
        IOUserClientDldHook<Depth>::kDld_registerNotificationPort1_hook );
    assert( Original );
    if( !Original )
        return kIOReturnUnsupported;
    
    return Original( reinterpret_cast<IOUserClient*>(this), port, type, refCon );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOReturn
IOUserClientDldHook<Depth>::registerNotificationPort2_hook(mach_port_t port, UInt32 type, UInt32 refCon )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnUnsupported;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return kIOReturnUnsupported;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return kIOReturnNotPermitted;
    
    typedef IOReturn (*registerNotificationPort2Func)( IOUserClient*, mach_port_t port, UInt32 type, UInt32 refCon );
    
    registerNotificationPort2Func Original = (registerNotificationPort2Func)commonHooker2->fGetOriginalFunctionExternal(
       (OSObject*)this,
       IOUserClientDldHook<Depth>::kDld_registerNotificationPort2_hook );
    assert( Original );
    if( !Original )
        return kIOReturnUnsupported;
    
    return Original( reinterpret_cast<IOUserClient*>(this), port, type, refCon );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOReturn
IOUserClientDldHook<Depth>::clientMemoryForType_hook( UInt32 type,
                                          IOOptionBits * options,
                                          IOMemoryDescriptor ** memory )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnUnsupported;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return kIOReturnUnsupported;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return kIOReturnNotPermitted;
    
    typedef IOReturn (*clientMemoryForTypeFunc)( IOUserClient*, UInt32 type,
                                                 IOOptionBits * options,
                                                 IOMemoryDescriptor ** memory );
    
    clientMemoryForTypeFunc Original = (clientMemoryForTypeFunc)commonHooker2->fGetOriginalFunctionExternal(
       (OSObject*)this,
       IOUserClientDldHook<Depth>::kDld_clientMemoryForType_hook );
    assert( Original );
    if( !Original )
        return kIOReturnUnsupported;
    
    return Original( reinterpret_cast<IOUserClient*>(this), type, options, memory );
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOReturn
IOUserClientDldHook<Depth>::getNotificationSemaphore_hook( UInt32 notification_type,
                                                           semaphore_t * semaphore )
{
    DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUserClientDldHook<Depth>,IOUserClient>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnUnsupported;
    
    assert( OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) );
    if( NULL == OSDynamicCast( IOUserClient, reinterpret_cast<IOService*>(this) ) )
        return kIOReturnUnsupported;
    
    if( kIOReturnSuccess != this->IOUserClientDldHook<Depth>::checkAndLogUserClientAccess(commonHooker2) )
        return kIOReturnNotPermitted;
    
    typedef IOReturn (*getNotificationSemaphoreFunc)( IOUserClient*, UInt32 notification_type,
                                                      semaphore_t * semaphore );
    
    getNotificationSemaphoreFunc Original = (getNotificationSemaphoreFunc)commonHooker2->fGetOriginalFunctionExternal(
       (OSObject*)this,
       IOUserClientDldHook<Depth>::kDld_getNotificationSemaphore_hook );
    assert( Original );
    if( !Original )
        return kIOReturnUnsupported;
    
    return Original( reinterpret_cast<IOUserClient*>(this), notification_type, semaphore );
}

//--------------------------------------------------------------------

#endif//IOUSERCLIENTDLDHOOK_H