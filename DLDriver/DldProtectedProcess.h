/*
 *  DldProtectedProcess.h
 *  DeviceLock
 *
 *  Created by Slava on 11/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */


#ifndef _DLDPROTECTEDPROCESS_H
#define _DLDPROTECTEDPROCESS_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldAclObject.h"

//--------------------------------------------------------------------

class DldProtectedProcess : public OSObject
{
    OSDeclareDefaultStructors( DldProtectedProcess )
    
private:
    
    pid_t           pid;
    
    //
    // a referenced ACL object, might be NULL
    //
    DldAclObject*   acl;
    
protected:
    
    virtual void free();
    
public:
    
    static DldProtectedProcess* withPidAndAcl( __in pid_t  pid, __in_opt DldAclObject* acl );
    
    pid_t getPID(){ return this->pid;};
    
    //
    // the returned object is not referenced
    //
    DldAclObject* getAclObject(){ return  this->acl;};
};

//--------------------------------------------------------------------

#endif // _DLDPROTECTEDPROCESS_H