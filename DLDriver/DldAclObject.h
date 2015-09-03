/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldCommon.h"
#include <sys/kauth.h>

#ifndef _DLDACLOBJECT_H
#define _DLDACLOBJECT_H

//--------------------------------------------------------------------

//
// the class is implemented in DldKernAuthorization.cpp
//
class DldAclObject: public OSObject
{
    OSDeclareDefaultStructors( DldAclObject )
    
private:
    
    kauth_acl_t    acl;
    DldDeviceType  type;
    
protected:
    
    virtual void free();
    
public:
    
    static DldAclObject* withAcl( __in kauth_acl_t acl );
    static DldAclObject* withAcl( __in kauth_acl_t acl, __in DldDeviceType  type );
    
    const kauth_acl_t   getAcl(){ return this->acl; }
    const DldDeviceType getType(){ return this->type; }
};

//--------------------------------------------------------------------

#endif//_DLDACLOBJECT_H


