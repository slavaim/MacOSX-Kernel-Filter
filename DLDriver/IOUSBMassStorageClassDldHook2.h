/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DEVICELOCKIOUSBMASSSTORAGEHOOK2_H
#define _DEVICELOCKIOUSBMASSSTORAGEHOOK2_H

#include <IOKit/usb/IOUSBMassStorageClass.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

class IOUSBMassStorageClassDldHook : public IOService
{
    OSDeclareDefaultStructors( IOUSBMassStorageClassDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOUSBMassStorageClassDldHook, IOService )
    
public:
    
    ////////////////////////////////////////////////////////
    //
    // hooking functions declaration
    //
    /////////////////////////////////////////////////////////
    
protected:
    
    //
	// The SendSCSICommand function will take a SCSITask Object and transport
	// it across the physical wire(s) to the device
    //
	virtual bool    		SendSCSICommand( 	
                                            SCSITaskIdentifier 		request, 
                                            SCSIServiceResponse *	serviceResponse,
                                            SCSITaskStatus		*	taskStatus );
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
private:
    
    bool                    EnterInDebuger;
};

#endif//_DEVICELOCKIOUSBMASSSTORAGEHOOK2_H