/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDKAUTHCREDENTRY_H
#define DLDKAUTHCREDENTRY_H

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldIOService.h"
#include "DldCommon.h"

class DldKauthCredEntry: public OSObject{
    
    OSDeclareDefaultStructors( DldKauthCredEntry );
    
private:
    
    //
    // a referenced credentials
    //
    kauth_cred_t       cred;
    
    //
    // pointer is not referenced, an is invalid if a process has terminated, might be NULL
    //
    proc_t             procp; // OPTIONAL
    
    //
    // a unique ID, a copy of the ID from the userInfo structure
    //
    UInt32             sessionID;
    
    //
    // a copy for a structure provided by the user mode daemon
    //
    DldLoggedUserInfo  userInfo;
    
    //
    // copies a provided cred object, so the caller must be sure
    // that the kauth cred is a full fleged structure,
    // to retrieve credentials entry for the current thread call
    // kauth_cred_get()
    //
    static DldKauthCredEntry* withProcCredCopyInt( __in proc_t  _proc, __in kauth_cred_t _cred, __in DldLoggedUserInfo* userInfo );
    
    //
    // copies a provided cred object, so the caller must be sure
    // that the kauth cred is a full fleged structure,
    // to retrieve credentials entry for the current thread call
    // kauth_cred_get()
    //
    static DldKauthCredEntry* withUserCredCopyInt( __in kauth_cred_t _cred, __in DldLoggedUserInfo* _userInfo );
    
protected:
    
    virtual bool initWithCredCopy( __in kauth_cred_t _cred );
    
    /*! @function free
     @abstract Frees data structures that were allocated by init()*/
    
    virtual void free( void );
    
public:
   
    //
    // copies a provided cred object, so the caller must be sure
    // that the kauth cred is a full fleged structure,
    // to retrieve credentials entry for the current thread call
    // kauth_cred_get()
    //
    static DldKauthCredEntry* withProcCredCopy( __in proc_t _proc, __in DldLoggedUserInfo* userInfo );
    static DldKauthCredEntry* withUserInfo( __in DldLoggedUserInfo* userInfo );
    
    //
    // returns a non refernced object, the caller must not dereference it
    //
    const kauth_cred_t getCred()
    {
        assert( this->cred );
        return this->cred;
    };
    
    //
    // returns a refernced object, the caller must dereference it by calling kauth_cred_unref
    //
    const kauth_cred_t getCredWithRef()
    {
        assert( this->cred );
        if( this->cred )
            kauth_cred_ref( this->cred );
            
        return this->cred;
    };
    
    //
    // returns a referenced object, a caller must dereference it
    // by calling kauth_cred_unref()
    //
    const kauth_cred_t getReferencedCred()
    {
        assert( preemption_enabled() );
        assert( this->cred );
        
        kauth_cred_ref( this->cred );
        return this->cred;
    };
    
    //
    // returns a proc pointer( if exist ) for the cred,
    // the returned pointer must not be referenced or
    // interpreted as a valid pointer
    //
    proc_t getProc(){ return this->procp; };
    
    UInt32 getID(){ return this->sessionID; };
    
    DldLoggedUserInfo*   getUserInfo(){ return &this->userInfo; };
    void updateUserInfo( __in DldLoggedUserInfo* info ){ this->userInfo = *info; };
};

#endif//DLDKAUTHCREDENTRY_H