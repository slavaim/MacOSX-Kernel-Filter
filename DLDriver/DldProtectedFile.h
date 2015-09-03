/*
 *  DldProtectedFile.h
 *  DeviceLock
 *
 *  Created by Slava on 16/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

#ifndef _DLDPROTECTEDFILE_H
#define _DLDPROTECTEDFILE_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldAclObject.h"
#include "DldAclWithProcsObject.h"

//--------------------------------------------------------------------

class DldProtectedFile : public OSObject
{
    OSDeclareDefaultStructors( DldProtectedFile )
    
private:
    
    //
    // a file or directory path, a referenced object
    //
    OSString*                path;
    
    //
    // a referenced ACL object, might be NULL,
    // defines per user security
    //
    DldAclObject*            usersAcl;
    
    //
    // a referenced processes ACL object,
    // defines per process security
    //
    DldAclWithProcsObject*   procsAcl;
    
protected:
    
    virtual void free();
    
    
public:
    
    //
    // the procsAcl is copied so a caller must release its copy of the structure
    //
    static DldProtectedFile* withPathAndAcls( __in OSString* path,
                                              __in_opt DldAclObject* usersAcl,
                                              __in_opt DldAclWithProcsObject* procsAcl );
    
    OSString* getPath(){ return this->path;};
    
    //
    // the returned object is not referenced
    //
    DldAclObject* getAclObject(){ return  this->usersAcl;};
    
    //
    // returns a file processes acl
    //
    DldAclWithProcsObject*   getFileProcAcl() { return this->procsAcl;};
    
    bool isOnProtectedPath( __in char* pathToCheck, __in size_t pathLength /*w/o zero terminator*/ );
    
    bool isAccessAllowed( __in vfs_context_t vfscontext,
                          __in kauth_cred_t    credential,
                          __in kauth_ace_rights_t requestedAccess );
};

//--------------------------------------------------------------------

#endif // _DLDPROTECTEDFILE_H