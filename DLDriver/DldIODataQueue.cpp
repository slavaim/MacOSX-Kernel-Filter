/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIODataQueue.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIODataQueue, OSObject )

//--------------------------------------------------------------------

bool DldIODataQueue::initWithCapacity( __in UInt32 size )
{
    assert( 0x0 != size );
    assert( preemption_enabled() );
    
    if( !super::init() ){
        return false;
    }
    
    this->lock = IOLockAlloc();
    assert( this->lock );
    if( !this->lock )
        return false;
    
    this->dataQueue = (DldIODataQueueMemory*)IOMallocAligned(round_page(size + DLD_DATA_QUEUE_MEMORY_HEADER_SIZE), PAGE_SIZE);
    assert( this->dataQueue );
    if( !this->dataQueue ){
        
        IOLockFree( this->lock );
        return false;
    }
    
    bzero( this->dataQueue, sizeof( *this->dataQueue ) );
    this->dataQueue->queueSize = size;
    
    return true;
}

//--------------------------------------------------------------------

DldIODataQueue* DldIODataQueue::withCapacity( __in UInt32 size )
{
    DldIODataQueue *dataQueue = new DldIODataQueue();
    
    assert( preemption_enabled() );
    
    if( dataQueue ){
        
        if( !dataQueue->initWithCapacity( size ) ){
            
            assert( !"dataQueue->initWithCapacity(size) failed" );
            
            dataQueue->release();
            dataQueue = 0;
        }
        
    }
    
    return dataQueue;
}

//--------------------------------------------------------------------

void DldIODataQueue::free()
{
    assert( preemption_enabled() );
    
    if( this->dataQueue )
        IOFreeAligned( this->dataQueue, round_page(dataQueue->queueSize + DATA_QUEUE_MEMORY_HEADER_SIZE) );
        
    if( this->lock )
        IOLockFree( this->lock );
    
    super::free();
    
    return;
}

//--------------------------------------------------------------------

bool DldIODataQueue::waitForData()
{
    
    assert( preemption_enabled() );
    
    bool wait;
    
    IOLockLock( this->lock );
    {// start of the lock
        
        //
        // wait if the queue is empty
        //
        wait = ( this->dataQueue->head == this->dataQueue->tail );

        if( wait && THREAD_WAITING != assert_wait( this->dataQueue, THREAD_UNINT ) )
            wait = false;
        
    }// end of the lock
    IOLockUnlock( this->lock );
    
    if( wait )
        thread_block( THREAD_CONTINUE_NULL );
    
    return true;
}

//--------------------------------------------------------------------

void DldIODataQueue::sendDataAvailableNotificationOneThread()
{
    thread_wakeup_one( this->dataQueue );
}

//--------------------------------------------------------------------

void DldIODataQueue::sendDataAvailableNotificationAllThreads()
{
    thread_wakeup( this->dataQueue );
}

//--------------------------------------------------------------------

bool DldIODataQueue::enqueueScatterGather(void * dataArray[], UInt32 dataSizeArray[], UInt32 entries )
{
    UInt32    newTail;
    UInt32    oldTail;
    bool      wakeupWaiter;
    DldIODataQueueEntry*  entry = NULL;
    UInt32    i;
    vm_size_t dataSize = 0x0;
    
    assert( preemption_enabled() );
    
    for( i = 0x0; i < entries; ++i ){
        
        dataSize += dataSizeArray[ i ];
        assert( dataArray[ i ] );
    }// end for
    
    assert( 0x0 != dataSize );
    
    IOLockLock( this->lock );
    {// start of the lock
        
        UInt32                head      = dataQueue->head;
        UInt32                tail      = dataQueue->tail;
        vm_size_t             entrySize = dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE;
        
        newTail = tail;
        oldTail = tail;
        
        if ( tail >= head )
        {
            //
            // Is there enough room at the end for the entry?
            //
            if ( (tail + entrySize) <= dataQueue->queueSize )
            {
                entry = (DldIODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);
                
                entry->dataSize = dataSize;
                
                //
                // The tail can be out of bound when the size of the new entry
                // exactly matches the available space at the end of the queue.
                // The tail can range from 0 to dataQueue->queueSize inclusive.
                //
                
                dataQueue->tail += entrySize;
            }
            else if ( head > entrySize ) 	// Is there enough room at the beginning?
            {
                //
                // Wrap around to the beginning, but do not allow the tail to catch
                // up to the head.
                //
                
                dataQueue->queue->dataSize = dataSize;
                
                //
                // We need to make sure that there is enough room to set the size before
                // doing this. The consumer checks for this and will look for the size
                // at the beginning if there isn't room for it at the end.
                //
                
                if ( ( dataQueue->queueSize - tail ) >= DLD_DATA_QUEUE_ENTRY_HEADER_SIZE )
                {
                    //
                    // set an intermediate entry, the entry has only a valid header and
                    // used as a trampoline to the entry at the begining of the queue
                    //
                    
                    DldIODataQueueEntry* intermediateEntry = (DldIODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);
                    
                    intermediateEntry->dataSize = dataSize;
                    intermediateEntry->waitingForHeadPropogation = false;
                    intermediateEntry->waitingForTailPropogation = false;
                    #if defined( DBG )
                    intermediateEntry->signature = DLD_QUEUE_SIGNATURE;
                    intermediateEntry->intermediate = true;
                    #endif//#if defined( DBG )
                }
                
                entry = &dataQueue->queue[0];
                
                dataQueue->tail = entrySize;
            }
            else
            {
                //return false;	// queue is full
            }
        }
        else
        {
            //
            // Do not allow the tail to catch up to the head when the queue is full.
            // That's why the comparison uses a '>' rather than '>='.
            //
            
            if ( (head - tail) > entrySize )
            {
                entry = (DldIODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);
                
                entry->dataSize = dataSize;
                dataQueue->tail += entrySize;
            }
            else
            {
                //return false;	// queue is full
            }
        }
        
        
        if( entry ){
            
            entry->waitingForHeadPropogation = false;
            entry->waitingForTailPropogation = false;
            
#if defined( DBG )
            entry->signature = DLD_QUEUE_SIGNATURE;
            entry->intermediate = false;
#endif//DBG
        }
        
        newTail = dataQueue->tail;
        
    }// end of the lock
    IOLockUnlock( this->lock );
    
    if( !entry ){
        
        //
        // the queue is full
        //
        return false;
    }
    
    assert( newTail != oldTail );
    
    //
    // copy the data in he reserved queue space
    //
    UInt32 accumulatedSize = 0x0;
    
    for( i = 0x0; i < entries; ++i ){
        
        memcpy( (char*)entry->data + accumulatedSize, dataArray[ i ], dataSizeArray[ i ] );
        
        accumulatedSize += dataSizeArray[ i ];
        
    }// end for
    
    assert( accumulatedSize == dataSize );
    
    IOLockLock( this->lock );
    {// start of the lock
        
        //
        // propagate a valid data pointers
        //
        
        if( oldTail != dataQueue->validDataTail ){
            
            wait_result_t wresult;
            
            //
            // back off and give concurrent producers to complete the data fill in
            //
            
            dataQueue->waitersForTailPropogation += 0x1;
            entry->waitingForTailPropogation = true;
            
            wresult = assert_wait( ((char*)dataQueue->queue + oldTail), THREAD_UNINT );
            
            //
            // reschedule
            //
            if( THREAD_WAITING == wresult ){
                
                //
                // free the lock before rescheduling
                //
                IOLockUnlock( this->lock );
                
                //
                // reschedule
                //
                assert( preemption_enabled() );
                thread_block( THREAD_CONTINUE_NULL );
                
                //
                // reacquire the lock
                //
                IOLockLock( this->lock );
                
            }// end if( THREAD_WAITING == wresult )
            
            assert( dataQueue->waitersForTailPropogation > 0x0 );
            
            dataQueue->waitersForTailPropogation -= 0x1;
            
            //
            // there must be no more than one waiter
            //
            assert( oldTail == dataQueue->validDataTail );
            
            
        }// end if
        
        assert( oldTail == dataQueue->validDataTail );
        
        //
        // all data up to this one have been filled in
        // so propagate the valid data tail
        //
        dataQueue->validDataTail = newTail;
        
        assert( DLD_QUEUE_SIGNATURE == entry->signature );
        
        //
        // check for waiters
        //
        wakeupWaiter = ( 0x0 != dataQueue->waitersForTailPropogation );
        if( wakeupWaiter ){
            
            //
            // get the entry next to the current, it must exists as somebody
            // is waiting for the tail propogation
            //
            
            //DldIODataQueueEntry*   nextEntry;
            DldIODataQueueEntry*   nextEntry;
            UInt32                 nextEntryOffset;
            
            nextEntryOffset = newTail;
            nextEntry = (DldIODataQueueEntry *)((char *)dataQueue->queue + nextEntryOffset);
            
            assert( DLD_QUEUE_SIGNATURE == nextEntry->signature );
            assert( false == nextEntry->intermediate );
            
            //
            // rechek whether the next entry producer is really waiting,
            // this is required to avoid using a dequeued entry
            // after the lock is released
            //
            wakeupWaiter = nextEntry->waitingForTailPropogation;
        }
        
    }// end of the lock
    IOLockUnlock( this->lock );
    
    
    if( wakeupWaiter ){
        
        DldIODataQueueEntry* nextEntry = (DldIODataQueueEntry*)( (char*)dataQueue->queue + newTail );
        
        //
        // there must be exactly one waiter, but not necesary at the
        // newTail offset which might be for an intermadiate entry
        //
        assert( DLD_QUEUE_SIGNATURE == nextEntry->signature );
        assert( nextEntry->waitingForTailPropogation );
        
        thread_wakeup( nextEntry );
    }
    
    return true;
}


//--------------------------------------------------------------------

//
// false is returned when there are no entries left,
// the header of the retrurned enrty must not be changed
// and the entry must be provided as a parameter for 
// dequeueEntry when the entry is no longer needed
//
bool DldIODataQueue::dequeueDataInPlace( __out DldIODataQueueEntry** inPlaceEntry )
{
    DldIODataQueueEntry *  entry           = 0;
    
    assert( dataQueue );
    
    IOLockLock( this->lock );
    {// start of the lock
        
        UInt32                 newHeadOffset = 0;

        
        if( dataQueue->validDataHead != dataQueue->validDataTail ){
            
            DldIODataQueueEntry*   head;
            UInt32                 headOffset;
            
            headOffset = dataQueue->validDataHead;
            head = (DldIODataQueueEntry *)((char *)dataQueue->queue + headOffset);
            
            //
            // we wraped around to beginning, so read from there
            // either there was not even room for the header
            //
            if( (dataQueue->head +  DLD_DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
               // or there was room for the header, but not for the data
               ((dataQueue->head + head->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize)) {
                
                entry = dataQueue->queue;
                newHeadOffset = entry->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE;
                // else it is at the end
                
            } else {
                
                entry = head;
                newHeadOffset = headOffset + entry->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE;
            }
            
        }// else if( dataQueue->validDataHead != dataQueue->validDataTail )
        

        if (entry) {
            
            assert( DLD_QUEUE_SIGNATURE == entry->signature );
            assert( false == entry->intermediate );
            
            *inPlaceEntry = entry;
            dataQueue->validDataHead = newHeadOffset;
            
        }
        
    }// end of the lock
    IOLockUnlock( this->lock );
    
    return ( NULL != entry );
}

//--------------------------------------------------------------------

void DldIODataQueue::removeEntry( __in DldIODataQueueEntry* entry )
{
    assert( preemption_enabled() );
    assert( DLD_QUEUE_SIGNATURE == entry->signature );
    assert( false == entry->intermediate );
    
    bool wakeupWaiter;
    DldIODataQueueEntry* nextEntry;
    
    IOLockLock( this->lock );
    {// start of the lock
        
        bool waitForHeadPropagation = false;
        
        //
        // check whether the head points to the entry this means that there
        // are no unprocessed entries before this one and the head can be moved
        // beyound this entry in the direction to the validDataHead
        //
        if( (DldIODataQueueEntry *)((char *)dataQueue->queue + dataQueue->head ) != entry ){
            
            //
            // check that that there was no wrap around, because of which the head does not point to the entry
            //
            bool                   wrapAround = false;
            DldIODataQueueEntry*   head;
            UInt32                 headOffset;
            
            headOffset = dataQueue->head;
            head = (DldIODataQueueEntry *)((char *)dataQueue->queue + headOffset);
            
            //
            // we wraped around to beginning, so read from there
            // either there was not even room for the header
            //
            if( (dataQueue->head +  DLD_DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
                // or there was room for the header, but not for the data
                ((dataQueue->head + head->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize) ){
                
                //
                // check whether the entry is the entry which wrapped around the queue
                //
                if( entry == dataQueue->queue )
                    wrapAround = true;
                else
                    wrapAround = false;
            }
            
            //
            // wait if there was not due to the wrap around
            //
            waitForHeadPropagation = !wrapAround;
            
        }
        
        if( waitForHeadPropagation ){
            
            wait_result_t  wresult;
            
            //
            // a concurrent consumer holds the previous entry,
            // back off for the concurrent consumer to complete its processing
            // 
            
            dataQueue->waitersForHeadPropogation += 0x1;
            entry->waitingForHeadPropogation = true;
            
            wresult = assert_wait( entry, THREAD_UNINT );
            
            //
            // reschedule
            //
            if( THREAD_WAITING == wresult ){
                
                //
                // free the lock before rescheduling
                //
                IOLockUnlock( this->lock );
                
                assert( preemption_enabled() );
                thread_block(THREAD_CONTINUE_NULL);
                
                //
                // reacquire the lock
                //
                IOLockLock( this->lock );
                
            }
            
            assert( dataQueue->waitersForHeadPropogation > 0x0 );
            
            dataQueue->waitersForHeadPropogation -= 0x1;
            entry->waitingForHeadPropogation = false;
            
            //
            // there must be no more than one waiter
            //
            assert( entry == (DldIODataQueueEntry *)((char *)dataQueue->queue + dataQueue->head ) );
            
        }
        
        //
        // propagate the head moving it closer to a valid data head
        //
        {
            
            assert( ((char *)dataQueue->queue + dataQueue->head ) == (char*)entry || (char*)entry == (char *)dataQueue->queue);
            DldIODataQueueEntry*   head;
            UInt32                 headOffset;
            
            headOffset = dataQueue->head;
            head = (DldIODataQueueEntry *)((char *)dataQueue->queue + headOffset);
            
            //
            // check whether we wraped around to beginning
            //
            if( (dataQueue->head +  DLD_DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
                // or there was room for the header, but not for the data
                ((dataQueue->head + head->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize) ){
                
                //
                // wrapped around
                //
                assert( (char*)entry == (char *)dataQueue->queue );
                dataQueue->head = entry->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE;
                
            } else { 
                
                assert( ((char *)dataQueue->queue + dataQueue->head ) == (char*)entry );
                dataQueue->head += entry->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE;
            }
            
        }
        
        //
        // check for waiters
        //
        wakeupWaiter = ( 0x0 != dataQueue->waitersForHeadPropogation );
        if( wakeupWaiter ){
            
            //
            // get the entry next to the current, it must exist as somebody
            // is waiting for the head propogation
            //
            
            DldIODataQueueEntry*   head;
            UInt32                 headOffset;
            
            headOffset = dataQueue->head;
            head = (DldIODataQueueEntry *)((char *)dataQueue->queue + headOffset);
            
            //
            // we wraped around to beginning, so read from there
            // either there was not even room for the header
            //
            if( (dataQueue->head +  DLD_DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
               // or there was room for the header, but not for the data
               ((dataQueue->head + head->dataSize + DLD_DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize)) {
                
                nextEntry = dataQueue->queue;
                // else it is at the end
                
            } else {
                
                nextEntry = head;
            }
            
            assert( DLD_QUEUE_SIGNATURE == nextEntry->signature );
            assert( false == nextEntry->intermediate );
            
            //
            // rechek whether the next entry consumer is really waiting,
            // this is required to avoid using a dequeued entry
            // after the lock is released
            //
            wakeupWaiter = nextEntry->waitingForHeadPropogation;
            
        }// else if( wakeupWaiter )
        
        
    }// end of the lock
    IOLockUnlock( this->lock );
    
    if( wakeupWaiter ){
        
        //
        // there must be exactly one waiter
        //
        
        assert( nextEntry->waitingForHeadPropogation );
        assert( DLD_QUEUE_SIGNATURE == nextEntry->signature );
        assert( false == nextEntry->intermediate );
        
        thread_wakeup( nextEntry );
    }
    
}

//--------------------------------------------------------------------
