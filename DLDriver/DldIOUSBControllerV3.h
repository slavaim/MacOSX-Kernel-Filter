/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDIOUSBCONTROLLERV3_H
#define DLDIOUSBCONTROLLERV3_H

#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBHubDevice.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

class DldIOUSBControllerV3: public DldHookerBaseInterface{
    
public:
    
    //
    // IOUSBControllerV3 methods
    //
    virtual IOReturn 		Read(IOUSBControllerV3*   usbController, IOMemoryDescriptor *buffer,
                                 USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                 IOUSBCompletion *completion, UInt32 noDataTimeout,
                                 UInt32 completionTimeout, IOByteCount reqCount);
    
    virtual IOReturn 		Write(IOUSBControllerV3*   usbController, IOMemoryDescriptor *buffer,
                                  USBDeviceAddress address, IOUSBController::Endpoint *endpoint,
                                  IOUSBCompletion *completion, UInt32 noDataTimeout,
                                  UInt32 completionTimeout, IOByteCount reqCount);
    
    virtual IOReturn		IsocIO(IOUSBControllerV3*   usbController, IOMemoryDescriptor *buffer,
                                   UInt64 frameStart, UInt32 numFrames,
                                   IOUSBIsocFrame *frameList, USBDeviceAddress address,
                                   IOUSBController::Endpoint *endpoint, IOUSBIsocCompletion *completion );
    
    virtual IOReturn		IsocIO(IOUSBControllerV3*   usbController, IOMemoryDescriptor *buffer,
                                   UInt64 frameStart, UInt32 numFrames,
                                   IOUSBLowLatencyIsocFrame *frameList, USBDeviceAddress address,
                                   IOUSBController::Endpoint *endpoint, IOUSBLowLatencyIsocCompletion *completion,
                                   UInt32 updateFrequency );	
    
    //
    // IOUSBControllerV2 methods
    //
    virtual IOReturn		ReadV2(IOUSBControllerV3*   usbController, IOMemoryDescriptor *buffer,
                                   USBDeviceAddress	address, IOUSBController::Endpoint *endpoint,
                                   IOUSBCompletionWithTimeStamp *completion, UInt32 noDataTimeout,
                                   UInt32	completionTimeout, IOByteCount reqCount);
	
    
};

#endif//DLDIOUSBCONTROLLERV3_H