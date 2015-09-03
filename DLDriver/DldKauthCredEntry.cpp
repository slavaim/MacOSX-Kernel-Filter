/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include <AvailabilityMacros.h>
#include "DldKauthCredEntry.h"
#include <sys/proc.h>

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldKauthCredEntry, OSObject )

//--------------------------------------------------------------------

bool DldKauthCredEntry::initWithCredCopy( __in kauth_cred_t _cred )
{
    assert( !this->cred );
    
    if( !super::init() ){
        
        DBG_PRINT_ERROR(("init() failed\n"));
        return false;
    }
    
    //
    // kauth_cred_create has an extremely tricky semantics
    // related to authentivation object reference, from
    // my point of view the single outcome
    // of this semantic is a system crash due to premature
    // object dereferencing, so I use a simple template without
    // any authentication object instead of the full fledged
    // credentials provided as a parameter
    //
    struct ucred    templateCred;
    
    bzero( &templateCred, sizeof( templateCred ) );
    
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
    templateCred.cr_posix.cr_uid    = _cred->cr_posix.cr_uid;   // an effective UID
    templateCred.cr_posix.cr_ruid   = _cred->cr_posix.cr_ruid;  // a real UID
    templateCred.cr_posix.cr_svuid  = _cred->cr_posix.cr_svuid; // a saved UID
    templateCred.cr_posix.cr_rgid   = _cred->cr_posix.cr_rgid;  // a real  GID
    templateCred.cr_posix.cr_svgid  = _cred->cr_posix.cr_svgid;  // a saved  GID
    templateCred.cr_posix.cr_gmuid  = _cred->cr_posix.cr_gmuid; // a UID for checking group membership with a user mode security service
    
    for( int i = 0x0; i < _cred->cr_posix.cr_ngroups; ++i )
        templateCred.cr_posix.cr_groups[ i ] = _cred->cr_posix.cr_groups[ i ];
    
    templateCred.cr_posix.cr_ngroups = _cred->cr_posix.cr_ngroups;
#else
    templateCred.cr_uid    = _cred->cr_uid;   // an effective UID
    templateCred.cr_ruid   = _cred->cr_ruid;  // a real UID
    templateCred.cr_svuid  = _cred->cr_svuid; // a saved UID
    templateCred.cr_rgid   = _cred->cr_rgid;  // a real  GID
    templateCred.cr_svgid  = _cred->cr_svgid; // a saved  GID
    templateCred.cr_gmuid  = _cred->cr_gmuid; // a UID for checking group membership with a user mode security service
    
    for( int i = 0x0; i < _cred->cr_ngroups; ++i )
        templateCred.cr_groups[ i ] = _cred->cr_groups[ i ];
    
    templateCred.cr_ngroups = _cred->cr_ngroups;
#endif
    
    this->cred = kauth_cred_create( &templateCred );
    assert( this->cred );
    if( !this->cred ){
        
        DBG_PRINT_ERROR(("kauth_cred_create( 0x%p ) failed\n", _cred));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

/*! @function free
 @abstract Frees data structures that were allocated by init()*/

void DldKauthCredEntry::free( void )
{
    if( this->cred )
        kauth_cred_unref( &this->cred );
        
    super::free();
}

//--------------------------------------------------------------------

//
// copies a provided cred object
//
DldKauthCredEntry* DldKauthCredEntry::withProcCredCopyInt( __in proc_t  _proc, __in kauth_cred_t _cred, __in DldLoggedUserInfo* _userInfo )
{
    DldKauthCredEntry* entry;
    
    assert( _cred && _userInfo );
    
#if defined( DBG )
    proc_t  proc;
    proc = proc_find( _userInfo->agentPID );// the returned object is referenced!
    assert( proc == _proc );
    if( NULL != proc )
        proc_rele( proc );
#endif//defined( DBG )
    
    entry = new DldKauthCredEntry();
    assert( entry );
    if( !entry ){
        
        DBG_PRINT_ERROR(("new DldKauthCredEntry() failed\n"));
        return NULL;
    }
    
    if( !entry->initWithCredCopy( _cred ) ){
        
        DBG_PRINT_ERROR(("entry->initWithCredCopy( 0x%p ) failed\n", _cred));
        entry->release();
        return NULL;
    }
    
    entry->procp     = _proc;// not referenced!
    entry->sessionID = _userInfo->sessionID;
    entry->userInfo  = *_userInfo;
    
    return entry;
}

//--------------------------------------------------------------------

//
// copies a provided cred object
//
DldKauthCredEntry* DldKauthCredEntry::withUserCredCopyInt( __in kauth_cred_t _cred, __in DldLoggedUserInfo* _userInfo )
{
    DldKauthCredEntry* entry;
    
    assert( _cred && _userInfo );
        
    entry = new DldKauthCredEntry();
    assert( entry );
    if( !entry ){
        
        DBG_PRINT_ERROR(("new DldKauthCredEntry() failed\n"));
        return NULL;
    }
    
    if( !entry->initWithCredCopy( _cred ) ){
        
        DBG_PRINT_ERROR(("entry->initWithCredCopy( 0x%p ) failed\n", _cred));
        entry->release();
        return NULL;
    }
    
    entry->procp     = NULL;
    entry->sessionID = _userInfo->sessionID;
    entry->userInfo  = *_userInfo;
    
    return entry;
}

//--------------------------------------------------------------------

DldKauthCredEntry* DldKauthCredEntry::withProcCredCopy( __in proc_t _proc, __in DldLoggedUserInfo* userInfo )
{
    assert( proc_pid( _proc ) == userInfo->agentPID );
    if( proc_pid( _proc ) != userInfo->agentPID )
        return NULL;
    
    kauth_cred_t _cred = kauth_cred_proc_ref( _proc );
    assert( _cred );
    if( !_cred ){
        
        DBG_PRINT_ERROR(("kauth_cred_proc_ref( 0x%p ) returned NULL\n", _proc));
        return NULL;
    }
    
    DldKauthCredEntry* entry = DldKauthCredEntry::withProcCredCopyInt( _proc, _cred, userInfo );
    assert( entry );
    if( !entry ){
        
        DBG_PRINT_ERROR(("DldKauthCredEntry::withProcCredCopyInt( 0x%p ) failed\n", _cred));
        kauth_cred_unref( &_cred );
        return NULL;
    }
    
    kauth_cred_unref( &_cred );
    return entry;
}

//--------------------------------------------------------------------

DldKauthCredEntry* DldKauthCredEntry::withUserInfo( __in DldLoggedUserInfo* userInfo )
{
    
    //
    // create a credentials
    //
    struct ucred    templateCred;
    
    bzero( &templateCred, sizeof( templateCred ) );
    
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
    templateCred.cr_posix.cr_uid    = userInfo->userID;   // an effective UID
    templateCred.cr_posix.cr_ruid   = userInfo->userID;   // a real UID
    templateCred.cr_posix.cr_svuid  = userInfo->userID;   // a saved UID
    templateCred.cr_posix.cr_rgid   = userInfo->groupID;  // a real  GID
    templateCred.cr_posix.cr_svgid  = userInfo->groupID;  // a saved  GID
    templateCred.cr_posix.cr_gmuid  = userInfo->userID;   // a UID for checking group membership with a user mode security service
    
    templateCred.cr_posix.cr_groups[ 0 ] = userInfo->groupID;
    templateCred.cr_posix.cr_ngroups = 0x1;
#else
    templateCred.cr_uid    = userInfo->userID;   // an effective UID
    templateCred.cr_ruid   = userInfo->userID;   // a real UID
    templateCred.cr_svuid  = userInfo->userID;   // a saved UID
    templateCred.cr_rgid   = userInfo->groupID;  // a real  GID
    templateCred.cr_svgid  = userInfo->groupID;  // a saved  GID
    templateCred.cr_gmuid  = userInfo->userID;   // a UID for checking group membership with a user mode security service
    
    templateCred.cr_groups[ 0 ] = userInfo->groupID;
    templateCred.cr_ngroups = 0x1;
#endif
    
    kauth_cred_t  userCred = kauth_cred_create( &templateCred );
    assert( userCred );
    if( ! userCred ){
        
        DBG_PRINT_ERROR(("kauth_cred_create( &templateCred ) failed\n"));
        return NULL;
    }
    
    DldKauthCredEntry* entry = DldKauthCredEntry::withUserCredCopyInt( userCred, userInfo );
    assert( entry );
    if( !entry ){
        
        DBG_PRINT_ERROR(("DldKauthCredEntry::withUserCredCopyInt( 0x%p ) failed\n", userCred));
        kauth_cred_unref( &userCred );
        return NULL;
    }
    
    kauth_cred_unref( &userCred );
    return entry;
}

//--------------------------------------------------------------------
