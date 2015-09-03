/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDSUPPORTINGCODE_H
#define DLDSUPPORTINGCODE_H

#include <sys/types.h>
#include <sys/kauth.h>
#include <sys/wait.h>
#include "DldCommon.h"

#define DLD_INVALID_EVENT_VALUE ((UInt32)(-1))

inline
void
DldInitNotificationEvent(
    __in UInt32* event
    )
{
    *event = 0x0;
}

//
// sets the event in a signal state
//
void
DldSetNotificationEvent(
    __in UInt32* event
    );

//
// wait for event, the event is not reseted after the wait is completed
//
wait_result_t
DldWaitForNotificationEventWithTimeout(
    __in UInt32* event,
    __in uint32_t  uSecTimeout // mili seconds, if ( -1) the infinite timeout
    );

inline
void
DldWaitForNotificationEvent(
    __in UInt32* event
    )
{
    DldWaitForNotificationEventWithTimeout( event, (-1) );
}


inline
void
DldInitSynchronizationEvent(
    __in UInt32* event
    )
{
    DldInitNotificationEvent( event );
}

//
// sets the event in a signal state
//
inline
void
DldSetSynchronizationEvent(
    __in UInt32* event
    )
{
    DldSetNotificationEvent( event );
}

//
// wait for event, the event is reseted if the wait is completed not because of a timeout 
//
inline
wait_result_t
DldWaitForSynchronizationEventWithTimeout(
    __in UInt32* event,
    __in uint32_t  uSecTimeout // mili seconds, if ( -1) the infinite timeout
    )
{
    wait_result_t waitResult;
    
    waitResult = DldWaitForNotificationEventWithTimeout( event, uSecTimeout );
    if( THREAD_AWAKENED == waitResult ){
        
        //
        // reset the event, notice that you can lost the event set after
        // DldWaitForNotificationEvent was woken up and before the event
        // is reset
        //
        OSCompareAndSwap( 0x3, 0x0, event );
    }
    
    return waitResult;
}

inline
void
DldWaitForSynchronizationEvent(
    __in UInt32* event
    )
{
    
    DldWaitForNotificationEvent( event );
    
    //
    // reset the event, notice that you can lost the event set after
    // DldWaitForNotificationEvent was woken up and before the event
    // is reset
    //
    OSCompareAndSwap( 0x3, 0x0, event );
}

void
DldMemoryBarrier();


#endif//DLDSUPPORTINGCODE_H

