/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldSupportingCode.h"
#include "DldUndocumentedQuirks.h"

//--------------------------------------------------------------------

//
// the event states transitions
// 0 - initialized,
// 1 - preparing for waiting ( before entering a wq )
// 2 - ready for waiting ( after entering in a wq )
// 3 - signal state
//
// only a single thread can wait on a synchronization event, the case of multiple
// waiting threads will reasult in an infinite waiting and unpredictable behaviour
//

void
DldSetNotificationEvent(
    __in UInt32* event
)
{
    if( OSCompareAndSwap( 0x0, 0x3, event ) )
        return;
    
    assert( 0x1 == *event || 0x2 == *event );
    
    //
    // a collision with entering in a wq or already in a wq,
    // wait until entering in a wq, as entering is made with preemtion
    // disabled the loop won't last long
    //
    while( !OSCompareAndSwap( 0x2, 0x3, event ) ){
    }
    
    //
    // wake up a waiting thread
    //
    thread_wakeup( event );
    DLD_DBG_MAKE_POINTER_INVALID( event );
        
}

//--------------------------------------------------------------------

wait_result_t
DldWaitForNotificationEventWithTimeout(
    __in UInt32* event,
    __in uint32_t  uSecTimeout // mili seconds, if ( -1) the infinite timeout
    )
{
    wait_result_t  waitResult;
    bool wait;
    int  cookie;
    
    assert( DLD_INVALID_EVENT_VALUE != *event );
    
    //
    // check for a signal state, the event must be of notification type
    //
    if( OSCompareAndSwap( 0x3, 0x3, event ) )
        return THREAD_AWAKENED;
    
    assert( preemption_enabled() );
    
    //
    // disable preemtion to not delay 
    // DldSetNotificationEvent for a long time
    // and to not freeze in a wait queue forever
    // if rescheduling happens after assert_wait
    // and before OSCompareAndSwap( 0x1, 0x2, event )
    // thus leaving DldSetNotificationEvent in an
    // endless loop
    //
    cookie = DldDisablePreemption();
    {// start of scheduler disabling
        
        assert( !preemption_enabled() );
        
        if( OSCompareAndSwap( 0x0, 0x1, event ) ){
            
            if( (-1) == uSecTimeout )
                assert_wait( event, THREAD_UNINT );
            else
                assert_wait_timeout( (event_t)event, THREAD_UNINT, uSecTimeout, 1000*NSEC_PER_USEC );
            
            //
            // we are now in a wait queue,
            // we must not give any chance for the scheduler here
            // to move us from a CPU thus leaving in the waiting queue
            // forever, so the preemption has been disabled
            //
            
            //
            // say to DldSetNotificationEvent that we are in a wq
            //
            wait = OSCompareAndSwap( 0x1, 0x2, event );
            assert( wait );
            
            waitResult = THREAD_AWAKENED;
            
        } else {
            
            //
            // DldSetNotificationEvent managed to set the event
            //
            wait = false;
            assert( 0x3 == *event );
            
            waitResult = THREAD_AWAKENED;
        }
        
    }// end of scheduler disabling
    DldEnablePreemption( cookie );
    
    assert( preemption_enabled() );
    
    if( wait )
        waitResult = thread_block( THREAD_CONTINUE_NULL );
    
    if( THREAD_TIMED_OUT == waitResult ){
        
        //
        // change the "entering wq" state to the "non signal" state
        //
        OSCompareAndSwap( 0x2, 0x0, event );
    }
    
    assert( THREAD_AWAKENED == waitResult || THREAD_TIMED_OUT == waitResult );
    
    return waitResult;
}

//--------------------------------------------------------------------

volatile SInt32 memoryBarrier = 0x0;

void
DldMemoryBarrier()
{
    /*
     "...locked operations serialize all outstanding load and store operations
     (that is, wait for them to complete)." ..."Locked operations are atomic
     with respect to all other memory operations and all externally visible events.
     Only instruction fetch and page table accesses can pass locked instructions.
     Locked instructions can be used to synchronize data written by one processor
     and read by another processor." - Intel® 64 and IA-32 Architectures Software Developer’s Manual, Chapter 8.1.2.
     */
    OSIncrementAtomic( &memoryBarrier );
}

//--------------------------------------------------------------------
