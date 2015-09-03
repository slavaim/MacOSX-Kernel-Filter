/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDIOSCSIPROTOCOLINTERFACE_H
#define DLDIOSCSIPROTOCOLINTERFACE_H

#include <IOKit/scsi/IOSCSIProtocolInterface.h>
#include <IOKit/scsi/SCSITask.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//IOSCSIProtocolInterface

class DldIOSCSIProtocolInterface: public DldHookerBaseInterface{
    
public:
    
   	/*!
     @function ExecuteCommand
     @abstract Called to send a SCSITask and transport it across the physical wire(s) to the device.
     @discussion Called to send a SCSITask and transport it across the physical wire(s) to the device.
     Subclasses internal to IOSCSIArchitectureModelFamily will need to override this method. Third
     party subclasses should not need to override this method.
     @param request A valid SCSITaskIdentifier representing the task to transport across the wire(s).
     */
	virtual bool ExecuteCommand ( __in IOSCSIProtocolInterface* service, __inout SCSITaskIdentifier request );
   
};

#endif//DLDIOSCSIPROTOCOLINTERFACE_H