/*
 *  DldAclWithProcsObject.h
 *  DeviceLock
 *
 *  Created by Slava on 17/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

#ifndef _DLDFILEPROCACLOBJECT_H
#define _DLDFILEPROCACLOBJECT_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldAclObject.h"

//--------------------------------------------------------------------

class DldAclWithProcsObject: public OSObject
{
    OSDeclareDefaultStructors( DldAclWithProcsObject )
    
private:
    
    DldAclWithProcs*    acl; // might be NULL
    
protected:
    
    virtual void free();
    
public:
    
    static DldAclWithProcsObject* withAcl( __in_opt DldAclWithProcs* procsAcl );
    
    //
    // may return NULL
    //
    const DldAclWithProcs*   getAcl(){ return this->acl; }
};

//--------------------------------------------------------------------

#endif//_DLDFILEPROCACLOBJECT_H