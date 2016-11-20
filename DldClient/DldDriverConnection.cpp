/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "DldDriverConnection.h"

//--------------------------------------------------------------------

//
// the returned connection must be closed by calling IOServiceClose
//
kern_return_t
DldOpenDlDriver(
    __out io_connect_t*    connection
    )
{
    kern_return_t   kr; 
    io_iterator_t   iterator;
    io_service_t    serviceObject;
    CFDictionaryRef classToMatch;
    
    setbuf(stdout, NULL);
    
    if (!(classToMatch = IOServiceMatching(DLDRIVER_IOKIT_CLASS))){
        PRINT_ERROR(("failed to create matching dictionary\n"));
        return kIOReturnError;
    }
    
    //
    // IOServiceGetMatchingServices consumes classToMatch reference
    //
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, classToMatch,
                                      &iterator);
    if (kr != kIOReturnSuccess){
        
        PRINT_ERROR(("failed to retrieve matching services\n"));
        return kr;
    }
    
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject){
        
        PRINT_ERROR(("DldDriver service not found\n"));
        return kIOReturnError;
    }
    
    kr = IOServiceOpen( serviceObject, mach_task_self(), kDldUserClientCookie, connection );
    IOObjectRelease(serviceObject);
    if (kr != kIOReturnSuccess){
        
        PRINT_ERROR(("failed to open DldDriver service, an error is %i \n", kr));
        return kr;
    }
    
    kr = IOConnectCallScalarMethod( *connection, kt_DldUserClientOpen, NULL, 0, NULL, NULL);
    //kr = IOConnectMethodScalarIScalarO(connection, kt_kVnodeWatcherUserClientOpen, 0, 0);
    if (kr != KERN_SUCCESS) {
        (void)IOServiceClose( *connection );
        PRINT_ERROR(("DldDriver service is busy\n"));
        return kr;
    }
    
    return kr;
    
}

//--------------------------------------------------------------------


