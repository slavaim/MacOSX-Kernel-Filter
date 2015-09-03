/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */


#if 0

#include "IOUSBMassStorageClassDldHook2.h"

//--------------------------------------------------------------------

#define super IOService

OSDefineMetaClassAndStructors( IOUSBMassStorageClassDldHook, IOService )
DldDefineCommonIOServiceHookFunctionsAndStructors( IOUSBMassStorageClassDldHook, IOService )
DldDefineCommonIOServiceHook_HookObjectInt( IOUSBMassStorageClassDldHook, IOService )
DldDefineCommonIOServiceHook_UnHookObjectInt( IOUSBMassStorageClassDldHook, IOService )

//--------------------------------------------------------------------

bool
IOUSBMassStorageClassDldHook::SendSCSICommand( 	
                                              SCSITaskIdentifier 		request, 
                                              SCSIServiceResponse *	serviceResponse,
                                              SCSITaskStatus		*	taskStatus )
/*
 this is a hook, so "this" is an object of the IOUSBMassStorageClass class
 */
{
    if( IOUSBMassStorageClassDldHook::ClassInstance->EnterInDebuger ){
        
        __asm__ volatile( "int $0x3" );
    }
    
    //return super::SendSCSICommand( request, serviceResponse, taskStatus );
}

//--------------------------------------------------------------------

class IOUSBMassStorageClassDldHook3 : public IOUSBMassStorageClass{
    
    friend class IOUSBMassStorageClassDldHook;
    
protected:
	virtual bool    		SendSCSICommand( 	
                                            SCSITaskIdentifier 		request, 
                                            SCSIServiceResponse *	serviceResponse,
                                            SCSITaskStatus		*	taskStatus );
    
    virtual void PureVirtual() = 0;
};

void
IOUSBMassStorageClassDldHook::InitMembers()
{
    __asm__ volatile( "int $0x3" );
    
    DldInitMembers_Enter( IOUSBMassStorageClassDldHook, IOService )
    
    DldInitMembers_AddCommonHookedFunctionVtableIndices( IOUSBMassStorageClassDldHook, IOService )
    DldInitMembers_AddHookedFunctionVtableIndexForSuperClass( IOUSBMassStorageClassDldHook, SendSCSICommand, IOUSBMassStorageClassDldHook3 )
    // DldInitMembers_AddHookedFunctionVtableIndexForDerivedClass( IOUSBMassStorageClassDldHook, SendSCSICommand, IOUSBMassStorageClass )
    
    this->EnterInDebuger = false;
    
    DldInitMembers_Exit( IOUSBMassStorageClassDldHook, IOService )
}

//--------------------------------------------------------------------

#endif // 0