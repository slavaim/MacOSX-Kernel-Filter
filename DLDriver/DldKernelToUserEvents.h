/*
 *  DldKernelToUserEvents.h
 *  DeviceLock
 *
 *  Created by Slava on 8/01/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */

#ifndef _DLDKERNELTOUSEREVENTS_H
#define _DLDKERNELTOUSEREVENTS_H

#include <sys/types.h>
#include <libkern/OSTypes.h>
#include <libkern/OSAtomic.h>

class DldKernelToUserEvents{
    
private:
    SInt32  lastEventNumber;
    
public:
    
    DldKernelToUserEvents(): lastEventNumber(1){;}
    virtual ~DldKernelToUserEvents(){;}
    
    SInt32 getNextEventNumber(){ return OSIncrementAtomic( &this->lastEventNumber ); }
};

extern DldKernelToUserEvents       gKerneToUserEvents;

#endif // _DLDKERNELTOUSEREVENTS_H
