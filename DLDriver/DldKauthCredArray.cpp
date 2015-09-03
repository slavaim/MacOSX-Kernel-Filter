/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldKauthCredArray.h"
#include <sys/proc.h>

//--------------------------------------------------------------------

#define super OSArray

OSDefineMetaClassAndStructors( DldKauthCredArray, OSArray )

//--------------------------------------------------------------------

bool DldKauthCredArray::initWithCapacity( __in unsigned int capacity )
{
    assert( preemption_enabled() );
    
    if( !super::initWithCapacity( capacity ) ){
        
        assert( !"super::initWithCapacity( capacity ) failed" );
        DBG_PRINT_ERROR(( "super::initWithCapacity( %u ) failed\n", capacity ));
        
        return false;
    }
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( !this->rwLock ){
        
        DBG_PRINT_ERROR(("IORWLockAlloc() failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

/*! @function free
 @abstract Frees data structures that were allocated by init()*/

void DldKauthCredArray::free( void )
{
    assert( preemption_enabled() );
    
    if( this->rwLock ){
        
        IORWLockFree( this->rwLock );
    }
    
    super::free();
}

//--------------------------------------------------------------------

DldKauthCredArray* DldKauthCredArray::withCapacity( __in unsigned int capacity )
{
    assert( preemption_enabled() );
    assert( 0x0 != capacity );
    
    DldKauthCredArray*  array;
    
    array = new DldKauthCredArray();
    assert( array );
    if( !array ){
        
        DBG_PRINT_ERROR(( "new DldKauthCredArray() failed, capacity=%d \n", capacity ));
        return NULL;
    }
    
    
    if( !array->initWithCapacity( capacity ) ){
        
        assert(!"array->initWithCapacity( capacity ) failed");
        DBG_PRINT_ERROR(( "array->initWithCapacity( %u ) failed \n", capacity ));
        array->release();
        
        return NULL;
    }
    
    return array;
}

//--------------------------------------------------------------------

DldKauthCredEntry* DldKauthCredArray::getEntry( __in unsigned int i )
{
    assert( OSDynamicCast( DldKauthCredEntry, this->getObject( i ) ) );
    
    //
    // I need this function works fast enough, so do not use
    // the safe cast for release
    //
    return (DldKauthCredEntry*)this->getObject( i );
}

//--------------------------------------------------------------------

DldKauthCredEntry* DldKauthCredArray::getEntryByUidRef( __in uid_t uid )
{
    DldKauthCredEntry*  entry = NULL;
    
    this->LockExclusive();
    {// start of the lock
        
        for( int i = 0x0; i < this->getCount(); ++i ){
            
            if( this->getEntry(i)->getUserInfo()->userID != uid )
                continue;
            
            entry = this->getEntry(i);
            entry->retain();
            break;
        }// end for
        
    }// end of the lock
    this->UnlockExclusive();
    
    return entry;
}

//--------------------------------------------------------------------

bool DldKauthCredArray::addCredForProcWithLock(
    __in_opt proc_t _proc,
    __in DldLoggedUserInfo*  userInfo,
    __in bool checkForDuplicate
    )
{
    assert( preemption_enabled() );
    assert( _proc || ( 0x0 != userInfo->userID && 0x0 != userInfo->groupID ) );
    
#if defined( DBG )
    //
    // the caller MUST not hold the lock
    //
    assert( current_thread() != this->exclusiveThread );
#endif//DBG
    
    DldKauthCredEntry*   entry;
    
    if( _proc )
        entry = DldKauthCredEntry::withProcCredCopy( _proc, userInfo );
    else
        entry = DldKauthCredEntry::withUserInfo( userInfo );
    
    assert( entry );
    if( !entry ){
        
        DBG_PRINT_ERROR(("DldKauthCredEntry::withProcCredCopy( 0x%p ) or DldKauthCredEntry::withUserInfo() for session=%i failed\n", _proc, (int)userInfo->sessionID ));
        return false;
    }
    
#if defined(DBG)
    if( !checkForDuplicate ){
        
        this->LockShared();
        {// start of the lock
            for( int i = 0x0; i < this->getCount(); ++i ){
                
                assert( this->getEntry(i)->getID() != userInfo->sessionID );
            }
        }// end of the lock
        this->UnlockShared();
        
    }// end if( !checkForDuplicate )
#endif//DBG
    
    bool added = false;
    
    this->LockExclusive();
    {// start of the lock
        
        assert( !added );
        
        if( checkForDuplicate ){
            
            for( int i = 0x0; i < this->getCount() && !added; ++i ){
                added = ( this->getEntry(i)->getID() == userInfo->sessionID );
            }// end for
            
        }// end if( checkForDuplicate )
        
        if( !added )
            added = this->setObject( entry );
        
    }// end of the lock
    this->UnlockExclusive();
    
    assert( added );
    if( !added ){
        
        DBG_PRINT_ERROR(("this->setObject( entry ) failed for the 0x%p process, count=%u, capacity=%d\n",
                         _proc, this->getCount(), this->getCapacity() ));
        // do not return!
    }
    
    //
    // the OSArray refernces its entries, if the adding failed the entry
    // must be dereferenced to free resources
    //
    entry->release();
    return added;
}

//--------------------------------------------------------------------

void DldKauthCredArray::removeCredForSessionWithLock( __in UInt32 id  )
{

    assert( preemption_enabled() );
    
#if defined( DBG )
    //
    // the caller MUST not hold the lock
    //
    assert( current_thread() != this->exclusiveThread );
#endif//DBG
    
    this->LockExclusive();
    {// start of the lock
        
        int i;
        
        //
        // removeObject release the object, this requires the preemtion be enabled for object freeing
        //
        assert( preemption_enabled() );
        
        for( i = 0x0; i < this->getCount() && this->getEntry(i)->getID() != id; ++i ){;}
        
        if( i != this->getCount() )
            this->removeObject( i );
        
    }// end of the lock
    this->UnlockExclusive();
    
    //
    // remove the process user as active, do this after removing
    // the entry from the array to avoid making the thread entry
    // active and then removing it from the array
    //
    this->removeSessionAsActiveUserWithLock( id );
    
    return;
}

//--------------------------------------------------------------------

void DldKauthCredArray::setProcCredAsActiveUserWithLock( __in_opt proc_t _proc, __in DldLoggedUserInfo*  userInfo )
{
    assert( preemption_enabled() );
    
    bool found = false;
    
    this->LockShared();
    {// start of the lock
        
        if( NULL != this->activeUserEntry ){
            
            if( this->activeUserEntry->getID() == userInfo->sessionID )
                found = true;
        }
        
    }// end of the lock
    this->UnlockShared();
    
    if( found )
        return;
    
    DldKauthCredEntry*  oldActiveEntry = NULL;
    
    this->LockExclusive();
    {// start of the lock
        
        int i;
        
        for( i = 0x0; i < this->getCount() && this->getEntry(i)->getID() != userInfo->sessionID; ++i ){;}
        
        if( i != this->getCount() ){
            
            //
            // save the old entry
            //
            oldActiveEntry = this->activeUserEntry;
            
            //
            // set the new entry
            //
            this->activeUserEntry = this->getEntry(i);
            this->activeUserEntry->retain();
            
            found = true;
        }
        
    }// end of the lock
    this->UnlockExclusive();
    
    //
    // release the old entry
    //
    if( NULL != oldActiveEntry ){
        
        //
        // TO DO - this is a workaround as the service doesn't notify about session termination
        // TO DO - REMOVE THIS AFTER THE SERVICE HAS BEEN FIXED
        //
        this->removeCredForSessionWithLock( oldActiveEntry->getUserInfo()->sessionID ); 
        
        oldActiveEntry->release();
        DLD_DBG_MAKE_POINTER_INVALID( oldActiveEntry ); 
    }
    
    if( found )
        return;
    
    //
    // add a user entry
    //
    if( this->addCredForProcWithLock( _proc, userInfo, true ) ){
        
        //
        // repeat again
        //
        this->setProcCredAsActiveUserWithLock( _proc, userInfo );
    }
    
}

//--------------------------------------------------------------------

void DldKauthCredArray::removeSessionAsActiveUserWithLock( __in UInt32 id )
{
    assert( preemption_enabled() );
    
    //
    // refrain from acquiring the lock exclusively as long as possible
    //
    if( !this->activeUserEntry )
        return;
    
    bool active = false;
    
    DldKauthCredEntry*  oldActiveEntry = NULL;
    
    this->LockShared();
    {// start of the lock
        
        if( NULL != this->activeUserEntry ){
            
            if( this->activeUserEntry->getID() == id )
                active = true;
        }
        
    }// end of the lock
    this->UnlockShared();
    
    if( !active )
        return;
    
    this->LockExclusive();
    {// start of the lock
        
        if( NULL != this->activeUserEntry ){
            
            if( this->activeUserEntry->getID() == id ){
                
                oldActiveEntry = this->activeUserEntry;
                this->activeUserEntry = NULL;
            }
        }
        
    }// end of the lock
    this->UnlockExclusive();
    
    if( oldActiveEntry )
        oldActiveEntry->release();
}

//--------------------------------------------------------------------

/*
 a call stack, just FYI
 #3  0x46571772 in DldKauthCredArray::setUserState (this=0x5f72a00, userInfo=0x6d3b97c) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKauthCredArray.cpp:386
 #4  0x46550005 in DldIOUserClient::setUserState (this=0xb605900, vInBuffer=0x6d3b97c, vOutBuffer=0x31b3ac64, vInSize=0x18, vOutSizeP=0x31b3aa70) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOUserClient.cpp:910
 #5  0x00647b7f in shim_io_connect_method_structureI_structureO (method=0x4659b638, object=0xb605900, input=0x6d3b97c "\001", inputCount=24, output=0x31b3ac64 "\001", outputCount=0x31b3aa70) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:3592
 #6  0x0064c677 in IOUserClient::externalMethod (this=0xb605900, selector=5, args=0x31b3aaf0, dispatch=0x0, target=0x0, reference=0x0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:4199
 #7  0x0064a368 in is_io_connect_method (connection=0xb605900, selector=5, scalar_input=0x6d3b978, scalar_inputCnt=0, inband_input=0x6d3b97c "\001", inband_inputCnt=24, ool_input=0, ool_input_size=0, scalar_output=0xb6535c8, scalar_outputCnt=0xb6535c4, inband_output=0x31b3ac64 "\001", inband_outputCnt=0x31b3ac60, ool_output=0, ool_output_size=0x6d3b9b4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:2745
 #8  0x002c1261 in _Xio_connect_method (InHeadP=0x6d3b950, OutHeadP=0xb6535a0) at device/device_server.c:15466
 #9  0x00226d74 in ipc_kobject_server (request=0x6d3b900) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/ipc_kobject.c:339
 #10 0x002126b1 in ipc_kmsg_send (kmsg=0x6d3b900, option=0, send_timeout=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/ipc_kmsg.c:1371
 #11 0x0021e193 in mach_msg_overwrite_trap (args=0x31b3bf60) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:505
 #12 0x0021e37d in mach_msg_trap (args=0x31b3bf60) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:572
 #13 0x002d88fb in mach_call_munger (state=0x6279e20) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/bsd_i386.c:697 
 */
bool DldKauthCredArray::setUserState( __in DldLoggedUserInfo*  userInfo )
{

    DldKauthCredEntry*  entry    = NULL;
    bool                res      = true;
    
    if( kDldUserLoginStateLoggedOut == userInfo->loginState ){
        
        //
        // a user has gone
        //
        
        //
        // the process might have already died, so proc_find will be unable to retrieve it,
        // use the sessionID to find a client descriptor
        //
        
        this->removeCredForSessionWithLock( userInfo->sessionID );
        assert( res );
        goto __exit;
    }

    this->LockShared();
    {// start of the lock
        
        int i;
        
        for( i = 0x0; i < this->getCount() && this->getEntry(i)->getID() != userInfo->sessionID; ++i ){;}
        
        if( i != this->getCount() ){
            
            entry = this->getEntry(i);
            entry->retain();
            
            assert( entry->getUserInfo()->agentPID == userInfo->agentPID );
            
        }// end if
        
    }// end of the lock
    this->UnlockShared();
    
    if( !entry ){
        
        assert( kDldUserLoginStateLoggedOut != userInfo->loginState );
        
        proc_t pBsdProc = proc_find( userInfo->agentPID );
            
        //
        // create an entry, pBsdProc is optional
        //
        if( this->addCredForProcWithLock( pBsdProc, userInfo, true ) ){
            
            //
            // recursively call itself to enter in another branch of the if(){}else{}
            //
            res = this->setUserState( userInfo );
            
        } else {
            
            DBG_PRINT_ERROR(("this->addCredForProcWithLock( proc, userInfo, true ) failed PID = %u\n", userInfo->agentPID));
            res = false;
        }
        
        if( pBsdProc )
            proc_rele( pBsdProc );
        
    } else {
        
        
        //
        // at this point the entry for the user exists
        //
        assert( entry );
        assert( entry->getUserInfo()->agentPID == userInfo->agentPID );
        res = true;
        
        //
        // wave on any synchronization issues as it seems that exclusive lock
        // acquiring is an overkill for this situation
        //
        entry->updateUserInfo( userInfo );
        
        if( kDldUserSessionActive == userInfo->activityState ){
            
            proc_t pBsdProc = proc_find( userInfo->agentPID );
            
            this->setProcCredAsActiveUserWithLock( pBsdProc, userInfo );
            
            if( pBsdProc )
                proc_rele( pBsdProc );
            
        } else if( kDldUserSessionDeactivated == userInfo->activityState ){
            
            this->removeSessionAsActiveUserWithLock( userInfo->sessionID );
        }
        
    }
    
__exit:
    
    if( entry )
        entry->release();
    
    return res;
}

//--------------------------------------------------------------------

kauth_cred_t DldKauthCredArray::getActiveUserCredentialsRef()
{
    kauth_cred_t  cred = NULL;
    
    this->LockShared();
    { // start of the lock
        
        if( this->getActiveUserEntry() ){
            
            assert( this->getActiveUserEntry()->getCred() );
            
            cred = this->getActiveUserEntry()->getCred();
            kauth_cred_ref( cred );
        }
        
    } // end of the lock
    this->UnlockShared();
    
    return cred;
}

//--------------------------------------------------------------------

void DldKauthCredArray::LockShared()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
    
    assert( NULL == this->exclusiveThread );
    
}

void DldKauthCredArray::UnlockShared()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    assert( NULL == this->exclusiveThread );
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

void DldKauthCredArray::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined( DBG )
    assert( current_thread() != this->exclusiveThread );
#endif//DBG
    
    IORWLockWrite( this->rwLock );
    
#if defined( DBG )
    this->exclusiveThread = current_thread();
#endif//DBG
    
}

void DldKauthCredArray::UnlockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined( DBG )
    assert( current_thread() == this->exclusiveThread );
    this->exclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------
