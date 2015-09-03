/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DEVICELOCKIOUSBMASSSTORAGEHOOK_H
#define _DEVICELOCKIOUSBMASSSTORAGEHOOK_H

#include <IOKit/usb/IOUSBMassStorageClass.h>
#include "DldCommon.h"
#include "DldHookerCommonClass2.h"

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
class IOUSBMassStorageClassDldHook2 : public DldHookerBaseInterface
{

    /////////////////////////////////////////////
    //
    // start of the required declarations
    //
    /////////////////////////////////////////////
public:
    enum{
        kDld_SendSCSICommand_hook = 0x0,
        kDld_NumberOfAddedHooks
    };
    
    friend class DldIOKitHookEngine;
    friend class DldHookerCommonClass2<IOUSBMassStorageClassDldHook2<Depth>,IOUSBMassStorageClass>;
    
    static const char* fGetHookedClassName(){ return "IOUSBMassStorageClass"; };
    DldDeclareGetClassNameFunction();
    
protected:
    
    static DldInheritanceDepth fGetInheritanceDepth(){ return Depth; };
    DldHookerCommonClass2<IOUSBMassStorageClassDldHook2<Depth>,IOUSBMassStorageClass>*   mHookerCommon2;
    
protected:
    static IOUSBMassStorageClassDldHook2<Depth>* newWithDefaultSettings();
    virtual bool init();
    virtual void free();
    
    /////////////////////////////////////////////
    //
    // end of the required declarations
    //
    /////////////////////////////////////////////
    
protected:
    
    //
    // The SendSCSICommand function will take a SCSITask Object and transport
	// it across the physical wire(s) to the device
    //
	virtual bool SendSCSICommand_hook( SCSITaskIdentifier 		request, 
                                       SCSIServiceResponse *	serviceResponse,
                                       SCSITaskStatus	   *	taskStatus );
    
};

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOUSBMassStorageClassDldHook2<Depth>*
IOUSBMassStorageClassDldHook2<Depth>::newWithDefaultSettings()
{
    IOUSBMassStorageClassDldHook2<Depth>*  newObject;
    
    newObject = new IOUSBMassStorageClassDldHook2<Depth>();
    if( !newObject )
        return NULL;
    
    newObject->mHookerCommon2 = new DldHookerCommonClass2< IOUSBMassStorageClassDldHook2<Depth> ,IOUSBMassStorageClass >;
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
IOUSBMassStorageClassDldHook2<Depth>::init()
{
    // super::init()
    
    assert( this->mHookerCommon2 );
    
    //
    // a virtual class is needed to get the SendSCSICommand's index as this
    // function is private the compiler doesn't resolve any reference to it
    // from non friend classes
    //
    class PureVirtualClass: public IOUSBMassStorageClass{
        
        friend class IOUSBMassStorageClassDldHook2<Depth>;
        
    protected:
        
        //
        // The SendSCSICommand function will take a SCSITask Object and transport
        // it across the physical wire(s) to the device
        //
        virtual bool SendSCSICommand( SCSITaskIdentifier 		request, 
                                     SCSIServiceResponse *	    serviceResponse,
                                     SCSITaskStatus	  *	    taskStatus );
        
        virtual void PureVirtual() = 0;
    };
    
    if( this->mHookerCommon2->init( this ) ){
        
        //
        // add new hooking functions specific for the class
        //
        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          kDld_SendSCSICommand_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &PureVirtualClass::SendSCSICommand ),
                                                          DldHookerCommonClass2<IOUSBMassStorageClassDldHook2<Depth>,IOUSBMassStorageClass>::_ptmf2ptf( this, (void (DldHookerBaseInterface::*)(void)) &IOUSBMassStorageClassDldHook2<Depth>::SendSCSICommand_hook ) );
        
        
    } else {
        
        DBG_PRINT_ERROR(("this->mHookerCommon2.init( this ) failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
void
IOUSBMassStorageClassDldHook2<Depth>::free()
{
    if( this->mHookerCommon2 )
        this->mHookerCommon2->free();
    //super::free();
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
bool
IOUSBMassStorageClassDldHook2<Depth>::SendSCSICommand_hook( 	
                                                           SCSITaskIdentifier 		request, 
                                                           SCSIServiceResponse *	serviceResponse,
                                                           SCSITaskStatus		*	taskStatus )
/*
 this is a hook, so "this" is an object of the IOUSBMassStorageClass class
 */
{
    
    DldHookerCommonClass2<IOUSBMassStorageClassDldHook2<Depth>,IOUSBMassStorageClass>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOUSBMassStorageClassDldHook2<Depth>,IOUSBMassStorageClass>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
#if defined(DBG)
    //__asm__ volatile( "int $0x3" );
#endif//DBG
    
    typedef bool(*SendSCSICommandFunc)(
                                       IOUSBMassStorageClass*  __this,
                                       SCSITaskIdentifier 	   request, 
                                       SCSIServiceResponse *   serviceResponse,
                                       SCSITaskStatus      *   taskStatus );
    
    SendSCSICommandFunc  OrigSendSCSICommand;
    OrigSendSCSICommand = (SendSCSICommandFunc)commonHooker2->fGetOriginalFunctionExternal(
                                                                                           (OSObject*)this,
                                                                                           IOUSBMassStorageClassDldHook2<Depth>::kDld_SendSCSICommand_hook );
    assert( OrigSendSCSICommand );
    if( !OrigSendSCSICommand )
        return false;
    
    return OrigSendSCSICommand( reinterpret_cast<IOUSBMassStorageClass*>(this), request, serviceResponse, taskStatus );
}

//--------------------------------------------------------------------

#endif//_DEVICELOCKIOUSBMASSSTORAGEHOOK_H