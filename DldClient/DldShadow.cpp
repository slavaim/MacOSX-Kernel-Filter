/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "DldShadow.h"

//--------------------------------------------------------------------

char*
shadowTypeToStr( __in DldShadowType shadowType )
{
    switch( shadowType ){
            
        case DldShadowTypeUnknown:
            return "DldShadowTypeUnknown";
        case DldShadowTypeFileWrite:
            return "DldShadowTypeFileWrite";
        case DldShadowTypeFilePageout:
            return "DldShadowTypeFilePageout";
        case DldShadowTypeFileOpen:
            return "DldShadowTypeFileOpen";
        case DldShadowTypeFileClose:
            return "DldShadowTypeFileClose";
        case DldShadowTypeFileReclaim:
            return "DldShadowTypeFileReclaim";
        case DldShadowTypeOperationCompletion:
            return "DldShadowTypeOperationCompletion";
        case DldShadowTypeStopShadow:
            return "DldShadowTypeStopShadow";
        default:
            return "Unknown";
    }
    
}

//--------------------------------------------------------------------

kern_return_t
DldSetShadowFile(
    __in io_connect_t    connection,
    __in char* name,
    __in off_t maxSize,
    __in UInt32 shadowFileID
    )
{
    kern_return_t                   kr;
    DldShadowFileDescriptorHeader*  fileDscrHeader;
    size_t                          nameLength;
    size_t                          fileDscrSize;
    
    nameLength = strlen( name ) + sizeof( '\0' );
    fileDscrSize = (size_t)sizeof( *fileDscrHeader ) + nameLength;// so it will be 1 byte more than required
    
    fileDscrHeader = (DldShadowFileDescriptorHeader*)malloc( fileDscrSize );
    if( !fileDscrHeader ){
        
        PRINT_ERROR(("fileDscrHeader = malloc( %i ) failed\n", (int)fileDscrSize));
        return kIOReturnNoMemory;
    }
    
    memset( fileDscrHeader, 0x0, fileDscrSize );
    
    fileDscrHeader->size = fileDscrSize;// so it will be 1 byte more than required
    fileDscrHeader->nameLength = nameLength;
    fileDscrHeader->maxFileSize = maxSize;
    fileDscrHeader->shadowFileID = shadowFileID;
    
    printf( " fileDscrHeader->maxFileSize = %llu\n", (long long)fileDscrHeader->maxFileSize );
    
    memcpy( fileDscrHeader->name, name, nameLength );
    
    //char   buf[1024];
    size_t notUsed = fileDscrSize;
    
    kr = IOConnectCallStructMethod( connection,
                                    kt_DldUserClientSetShadowFile,
                                    (const void*)fileDscrHeader,
                                    fileDscrSize,
                                    fileDscrHeader,
                                    &notUsed );
    if( kIOReturnSuccess != kr ){
        
        PRINT_ERROR(("IOConnectCallStructMethod failed with kr = 0x%X\n", kr));
        goto __exit;
    }
    
__exit:
    
    free( fileDscrHeader );
    return kr;
}


//--------------------------------------------------------------------

IOReturn
shadowNotificationHandler(io_connect_t connection)
{
    kern_return_t       kr; 
    DldDriverShadowNotification    sdata;
    uint32_t            dataSize;
    IODataQueueMemory  *queueMappedMemory;
    vm_size_t           queueMappedMemorySize;
#if !__LP64__ || defined(IOCONNECT_MAPMEMORY_10_6)
    vm_address_t        address = nil;
    vm_size_t           size = 0;
#else
    mach_vm_address_t   address = NULL;
    mach_vm_size_t      size = 0x0;
#endif
    mach_port_t         recvPort;
    
    //
    // allocate a Mach port to receive notifications from the IODataQueue
    //
    if (!(recvPort = IODataQueueAllocateNotificationPort())) {
        PRINT_ERROR(("failed to allocate notification port\n"));
        return kIOReturnError;
    }
    
    //
    // this will call registerNotificationPort() inside our user client class
    //
    kr = IOConnectSetNotificationPort(connection, kt_DldNotifyTypeShadow, recvPort, 0);
    if (kr != kIOReturnSuccess) {
        
        PRINT_ERROR(("failed to register notification port (%d)\n", kr));
        mach_port_destroy(mach_task_self(), recvPort);
        return kr;
    }
    
    //
    // this will call clientMemoryForType() inside our user client class
    //
    kr = IOConnectMapMemory( connection,
                            kt_DldNotifyTypeShadow,
                            mach_task_self(),
                            &address,
                            &size,
                            kIOMapAnywhere );
    if (kr != kIOReturnSuccess) {
        PRINT_ERROR(("failed to map memory (%d)\n",kr));
        mach_port_destroy(mach_task_self(), recvPort);
        return kr;
    }
    
    queueMappedMemory = (IODataQueueMemory *)address;
    queueMappedMemorySize = size;    
    
    printf("before the while loop\n");
    
    //bool first_iteration = true;
    
    while( kIOReturnSuccess == IODataQueueWaitForAvailableData(queueMappedMemory, recvPort) ) {       
        
        //first_iteration = false;
        printf("a buffer has been received\n");//do not call as it stalls the queue!
        
        while (IODataQueueDataAvailable(queueMappedMemory)) {   
            
            dataSize = sizeof(sdata);
            
            kr = IODataQueueDequeue(queueMappedMemory, &sdata, &dataSize);
            if (kr == kIOReturnSuccess) {
                
                if (*(UInt32 *)&sdata == kt_DldStopListeningToMessages)
                    goto exit;
                
                printf( "[SHADOW NOTIFICATION] ID=%i offset=%lli size=%lli type=%s\n ",
                       (int)sdata.GeneralNotification.shadowFileID,
                       sdata.GeneralNotification.offset,
                       (long long)sdata.GeneralNotification.dataSize,
                       shadowTypeToStr( sdata.GeneralNotification.shadowType ) );
                
            } else {
                PRINT_ERROR(("*** error in receiving data (%d)\n", kr));
            }
            
        }// end while
        
    }// end while
    
exit:
    
    kr = IOConnectUnmapMemory( connection,
                              kt_DldNotifyTypeShadow,
                              mach_task_self(),
                              address );
    if (kr != kIOReturnSuccess){
        PRINT_ERROR(("failed to unmap memory (%d)\n", kr));
    }
    
    mach_port_destroy(mach_task_self(), recvPort);
    
    return kr;
}

//--------------------------------------------------------------------

