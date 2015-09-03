/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIODATAQUEUE_H
#define _DLDIODATAQUEUE_H

#include <IOKIt/IOLocks.h>
#include <IOKit/IODataQueue.h>
#include <IOKit/IODataQueueShared.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include <sys/wait.h>
#include "DldCommon.h"

/*
 DldIODataQueue objects are designed to be used in multiple producers / multiple consumers situation.
*/

/*!
 * @typedef IODataQueueEntry
 * @abstract Represents an entry within the data queue
 * @discussion This is a variable sized struct.  The data field simply represents the start of the data region.  The size of the data region is stored in the size field.  The whole size of the specific entry is the size of a UInt32 plus the size of the data region.  
 * @field size The size of the following data region.
 * @field data Represents the beginning of the data region.  The address of the data field is a pointer to the start of the data region.
 */
typedef struct _DldIODataQueueEntry{
    UInt32  dataSize;
    bool    waitingForHeadPropogation;
    bool    waitingForTailPropogation;
#if defined( DBG )
    #define DLD_QUEUE_SIGNATURE ((UInt32)0xFFFBBBAA)
    UInt32  signature;
    bool    intermediate;
#endif//DBG
    UInt64   data[1];//UInt64 type is used to get at least a natural pointer alignment for data
} DldIODataQueueEntry;

/*!
 * @typedef DldIODataQueueMemory
 * @abstract A struct mapping to the header region of a data queue.
 * @discussion This struct is variable sized.  The struct represents the data queue header information plus a pointer to the actual data queue itself.  The size of the struct is the combined size of the header fields (3 * sizeof(UInt32)) plus the actual size of the queue region.  This size is stored in the queueSize field.
 * @field queueSize The size of the queue region pointed to by the queue field.
 * @field head The location of the queue head.  This field is represented as a byte offset from the beginning of the queue memory region.
 * @field tail The location of the queue tail.  This field is represented as a byte offset from the beginning of the queue memory region.
 * @field queue Represents the beginning of the queue memory region.  The size of the region pointed to by queue is stored in the queueSize field.
 */
typedef struct _DldIODataQueueMemory {
    
    UInt32      queueSize;
    
    //
    // head and tail define the region 
    // from which the memory must not be allocated as
    // it contains data
    //
    UInt32      head;
    UInt32      tail;
   
    //
    // valid head and tail defines region inside the region
    // defined by head and tail, this region contains entries
    // with the data which won't be changed until the entry
    // is being removed, i.e. unmutable entries, beyound this
    // region the entries' data can be changed but the headers
    // remain valid
    //
    UInt32      validDataHead;
    UInt32      validDataTail;

    UInt32      waitersForHeadPropogation;
    UInt32      waitersForTailPropogation;
    
    DldIODataQueueEntry  queue[1];
    
} DldIODataQueueMemory;


/*!
 * @defined DLD_DATA_QUEUE_MEMORY_HEADER_SIZE Represents the size of the data queue memory header independent of the actual size of the queue data itself.  The total size of the queue memory is equal to this value plus the size of the queue appendix and the size of the queue data region which is stored in the queueSize field of IODataQueueMeory.
 */
#define DLD_DATA_QUEUE_MEMORY_HEADER_SIZE (sizeof(DldIODataQueueMemory) - sizeof(DldIODataQueueEntry))


/*!
 * @defined DLD_DATA_QUEUE_ENTRY_HEADER_SIZE Represents the size of the data queue entry header independent of the actual size of the data in the entry.  This is the overhead of each entry in the queue.  The total size of an entry is equal to this value plus the size stored in the entry's size field (in IODataQueueEntry).
 */
#define DLD_DATA_QUEUE_ENTRY_HEADER_SIZE ( (vm_size_t)(&((DldIODataQueueEntry*)NULL)->data[0]) )

//--------------------------------------------------------------------

class DldIODataQueue : public OSObject
{
    OSDeclareDefaultStructors( DldIODataQueue )
    
private:
    
    DldIODataQueueMemory*	dataQueue;
    IOLock*                 lock;
    
    bool initWithCapacity( __in UInt32 size );
    
protected:
    
    virtual void free();
    
public:
    
    //
    // always returns true
    //
    virtual bool waitForData();
    
    static DldIODataQueue* withCapacity( __in UInt32 size );
    
    void sendDataAvailableNotificationOneThread();
    
    void sendDataAvailableNotificationAllThreads();
    
    //
    // data is copied in a buffer, this is a scatter-gather routine
    //
    bool enqueueScatterGather(void * dataArray[], UInt32 dataSizeArray[], UInt32 entries );
    
    //
    // returns a pointer to an entry from the buffer, until the entry
    // is removed it represents an unpenetrable barrier for a new
    // entries allocation, the false value is returned when there is
    // no entry
    //
    bool dequeueDataInPlace( __out DldIODataQueueEntry** inPlaceEntry );
    
    //
    // removes an entry returned by dequeueDataInPlace from the buffer
    //
    void removeEntry( __in DldIODataQueueEntry* entry );

};

//
// ipc_kmsg_copyin_body is helpful for an OOL memory copying design
//

//--------------------------------------------------------------------

#endif//_DLDIODATAQUEUE_H