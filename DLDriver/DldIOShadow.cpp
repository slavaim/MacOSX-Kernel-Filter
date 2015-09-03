/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOShadow.h"
#include "DldSupportingCode.h"
#include "DldUndocumentedQuirks.h"
#include <sys/ubc.h>
#include <sys/proc.h>

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIOShadow, OSObject )

////////
// test!
//
DldIOShadowFile* gShadowFile;
//
// end of the test!
////////


//--------------------------------------------------------------------

bool DldIOShadow::isUserClientPresent()
{
    return ( NULL != this->userClient );
}

//--------------------------------------------------------------------

DldIOShadow* DldIOShadow::withInitializer( __in DldShadowObjectInitializer* initializer )
{
    DldIOShadow* newObj;
    
    newObj = new DldIOShadow();
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

bool DldIOShadow::initWithInitializer( __in DldShadowObjectInitializer* initializer )
{
    
    assert( preemption_enabled() );
    
    kern_return_t  result;
    
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
    
    this->shadowFilesArray = OSArray::withCapacity( 0x3 );
    assert( this->shadowFilesArray );
    if( !this->shadowFilesArray ){
        
        DBG_PRINT_ERROR(("this->shadowFilesArray = OSArray::withCapacity( 0x3 ) failed\n" ));
        return false;
    }
    
    ////////
    // test!
    //
    /*
    gShadowFile = DldIOShadowFile::withFileName( "/work/shadow.txt", strlen("/work/shadow.txt")+0x1 );
    if( !gShadowFile ){
        
        assert( !"DldIOShadowFile::withFileName failed" );
        return false;
    }
    
    gShadowFile->setMaxFileSize( 0x100*0x1000*0x1000 );
     */
    //
    // end of the test!
    ////////
    
    this->dataQueue = DldIODataQueue::withCapacity( initializer->queueSize );
    assert( this->dataQueue );
    if( !this->dataQueue ){
        
        DBG_PRINT_ERROR(("DldIODataQueue::withCapacity( %u ) failed\n", (int)initializer->queueSize ));
        return false;
    }
    
    //
    // start the shadow thread, reference the object to avoid its premature deletion
    //
    this->retain();
    
    result = kernel_thread_start( ( thread_continue_t )&DldIOShadow::shadowThreadMain,
                                  this,
                                  &this->thread );
	assert( KERN_SUCCESS == result );
    if ( KERN_SUCCESS != result ) {
        
        DBG_PRINT_ERROR(("kernel_thread_start() failed with an 0x%X error\n", result));
        
        this->release();
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

void DldIOShadow::stop()
{
    DldShadowMsgHdr  terminateMsgHdr;
    void*            msg[1];
    UInt32           size[1];
    
    assert( preemption_enabled() );
    
    this->stopped = true;
    
    bzero( &msg, sizeof( msg ) );
    
    terminateMsgHdr.type = DldShadowTypeStopShadow;
#if defined( DBG )
    terminateMsgHdr.signature = DLD_SHADOW_HDR_SIGNATURE;
#endif//DBG
    
    msg [ 0 ] = &terminateMsgHdr;
    size[ 0 ] = sizeof( terminateMsgHdr );
    
    //
    // stop the shadow thread
    //
    if( this->thread && this->dataQueue->enqueueScatterGather( msg, size, 1 ) ){
        
        //
        // wait for the thread termination
        //
        //TO DO(!!!)
    }
    
    //
    // release all files
    //
    if( this->shadowFilesArray )
        this->shadowFilesArray->flushCollection();
    
    ////////
    // test!
    //
    if( gShadowFile )
        gShadowFile->release();
    //
    // end of the test!
    ////////
    
}

//--------------------------------------------------------------------

void DldIOShadow::free()
{
    
    assert( preemption_enabled() );
    assert( NULL == this->userClient );
    
    if( !this->stopped )
        this->stop();
    
    //
    // release the thread object
    //
    if( this->thread )
        thread_deallocate( this->thread );
    
    //
    // delete the queue
    //
    if( this->dataQueue )
        this->dataQueue->release();
    
    //
    // delete the array
    //
    if( this->shadowFilesArray )
        this->shadowFilesArray->release();
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
}

//--------------------------------------------------------------------

void DldIOShadow::shadowThreadMain( void * this_class )
{
    DldIOShadow*   __this = (DldIOShadow*)this_class;
    
    assert( __this->thread );
    assert( __this->dataQueue );
    
    while( __this->dataQueue->waitForData() ){
        
        DldIODataQueueEntry*  entry;
        bool                  terminateThread = false;
        
        //
        // get the least recent entry
        //
        while( __this->dataQueue->dequeueDataInPlace( &entry ) ){
            
            //
            // entry's data contains the shadow data
            //
            DldShadowMsgHdr* msgHdr = (DldShadowMsgHdr*)entry->data;
            
            assert( entry->dataSize >= sizeof( *msgHdr ) );
            
#if defined( DBG )
            assert( DLD_SHADOW_HDR_SIGNATURE == msgHdr->signature );
#endif//DBG
            
            terminateThread = (DldShadowTypeStopShadow == msgHdr->type);
            if( terminateThread ){
                
                __this->dataQueue->removeEntry( entry );
                break;
            }
            
            //
            // process the received data
            //
            switch( msgHdr->type ){
                    
                case DldShadowTypeFileWrite:
                case DldShadowTypeFilePageout:
                {
                    DldShadowWriteVnodeArgs   writeArgs;
                    
                    //
                    // make a local copy to release the queue space asap,
                    // as the entry makes an unpenetrable barrier for
                    // allocations
                    //
                    assert( entry->dataSize >= sizeof( *msgHdr ) + sizeof( writeArgs ) );
                    memcpy( &writeArgs, (msgHdr+1), sizeof( writeArgs ) );
                           
                    #if defined( DBG )
                    assert( DLD_SHADOW_WRVN_SIGNATURE == writeArgs.signature );
                    #endif//DBG
                           
                    __this->dataQueue->removeEntry( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( msgHdr );
                           
                    __this->processFileWriteWQ( &writeArgs );
                    break;
                }
                    
                case DldShadowTypeSCSICommand:
                {
                    DldShadowSCSIArgs   scsiArgs;
                    
                    //
                    // make a local copy to release the queue space asap,
                    // as the entry makes an unpenetrable barrier for
                    // allocations
                    //
                    assert( entry->dataSize >= sizeof( *msgHdr ) + sizeof( scsiArgs ) );
                    memcpy( &scsiArgs, (msgHdr+1), sizeof( scsiArgs ) );
                    
                    #if defined( DBG )
                    assert( DLD_SHADOW_SCSI_SIGNATURE == scsiArgs.signature );
                    #endif//DBG
                    
                    __this->dataQueue->removeEntry( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( msgHdr );
                    
                    __this->processSCSIOperationWQ( &scsiArgs );
                    break;
                }
                    
                case DldShadowTypeOperationCompletion:
                {
                    DldShadowOperationCompletionArgs  compArgs;
                    
                    //
                    // make a local copy to release the queue space asap,
                    // as the entry makes an unpenetrable barrier for
                    // allocations
                    //
                    assert( entry->dataSize >= sizeof( *msgHdr ) + sizeof( compArgs ) );
                    memcpy( &compArgs, (msgHdr+1), sizeof( compArgs ) );
                    
                    #if defined( DBG )
                    assert( DLD_SHADOW_COMP_SIGNATURE == compArgs.signature );
                    #endif//DBG
                    
                    __this->dataQueue->removeEntry( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( msgHdr );
                    
                    __this->shadowOperationCompletionWQ( &compArgs );
                    break;
                }
                
                case DldShadowTypeFileOpen:
                case DldShadowTypeFileClose:
                case DldShadowTypeFileReclaim:
                {
                    
                    DldShadowCreateCloseVnode  args;
                    
                    //
                    // make a local copy to release the queue space asap,
                    // as the entry makes an unpenetrable barrier for
                    // allocations
                    //
                    assert( entry->dataSize >= sizeof( *msgHdr ) + sizeof( args ) );
                    memcpy( &args, (msgHdr+1), sizeof( args ) );
                    
#if defined( DBG )
                    assert( DLD_SHADOW_OCR_SIGNATURE == args.signature );
                    assert( DldShadowTypeFileOpen == args.type ||
                            DldShadowTypeFileClose == args.type ||
                            DldShadowTypeFileReclaim == args.type );
#endif//DBG
                    
                    __this->dataQueue->removeEntry( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( msgHdr );
                    
                    __this->shadowCreateCloseWQ( &args );
                    break;
                }
                    
                default:
                    
                    assert( !"unknown entry!" );
                    DBG_PRINT_ERROR(("an unknown entry type 0x%X\n", (unsigned int)msgHdr->type ));
                    
                    //
                    // free the queue space occupied by the entry
                    //
                    __this->dataQueue->removeEntry( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( entry );
                    DLD_DBG_MAKE_POINTER_INVALID( msgHdr );
                           
                    break;
                    
            }// end switch( msgHdr->type )
            
        }// end while( __this->dataQueue->dequeueDataInPlace( &entry ) )
        
        if( terminateThread )
            break;
        
    }// end while( __this->dataQueue->waitForData() )
    
    DBG_PRINT(("terminating the shadowThreadMain thread"));
    
    __this->release();
    thread_terminate( current_thread() );
}

//--------------------------------------------------------------------

//
// returns true if the quota has been exceed at least once, this
// doen't mean that the quota is currently exceeded
//
bool DldIOShadow::quotaWasExceededAtLeastOnce()
{
    return this->quotaExceeded;
}

//--------------------------------------------------------------------

//
// called when the quota is exceeded
//
void DldIOShadow::quotaExceedingDetected()
{
    this->quotaExceeded = true;
}

//--------------------------------------------------------------------

bool DldIOShadow::addShadowFile( __in DldIOShadowFile* shadowFile )
{
    assert( preemption_enabled() );
    assert( this->shadowFilesArray );
    
    bool added;
    
    this->LockExclusive();
    {// start of the lock
       
#if defined(DBG )
        //
        // prepare to check that we won't ovewrite the active file
        //
        DldIOShadowFile* activeFile = NULL;
        if( this->shadowFilesArray->getCount() >= 0x1 ){
            
            activeFile = OSDynamicCast( DldIOShadowFile, this->shadowFilesArray->getObject( 0x0 ) );
            assert( activeFile );
        }
        
#endif//DBG
        
        //
        // add a new file at the end of the array
        //
        added = this->shadowFilesArray->setObject( shadowFile );
        if( added && !shadowFile->isQuota() ){
            
            //
            // there is a file without quota, so the user has disabled the quota
            //
            this->quotaExceeded = false;
        }
        
#if defined(DBG )
        //
        // check that we didn't ovewrite the active file
        //
        if( this->shadowFilesArray->getCount() > 0x1 ){
            
            assert( activeFile );
            assert( OSDynamicCast( DldIOShadowFile, this->shadowFilesArray->getObject( 0x0 ) ) == activeFile );
        }
        
#endif//DBG
        
    }// end of the lock
    this->UnLockExclusive();
    
    return added;
}

//--------------------------------------------------------------------

bool DldIOShadow::isShadowResourcesAllocated()
{
    return ( 0x0 != this->shadowFilesArray->getCount() && NULL != this->userClient );
}

//--------------------------------------------------------------------

IOReturn DldIOShadow::registerUserClient( __in DldIOUserClient* client )
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

IOReturn DldIOShadow::unregisterUserClient( __in DldIOUserClient* client )
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

IOReturn DldIOShadow::shadowNotification( __in DldDriverShadowNotificationInt*  notificationData )
{
    
    IOReturn          RC;
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
        
        return kIOReturnSuccess;
    }
    
    //
    // send the notifcation
    //
    bool   repeat = false;
    SInt32 resendCounter = gGlobalSettings.maxAttemptsToResendBuffer;
    bool   kernelThread = ( 0x0 == proc_pid( current_proc() ) );
    do{
        
        RC = currentClient->shadowNotification( notificationData );
        
        //
        // allow to sleep if this is not a kernel request 
        //
        repeat = ( kIOReturnNoMemory == RC ) &&
                 ( ! kernelThread ) &&
                 ( (--resendCounter) > 0 ) &&
                 ( 0x0 != preemption_enabled() );
        if( repeat ){
            
            IOSleep(  gGlobalSettings.millisecondsWaitForFreeSpaceInNoificationsBuffer );
        }
    }while( repeat );
    
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR(("currentClient->shadowNotification( notificationData ) failed, error=0x%xX\n", RC));
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
    
    return RC;
    
}

//--------------------------------------------------------------------

IOReturn DldIOShadow::shadowFileRemovedNotification( __in DldIOShadowFile* shadowFile )
{
    off_t shadowFileOffset;
    (void)shadowFile->reserveOffset(0, &shadowFileOffset);
    
    DldDriverShadowNotificationInt   notifyDataInt;
    DldDriverShadowNotification      notifyData;
    
    bzero( &notifyDataInt, sizeof( notifyDataInt ) );
    bzero( &notifyData, sizeof( notifyData ) );
    
    notifyData.GeneralNotification.type         = DLD_SN_TYPE_CUTOFF;
    notifyData.GeneralNotification.shadowFileID = shadowFile->getID();
    notifyData.GeneralNotification.offset       = shadowFileOffset;
    
    notifyDataInt.notifyDataSize = sizeof( notifyData );
    notifyDataInt.notifyData = &notifyData;
    
    return this->shadowNotification( &notifyDataInt );
}

//--------------------------------------------------------------------

DldIOShadowFile* DldIOShadow::getCurrenShadowFileReserveSpace( 
    __in_opt off_t sizeToWrite,
    __out_opt off_t*  reservedOffset_t
    )
{
    assert( preemption_enabled() );
    assert( current_task() == kernel_task );
    assert( this->shadowFilesArray );
    
    DldIOShadowFile*  currentFile = NULL;
    off_t             dummyReservedOffset;
    
    
    if( NULL == reservedOffset_t )
        reservedOffset_t = &dummyReservedOffset;
    
        
    this->LockShared();
    {// start of the lock
        
        currentFile = OSDynamicCast( DldIOShadowFile, this->shadowFilesArray->getObject( 0x0 ) );
        if( currentFile )
            currentFile->retain();
        
    }// end of the lock
    this->UnLockShared();
    
    if( !currentFile )
        return NULL;
    
    if( !currentFile->reserveOffset( sizeToWrite, reservedOffset_t ) ){
        
        bool didRemoveCurrentFile = false;
        
        //
        // there is no place to accomodate the data,
        // get the next file
        //
        
        this->LockExclusive();
        {// start of the lock
            
            //
            // there might be concurrency in removing as reserveOffset is not
            // called under the lock protection, so check that we are not removing
            // a next shadow file which replaced currentFile,
            // the removal will move all object to one position to the start of the array
            //
            if( (OSObject*)currentFile == this->shadowFilesArray->getObject( 0x0 ) ){

                //
                // removeObject calls release() for the removed object
                //
                this->shadowFilesArray->removeObject( 0x0 );
                didRemoveCurrentFile = true;
            }
            
        }// end of the lock
        this->UnLockExclusive();
        
        if (didRemoveCurrentFile) {
            (void)this->shadowFileRemovedNotification( currentFile );
        }
        
        //
        // the object has been referenced just after calling getObject()
        //
        currentFile->release();
        DLD_DBG_MAKE_POINTER_INVALID( currentFile ); 
        
        //
        // repeat an attempt recursively, as this is a tail recursion it can be converted
        // to a loop
        //
        return this->getCurrenShadowFileReserveSpace( sizeToWrite, reservedOffset_t );
    }
    
    //
    // switch to the next file if after expanding the switching threshold
    // has been crossed, switch here only if there is a registered
    // next file else carry on up to the limit if any
    //
    if( currentFile->isFileSwicthingRequired() && ( this->shadowFilesArray->getCount() > 0x1 ) ){

        bool didRemoveCurrentFile = false;

        this->LockExclusive();
        {// start of the lock
            
            //
            // there might be concurrency in removing as reserveOffset is not
            // called under the lock protection, so check that we are not removing
            // a next shadow file which replaced currentFile,
            // the removal will move all object to one position to the start of the array
            //
            if( (OSObject*)currentFile == this->shadowFilesArray->getObject( 0x0 ) &&
                ( this->shadowFilesArray->getCount() > 0x1 ) ){
                
                //
                // removeObject calls release() for the removed object
                //
                this->shadowFilesArray->removeObject( 0x0 );
                didRemoveCurrentFile = true;
            }
            
        }// end of the lock
        this->UnLockExclusive();
        
        if (didRemoveCurrentFile) {
            (void)this->shadowFileRemovedNotification( currentFile );
        }
        
        //
        // we still retain a reference to the currentFile object
        // as it has been referenced just after getObject()
        //
    }
    
    return currentFile;
}

//--------------------------------------------------------------------

void DldIOShadow::releaseAllShadowFiles()
{
    assert( preemption_enabled() );
    assert( this->shadowFilesArray );
    
    this->LockExclusive();
    {// start of the lock
        
        this->shadowFilesArray->flushCollection();
        
    }// end of the lock
    this->UnLockExclusive();
}

//--------------------------------------------------------------------

//
// if false is returned the *commonParams->shadowCompletionRC value
// is invalid, if true is returned it contains a valid value
// after commonParams->completionEvent has been set to a signal state,
// the code here are mainly borrowed from hfs_readwrite.c,
// a good example for the meory descriptors management is
// the IOMemoryDescriptor * DKR_GET_BUFFER(dkr_t dkr, dkrtype_t dkrtype)
// function's code
//
bool DldIOShadow::shadowFileWriteInt(
    __in  DldShadowType type,
    __in  DldIOVnode* dldVnode,
    __in  DldWriteVnode_ap ap,
    __inout DldCommonShadowParams* commonParams
    )
{
    bool                      success = true;
    DldShadowMsgHdr           msgHdr;
    DldShadowWriteVnodeArgs   args;
    void*                     msg [ 2 ];
    UInt32                    size[ 2 ];
    UInt32                    validEntries = 2;
    vfs_context_t             context = NULL;
    bool                      zeroLengthBuffer = false;
    bool                      noDirtyPages = false;
    int                       numberOfBuffers = 0x0;
    bool                      waitForShadowCompetion = false;
    
    assert( preemption_enabled() );
    assert( 0x0 == *commonParams->completionEvent );
    assert( DldShadowTypeFileWrite == type || DldShadowTypeFilePageout == type );
    assert( dldVnode );

    //
    // for the current release only the regular file IO is processed,
    // skip the internal data write or ttys writes,
    //
    if( VREG != dldVnode->v_type ||
        dldVnode->flags.internal ||
        dldVnode->flags.system   ||
        dldVnode->flags.swap ){
        
        //
        // this is a successful operation
        //
        *commonParams->shadowCompletionRC = kIOReturnSuccess;
        DldSetNotificationEvent( commonParams->completionEvent );
        return true;
    }
    
    //
    // do not shadow if there is no any valid shadow file
    //
    if( !this->isShadowResourcesAllocated() ){
       
        //
        // actually, this is an error as we will be unable to save data,
        // we need a second call to find whether the quota has been exceeded
        // or the client has not been registered yet
        //
        
        bool  failed = this->quotaWasExceededAtLeastOnce();
        
        *commonParams->shadowCompletionRC = failed? kIOReturnNoResources: kIOReturnSuccess;
        DldSetNotificationEvent( commonParams->completionEvent );
        
        return !failed;
    }
    
    
    if( DldShadowTypeFileWrite != type && DldShadowTypeFilePageout != type ){
        
        DBG_PRINT_ERROR(( "a wrong type %u\n", (int)type ));
        return false;
    }
    
    
    switch( type ){
            
        case DldShadowTypeFileWrite:
            context = ap.ap_write->a_context;
            zeroLengthBuffer = ( 0x0 == uio_iovcnt( ap.ap_write->a_uio ) || 0x0 == uio_resid( ap.ap_write->a_uio ) );
            numberOfBuffers = uio_iovcnt( ap.ap_write->a_uio );
            break;
            
        case DldShadowTypeFilePageout:
            context = ap.ap_pageout->a_context;
            zeroLengthBuffer = ( 0x0 == ap.ap_pageout->a_size );
            numberOfBuffers = 0x1;
            break;
            
        default:
            panic( "a wrong type %u", (int)type );
            break;
            
    }// end switch
 
    assert( NULL != context && NULL != vfs_context_proc( context ) );
    
    //
    // actualy vfs_context_proc() never returns NULL, if there is no thread
    // associated with the context the current proc is returned
    //
    if( zeroLengthBuffer ||
        NULL == context ||
        NULL == vfs_context_proc( context ) ){
        
        //
        // this is a successful operation
        //
        *commonParams->shadowCompletionRC = kIOReturnSuccess;
        DldSetNotificationEvent( commonParams->completionEvent );
        return true;
    }
    
    //
    // init the event for shadow completion
    //
    DldInitNotificationEvent( commonParams->completionEvent );
    
    //
    // zero the stack allocated structures,
    // up to this point "goto __exit" is not allowed
    //
    
    bzero( &msgHdr, sizeof( msgHdr ) );
    bzero( &args, sizeof( args ) );
    
    //
    // from this point it is safe to "goto __exit"
    //
    
#if defined( DBG )
    msgHdr.signature = DLD_SHADOW_HDR_SIGNATURE;
    args.signature   = DLD_SHADOW_WRVN_SIGNATURE;
#endif//DBG
    
    //
    // init the message header
    //
    
    msgHdr.type = type;
    
    //
    // fill in the args structure
    //
    
    args.type = type;
    args.commonParams = *commonParams;
    
    switch( type ){
            
        case DldShadowTypeFileWrite:
            args.ap.ap_write = *ap.ap_write;
            break;
            
        case DldShadowTypeFilePageout:
            args.ap.ap_pageout = *ap.ap_pageout;
            break;
            
        default:
            panic( "a wrong type %u", (int)type );
            break;
            
    }// end switch
    
    //
    // all resources allocated or retained for the args structure
    // will be released here in case of error or by processFileWriteWQ
    // if the request is sent to a worker thread
    //
    
    args.memDscArray = OSArray::withCapacity( numberOfBuffers );
    assert( args.memDscArray );
    if( !args.memDscArray ){
        
        success = false;
        goto __exit;
    }
    
    args.rangeDscArray = OSArray::withCapacity( numberOfBuffers );
    assert( args.rangeDscArray );
    if( !args.rangeDscArray ){
        
        success = false;
        goto __exit;
    }
    
    args.dldVnode = dldVnode;
    args.dldVnode->retain();
    
    assert( success );
    
    //
    // create a memory descriptor for each memory range
    //
    for( int i = 0x0; i < numberOfBuffers; ++i ){
        
        switch( type ){
                
            case DldShadowTypeFileWrite:
            {
                int                   ret;
                IOMemoryDescriptor*   memDscr = NULL;
                DldRangeDescriptor    rangeDsc = { 0x0 };
                OSData*               rangeDscAsData;
                user_addr_t           baseaddr;
                user_size_t           length;
                task_t                writerTask;
                
                assert( UIO_WRITE == uio_rw(ap.ap_write->a_uio) );
                
                //
                // we will lock pages in the memory
                //
                args.pagesAreWired = true;
                
                ret = uio_getiov( ap.ap_write->a_uio, i, &baseaddr, &length );
                assert( (-1) != ret );
                
                rangeDsc.fileOffset = uio_offset( ap.ap_write->a_uio );
                rangeDsc.rangeSize = length;
                
                //
                // use vfs_context_proc( ap->a_a_context ) to get the task,
                // the current task can't be used as for an asyncronous IO
                // the user task's map is attached to a kernel thread but
                // this doesn't change the task which is the kernel task,
                // for reference see aio_work_thread()
                //
                if( uio_isuserspace( ap.ap_write->a_uio ) )
                    writerTask = vfs_context_proc( context )? DldBsdProcToTask( vfs_context_proc( context ) ): current_task();
                else
                    writerTask = kernel_task;

                memDscr = IOMemoryDescriptor::withAddressRange( baseaddr,
                                                                length,
                                                                kIODirectionOut,
                                                                writerTask );
                
                assert( memDscr );
                if( !memDscr ){
                    
                    success = false;
                    break;
                }
                
                success = args.memDscArray->setObject( memDscr );
                assert( success );
                //
                // do not break here as the memDscr object must be released
                //
                
                //
                // wire in the context of the caller because of the thread safety issues of the prepare() call
                //
                if( success )
                    memDscr->prepare();
                
                //
                // if added the object is retained
                // if not added the allocated resources must be released
                //
                memDscr->release();
                DLD_DBG_MAKE_POINTER_INVALID( memDscr );
                
                if( !success )
                    break;
                
                rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
                assert( rangeDscAsData );
                if( !rangeDscAsData ){
                    
                    success = false;
                    break;
                }
                
                success = args.rangeDscArray->setObject( rangeDscAsData );
                assert( success );
                //
                // do not break before releasing the rangeDscAsData object
                //
                
                rangeDscAsData->release();
                
                if( !success )
                    break;
                
                break;
                
            }// end case DldShadowTypeFileWrite
                
            case DldShadowTypeFilePageout:
            {
                upl_t 		      upl;
                upl_page_info_t*  pl;
                bool              is_pageoutv2 = false;
                kern_return_t     RC;
                
#if defined( DBG )
                upl = NULL;
                pl = NULL;
#endif//DBG
                
                //
                // we can tell if we're getting the new or old behavior from the UPL
                //
                is_pageoutv2 = ( NULL == ap.ap_pageout->a_pl );
                    
                //
                // we're in control of any UPL we commit, so disregard
                // the ap.ap_pageout->a_pl_offset value
                //
                // vm_offset_t a_pl_offset = 0x0;
                
                upl = NULL;
                pl  = NULL;
                
                if( is_pageoutv2 ){
                    
                    off_t f_offset;
                    int   offset;
                    int   isize; 
                    int   pg_index;
                    int   i = 0x0;
                    
                    // 
                    // create a upl for the range,
                    // UPL_SET_IO_WIRE is useless as not taken into account,
                    // consider UPL_SET_LITE
                    // a snapshot of some fields for a created upl is
                    //   ref_count = 1, 
                    //   flags = ??? ( UPL_RET_ONLY_DIRTY ???? )
                    //   src_object = 0x0, 
                    //   offset = 22216704, 
                    //   size = 131072, 
                    //   kaddr = 0, 
                    //   map_object = 0x81886f0, 
                    //   highest_page = 270271, 
                    //   vector_upl = 0x0
                    //
                    // BE CAUTIOS TO NOT REMOVE DIRTY FLAG FROM THE PAGES WHILE CREATING OR ABORTING THE UPL
                    
                    //
                    // how it works, the system creates a temporary vm_object as this is not a lite UPL,
                    // then inserts a temporary vm_page descriptors for each dirty page by calling
                    // vm_pageclean_setup, the original vm_page preserves the dirty flag, do not set
                    // any of (UPL_UBC_MSYNC | UPL_UBC_PAGEOUT | UPL_UBC_PAGEIN) flags as they are resulted
                    // in creating a lite UPL thus clearing the dirty flags, the call stack is
                    /*
                     #0  vm_pageclean_setup (m=0x3f3a12c, new_m=0x6a1e44c, new_object=0xa8090c0, new_offset=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/vm_pageout.c:634
                     #1  0x0029849a in vm_object_upl_request (object=0x98430c0, offset=0, size=401408, upl_ptr=0x3235377c, user_page_list=0x97701d0, page_list_count=0x0, cntrl_flags=108) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/vm_pageout.c:3612
                     #2  0x004faac7 in ubc_create_upl (vp=0xa776864, f_offset=0, bufsize=401408, uplp=0x3235377c, plp=0x323536f0, uplflags=<value temporarily unavailable, due to optimizations>) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/ubc_subr.c:1869
                     #3  0x46eee014 in DldIOShadow::shadowFileWriteInt (this=0xa066920, type=DldShadowTypeFilePageout, dldVnode=0xa0fdf40, ap={ap_write = 0x323539cc, ap_pageout = 0x323539cc}, commonParams=0x32353860) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldIOShadow.cpp:1084
                     #4  0x46ee6d53 in DldIOShadow::shadowFilePageout (this=0xa066920, dldVnode=0xa0fdf40, ap=0x323539cc, commonParams=0x32353860) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldIOShadow.cpp:1963
                     #5  0x46f11f08 in DldFsdPageoutHook (ap=0x323539cc) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldVNodeHook.cpp:1175
                     #6  0x0032a2e4 in VNOP_PAGEOUT (vp=0xa776864, pl=0x0, pl_offset=0, f_offset=0, size=401408, flags=25, ctx=0x72d0114) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:5444
                     #7  0x00529a13 in vnode_pageout (vp=0xa776864, upl=0x0, upl_offset=0, f_offset=0, size=401408, flags=<value temporarily unavailable, due to optimizations>, errorp=0x32353adc) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vm/vnode_pager.c:208
                     #8  0x00268f1f in vnode_pager_cluster_write (vnode_object=0xa77ed6c, offset=0, cnt=401408, resid_offset=0x0, io_error=0x0, upl_flags=25) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/bsd_vm.c:991
                     #9  0x00269565 in vnode_pager_data_return (mem_obj=0xa77ed6c, offset=0, data_cnt=401408, resid_offset=0x0, io_error=0x0, dirty=1, kernel_copy=1, upl_flags=17) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/bsd_vm.c:687
                     #10 0x0026c698 in vm_object_update (object=0x98430c0, offset=0, size=401408, resid_offset=0x0, io_errno=0x0, should_return=2, flags=<value temporarily unavailable, due to optimizations>, protection=8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/memory_object.c:2152
                     #11 0x0026c933 in vm_object_sync (object=0x98430c0, offset=0, size=401408, should_flush=0, should_return=1, should_iosync=2) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/memory_object.c:542
                     #12 0x00279b99 in vm_map_msync (map=0x5c04b04, address=4298870784, size=<value temporarily unavailable, due to optimizations>, sync_flags=34) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/vm/vm_map.c:12098
                     #13 0x004d569d in msync_nocancel (p=0x69a2570, uap=0x72e4d88, retval=0x72d0054) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_mman.c:626
                     #14 0x0054e9fd in unix_syscall64 (state=0x72e4d84) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365                     
                     */
                    
                    RC = ubc_create_upl( ap.ap_pageout->a_vp,
                                         ap.ap_pageout->a_f_offset,
                                         ap.ap_pageout->a_size,
                                         &upl,
                                         &pl,
                                         UPL_RET_ONLY_DIRTY ); 
                    assert( upl && pl );
                    assert( KERN_SUCCESS == RC );
                    if( KERN_SUCCESS != RC ){
                        
                        OSString* fileNameRef = dldVnode->getNameRef();
                        assert( fileNameRef );
                        
                        DBG_PRINT_ERROR(( "ubc_create_upl() failed with RC=0x%X for %s file\n", RC, fileNameRef->getCStringNoCopy() ));
                        success = false;
                        
                        fileNameRef->release();
                        DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
                        break;
                    }
                    
                    //
                    // save the upl address to free upl on completion
                    //
                    args.upl = upl;
#if defined( DBG )
                    assert( pl );
                    assert( upl );
#endif//DBG
                    
                    isize = ap.ap_pageout->a_size;
                    f_offset = ap.ap_pageout->a_f_offset;
                    
                    // 
                    // Scan from the back to find the last page in the UPL
                    //
                    for( pg_index = ((isize) / PAGE_SIZE); pg_index > 0; ){
                        
                        if( upl_page_present(pl, --pg_index) )
                            break;
                        if (pg_index == 0) {
                            noDirtyPages = true;
                            break;
                        }
                        
                    }
                    
                    if( noDirtyPages )
                        break;
                    
                    // 
                    // initialize the offset variables before we touch the UPL.
                    // a_f_offset is the position into the file, in bytes
                    // offset is the position into the UPL, in bytes
                    // pg_index is the pg# of the UPL we're operating on.
                    // isize is the offset into the UPL of the last non-clean page. 
                    //
                    isize = ((pg_index + 1) * PAGE_SIZE);	
                    
                    offset = 0;
                    pg_index = 0;
                    
                    while( 0x0 != isize ){
                        
                        int                   xsize;
                        int                   num_of_pages;
                        IOMemoryDescriptor*   memDscr;
                        DldRangeDescriptor    rangeDsc  = { 0x0 };
                        OSData*               rangeDscAsData;
                        IOOptionBits          options;
                        
                        if( !upl_page_present(pl, pg_index) ){
                            
                            //
                            // we asked for RET_ONLY_DIRTY, so it's possible
                            // to get back empty slots in the UPL.
                            // just skip over them
                            //
                            f_offset += PAGE_SIZE;
                            offset   += PAGE_SIZE;
                            isize    -= PAGE_SIZE;
                            pg_index++;
                            
                            continue;
                        }
                        
                        if( !upl_dirty_page( pl, pg_index ) ){
                            
                            assert( !"unforeseen clean page" );
                            DBG_PRINT_ERROR(("unforeseen clean page @ index %d for UPL %p\n", pg_index, upl));
                            //
                            // continue processing as the page is present and there won't be a page fault,
                            // but HFS FSD panics in this case, so in any case we are on the road to hell
                            //
                        }
                        
                        // 
                        // We know that we have at least one dirty page.
                        // Now checking to see how many in a row we have
                        //
                        num_of_pages = 1;
                        xsize = isize - PAGE_SIZE;
                        
                        while( 0x0 != xsize ){
                            
                            if( !upl_dirty_page( pl, pg_index + num_of_pages) )
                                break;
                            
                            num_of_pages++;
                            xsize -= PAGE_SIZE;
                            
                        }
                        xsize = num_of_pages * PAGE_SIZE;
                        
                        //
                        // so the dirty range has been found
                        //
                        
                        rangeDsc.fileOffset = f_offset;
                        rangeDsc.rangeSize = xsize;
                        
                        //
                        // create a memory descriptor for the range
                        //
                        
                        options = kIOMemoryTypeUPL | kIODirectionOut;
                        
                        memDscr = IOMemoryDescriptor::withOptions( upl,
                                                                   xsize,  // range size
                                                                   offset, // offset in upl
                                                                   0,
                                                                   options );
                        assert( memDscr );
                        if( !memDscr ){
                            
                            OSString* fileNameRef = dldVnode->getNameRef();
                            assert( fileNameRef );
                            
                            DBG_PRINT_ERROR(("IOMemoryDescriptor::withOptions() for %s file\n", fileNameRef->getCStringNoCopy() ));
                            success = false;
                            
                            fileNameRef->release();
                            DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
                            break;
                        }
                        
                        success = args.memDscArray->setObject( memDscr );
                        assert( success );
                        //
                        // do not break here as the memDscr object must be released
                        //
                        
                        //
                        // if added the object is retained
                        // if not added the allocated resources must be released
                        //
                        memDscr->release();
                        DLD_DBG_MAKE_POINTER_INVALID( memDscr );
                        
                        if( !success )
                            break;
                        
                        rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
                        assert( rangeDscAsData );
                        if( !rangeDscAsData ){
                            
                            success = false;
                            break;
                        }
                        
                        success = args.rangeDscArray->setObject( rangeDscAsData );
                        assert( success );
                        //
                        // do not break before releasing the rangeDscAsData object
                        //
                        
                        rangeDscAsData->release();
                        
                        if( !success )
                            break;
                        
                        //
                        // move to the next range( clean ) in the upl
                        //
                        f_offset += xsize;
                        offset   += xsize;
                        isize    -= xsize;
                        pg_index += num_of_pages;
                        ++i;
                        
                    }// end while( 0x0 != isize )
                    
                    
                } // end block for v2 pageout behavior
                else {
                    
                    //
                    // the old pageout behavior
                    //
                    
                    IOMemoryDescriptor*   memDscr;
                    DldRangeDescriptor    rangeDsc  = { 0x0 };
                    OSData*               rangeDscAsData;
                    IOOptionBits          options;
                    
                    //
                    // you can't create UPL here as the pages are marked as busy
                    // and this will result in a dead lock in the ubc_create_upl
                    // like in the following call stack
                    /*
                     #0  machine_switch_context (old=0x90977a8, continuation=0, new=0x6c997a8) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/pcb.c:869
                     #1  0x00226e57 in thread_invoke (self=0x90977a8, thread=0x6c997a8, reason=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1628
                     #2  0x002270f6 in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1863
                     #3  0x00227184 in thread_block (continuation=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1880
                     #4  0x00221566 in lck_rw_sleep (lck=0x8e96aa0, lck_sleep_action=0, event=0x40c396c, interruptible=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/locks.c:861
                     #5  0x00274b88 in vm_object_upl_request (object=0x8e96a98, offset=118784, size=282624, upl_ptr=0x31dbb80c, user_page_list=0x8ebe834, page_list_count=0x0, cntrl_flags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_pageout.c:3552
                     #6  0x0049f651 in ubc_create_upl (vp=0x7e8c3a8, f_offset=118784, bufsize=282624, uplp=0x31dbb80c, plp=0x31dbb780, uplflags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/ubc_subr.c:1869
                     #7  0x46edcf16 in DldIOShadow::shadowFileWriteInt (this=0x605c900, type=DldShadowTypeFilePageout, dldVnode=0x8e79980, ap={ap_write = 0x31dbba5c, ap_pageout = 0x31dbba5c}, commonParams=0x31dbb8f0) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldIOShadow.cpp:1072
                     #8  0x46ed5d53 in DldIOShadow::shadowFilePageout (this=0x605c900, dldVnode=0x8e79980, ap=0x31dbba5c, commonParams=0x31dbb8f0) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldIOShadow.cpp:1882
                     #9  0x46f00d1a in DldFsdPageoutHook (ap=0x31dbba5c) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldVNodeHook.cpp:1175
                     #10 0x002f66a0 in VNOP_PAGEOUT (vp=0x7e8c3a8, pl=0x7763c00, pl_offset=0, f_offset=118784, size=282624, flags=25, ctx=0x8eb7e14) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:5444
                     #11 0x004cdf0f in vnode_pageout (vp=0x7e8c3a8, upl=0x7763c00, upl_offset=0, f_offset=118784, size=282624, flags=25, errorp=0x31dbbb6c) at /SourceCache/xnu/xnu-1504.7.4/bsd/vm/vnode_pager.c:368
                     #12 0x002503a5 in vnode_pager_cluster_write (vnode_object=0x7eb9e60, offset=118784, cnt=282624, resid_offset=0x0, io_error=0x0, upl_flags=25) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/bsd_vm.c:991
                     #13 0x00250ba5 in vnode_pager_data_return (mem_obj=0x7eb9e60, offset=0, data_cnt=282624, resid_offset=0x0, io_error=0x0, dirty=1, kernel_copy=1, upl_flags=17) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/bsd_vm.c:687
                     #14 0x00252a3c in vm_object_update (object=0x8e96a98, offset=0, size=401408, resid_offset=0x0, io_errno=0x0, should_return=2, flags=<value temporarily unavailable, due to optimizations>, protection=8) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/memory_object.c:2152
                     #15 0x00252c19 in vm_object_sync (object=0x8e96a98, offset=0, size=401408, should_flush=0, should_return=1, should_iosync=2) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/memory_object.c:542
                     #16 0x0025c208 in vm_map_msync (map=0x7771290, address=4298870784, size=<value temporarily unavailable, due to optimizations>, sync_flags=34) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_map.c:12125
                     #17 0x0047a7f3 in msync_nocancel (p=0x6eec540, uap=0x78bcd48, retval=0x8eb7d54) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_mman.c:626
                     #18 0x004edaf8 in unix_syscall64 (state=0x78bcd44) at /SourceCache/xnu/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:433                     
                     */
                    upl = ap.ap_pageout->a_pl;
                    args.upl = NULL;
                    
                    //
                    // as we are using the upl from the system we need to wait for shadow completion
                    // as we can't control UPL life expectancy and do not want to set the UPL_NOCOMMIT
                    // flag as the underlying FSD is better aware what to do with UPL's pages
                    //
                    waitForShadowCompetion = TRUE;
                    
                    rangeDsc.fileOffset = ap.ap_pageout->a_f_offset;
                    rangeDsc.rangeSize  = ap.ap_pageout->a_size;
                    
                    //
                    // create a memory descriptor for the range
                    //
                    
                    options = kIOMemoryTypeUPL | kIODirectionOut;
                    
                    memDscr = IOMemoryDescriptor::withOptions( upl,
                                                               ap.ap_pageout->a_size,  // range size
                                                               0x0,                    // offset in upl
                                                               0,
                                                               options );
                    assert( memDscr );
                    if( !memDscr ){
                        
                        OSString*    fileNameRef = dldVnode->getNameRef();
                        assert( fileNameRef );
                        
                        DBG_PRINT_ERROR(("IOMemoryDescriptor::withOptions() for %s file\n", fileNameRef->getCStringNoCopy() ));
                        success = false;
                        
                        fileNameRef->release();
                        DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
                        break;
                    }
                    
                    success = args.memDscArray->setObject( memDscr );
                    assert( success );
                    //
                    // do not break here as the memDscr object must be released
                    //
                    
                    //
                    // if added the object is retained
                    // if not added the allocated resources must be released
                    //
                    memDscr->release();
                    DLD_DBG_MAKE_POINTER_INVALID( memDscr );
                    
                    if( !success )
                        break;
                    
                    rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
                    assert( rangeDscAsData );
                    if( !rangeDscAsData ){
                        
                        success = false;
                        break;
                    }
                    
                    success = args.rangeDscArray->setObject( rangeDscAsData );
                    assert( success );
                    //
                    // do not break before releasing the rangeDscAsData object
                    //
                    
                    rangeDscAsData->release();
                    
                    if( !success )
                        break;
                    
                }// end block for old pageout behavior

                break;
                
            }// end of case DldShadowTypeFilePageout
                
            default:
                panic( "a wrong type %u", (int)type );
                break;
                
        }// end switch
        
        
        if( !success )
            break;
        
        //
        // check whether the above processing has
        // found that there is no data to be paged out
        // as there are no dirty pages
        //
        if( noDirtyPages ){
            
            success = true;
            break;
        }
        
        
    }// end for
    
    
    if( !success || noDirtyPages ){
        
        if( success ){
            
            assert( noDirtyPages );
            
            //
            // this is a successful operation
            //
            *commonParams->shadowCompletionRC = kIOReturnSuccess;
            DldSetNotificationEvent( commonParams->completionEvent );
        }
        
        goto __exit;
    }
    
    
    //
    // send the requests to a worker thread
    //
    
    msg [ 0 ] = &msgHdr;
    size[ 0 ] = sizeof( msgHdr );
    
    msg [ 1 ] = &args;
    size[ 1 ] = sizeof( args );
    
    if( this->dataQueue->enqueueScatterGather( msg, size, validEntries ) ){
        
        this->dataQueue->sendDataAvailableNotificationAllThreads();
        
        if( waitForShadowCompetion ){
            
            //
            // wait for completion, usually this happens when we have borrowed
            // the system UPL instead creating our own ( creating is not possible
            // in the case of the old pageout behaviour )
            //

            DldWaitForNotificationEventWithTimeout( commonParams->completionEvent, (-1)/*wait infinitely*/ );
        }
        
    } else {
        
        success = false;
    }
    
__exit:
    
    if( !success ){
        
        if( args.memDscArray ){
            
            //
            // if the pages have been wired then unwire them
            //
            if( args.pagesAreWired ){
                
                for( int i = 0x0; i < args.memDscArray->getCount(); ++i ){
                    
                    IOMemoryDescriptor*  memDscr = OSDynamicCast( IOMemoryDescriptor, args.memDscArray->getObject( i ) );
                    assert( memDscr );
                    
                    memDscr->complete();
                }
                
            } // end if( args.pagesAreWired )
            
            args.memDscArray->flushCollection();
            args.memDscArray->release();
        }
        
        if( args.rangeDscArray ){
            
            args.rangeDscArray->flushCollection();
            args.rangeDscArray->release();
        }
        
        //
        // free the upl after the memory descriptors array was flushed
        //
        if( args.upl )
            ubc_upl_abort( args.upl, UPL_ABORT_FREE_ON_EMPTY );
        
        if( args.dldVnode )
            args.dldVnode->release();
            
    }
    
    return success;
}

//--------------------------------------------------------------------

void
DldIOShadow::processFileWriteWQ(
    __in DldShadowWriteVnodeArgs*   args
    )
{
    unsigned int   count;
    IOReturn       RC = kIOReturnSuccess;
    bool           prepareForIO = false;
    
    assert( preemption_enabled() );
    assert( args->memDscArray->getCount() == args->rangeDscArray->getCount() );
    assert( args->dldVnode );
#if defined( DBG )
    assert( DLD_SHADOW_WRVN_SIGNATURE == args->signature );
#endif//DBG
    
    if( DldShadowTypeFilePageout == args->type ){
        
        //
        // the physical memory should be "prepared" by wiring it
        // as there is no target task to provide vm objects
        //
        prepareForIO = true;
        
    }
    
    count = args->memDscArray->getCount();
    
    for( int i = 0x0; i < count; ++i ){
        

        IOMemoryDescriptor*    memDscr;
        bool                   prepared = false;
        DldRangeDescriptor     rangeDsc;
        OSData*                rangeDscAsData;
        
        //
        // the reference count is not incremented,
        // the object is retained by the array itself
        //
        rangeDscAsData = OSDynamicCast( OSData, args->rangeDscArray->getObject( i ) );
        assert( rangeDscAsData );
        if( !rangeDscAsData || ( rangeDscAsData->getLength() != sizeof( rangeDsc ) ) ){
         
            DBG_PRINT_ERROR(("OSDynamicCast( OSData, args->rangeDscArray->getObject( %u ) ) failed or data length is wrong\n", i));
            continue;
        }
        
        //
        // extract offset:length data to a local variable
        //
        memcpy( &rangeDsc, rangeDscAsData->getBytesNoCopy(), sizeof( rangeDsc ) );
        
        //
        // the reference count is not incremented,
        // the object is retained by the array itself
        //
        memDscr = OSDynamicCast( IOMemoryDescriptor, args->memDscArray->getObject( i ) );
        assert( memDscr );
        if( !memDscr ){
            
            DBG_PRINT_ERROR(("OSDynamicCast( IOMemoryDescriptor, args->memDscArray->getObject( %u ) ) failed\n", i));
            continue;
        }
        
        if( prepareForIO ){
            
            assert( !prepared );
            
            //
            // wire the physical pages
            //
            RC = memDscr->prepare( kIODirectionOut );
            assert( kIOReturnSuccess == RC );
            if( kIOReturnSuccess != RC ){
                
                DBG_PRINT_ERROR(("memDscr->prepare( kIODirectionOut )failed with the 0x%X code\n", RC));
                break;
            }
            
            prepared = true;
            
        }// end if( prepareForIO )
        
        //
        // perform IO in chunks as a 64 bit user process can provide
        // a 64 bit wide size to a 32 bit kernel process
        //
        mach_vm_size_t    fullLength;
        mach_vm_size_t    residual;
        
        fullLength = (mach_vm_size_t)memDscr->getLength();
        residual = fullLength;
        
        while( 0x0 != residual ){
            
            mach_vm_size_t    offset;// offset in the memDscr
            mach_vm_size_t    lengthToMap;
            mach_vm_size_t    mapLimit;
            
            //
            // map up to the mapLimit as the kernel
            // space might be severely fragmented,
            // in case of a upl the maximum size
            // of he provided buffer is 1MB so the
            // limits are not an issue
            //
#if defined( DBG )
            mapLimit = 0x1000*0x1000; // 16 MB
#else
            mapLimit = 0x2*0x1000*0x1000; // 32 MB this is in correspondence with the MAX_UPL_SIZE = 8192*4096
#endif//DBG
            
            offset = fullLength - residual;
            lengthToMap = (residual >= mapLimit)? mapLimit: residual;
            
            //
            // do not left a small 16 KB chunk at the end,
            // saving on such a small amont is questionable
            //
            if( (residual - lengthToMap) < 0x4*0x1000 )
                lengthToMap = residual;
            
            //
            // map user data in the kernel space, kIOMapReadOnly is essential as 
            // IOGeneralMemoryDescriptor::doMap checks for it and if absent
            // requests write access to the memory region
            //
            IOMemoryMap* map = memDscr->createMappingInTask( kernel_task, // as this is a kernel thread the current_task() returns kernel_task, but lets be more assertive
                                                             0x0,            // mach_vm_address_t  atAddress
                                                             kIOMapAnywhere | kIOMapReadOnly, // IOOptionBits		options
                                                             offset,         // mach_vm_size_t   offset
                                                             lengthToMap     // mach_vm_size_t  length
                                                             );
            if( !map ){
                
                DBG_PRINT_ERROR(("memDscr->createMappingInTask() failed\n"));
#if defined( DBG )
                //
                // enter in a debuger and call mapping again to find a reason of the failure
                //
                __asm__ volatile( "int $0x3" );
                map = memDscr->createMappingInTask( kernel_task, // task_t   intoTask
                                                    0x0,            // mach_vm_address_t  atAddress
                                                    kIOMapAnywhere | kIOMapReadOnly, // IOOptionBits		options
                                                    offset,         // mach_vm_size_t   offset
                                                    lengthToMap     // mach_vm_size_t  length
                                                   );
                if( map ){
                    
                    map->unmap();
                    map->release();
                    DLD_DBG_MAKE_POINTER_INVALID( map );
                }
#endif//DBG
                RC = kIOReturnNoMemory;
                break;
                
            }// end if( !map )
            
            //
            // this should not fail
            //
            IOVirtualAddress mappedAddr = map->getVirtualAddress();
            assert( mappedAddr );
            if( NULL == mappedAddr ){
                
                DBG_PRINT_ERROR(("map->getVirtualAddress() failed\n"));
                
                map->unmap();
                map->release();
                DLD_DBG_MAKE_POINTER_INVALID( map );
                
                RC = kIOReturnNoMemory;
                break;
                
            }//end if 
            
            //
            // get the range length
            //
            mach_vm_size_t mappedLength = (mach_vm_size_t)map->getLength();
            assert( 0x0 != mappedLength );
            
            //
            // write data ( addr, length )
            //
            
            off_t                 shadowFileOffset;
            off_t                 fullDataSize;
            DldShadowDataHeader   shadowHeader;
            
            fullDataSize = mappedLength + sizeof( DldShadowDataHeader );
            
            //
            // prepare the shadow header
            //
            bzero( &shadowHeader, sizeof( shadowHeader ) );
            
            shadowHeader.signature   = DLD_SHADOW_DATA_HEADER_SIGNATURE;
            shadowHeader.size        = fullDataSize;
            shadowHeader.type        = args->type;
            shadowHeader.operationID = args->commonParams.operationID;
            
            shadowHeader.operationData.write.vnodeID = args->dldVnode->vnodeID;
            shadowHeader.operationData.write.offset  = (UInt64)(rangeDsc.fileOffset + (rangeDsc.rangeSize - residual));
            shadowHeader.operationData.write.size    = (UInt64)mappedLength;
            
#if defined( DBG )
            shadowFileOffset = DLD_IGNR_FSIZE;
#endif//DBG
            
            //
            // get the current shadow file and reserve a room for the header and data
            //
            DldIOShadowFile*   shadowFile = this->getCurrenShadowFileReserveSpace( fullDataSize, &shadowFileOffset );
            if( shadowFile ){
                
#if defined( DBG )
                assert( DLD_IGNR_FSIZE != shadowFileOffset );
#endif//DBG
                //
                // write the header
                //
                RC = shadowFile->write( (void*)&shadowHeader, sizeof( shadowHeader ), shadowFileOffset );
                if( kIOReturnSuccess == RC ){
                    
                    //
                    // write the file data
                    //
                    RC = shadowFile->write( (void*)mappedAddr, mappedLength, shadowFileOffset + sizeof( shadowHeader ) );
                    if( kIOReturnSuccess != RC ){
                        
                        DBG_PRINT_ERROR(("shadowFile(%s)->write( %llu, %llu) failed with the 0x%X code\n",
                                         shadowFile->getPath(), mappedLength, shadowFileOffset + sizeof( shadowHeader ), RC));
                        // do not break here, this is just for logging
                    }
                    
                } else{
                    
                    DBG_PRINT_ERROR(("shadowFile(%s)->write( %u, %llu) failed to write the header with the 0x%X code\n", shadowFile->getPath(), (int)sizeof( shadowHeader ), shadowFileOffset, RC));
                    // do not break here, this is just for logging
                }
                

                //
                // send a notification to a user clent
                //
                
                DldDriverShadowNotificationInt   notifyDataInt;
                DldDriverShadowNotification      notifyData;
                
                bzero( &notifyDataInt, sizeof( notifyDataInt ) );
                bzero( &notifyData, sizeof( notifyData ) );
                
                notifyData.GeneralNotification.type         = DLD_SN_TYPE_GENERAL;
                notifyData.GeneralNotification.shadowType   = shadowHeader.type;
                notifyData.GeneralNotification.shadowFileID = shadowFile->getID();
                notifyData.GeneralNotification.offset       = shadowFileOffset;
                notifyData.GeneralNotification.dataSize     = fullDataSize;
                notifyData.GeneralNotification.kRC          = RC;
                
                notifyDataInt.notifyDataSize = sizeof( notifyData );
                notifyDataInt.notifyData = &notifyData;
                
                RC = this->shadowNotification( &notifyDataInt );
                
                //
                // not it is time to release the file
                //
                shadowFile->release();
                DLD_DBG_MAKE_POINTER_INVALID( shadowFile );
                //
                // do not exit as the mapping must be destroyed
                //
                
            } else {
                
                //
                // there is no any shadow file to write data in
                //
                RC = kIOReturnNoResources;
            }
            
            //
            // unmap data
            //
            map->unmap();
            map->release();
            DLD_DBG_MAKE_POINTER_INVALID( map );
            
            //
            // exith the while loop in case of error
            //
            if( kIOReturnSuccess != RC )
                break;
            
            assert( kIOReturnSuccess == RC );
            
            //
            // change the residual bytes count to account for the current range
            //
            residual = residual - mappedLength;
            
        }// end while( 0x0 != residual )
        
        
        if( prepared ){
            
            IOReturn  compRC;
            
            //
            // wire the physical pages
            //
            compRC = memDscr->complete();
            assert( kIOReturnSuccess == compRC );
            if( kIOReturnSuccess != compRC ){
                
                DBG_PRINT_ERROR(("memDscr->complete() failed with the 0x%X code\n", compRC));
                
                if( kIOReturnSuccess == RC )
                    RC = compRC;
            }
            
        }// end if( prepareForIO )
        
        //
        // exit the for loop in case of error
        //
        if( kIOReturnSuccess != RC )
            break;
        
    }// end for( int i = 0x0; i < count; ++i )
    
    if( args->commonParams.shadowCompletionRC )
        *(args->commonParams.shadowCompletionRC) = RC;
    
    //
    // report about shadow completion
    //
    DldSetNotificationEvent( args->commonParams.completionEvent );
    
    //
    // if the pages have been wires then unwire them
    //
    if( args->pagesAreWired ){
        
        for( int i = 0x0; i < args->memDscArray->getCount(); ++i ){
            
            IOMemoryDescriptor*  memDscr = OSDynamicCast( IOMemoryDescriptor, args->memDscArray->getObject( i ) );
            assert( memDscr );
            
            memDscr->complete();
        }
        
    } // end if( args.pagesAreWired )
    
    //
    // free the allocated resources
    //
    
    //
    // destroy all memory objects
    //
    args->memDscArray->flushCollection();
    args->memDscArray->release();
     
    //
    // destroy all range descriptors objects
    //
    args->rangeDscArray->flushCollection();
    args->rangeDscArray->release();
    
    if( args->upl ){
        
        //
        // the memory descriptors must be freed before upl
        //
        
        //
        // if there is a pointer to upl then the upl
        // has been allocated by us and must be freed
        //
        assert( DldShadowTypeFilePageout == args->type );
        assert( args->upl != args->ap.ap_pageout.a_pl );
        
        ubc_upl_abort( args->upl, UPL_ABORT_FREE_ON_EMPTY );
        
    }// end if( args->upl )
    
    if( args->dldVnode )
        args->dldVnode->release();
    
}

//--------------------------------------------------------------------

//
// if false is returned the *commonParams->shadowCompletionRC value
// is invalid, if true is returned it contains a valid value
// after commonParams->completionEvent has been set to a signal state,
//
bool DldIOShadow::shadowFileWrite(
    __in  DldIOVnode* dldVnode,
    __in  struct vnop_write_args *ap,
    __inout DldCommonShadowParams* commonParams
    )
/*
 struct vnop_write_args {
 struct vnodeop_desc *a_desc;
 vnode_t a_vp;
 struct uio *a_uio;
 int a_ioflag;
 vfs_context_t a_context;
 };
 */
{
    DldWriteVnode_ap ap_union;
    
    ap_union.ap_write = ap;
    
    return this->shadowFileWriteInt( DldShadowTypeFileWrite,
                                     dldVnode,
                                     ap_union,
                                     commonParams );
}

//--------------------------------------------------------------------

//
// if false is returned the *commonParams->shadowCompletionRC value
// is invalid, if true is returned it contains a valid value
// after commonParams->completionEvent has been set to a signal state,
//
bool DldIOShadow::shadowFilePageout(
    __in DldIOVnode* dldVnode,
    __in struct vnop_pageout_args *ap,
    __inout DldCommonShadowParams* commonParams
    )
/*
 struct vnop_pageout_args {
 vnode_t a_vp,
 upl_t         a_pl,
 vm_offset_t   a_pl_offset,
 off_t         a_f_offset,
 size_t        a_size,
 int           a_flags
 vfs_context_t a_context;
 };
 */
{
    
    DldWriteVnode_ap ap_union;
    
    ap_union.ap_pageout = ap;
    
    return this->shadowFileWriteInt( DldShadowTypeFilePageout,
                                     dldVnode,
                                     ap_union,
                                     commonParams );
    
}

//--------------------------------------------------------------------

void DldIOShadow::shadowCSITaskCompletionCallbackHook( __in SCSITaskIdentifier   request )
{
    assert( preemption_enabled() );
    
    DldSCSITask* referncedTask;
    
    referncedTask = DldSCSITask::GetReferencedSCSITask( request );
    assert( referncedTask );
    if( !referncedTask ){
        
        DBG_PRINT_ERROR(("DldSCSITask::GetReferencedSCSITask( 0x%p ) returned NULL, this is an error!", request));
        return;
    }
    
    //
    // wait for the shadow completion, see processSCSIOperationWQ()
    //
    referncedTask->waitForShadowCompletion();
    
    //
    // shadow the operation result
    //
    DldShadowOperationCompletion  comp;
    
    bzero( &comp, sizeof( comp ) );
    
    comp.operationID     = referncedTask->getShadowOperationID();
    comp.retval          = referncedTask->GetTaskStatus();// so this is not quite IOReturn
    comp.shadowingRetval = referncedTask->getShadowStatus();
    
    comp.additionalData.scsiOperationStatus.serviceResponse			  = referncedTask->GetServiceResponse();
    comp.additionalData.scsiOperationStatus.taskStatus				  = referncedTask->GetTaskStatus();
    comp.additionalData.scsiOperationStatus.realizedDataTransferCount = referncedTask->GetRealizedDataTransferCount();
    
    gShadow->shadowOperationCompletion( NULL, &comp );
    
    //
    // call an original completion callback
    //
    referncedTask->CallOriginalCompletion();
    
    //
    // the first refernce came from GetReferencedSCSITask()
    //
    referncedTask->release();
    
    //
    // we don't need the task anymore
    //
    referncedTask->release();
}

//--------------------------------------------------------------------

//
// if false is returned the *commonParams->shadowCompletionRC value
// is invalid, if true is returned it contains a valid value
// after commonParams->completionEvent has been set to a signal state,
//
bool
DldIOShadow::shadowSCSIOperation(
    __in DldIOService*      dldService,
    __in SCSITaskIdentifier request,
    __inout DldCommonShadowParams* commonParams
    )
{
    assert( preemption_enabled() );
    
    DldShadowSCSIArgs  scsiArgs;
    DldShadowMsgHdr    msgHdr;
    void*              msg [ 2 ];
    UInt32             size[ 2 ];
    UInt32             validEntries = 2;
    
    DldSCSITask*  dldRequest = DldSCSITask::withSCSITaskIdentifier( request, dldService );
    if( !dldRequest ){
        
        DBG_PRINT_ERROR(("DldSCSITask::withSCSITaskIdentifier( 0x%p ) failed\n", request));
        return false;
    }
    
    //
    // remember the operation ID
    //
    dldRequest->setShadowOperationID( commonParams->operationID );
    
    //
    // wire the buffers now as the prepare() is not thread safe so do the job in the context
    // of the caller who creates the memory descriptor ( this is usually a thread
    // in which a SCSITaskUserClient has been called which creates the descriptor )
    //
    if( kIOReturnSuccess != dldRequest->prepareDataBuffer() ){
        
        DBG_PRINT_ERROR(("DldSCSITask::prepareDataBuffer( 0x%p ) failed\n", request));
        dldRequest->release();
        return false;
    }
    
    if( dldRequest->GetDataBuffer() ){
        
        //
        // the current design assumes that this is a write request
        //
        assert( 0x0 != ( kIODirectionOut & dldRequest->GetDataBuffer()->getDirection() ) );
        if( 0x0 == ( kIODirectionOut & dldRequest->GetDataBuffer()->getDirection() ) ){
            
            DBG_PRINT_ERROR(( "SCSITask data buffer is not for write, direction=%u\n", dldRequest->GetDataBuffer()->getDirection() ));
            dldRequest->release();
            return false;
        }
    }
    
    //
    // hook the completion to report the result to the shadow subsystem, the 
    // dldRequest will be freed by the DldSCSITask::shadowCSITaskCompletionCallbackHook
    //
    if( !dldRequest->SetTaskCompletionCallbackHook( DldIOShadow::shadowCSITaskCompletionCallbackHook ) ){
        
        DBG_PRINT_ERROR(("DldSCSITask::SetTaskCompletionCallbackHook( 0x%p ) failed\n", request));
        
        dldRequest->release();
        return false;
    }
    
    //
    // from that point the request completion callback is hooked and the hook will wait
    // for the shadow completion on the event!
    //
    
#if defined( DBG )
    msgHdr.signature   = DLD_SHADOW_HDR_SIGNATURE;
    scsiArgs.signature = DLD_SHADOW_SCSI_SIGNATURE;
#endif//DBG
    
    msgHdr.type = DldShadowTypeSCSICommand;
    
    scsiArgs.dldSCSITask  = dldRequest;
    scsiArgs.commonParams = *commonParams;
    
    //
    // send the requests to a worker thread
    //
    
    msg [ 0 ] = &msgHdr;
    size[ 0 ] = sizeof( msgHdr );
    
    msg [ 1 ] = &scsiArgs;
    size[ 1 ] = sizeof( scsiArgs );
    
    if( this->dataQueue->enqueueScatterGather( msg, size, validEntries ) ){
        
        this->dataQueue->sendDataAvailableNotificationAllThreads();
        
    } else {
        
        //
        // posting failed
        //
        DBG_PRINT_ERROR(("enqueueScatterGather() failed, request=0x%p\n", request));
        
        //
        // unhook to avoid waiting for shadow completion
        //
        dldRequest->RestoreOriginalTaskCompletionCallback();
        dldRequest->release();
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

void
DldIOShadow::processSCSIOperationWQ(
    __in DldShadowSCSIArgs*   scsiArgs
    )
{
    assert( preemption_enabled() );
#if defined( DBG )
    assert( DLD_SHADOW_SCSI_SIGNATURE == scsiArgs->signature );
#endif//DBG
    IOReturn             RC = kIOReturnSuccess;
    DldSCSITask*         scsiTask;
    IOByteCount          residual;// bytes left to shadow
    IOByteCount          offset;// offset in the dataBuffer descriptor
    IOByteCount          dataLength;// full SCSI data length
    IOMemoryDescriptor*  dataBuffer;// SCSI data buffer
    void*                chunkBuffer;// an intermediate buffer to read data
    vm_size_t            chunkSize;// a size for the intermediate buffer
    
    offset      = 0x0;
    chunkBuffer = NULL;
    dataLength  = 0x0;
    //chunkSize   = 64*1024;
    chunkSize   = 128*1024;//128 KB, this is a size for a single SCSI request issued by the standard Mac OS X's disk burner
    
    scsiTask   = scsiArgs->dldSCSITask;
    
    assert( scsiTask->getDldIOService() );
    assert( scsiTask->getShadowOperationID() );
    
    dataBuffer = scsiTask->GetDataBuffer();
    if( dataBuffer ){
        
        dataLength = scsiTask->GetRequestedDataTransferCount();
        offset     = scsiTask->GetDataBufferOffset();
        assert( (offset+dataLength) <= dataBuffer->getLength() );
        
    }// end if( dataBuffer )
    
    if( chunkSize > dataLength )
        chunkSize = dataLength;
    
    residual = dataLength;
    
    if( dataLength ){
        
        //
        // allocate a buffer for a chunk
        //
        do{
            
            chunkBuffer = IOMalloc( chunkSize );
            if( !chunkBuffer )
                chunkSize = chunkSize/2;// try to be more thrifty
            
            //
            // continue attempts until the low bound of 8KB is not reached
            //
        }while( !chunkBuffer && chunkSize>(8*1024) );
        
        if( !chunkBuffer ){
            
            DBG_PRINT_ERROR(("IOMalloc( %u ) failed\n", (unsigned int)chunkSize ));
            RC = kIOReturnNoMemory;
            goto __exit;
        }
    }
    
    do{
        
        IOByteCount           bytesToCopy;
        off_t                 fullDataSize;
        DldShadowDataHeader   shadowHeader;
        off_t                 shadowFileOffset;
        
        bytesToCopy = (residual < chunkSize)? residual : chunkSize;
        if( bytesToCopy ){
            
            //
            // read the current chunk
            //
            bytesToCopy = dataBuffer->readBytes( offset, chunkBuffer, bytesToCopy );
            assert( bytesToCopy );
            assert( residual >= bytesToCopy );
            
        }// end if( bytesToCopy )
        
        fullDataSize = bytesToCopy + sizeof( DldShadowDataHeader );
        
        //
        // prepare the shadow header
        //
        bzero( &shadowHeader, sizeof( shadowHeader ) );
        
        shadowHeader.signature   = DLD_SHADOW_DATA_HEADER_SIGNATURE;
        shadowHeader.size        = fullDataSize;
        shadowHeader.type        = DldShadowTypeSCSICommand;
        shadowHeader.operationID = scsiArgs->commonParams.operationID;
        
        shadowHeader.operationData.SCSI.serviceID   = scsiTask->getDldIOService()->getServiceID();
        shadowHeader.operationData.SCSI.commandSize = scsiTask->GetCommandDescriptorBlockSize();
        shadowHeader.operationData.SCSI.offset      = offset;
        shadowHeader.operationData.SCSI.dataSize    = bytesToCopy;
        scsiTask->GetCommandDescriptorBlock( &shadowHeader.operationData.SCSI.cdb );
        
#if defined( DBG )
        shadowFileOffset = DLD_IGNR_FSIZE;
#endif//DBG
        
        //
        // get the current shadow file and reserve a room for the header and data
        //
        DldIOShadowFile*   shadowFile = this->getCurrenShadowFileReserveSpace( fullDataSize, &shadowFileOffset );
        if( shadowFile ){
            
#if defined( DBG )
            assert( DLD_IGNR_FSIZE != shadowFileOffset );
#endif//DBG
            //
            // write the header
            //
            RC = shadowFile->write( (void*)&shadowHeader, sizeof( shadowHeader ), shadowFileOffset );
            if( kIOReturnSuccess == RC ){
                
                //
                // write the file data
                //
                if( bytesToCopy ){
                    
                    RC = shadowFile->write( (void*)chunkBuffer, bytesToCopy, shadowFileOffset + sizeof( shadowHeader ) );
                    if( kIOReturnSuccess != RC ){
                        
                        DBG_PRINT_ERROR(("shadowFile(%s)->write( %llu, %llu) failed with the 0x%X code\n",
                                         shadowFile->getPath(), (long long)bytesToCopy, shadowFileOffset + sizeof( shadowHeader ), RC));
                        // do not break here, this is just for logging
                    }
                    
                }// end if( bytesToCopy )
                
            } else{
                
                DBG_PRINT_ERROR(("shadowFile(%s)->write( %u, %llu) failed to write the header with the 0x%X code\n", shadowFile->getPath(), (int)sizeof( shadowHeader ), shadowFileOffset, RC));
                // do not break here, this is just for logging
            }
            
            
            //
            // send a notification to a user clent
            //
            
            DldDriverShadowNotificationInt   notifyDataInt;
            DldDriverShadowNotification      notifyData;
            
            bzero( &notifyDataInt, sizeof( notifyDataInt ) );
            bzero( &notifyData, sizeof( notifyData ) );
            
            notifyData.GeneralNotification.type         = DLD_SN_TYPE_GENERAL;
            notifyData.GeneralNotification.shadowType   = shadowHeader.type;
            notifyData.GeneralNotification.shadowFileID = shadowFile->getID();
            notifyData.GeneralNotification.offset       = shadowFileOffset;
            notifyData.GeneralNotification.dataSize     = fullDataSize;
            notifyData.GeneralNotification.kRC          = RC;
            
            notifyDataInt.notifyDataSize = sizeof( notifyData );
            notifyDataInt.notifyData = &notifyData;
            
            RC = this->shadowNotification( &notifyDataInt );
            
            //
            // not it is time to release the file
            //
            shadowFile->release();
            DLD_DBG_MAKE_POINTER_INVALID( shadowFile );
            
        } else {
            
            //
            // there is no any shadow file to write data in
            //
            RC = kIOReturnNoResources;
        }

        if( kIOReturnSuccess != RC )
            break;
        
        //
        // move to the next chunk
        //
        assert( residual >= bytesToCopy );
        offset   += bytesToCopy;
        residual -= bytesToCopy;
        
    } while( 0x0 != residual );
    
__exit:
    
    if( chunkBuffer )
        IOFree( chunkBuffer, chunkSize );
    
    //
    // remember the shadowing status for this request to report it to the service on completion
    //
    scsiTask->setShadowStatus( RC );
    
    //
    // sen an event on which a callback hook is waiting or will be waiting for shadow completion
    //
    scsiTask->setShadowCompletionEvent();
    
    //
    // check that if there is memory allocated for the returned value
    // then there is an event on which the thread is waiting
    // as usualy the memory is allocated on the stack
    //
    assert( !( scsiArgs->commonParams.shadowCompletionRC && !scsiArgs->commonParams.completionEvent )  );
    
    if( scsiArgs->commonParams.shadowCompletionRC )
        *(scsiArgs->commonParams.shadowCompletionRC) = RC;
    
    //
    // report about shadow completion
    //
    if( scsiArgs->commonParams.completionEvent )
        DldSetNotificationEvent( scsiArgs->commonParams.completionEvent );
    
}

//--------------------------------------------------------------------

bool
DldIOShadow::shadowOperationCompletion(
    __in_opt DldIOVnode* dldVnode,//optional
    __in     DldShadowOperationCompletion* comp
    )
{
    
    DldShadowOperationCompletionArgs  compArgs;
    DldShadowMsgHdr           msgHdr;
    void*                     msg [ 2 ];
    UInt32                    size[ 2 ];
    UInt32                    validEntries = 2;
    bool                      success = true;
    
    bzero( &msgHdr, sizeof( msgHdr ) );
    bzero( &compArgs, sizeof( compArgs ) );
    
#if defined( DBG )
    msgHdr.signature   = DLD_SHADOW_HDR_SIGNATURE;
    compArgs.signature = DLD_SHADOW_COMP_SIGNATURE;
#endif//DBG
    
    msgHdr.type = DldShadowTypeOperationCompletion;
    
    compArgs.comp = *comp;
    compArgs.dldVnode = dldVnode;
    
    //
    // reference the vnode, it will be released by shadowOperationCompletionWQ
    //
    if( compArgs.dldVnode )
        compArgs.dldVnode->retain();
    
    //
    // send the requests to a worker thread
    //
    
    msg [ 0 ] = &msgHdr;
    size[ 0 ] = sizeof( msgHdr );
    
    msg [ 1 ] = &compArgs;
    size[ 1 ] = sizeof( compArgs );
    
    //
    // send and forget, no any completion event
    //
    if( this->dataQueue->enqueueScatterGather( msg, size, validEntries ) ){
        
        this->dataQueue->sendDataAvailableNotificationAllThreads();
        
    } else {
        
        if( compArgs.dldVnode )
            compArgs.dldVnode->release();
        
        success = false;
    }
    
    return success;
}

//--------------------------------------------------------------------

void
DldIOShadow::shadowOperationCompletionWQ(
    __in DldShadowOperationCompletionArgs*  compArgs
    )
{
    //
    // there is no way to report errors on completion shadowing,
    // in any case it is too late to undo an operation
    //
    
    off_t                 shadowFileOffset;
    off_t                 fullDataSize;
    DldShadowDataHeader   shadowHeader;
    IOReturn              RC = kIOReturnNoResources;
    
    fullDataSize = sizeof( DldShadowDataHeader );
    
    //
    // prepare the shadow header
    //
    bzero( &shadowHeader, sizeof( shadowHeader ) );
    
    shadowHeader.signature   = DLD_SHADOW_DATA_HEADER_SIGNATURE;
    shadowHeader.size        = fullDataSize;
    shadowHeader.type        = DldShadowTypeOperationCompletion;
    shadowHeader.operationID = compArgs->comp.operationID;
    
    shadowHeader.operationData.completion.vnodeID         = compArgs->dldVnode? compArgs->dldVnode->vnodeID: 0x0ll;
    shadowHeader.operationData.completion.retval          = compArgs->comp.retval;
    shadowHeader.operationData.completion.shadowingRetval = compArgs->comp.shadowingRetval;
    memcpy( &shadowHeader.operationData.completion.additionalData,
            &compArgs->comp.additionalData,
            min(sizeof(compArgs->comp.additionalData), sizeof(shadowHeader.operationData.completion.additionalData)));
    //
    // the structures must be synchronized!
    //
    assert( sizeof(compArgs->comp.additionalData) == sizeof(shadowHeader.operationData.completion.additionalData) );
    
#if defined( DBG )
    shadowFileOffset = DLD_IGNR_FSIZE;
#endif//DBG
    
    //
    // get the current shadow file and reserve a room for the header and data
    //
    DldIOShadowFile*   shadowFile = this->getCurrenShadowFileReserveSpace( fullDataSize, &shadowFileOffset );
    if( shadowFile ){
        
#if defined( DBG )
        assert( DLD_IGNR_FSIZE != shadowFileOffset );
#endif//DBG
        
        //
        // write the header, there is no data for completion
        //
        RC = shadowFile->write( (void*)&shadowHeader, sizeof( shadowHeader ), shadowFileOffset );
        if( kIOReturnSuccess != RC ){
            
            DBG_PRINT_ERROR(("shadowFile(%s)->write( %u, %llu) failed to write the header with the %u code\n",
                             shadowFile->getPath(), (int)sizeof( shadowHeader ), shadowFileOffset, RC));
            // do not break here, this is just for logging
        }
        
        //
        // send a notification to a user clent
        //
        
        DldDriverShadowNotificationInt   notifyDataInt;
        DldDriverShadowNotification      notifyData;
        
        bzero( &notifyDataInt, sizeof( notifyDataInt ) );
        bzero( &notifyData, sizeof( notifyData ) );
        
        notifyData.GeneralNotification.type         = DLD_SN_TYPE_GENERAL;
        notifyData.GeneralNotification.shadowType   = shadowHeader.type;
        notifyData.GeneralNotification.offset       = shadowFileOffset;
        notifyData.GeneralNotification.dataSize     = fullDataSize;
        notifyData.GeneralNotification.shadowFileID = shadowFile->getID();
        notifyData.GeneralNotification.kRC          = RC;
        
        notifyDataInt.notifyDataSize = sizeof( notifyData );
        notifyDataInt.notifyData     = &notifyData;
        
        RC = this->shadowNotification( &notifyDataInt );
        assert( kIOReturnSuccess == RC );
        if( kIOReturnSuccess != RC ){
            
            DBG_PRINT_ERROR(("currentClient->shadowNotification( notificationData ) failed, error=0x%xX\n", RC));
        }
        
        //
        // now it is time to release the file
        //
        shadowFile->release();
        DLD_DBG_MAKE_POINTER_INVALID( shadowFile );
        
    }// end if( shadowFile )
    
    //
    // vnode was retained when its pointer was saved in the structure
    //
    if( compArgs->dldVnode )
        compArgs->dldVnode->release();
        
}

//--------------------------------------------------------------------

void
DldIOShadow::shadowCreateCloseWQ(
    __in DldShadowCreateCloseVnode*  args
    )
{
    //
    // there is no way to report errors on completion shadowing,
    // in any case it is too late to undo an operation
    //
    
    off_t                 shadowFileOffset;
    off_t                 fullDataSize;
    DldShadowDataHeader   shadowHeader;
    IOReturn              RC = kIOReturnNoResources;
    
    fullDataSize = sizeof( DldShadowDataHeader ) + (args->logData? args->logData->logDataSize: 0x0);
    
    //
    // prepare the shadow header
    //
    bzero( &shadowHeader, sizeof( shadowHeader ) );
    
    shadowHeader.signature   = DLD_SHADOW_DATA_HEADER_SIGNATURE;
    shadowHeader.size        = fullDataSize;
    shadowHeader.type        = args->type;
    shadowHeader.operationID = args->commonParams.operationID;
    
    switch( args->type ){
        case DldShadowTypeFileOpen:
            
            shadowHeader.operationData.open.vnodeID = args->dldVnode->vnodeID;
            break;
            
        case DldShadowTypeFileClose:
            
            shadowHeader.operationData.close.vnodeID = args->dldVnode->vnodeID;
            break;
            
        case DldShadowTypeFileReclaim:
            
            shadowHeader.operationData.reclaim.vnodeID = args->dldVnode->vnodeID;
            break;
            
        default:
            panic("unlnown type");
            break;
    }// end switch( args->type )
    
#if defined( DBG )
    shadowFileOffset = DLD_IGNR_FSIZE;
#endif//DBG
    
    //
    // get the current shadow file and reserve a room for the header and data
    //
    DldIOShadowFile*   shadowFile = this->getCurrenShadowFileReserveSpace( fullDataSize, &shadowFileOffset );
    if( shadowFile ){
        
#if defined( DBG )
        assert( DLD_IGNR_FSIZE != shadowFileOffset );
#endif//DBG
        
        //
        // write the header
        //
        RC = shadowFile->write( (void*)&shadowHeader, sizeof( shadowHeader ), shadowFileOffset );
        if( kIOReturnSuccess != RC ){
            
            DBG_PRINT_ERROR(("shadowFile(%s)->write( %u, %llu) failed to write the header with the 0x%X code\n",
                             shadowFile->getPath(), (int)sizeof( shadowHeader ), shadowFileOffset, RC));
            // do not break here, this is just for logging
        }
        
        if( kIOReturnSuccess == RC && args->logData ){
            
            //
            // write the log data
            //
            RC = shadowFile->write( (void*)args->logData->logData, args->logData->logDataSize, shadowFileOffset + sizeof( shadowHeader ) );
            if( kIOReturnSuccess != RC ){
                
                //
                // can fal when for example Spotlight regenerates its database
                //
                DBG_PRINT_ERROR(("shadowFile(%s)->write(%u, %llu) for data failed with RC=0x%X\n",
                                 shadowFile->getPath(), (int)args->logData->logDataSize, shadowFileOffset + sizeof( shadowHeader ), RC));
                // do not break here, this is just for logging
            }
        }
        
        //
        // send a notification to a user clent
        //
        
        DldDriverShadowNotificationInt   notifyDataInt;
        DldDriverShadowNotification      notifyData;
        
        bzero( &notifyDataInt, sizeof( notifyDataInt ) );
        bzero( &notifyData, sizeof( notifyData ) );
        
        notifyData.GeneralNotification.type         = DLD_SN_TYPE_GENERAL;
        notifyData.GeneralNotification.shadowType   = shadowHeader.type;
        notifyData.GeneralNotification.offset       = shadowFileOffset;
        notifyData.GeneralNotification.dataSize     = fullDataSize;
        notifyData.GeneralNotification.shadowFileID = shadowFile->getID();
        notifyData.GeneralNotification.kRC          = RC;
        
        notifyDataInt.notifyDataSize = sizeof( notifyData );
        notifyDataInt.notifyData     = &notifyData;
        
        RC = this->shadowNotification( &notifyDataInt );
        //assert( kIOReturnSuccess == RC );
        if( kIOReturnSuccess != RC ){
            
            //
            // can fal when for example Spotlight regenerates its database
            //
            DBG_PRINT_ERROR(("this->shadowNotification( &notifyDataInt ) failed with RC=0x%X\n", RC));
        }
        
        //
        // now it is time to release the file
        //
        shadowFile->release();
        DLD_DBG_MAKE_POINTER_INVALID( shadowFile );
        
    }// end if( shadowFile )
    
    //
    // check that if there is memory allocated for the returned value
    // then there is an event on which the thread is waiting
    // as usualy the memory is allocated on the stack
    //
    assert( !( args->logData && !args->commonParams.completionEvent ) );
    assert( !( args->commonParams.shadowCompletionRC && !args->commonParams.completionEvent )  );
    
    if( args->commonParams.shadowCompletionRC )
        *(args->commonParams.shadowCompletionRC) = RC;
    
    //
    // report about shadow completion
    //
    if( args->commonParams.completionEvent )
        DldSetNotificationEvent( args->commonParams.completionEvent );
    
    //
    // vnode was retained when its pointer was saved in the structure
    //
    if( args->dldVnode )
        args->dldVnode->release();
    
}

//--------------------------------------------------------------------

bool DldIOShadow::shadowFileOpenCloseInt(
    __in          DldShadowType  type,
    __in          DldIOVnode* dldVnode,
    __in __opt    DldDriverDataLogInt*  logData,
    __inout __opt DldCommonShadowParams* commonParams
    )
{
    DldShadowCreateCloseVnode  args;
    DldShadowMsgHdr            msgHdr;
    void*                      msg [ 2 ];
    UInt32                     size[ 2 ];
    UInt32                     validEntries = 2;
    bool                       success = true;
    
    assert( preemption_enabled() );
    assert( DldShadowTypeFileOpen == type || DldShadowTypeFileClose == type || DldShadowTypeFileReclaim == type );
    assert( dldVnode );
    
    //
    // if the log data is not NULL the event is required for waiting as the caller allocates
    // the memory on the stack
    //
    assert( !( logData && !commonParams) );
    assert( !( commonParams && !commonParams->completionEvent ) );
    
    bzero( &args, sizeof( args ) );
    bzero( &msgHdr, sizeof( msgHdr ) );
    
    msgHdr.type = type;
    args.type   = type;
    
#if defined( DBG )
    msgHdr.signature  = DLD_SHADOW_HDR_SIGNATURE;
    args.signature    = DLD_SHADOW_OCR_SIGNATURE;
#endif//DBG
    
    args.dldVnode = dldVnode;
    dldVnode->retain();
    
    //
    // logData might be NULL
    //
    args.logData = logData;
    
    if( commonParams ){
        
        //
        //init the event as there is no requirement for the caller to do this
        //
        DldInitNotificationEvent( commonParams->completionEvent );
        args.commonParams = *commonParams;
        
    } else {
        
        //
        // there is no commonParam from the caller
        // but we need an uniqueID in any case
        //
        args.commonParams.operationID = this->generateUniqueID();
    }
    
    //
    // send the requests to a worker thread
    //
    
    msg [ 0 ] = &msgHdr;
    size[ 0 ] = sizeof( msgHdr );
    
    msg [ 1 ] = &args;
    size[ 1 ] = sizeof( args );
    
    success = this->dataQueue->enqueueScatterGather( msg, size, validEntries );
    if( success ){
        
        this->dataQueue->sendDataAvailableNotificationAllThreads();
        
        if( NULL == commonParams ){
            
            //
            // send a fake completion as reclam should not be delayed
            // waiting for shadowing as this might penalise an unrelated
            // application
            //
            
            DldShadowOperationCompletion  comp;
            
            bzero( &comp, sizeof( comp ) );
            comp.operationID = args.commonParams.operationID;
            comp.retval      = KERN_SUCCESS;
            
            gShadow->shadowOperationCompletion( dldVnode, &comp );
            
        }
        
    } else {
        
        if( args.dldVnode )
            args.dldVnode->release();
        
    }    
    
    return success;
}

//--------------------------------------------------------------------

bool DldIOShadow::shadowFileOpen(
    __in DldIOVnode* dldVnode,
    __in DldDriverDataLogInt*  logData,
    __inout DldCommonShadowParams* commonParams
    )
{
    return this->shadowFileOpenCloseInt( DldShadowTypeFileOpen,
                                         dldVnode,
                                         logData,
                                         commonParams );
}

//--------------------------------------------------------------------

bool DldIOShadow::shadowFileClose(
    __in DldIOVnode* dldVnode,
    __inout DldCommonShadowParams* commonParams
    )
{
    return this->shadowFileOpenCloseInt( DldShadowTypeFileClose,
                                         dldVnode,
                                         NULL,
                                         commonParams );
}

//--------------------------------------------------------------------

bool DldIOShadow::shadowFileReclaim( __in DldIOVnode* dldVnode )
{
    return this->shadowFileOpenCloseInt( DldShadowTypeFileReclaim,
                                         dldVnode,
                                         NULL,
                                         NULL );
}

//--------------------------------------------------------------------

void
DldIOShadow::LockShared()
{   
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

void
DldIOShadow::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldIOShadow::LockExclusive()
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
    
};

//--------------------------------------------------------------------

void
DldIOShadow::UnLockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() == this->exclusiveThread );
    this->exclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

