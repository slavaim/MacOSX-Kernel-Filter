/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */
#ifdef _DLD_MACOSX_CAWL

#include "DldDiskCAWL.h"
#include "DldVnodeHashTable.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldDiskCAWL, OSObject )

//--------------------------------------------------------------------

DldDiskCAWL* DldDiskCAWL::withInitializer( __in DldDiskCAWLObjectInitializer* initializer )
{
    DldDiskCAWL* newObj;
    
    newObj = new DldDiskCAWL();
    assert( newObj );
    if( !newObj )
        return NULL;
    
    if( !newObj->initWithInitializer( initializer ) ){
        
        DBG_PRINT_ERROR(("newObj->initWithInitializer( initializer ) failed\n"));
        newObj->release();
        
        return NULL;
        
    }// end if( !newObj->initWithInitializer( initializer ) )
    
    return newObj;
}

//--------------------------------------------------------------------

bool DldDiskCAWL::initWithInitializer( __in DldDiskCAWLObjectInitializer* initializer )
{
    
    assert( preemption_enabled() );
    
    if( !super::init() ){
        
        DBG_PRINT_ERROR(("super::init() failed\n"));
        return false;
        
    }//end if( !super::init() )
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( !this->rwLock ){
        
        DBG_PRINT_ERROR(("this->rwLock = IORWLockAlloc() failed\n" ));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

void DldDiskCAWL::free()
{
    assert( preemption_enabled() );
    assert( NULL == this->userClient );
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
}

//--------------------------------------------------------------------

IOReturn DldDiskCAWL::registerUserClient( __in DldIOUserClient* client )
{
    bool registered;
    
    if( this->pendingUnregistration ){
        
        DBG_PRINT_ERROR(("this->pendingUnregistration\n"));
        return kIOReturnError;
    }
    
    registered = OSCompareAndSwapPtr( NULL, (void*)client, &this->userClient );
    assert( registered );
    if( !registered ){
        
        DBG_PRINT_ERROR(("!registered\n"));
        return kIOReturnError;
    }
    
    client->retain();
    
    return registered? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

IOReturn DldDiskCAWL::unregisterUserClient( __in DldIOUserClient* client )
{
    bool   unregistered;
    DldIOUserClient*  currentClient;
    
    currentClient = (DldIOUserClient*)this->userClient;
    assert( currentClient == client );
    if( currentClient != client ){
        
        DBG_PRINT_ERROR(("currentClient != client\n"));
        return kIOReturnError;
    }
    
    this->pendingUnregistration = true;
    
    unregistered = OSCompareAndSwapPtr( (void*)currentClient, NULL, &this->userClient );
    assert( unregistered && NULL == this->userClient );
    if( !unregistered ){
        
        DBG_PRINT_ERROR(("!unregistered\n"));
        
        this->pendingUnregistration = false;
        return kIOReturnError;
    }
    
    do { // wait for any existing client invocations to return
        
        struct timespec ts = { 1, 0 }; // one second
        (void)msleep( &this->clientInvocations,      // wait channel
                     NULL,                          // mutex
                     PUSER,                         // priority
                     "DldIOLog::unregisterUserClient()", // wait message
                     &ts );                         // sleep interval
        
    } while( this->clientInvocations != 0 );
    
    currentClient->release();
    this->pendingUnregistration = false;
    
    return unregistered? kIOReturnSuccess: kIOReturnError;
}

//--------------------------------------------------------------------

bool DldDiskCAWL::isUserClientPresent()
{
    return ( NULL != this->userClient );
}

//--------------------------------------------------------------------

//
// send a CAWL notification to the service
//
errno_t
DldDiskCAWL::diskCawlNotification(
    __in     DldIOVnode* dldVnode, // covering vnode!
    __in     DldCawlOpcode opCode,
    __in_opt vfs_context_t vfsContext, // valid for create and open only
    __in_opt DldDiskCAWL::NotificationData* data
    )
{
    errno_t error = KERN_SUCCESS;
    bool    skipNotification = false;
    
    assert( preemption_enabled() );
    assert( dldVnode->isControlledByServiceCAWL() );
    
    DldIOUserClient*  currentClient;
    
    //
    // if ther is no user client, then nobody call for logging
    //
    if( NULL == this->userClient || this->pendingUnregistration )
        return kIOReturnSuccess;
    
    OSIncrementAtomic( &this->clientInvocations );
    
    currentClient = (DldIOUserClient*)this->userClient;
    
    //
    // if the current client is NULL or can't be atomicaly exchanged
    // with the same value then the unregistration is in progress,
    // the call to OSCompareAndSwapPtr( NULL, NULL, &this->userClient )
    // checks the this->userClient for NULL atomically 
    //
    if( !currentClient ||
        !OSCompareAndSwapPtr( currentClient, currentClient, &this->userClient ) ||
        OSCompareAndSwapPtr( NULL, NULL, &this->userClient ) ){
        
        //
        // the unregistration is in the progress and waiting for all
        // invocations to return
        //
        assert( this->pendingUnregistration );
        if( 0x1 == OSDecrementAtomic( &this->clientInvocations ) ){
            
            //
            // this was the last invocation
            //
            wakeup( &this->clientInvocations );
        }
        
        return KERN_SUCCESS;
    }
    
    //
    // from that point the client is valid
    //
    
    DldDriverDiskCAWLNotificationInt   notifyDataInt;
    DldDriverDiskCAWLNotification      notifyDataTemplate;
    
    assert( kDldCawlOpUnknown != opCode && opCode < kDldCawlOpMax );
    
    notifyDataTemplate.nameLength     = 0x0;
    notifyDataTemplate.notificationID = OSIncrementAtomic64( &this->IdGenerator );
    notifyDataTemplate.vnodeID        = dldVnode->vnodeID;
    notifyDataTemplate.vnodeHandle    = (UInt64)dldVnode->vnode;
    notifyDataTemplate.opcode         = opCode;
    
#if defined(DBG)
    notifyDataInt.signature = DLD_CAWL_NOTIFY_SIGNATURE;
#endif // DBG
    notifyDataInt.responseReceived  = false;
    notifyDataInt.disableAccess     = false;
    notifyDataInt.logOperation      = false;
    notifyDataInt.notifyDataSize    = sizeof( notifyDataTemplate );
    notifyDataInt.notifyData        = &notifyDataTemplate;
    notifyDataInt.notificationID    = notifyDataTemplate.notificationID;
    InitializeListHead( &notifyDataInt.listEntry );
    
    //
    // allow all operations for the service, so do not notify about this operation
    //
    if( vfsContext && currentClient->isClientProc( vfs_context_proc( vfsContext ) ) )
        goto __exit;
    
    switch( opCode ){
        case kDldCawlOpAccessRequest:
        case kDldCawlOpFileOpen:
        {
            OSString*               name;
            DldAccessCheckParam*    param = data->accessRequest.accessParam;
            kauth_cred_t            effectiveUserCredential = NULL;
            kauth_cred_t            realUserCredentialRef   = NULL;
            DldIOService*           dldIOService = NULL;
            DldDeviceType           deviceType;
            
            //
            // user selection affects on the user credentials choice!
            //
            assert( kDefaultUserSelectionFlavor == param->userSelectionFlavor );
            
            //
            // get the IOMedia object
            //
            if( param->dldIOService ){
                
                assert( param->dldIOService );
                
                dldIOService = param->dldIOService;
                dldIOService->retain();
                
            } else if( param->service ){
                
                assert( param->service );
                
                dldIOService = DldIOService::RetrieveDldIOServiceForIOService( param->service );
                
            } else {
                
                //
                // the param->deviceType value will be used as a device type
                //
                dldIOService = NULL;
                
            }// end !if( !dldIOService )
            
            //
            // don't apply the security settings to not started objects as this might preclude
            // a driver stack from intialization and moreover results in false negative
            // because of information lack for the full device description
            //
            if( dldIOService && kDldPnPStateStarted != dldIOService->getPnPState() ){
                
                skipNotification = true;
                dldIOService->release();
                break;
            }
            
            //
            // get the full file name
            //
            name = dldVnode->getNameRef();
            assert( name );
            
            notifyDataInt.notifyDataSize = offsetof( DldDriverDiskCAWLNotification, fileName ) + name->getLength() + sizeof( '\0' );
            
            notifyDataInt.notifyData = (DldDriverDiskCAWLNotification*)IOMalloc( notifyDataInt.notifyDataSize );
            assert( notifyDataInt.notifyData );
            if( !notifyDataInt.notifyData ){
                
                DBG_PRINT_ERROR(("IOMalloc( dataSize ) failed\n"));
                
                name->release();
                DLD_DBG_MAKE_POINTER_INVALID( name );
                
                if( dldIOService )
                    dldIOService->release();
                
                break;
            } // if( !notifyDataInt.notifyData )
            
            bzero( notifyDataInt.notifyData, notifyDataInt.notifyDataSize );
            
            //
            // copy the template
            //
            memcpy( notifyDataInt.notifyData, &notifyDataTemplate, offsetof( DldDriverDiskCAWLNotification, fileName ) );
            
            //
            // copy the name
            //
            memcpy( notifyDataInt.notifyData->fileName, name->getCStringNoCopy(), name->getLength() );
            
            //
            // set the zero terminator
            //
            notifyDataInt.notifyData->fileName[ name->getLength() ] = '\0';
            notifyDataInt.notifyData->nameLength = name->getLength() + sizeof( '\0' );
            
            //
            // set the user data, open and accessRequests are of the same type,
            // there is some similarities between the following code and the
            // isAccessAllowed() code as they do the similar job
            //
            
            //
            // the dldIOService property is a winner
            //
            if( dldIOService && dldIOService->getObjectProperty() ){
                
                assert( dldIOService->getObjectProperty() );
                
                deviceType = dldIOService->getObjectProperty()->dataU.property->deviceType;
                if( 0x0 == deviceType.combined )
                    deviceType = param->deviceType;
                
                //
                // in that case save the device type as it will be required for logging
                //
                param->deviceType = deviceType;
                
            } else {
                
                assert( 0x0 != deviceType.combined );
                deviceType = param->deviceType;
            }
            
            //
            // effective credentials
            //
            effectiveUserCredential = param->credential;
            assert( effectiveUserCredential );
            
            //
            // get the real user credentials if they are differ from the effective one
            //
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
            if( effectiveUserCredential->cr_posix.cr_ruid != effectiveUserCredential->cr_posix.cr_uid ){
#else
            if( effectiveUserCredential->cr_ruid != effectiveUserCredential->cr_uid ){
#endif
                
                //
                // create a credential for a real user,
                // actually, kauth_cred_create can't fail, 
                // it sticks forever in case of low memory
                //
                realUserCredentialRef = DldGetRealUserCredReferenceByEffectiveUser( effectiveUserCredential );
                assert( realUserCredentialRef );
                if( !realUserCredentialRef ){
                   
                    //
                    // carry on with the existing credentials
                    //
                    DBG_PRINT_ERROR(("kauth_cred_create() failed\n"));
                }
            }
            
            notifyDataInt.notifyData->operationData.accessRequest.deviceType = deviceType;
            notifyDataInt.notifyData->operationData.accessRequest.action     = param->dldRequestedAccess;
            
            //
            // kauth_cred_getguid can return an error, disregard it
            //
            kauth_cred_getguid( effectiveUserCredential,
                                &notifyDataInt.notifyData->operationData.accessRequest.effectiveUserGuid );
            
            notifyDataInt.notifyData->operationData.accessRequest.effectiveUserUid = kauth_cred_getuid( effectiveUserCredential );
            notifyDataInt.notifyData->operationData.accessRequest.effectiveUserGid = kauth_cred_getgid( effectiveUserCredential );
            
            if( realUserCredentialRef ){
                
                kauth_cred_getguid( realUserCredentialRef,
                                    &notifyDataInt.notifyData->operationData.accessRequest.realUserGuid );
                
                notifyDataInt.notifyData->operationData.accessRequest.realUserUid = kauth_cred_getuid( realUserCredentialRef );
                notifyDataInt.notifyData->operationData.accessRequest.realUserGid = kauth_cred_getgid( realUserCredentialRef );
            } // end if( realUserCredentialRef )
            
            //
            // release the resources
            //
            
            if( realUserCredentialRef )
                kauth_cred_unref( &realUserCredentialRef );
            
            name->release();
            DLD_DBG_MAKE_POINTER_INVALID( name );
            
            if( dldIOService )
                dldIOService->release();
            
            break;
        } // end case kDldCawlOpAccessRequest
            
        case kDldCawlOpFileWrite:
            
            notifyDataInt.notifyData->operationData.write.timeStamp = data->write.timeStamp;
            notifyDataInt.notifyData->operationData.write.offset    = data->write.offset;
            notifyDataInt.notifyData->operationData.write.size      = data->write.size;
            
            memcpy( notifyDataInt.notifyData->operationData.write.backingSparseFileID,
                    data->write.backingSparseFileID,
                    sizeof( data->write.backingSparseFileID ) );
            
            break;
            
        default:
            assert( &notifyDataTemplate == notifyDataInt.notifyData );
            break;
            
    } // end switch
    
    if( error || skipNotification ){
        
        //
        // check for leakage
        //
        assert( &notifyDataTemplate == notifyDataInt.notifyData );
        goto __exit;
    }
    
    assert( !error );
    
    //
    // insert in the vnode cawl list
    //
    IOSimpleLockLock( dldVnode->spinLock );
    { // start of the locked region
        
        InsertTailList( &dldVnode->cawlWaitListHead, &notifyDataInt.listEntry );
        
    } // end of the locked region
    IOSimpleLockUnlock( dldVnode->spinLock );
    
    //
    // send a notification to the service
    //
    error = currentClient->diskCawlNotification( &notifyDataInt );
    assert( !error );
    
    //
    // free the space, we do not need this structure anymore,
    // do not hold it while waiting with a long timeout this
    // can put a pressure on the kernel memory pool
    //
    if( &notifyDataTemplate != notifyDataInt.notifyData ){
        
        assert( NULL != notifyDataInt.notifyData );
        
        IOFree( notifyDataInt.notifyData, notifyDataInt.notifyDataSize );
        notifyDataInt.notifyData     = NULL;
        notifyDataInt.notifyDataSize = 0x0;
    }
    
    if( !error && kDldCawlOpAccessRequest == opCode ){
        
        wait_result_t  wait = THREAD_AWAKENED;
        
#if defined(DBG) // TO DO - remove, this is a test as CAWL is not implemented in the service
        { // start of the test
            //
            // emulate service response
            //
            
            errno_t     respError;
            DldServiceDiskCawlResponse serviceResponse;
            
            bzero( &serviceResponse, sizeof( serviceResponse ) );
            
            serviceResponse.notificationID  = notifyDataTemplate.notificationID;
            serviceResponse.vnodeID         = notifyDataTemplate.vnodeID;
            serviceResponse.vnodeHandle     = notifyDataTemplate.vnodeHandle;
            serviceResponse.opcode          = notifyDataTemplate.opcode;
            
            serviceResponse.operationData.accessRequest.disableAccess = false;
            serviceResponse.operationData.accessRequest.logOperation  = false;
            
            respError = this->processServiceResponse( &serviceResponse );
            assert( !respError );
        } // end of the test
#endif // DBG
        
        //
        // wait for a response from the service
        //
        IOSimpleLockLock( dldVnode->spinLock );
        { // start of the locked region
            
            if( !notifyDataInt.responseReceived ){
                //
                // wait for response with 2 minutes timeout
                //
                wait = assert_wait_timeout((event_t)&notifyDataInt,
                                           THREAD_UNINT,
                                           120000, /*120 secs*/
                                           1000*NSEC_PER_USEC);
            }
        } // end of the locked region
        IOSimpleLockUnlock( dldVnode->spinLock );
        
        if( THREAD_WAITING == wait )
            wait = thread_block( THREAD_CONTINUE_NULL );
        
        //
        // if the service has not responded then this is an error as the service is probably dead
        //
        if( THREAD_TIMED_OUT == wait )
            error = ETIMEDOUT;
        
    } // end if( !error && kDldCawlOpAccessRequest == opCode )
    
    //
    // remove from the vnode cawl list
    //
    IOSimpleLockLock( dldVnode->spinLock );
    { // start of the locked region
        
        RemoveEntryList( &notifyDataInt.listEntry );
#if defined(DBG)
        notifyDataInt.signature = 0x0;
#endif // DBG
        
    } // end of the locked region
    IOSimpleLockUnlock( dldVnode->spinLock );
    
__exit:
    
    if( NULL != notifyDataInt.notifyData && &notifyDataTemplate != notifyDataInt.notifyData ){
        
        IOFree( notifyDataInt.notifyData, notifyDataInt.notifyDataSize );
    }
    
    //
    // do not exchange or add any condition before OSDecrementAtomic as it must be always done!
    //
    if( 0x1 == OSDecrementAtomic( &this->clientInvocations ) && NULL == this->userClient ){
        
        //
        // this was the last invocation
        //
        wakeup( &this->clientInvocations );
    }

    
    if( kDldCawlOpAccessRequest == opCode ){
        
        DldAccessCheckParam*    param = data->accessRequest.accessParam;
        
        assert( !param->output.access.result[ DldFullTypeFlavor ].disable  &&
                !param->output.access.result[ DldMajorTypeFlavor ].disable &&
                !param->output.access.result[ DldParentTypeFlavor ].disable &&
                !param->output.access.result[ DldFullTypeFlavor ].log &&
                !param->output.access.result[ DldMajorTypeFlavor ].log &&
                !param->output.access.result[ DldParentTypeFlavor ].log );
        
        if( error ){
            
            //
            // disable access,
            // TO DO need to check the "CAWL: disable access on error" flag
            //
            DBG_PRINT_ERROR(( "CAWL access is disabled because of error(%u)\n", error ));
            param->output.access.result[ DldFullTypeFlavor ].disable = true;
        }
        
        if( notifyDataInt.disableAccess ){
            
            //
            // disable access
            //
            param->output.access.result[ DldFullTypeFlavor ].disable = true;
        }
        
        if( notifyDataInt.logOperation ){
            
            //
            // disable access
            //
            param->output.access.result[ DldFullTypeFlavor ].log = true;
        }
        
    } // if( kDldCawlOpAccessRequest == opCode )
    
    return error;
}

//--------------------------------------------------------------------

//
// process a response from the service
//
errno_t
DldDiskCAWL::processServiceResponse( __in DldServiceDiskCawlResponse* serviceResponse )
{
    assert( preemption_enabled() );
    assert( kDldCawlOpAccessRequest == serviceResponse->opcode );
    
    DldIOVnode*  dldVnode;
    
    //
    // only Access Request requires the service response
    //
    if( kDldCawlOpAccessRequest != serviceResponse->opcode ){
        
        DBG_PRINT_ERROR(("the service provided a wrong opcode\n"));
        return EINVAL;
    }
    
    dldVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode( (vnode_t)serviceResponse->vnodeHandle );
    if( NULL == dldVnode ){
        
        DBG_PRINT_ERROR(("the service provided a wrong vnode handle\n"));
        return EINVAL;
    }
    
    if( dldVnode->vnodeID != serviceResponse->vnodeID ){
        
        DBG_PRINT_ERROR(("the service provided a wrong vnode ID, maybe the handle has been reused\n"));
        dldVnode->release();
        return EINVAL;
    }
    
    IOSimpleLockLock( dldVnode->spinLock );
    { // start of the locked region
        
        for( PLIST_ENTRY  listEntry = dldVnode->cawlWaitListHead.Flink;
             listEntry != &dldVnode->cawlWaitListHead;
             listEntry = listEntry->Flink )
        {
            DldDriverDiskCAWLNotificationInt*   notifyDataInt;
            
            notifyDataInt = CONTAINING_RECORD( listEntry, DldDriverDiskCAWLNotificationInt, listEntry );
            
#if defined(DBG)
            assert( DLD_CAWL_NOTIFY_SIGNATURE == notifyDataInt->signature );
#endif // DBG
            
            if( notifyDataInt->notificationID == serviceResponse->notificationID ){
                
                assert( !notifyDataInt->responseReceived );
                
                //
                // do everething under the lock as waiting is with a timeout
                // so the structure's memory might be freed and reused just
                // after releasing the spin lock
                //
                notifyDataInt->responseReceived = true;
                notifyDataInt->disableAccess = serviceResponse->operationData.accessRequest.disableAccess;
                notifyDataInt->logOperation  = serviceResponse->operationData.accessRequest.logOperation;
                
                thread_wakeup( notifyDataInt );
                break;
            } // end if( notifyDataInt->notificationID == serviceResponce->notificationID )
            
        } // end for
        
    } // end of the locked region
    IOSimpleLockUnlock( dldVnode->spinLock );
    
    dldVnode->release();
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

void
DldDiskCAWL::LockShared()
{   
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
}

//--------------------------------------------------------------------

void
DldDiskCAWL::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

void
DldDiskCAWL::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() != this->exclusiveThread );
#endif//DBG
    
    IORWLockWrite( this->rwLock );
    
#if defined(DBG)
    assert( NULL == this->exclusiveThread );
    this->exclusiveThread = current_thread();
#endif//DBG
    
}

//--------------------------------------------------------------------

void
DldDiskCAWL::UnLockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() == this->exclusiveThread );
    this->exclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

#endif // #ifdef _DLD_MACOSX_CAWL

