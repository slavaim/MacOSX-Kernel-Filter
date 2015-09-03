/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef IOSCSIPERIPHERALDEVICETYPE05DLDHOOK_H
#define IOSCSIPERIPHERALDEVICETYPE05DLDHOOK_H

#include "DldCommon.h"
#include "DldHookerCommonClass.h"
#include "DldHookerCommonClass2.h"
#include "DldIOSCSIMultimediaCommandsDevice.h"
#include "DldIOKitHookDictionaryEntry.h"
#include <IOKit/scsi/IOSCSIPeripheralDeviceType05.h>

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
class IOSCSIPeripheralDeviceType05DldHook: public DldDldIOSCSIMultimediaCommandsDevice{
    
    /////////////////////////////////////////////
    //
    // start of the required declarations
    //
    /////////////////////////////////////////////
    
public:
    enum{
        kDld_ExecuteCommand_hook = 0x0,
        kDld_NumberOfAddedHooks
    };
    
    friend class DldIOKitHookEngine;
    friend class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<Depth>,IOSCSIPeripheralDeviceType05>;
    
    static const char* fGetHookedClassName(){ return "IOSCSIPeripheralDeviceType05"; };
    DldDeclareGetClassNameFunction();
    
protected:
    
    static DldInheritanceDepth fGetInheritanceDepth(){ return Depth; };
    DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<Depth>,IOSCSIPeripheralDeviceType05>*   mHookerCommon2;
    
protected:
    static IOSCSIPeripheralDeviceType05DldHook<Depth>* newWithDefaultSettings();
    virtual bool init();
    virtual void free();
    
    /////////////////////////////////////////////
    //
    // end of the required declarations
    //
    /////////////////////////////////////////////
    
protected:
    
    // -- SCSI Protocol Interface Methods	--
    // The ExecuteCommand method will take a SCSI Task and transport
    // it across the physical wire(s) to the device
    virtual void		ExecuteCommand_hook ( SCSITaskIdentifier request );
    
};

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOSCSIPeripheralDeviceType05DldHook<Depth>*
IOSCSIPeripheralDeviceType05DldHook<Depth>::newWithDefaultSettings()
{
    IOSCSIPeripheralDeviceType05DldHook<Depth>*  newObject;
    
    newObject = new IOSCSIPeripheralDeviceType05DldHook<Depth>();
    if( !newObject )
        return NULL;
    
    newObject->mHookerCommon2 = new DldHookerCommonClass2< IOSCSIPeripheralDeviceType05DldHook<Depth> ,IOSCSIPeripheralDeviceType05 >;
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
IOSCSIPeripheralDeviceType05DldHook<Depth>::init()
{
    // super::init()
    
    assert( this->mHookerCommon2 );
    
    if( this->mHookerCommon2->init( this ) ){
        
        //
        // add new hooking functions specific for the class
        //
        this->mHookerCommon2->fAddHookingFunctionExternal(
                                                          IOSCSIPeripheralDeviceType05DldHook<Depth>::kDld_ExecuteCommand_hook,
                                                          DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOSCSIPeripheralDeviceType05::ExecuteCommand ),
                                                          DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<Depth>,IOSCSIPeripheralDeviceType05>::_ptmf2ptf( 
                                                            this,
                                                            (void (DldHookerBaseInterface::*)(void)) &IOSCSIPeripheralDeviceType05DldHook<Depth>::ExecuteCommand_hook ) );
        
        
    } else {
        
        DBG_PRINT_ERROR(("this->mHookerCommon2.init( this ) failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
void
IOSCSIPeripheralDeviceType05DldHook<Depth>::free()
{
    if( this->mHookerCommon2 )
        this->mHookerCommon2->free();
    //super::free();
}

//--------------------------------------------------------------------

//
// this is a hook, so "this" is of the IOSCSIPeripheralDeviceType05 type
//
template<DldInheritanceDepth Depth>
void
IOSCSIPeripheralDeviceType05DldHook<Depth>::ExecuteCommand_hook ( SCSITaskIdentifier request )
{
    DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<Depth>,IOSCSIPeripheralDeviceType05>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<Depth>,IOSCSIPeripheralDeviceType05>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 ){
        
        DBG_PRINT_ERROR(("fCommonHooker2() failed\n"));
        DldSCSITaskCompleteAccessDenied( request );
        return;
    }
    
    if( !commonHooker2->fGetContainingClassObject()->ExecuteCommand( reinterpret_cast<IOSCSIPeripheralDeviceType05*>(this), request ) ){
        
        //
        // the request has been completed
        //
        return;
    }
    
    typedef void (*ExecuteCommandFunc)( IOSCSIPeripheralDeviceType05*  __this,
                                        SCSITaskIdentifier request);
    
    ExecuteCommandFunc  Original = (ExecuteCommandFunc)commonHooker2->fGetOriginalFunctionExternal(
                                                                                               (OSObject*)this,
                                                                                               IOSCSIPeripheralDeviceType05DldHook<Depth>::kDld_ExecuteCommand_hook );
    assert( Original );
    Original( reinterpret_cast<IOSCSIPeripheralDeviceType05*>(this), request );
    return;
}

//--------------------------------------------------------------------

#endif//IOSCSIPERIPHERALDEVICETYPE05DLDHOOK_H