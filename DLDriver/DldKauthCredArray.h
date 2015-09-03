/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDKAUTHCREDARRAY_H
#define DLDKAUTHCREDARRAY_H

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldIOService.h"
#include "DldCommon.h"
#include "DldKauthCredEntry.h"

class DldKauthCredArray: public OSArray{
  
    OSDeclareDefaultStructors( DldKauthCredArray )
    
protected:
    
    //
    // a lock to protect the array access
    //
    IORWLock*    rwLock;
    
#if defined( DBG )
    thread_t exclusiveThread;
#endif//DBG
    
    //
    // a referenced entry with the credentials for the current
    // active user, i.e. user logged in Aqua
    //
    DldKauthCredEntry* activeUserEntry;
    
    //
    // capacity can't be 0x0
    //
    virtual bool initWithCapacity( __in unsigned int capacity );
    
    /*! @function free
     @abstract Frees data structures that were allocated by init()*/
    
    virtual void free( void );

public:
    
    /*!
     * @function withCapacity
     *
     * @abstract
     * Creates and initializes an empty OSArray.
     * 
     * @param  capacity  The initial storage capacity of the array object.
     *
     * @result
     * An empty instance of OSArray with a retain count of 1;
     * <code>NULL</code> on failure.
     *
     * @discussion
     * <code>capacity</code> must be nonzero.
     * The new array will grow as needed to accommodate more objects
     * (<i>unlike</i> @link //apple_ref/doc/uid/20001502 CFMutableArray@/link,
     * for which the initial capacity is a hard limit).
     */
    static DldKauthCredArray* withCapacity( __in unsigned int capacity );
    
    //
    // returns an unreferenced entry, the caller must hold the lock
    //
    DldKauthCredEntry* getEntry( __in unsigned int i );
    
    //
    // returns a referenced entry by UID, a caller must not hold the lock,
    // if there are multiply entries for a user the first found is reterned
    //
    DldKauthCredEntry* getEntryByUidRef( __in uid_t uid );
    
    //
    // returns an unreferenced entry, the caller must hold the lock,
    // the NULL value might be returned
    //
    DldKauthCredEntry* getActiveUserEntry(){ return this->activeUserEntry; };
    
    //
    // returns a referenced credentials for the current user, a caller must release,
    // the NULL value might be returned
    //
    kauth_cred_t getActiveUserCredentialsRef();
    
    //
    // the caller must NOT acquire the lock, the lock is acquired inside
    // the function this exception has been made to reduce the time
    // while the lock is hold exclusively and reduce the type of operations
    // performed when the lock is hold
    //
    bool addCredForProcWithLock( __in_opt proc_t _proc,
                                 __in DldLoggedUserInfo*  userInfo,
                                 __in bool checkForDuplicate = false );
    
    void removeCredForSessionWithLock( __in UInt32 id  );
    
    void setProcCredAsActiveUserWithLock( __in_opt proc_t _proc, __in DldLoggedUserInfo* userInfo );
    void removeSessionAsActiveUserWithLock( __in UInt32 id );
    
    bool setUserState( __in DldLoggedUserInfo*  userState );
    
    void LockShared();
    void UnlockShared();
    
    //
    // a care must be taken wneh the lock is acquired exclusively
    // as this might deadlock the system if cost processing
    // operations are performed when the lock is hold
    //
    void LockExclusive();
    void UnlockExclusive();
    
};

extern DldKauthCredArray*   gCredsArray;

#endif//DLDKAUTHCREDARRAY_H