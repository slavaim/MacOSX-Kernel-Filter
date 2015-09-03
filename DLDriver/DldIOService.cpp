/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOService.h"
#include "DldSupportingCode.h"

#define super DldIORegistryEntry

OSDefineMetaClassAndStructors( DldIOService, DldIORegistryEntry )

static UInt32   gServiceIDGenerator = 0x0;

#if defined( DBG )
static SInt32  gDldIOServiceCount = 0x0;
#endif//DBG

//--------------------------------------------------------------------

void
DldIOService::LockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

void
DldIOService::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldIOService::LockExclusive()
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
DldIOService::UnLockExclusive()
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

DldIOService*
DldIOService::RetrieveDldIOServiceForIOService( __in IOService* service )
{
    DldIORegistryEntry*   entry;
    
    assert( DldRegistryEntriesHashTable::sHashTable );
    assert( preemption_enabled() );
    
    DldRegistryEntriesHashTable::sHashTable->LockShared();
    {// start of the lock
            
        //
        // the returned entry is refernced
        //
        entry = DldRegistryEntriesHashTable::sHashTable->RetrieveObjectEntry( (OSObject*)service );
        
    }// end of he lock
    DldRegistryEntriesHashTable::sHashTable->UnLockShared();
    
    if( !entry )
        return NULL;
    
    DldIOService*   DldService;
    
    DldService = OSDynamicCast( DldIOService, entry );
    assert( DldService );
    
    //
    // return a referenced object
    //
    return DldService;
}

//--------------------------------------------------------------------

DldIOService*
DldIOService::withIOServiceAddToHash(
    __in IOService* service,
    __in bool postponePropertyUpdate
    )
/*
 this function has an idempotent semantic!
 */
{
    
    assert( DldRegistryEntriesHashTable::sHashTable );
    assert( gDldDeviceTreePlan );
    
    if( !DldRegistryEntriesHashTable::sHashTable )
        return NULL;
    
    //
    // allocate a new object before acquiring the lock as creating a new
    // entry under the lock protection is an overkill solution prone to deadlocks
    //
    DldIOService* serviceEntry = DldIOService::withIOService( service, postponePropertyUpdate );
    assert( serviceEntry );
    if( !serviceEntry )
        return NULL;
    
    DldIORegistryEntry* entryFoundInHash = NULL;
    bool bAddedInHash = false;
    
    DldRegistryEntriesHashTable::sHashTable->LockExclusive();
    {// start of the lock
        
        assert( !entryFoundInHash && !bAddedInHash );
        
        //
        // check that the DldIoService object for the service object
        // has not been already inserted in the hash table
        //
        entryFoundInHash = DldRegistryEntriesHashTable::sHashTable->RetrieveObjectEntry( service );
        if( !entryFoundInHash ){
            
            //
            // add a new object in the hash table
            //
            bAddedInHash = DldRegistryEntriesHashTable::sHashTable->AddObject( service, serviceEntry ) ;
            assert( bAddedInHash );
            
        }
        
    }// end of the lock
    DldRegistryEntriesHashTable::sHashTable->UnLockExclusive();
    
    if( !bAddedInHash ){
        
        assert( DLD_IS_POINTER_VALID( serviceEntry ) );
        
        if( !entryFoundInHash ){
            
            assert( !"DldIOService::withIOServiceAddToHash sHashTable->AddObject( service, Entry ) failed" );
            DBG_PRINT_ERROR( ( "DldIOService::withIOServiceAddToHash sHashTable->AddObject( service, Entry ) failed\n" ) );
            
        }// end if( !FoundInHash )
        
        //
        // the entry was not added to the hash tabe
        //
        serviceEntry->release();
        DLD_DBG_MAKE_POINTER_INVALID( serviceEntry );
        
        serviceEntry = OSDynamicCast( DldIOService, entryFoundInHash );
        assert( serviceEntry );
        if( !serviceEntry )
            entryFoundInHash->release();
        
    }// end if( !bAdded )
    
    assert( DLD_IS_POINTER_VALID( serviceEntry ) );
    
    return serviceEntry;
}

//--------------------------------------------------------------------

DldIOService*
DldIOService::withIOService( __in IOService* service, __in bool postponePropertyUpdate )
{
    DldIOService*   dldIOService;
    
    dldIOService = new DldIOService();
    assert( dldIOService );
    if( !dldIOService )
        return NULL;
    
    dldIOService->service   = service;
    dldIOService->serviceID = OSIncrementAtomic( &gServiceIDGenerator )+0x1;
    
    dldIOService->rwLock = IORWLockAlloc();
    assert( dldIOService->rwLock );
    if( !dldIOService->rwLock ){
        
        dldIOService->release();
        return NULL;
    }
    
    OSDictionary * PropertyDictionary;
    PropertyDictionary = service->dictionaryWithProperties();
    
    if( PropertyDictionary ){
        
        if( !dldIOService->init( PropertyDictionary ) ){
            
            PropertyDictionary->release();
            dldIOService->release();
            return NULL;
            
        }// end if( !dldIOService->init(
        
        PropertyDictionary->release();
        
    } else {
        
        if( !dldIOService->init() ){
            
            dldIOService->release();
            return NULL;
            
        }// end if( !dldIOService->init()
    }
    
    //
    // add Dld specific properties for the DldIoService object
    //
    OSNumber*  ObjectAddress = OSNumber::withNumber( (unsigned long long)dldIOService, sizeof( dldIOService )*8 );
    assert( ObjectAddress );
    if( ObjectAddress ){
        
        dldIOService->setProperty( DldStrPropertyDldIOService, ObjectAddress );
        ObjectAddress->release();
        DLD_DBG_MAKE_POINTER_INVALID( ObjectAddress );
    }
    
    if( service->getMetaClass() && service->getMetaClass()->getClassName() ){
        
        dldIOService->setProperty( DldStrPropertyClassName, service->getMetaClass()->getClassName() );
        
        //
        // set a meaningful name if it doesn't exist, the name is displayed as an entry name by ioreg/IORegistryExplorer
        //
        if( NULL == dldIOService->getName() ||
            0x0 == strcmp( dldIOService->getMetaClass()->getClassName(), dldIOService->getName() ) ){
            
            dldIOService->setName( service->getMetaClass()->getClassName() );
            
        }// end if( NULL == dldIOService->getName() ||
    }
    
    //
    // init property ( if any ), might be NULL
    //
    dldIOService->property = DldObjectPropertyEntry::withObject( service, dldIOService, postponePropertyUpdate );
    
#if defined( DBG )
    OSIncrementAtomic( &gDldIOServiceCount );
#endif//DBG
    
    return dldIOService;
}

//--------------------------------------------------------------------

bool DldIOService::requestTerminate( DldIOService * provider, IOOptionBits options )
{
    this->logServiceOperation( kDldIOSO_requestTerminate );
    return true;
}

//--------------------------------------------------------------------

bool DldIOService::willTerminate( DldIOService * provider, IOOptionBits options )
{
    this->ioServiceFlags.willTerminate = 0x1;
    this->logServiceOperation( kDldIOSO_willTerminate );
    return true;
}

//--------------------------------------------------------------------

//
// if the object doesn't have clients and attached to multiple providers
// only didTerminate will be called for it, terminate() and willTerminate() are not called,
// see IOService::terminatePhase1(), IOService::terminateWorker(), and IOService::requestTerminate()
// which doesn't call terminateClient() if there is a multiple providers case
//
bool DldIOService::didTerminate( DldIOService * provider, IOOptionBits options, bool * defer )
{
    if( *defer ){
        
        //
        // deferral responce,
        // see IOKitFundamentals->Clearing I/O Queues
        /*
         an excerpt from the IOKitFundamentals.pdf:
         
         Certain objects at the top of the stack—particularly user clients—may decide to keep a count of I/O requests
         they have issued and haven’t received a response for (“in-flight” I/O requests). (To ensure the validity of
         this count, the object should increment and decrement the count in a work-loop context.) By this tracking
         mechanism, they can determine if any I/O request hasn’t been completed. If this is the case, they can implement
         didTerminate to return a deferral response of true, thereby deferring the termination until the final I/O
         request completes. At this point, they can signal that termination should proceed by invoking didTerminate
         on themselves and returning a deferral response of false.
         */
        
        return true;
    }
    
    //
    // see the above comment about leaf object's multiple providers case
    // when terminate() is not called
    //
    if( 0x0 == this->ioServiceFlags.terminate ){
        
        //
        // emulate a terminate call
        //
        this->terminate( 0x0 );
    }

    //
    // remove all ancillary properties as they might contain back references to
    // the primary property thus creating a reference loop which will prevent
    // from both objects being destroyed as the reference counts don't drop to zero
    //
    
    if( this->property ){
        
        this->property->removeAncillaryProperties();
        
    }// end if( this->property )
    
    this->ioServiceFlags.didTerminate = 0x1;
    this->logServiceOperation( kDldIOSO_didTerminate );
    return true;
}

//--------------------------------------------------------------------

bool DldIOService::terminateClient( DldIOService * client, IOOptionBits options )
{
    this->logServiceOperation( kDldIOSO_terminateClient );
    return true;
}

//--------------------------------------------------------------------

bool DldIOService::terminate( IOOptionBits options )
{
    assert( preemption_enabled() );
    
    if( 0x1 == this->ioServiceFlags.terminate ){
        
        //
        // there is obviously a sequence of terminate hooks,
        // we can skip al subsequent hooks but this doesn't harm
        // it they are called multiple times so continue
        //
    }
        
    //
    // the only case where the lock is held to set the flag as
    // the case is a barrier - when it has been passed the service
    // field can't be referenced
    //
    this->LockExclusive();
    {// start of the lock
        this->ioServiceFlags.terminate = 0x1;
    }// end of the lock
    this->UnLockExclusive();
    
    //
    // wait for all references provided by this object have been released
    //
    this->waitForZeroReferenceNotification();
    
    this->logServiceOperation( kDldIOSO_terminate );
    
    return true;
}

//--------------------------------------------------------------------

bool DldIOService::finalize( IOOptionBits options )
{
    assert( this->ioServiceFlags.terminate );
    
    //
    // remove all ancillary properties as they might contain back references to
    // the primary property thus creating a reference loop which will prevent
    // from both objects being destroyed as the reference counts don't drop to zero
    //
    
    if( this->property ){
        
        this->property->removeAncillaryProperties();
        
    }// end if( this->property )
    
    this->logServiceOperation( kDldIOSO_finalize );
    return true;
}

//--------------------------------------------------------------------

//
// if the provider is NULL then the object is attached to the root
// or is started from the attach routine of the child, this can happen
// if the object is related to the object for which the start routine
// has not been called ( e.g. see an encryption driver in the Singh's book
// pp 1315-1316 there an object is created, attached but start routine is not called )
//
bool
DldIOService::start(
    __in_opt IOService* service,
    __in_opt DldIOService * provider
    )
{
    if( kDldPnPStateStarted == this->getPnPState() ){
        
        //
        // the start has been already processed ( e.g. a sequence of hooks )
        //
        return true;
    }
    
    //
    // mark the object as being started to tell updateDescriptor that
    // the IOKit service object is fully started and can be queried
    // for a complete and valid information
    //
    this->setPnPState( kDldPnPStateReadyToStart );
    
    //
    // the object might be reused, so clear the old state flags
    //
    this->ioServiceFlags.willTerminate = 0x0;
    this->ioServiceFlags.terminate = 0x0;
    this->ioServiceFlags.didTerminate = 0x0;
    this->ioServiceFlags.detached = 0x0;
    
    if( this->getObjectProperty() ){
        
        //
        // update the descriptor after the full start, do not hold the lock
        // as updateDescriptor acquires the lock as necessary
        //
        this->getObjectProperty()->updateDescriptor( service, this, true );
        
    }
    
    //
    // connect to a USB object if this is a bluetooth HIC's object or bluetooth serial manager's object
    //
    this->processBluetoothStack();
    
    //
    // mark the object started, from this point the access will be checked
    //
    this->setPnPState( kDldPnPStateStarted );
    
    this->ioServiceFlags.start = 0x1;
    
    this->logServiceOperation( kDldIOSO_start );
    
    return true;
}

//--------------------------------------------------------------------

bool DldIOService::open(
    __in IOService*    service,
    __in IOService *   forClient,
    __in IOOptionBits  options,
    __in void *		   arg
   )
{
    //
    // IOApplePartitionScheme's objects can be opened during matching procedure by itself
    /*
     #3  0x45f85c1f in DldIOService::open (this=0xb3d9180, service=0x5e37b00, forClient=0x5e37b00, options=0, arg=0x1) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOService.cpp:364
     #4  0x45f5f255 in DldHookerCommonClass::open (this=0x69ec254, serviceObject=0x5e37b00, forClient=0x5e37b00, options=0, arg=0x1) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldHookerCommonClass.cpp:621
     #5  0x45f93a37 in IOServiceVtableDldHookDldInheritanceDepth_0::open_hook (this=0x5e37b00, forClient=0x5e37b00, options=0, arg=0x1) at /work/DeviceLockProject/DeviceLockIOKitDriver/IOServiceDldHook.cpp:20
     #6  0x0117ebe7 in IOStorage::open (this=0x5e37b00, client=0x5e37b00, options=0, access=1) at /SourceCache/IOStorageFamily/IOStorageFamily-116/IOStorage.cpp:216
     #7  0x0116fc1c in IOApplePartitionScheme::scan (this=0x5e37b00, score=0x318c3f0c) at /SourceCache/IOStorageFamily/IOStorageFamily-116/IOApplePartitionScheme.cpp:258
     #8  0x0116f14f in IOApplePartitionScheme::probe (this=0x5e37b00, provider=0x6922500, score=0x318c3f0c) at /SourceCache/IOStorageFamily/IOStorageFamily-116/IOApplePartitionScheme.cpp:101
     #9  0x0053697f in IOService::probeCandidates (this=0x6922500, matches=0xb3d82c0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOService.cpp:2702
     #10 0x005371b8 in IOService::doServiceMatch (this=0x6922500, options=0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOService.cpp:3088
     #11 0x00538e67 in _IOConfigThread::main (arg=0x68e0380, result=0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOService.cpp:3350
     
     
     (gdb)showregistryentry 0x5e37b00
     +-o IOApplePartitionScheme  <object 0x05e37b00, id 0x1000006c0, vtable 0x1180ac0, !registered, !matched, active, busy 0, retain count 4>
     {
     "IOClass" = "IOApplePartitionScheme"
     "IOMatchCategory" = "IOStorage"
     "IOProbeScore" = 2000
     "IOPropertyMatch" = ({"Whole"=Yes},{"Content Hint"="CD_ROM_Mode_1","Writable"=No},{"Content Hint"="CD_ROM_Mode_2_Form_1","Writable"=No})
     "IOProviderClass" = "IOMedia"
     "Content Mask" = "Apple_partition_scheme"
     "CFBundleIdentifier" = "com.apple.iokit.IOStorageFamily"
     }
     */
    //
    assert( kDldPnPStateStarted == this->getPnPState() || service == forClient );
    assert( preemption_enabled() );
    
    if( this->getObjectProperty() ){
        
        //
        // update the descriptor as for example IOUSBInterface's CreatePipes()
        // is called from the handleOpen() which is called from IOService::open,
        // this->service is used here as we are sure - the service will be alive
        // at least until open returns
        //
        this->getObjectProperty()->updateDescriptor( service, this, false );
        
    }
    
    this->logServiceOperation( kDldIOSO_open );
    
    return true;
}

//--------------------------------------------------------------------

void DldIOService::setPnPState( __in DldPnPState pnpState )
{
    assert( pnpState < kDldPnPStateMax );
    
    this->pnpState = pnpState;
    
    if( this->getObjectProperty() )
        this->getObjectProperty()->setPnPState( pnpState );
    
    this->setProperty( DldStrPropertyPnPState, DldPnPStateToString( pnpState ) );
}

//--------------------------------------------------------------------

DldPnPState DldIOService::getPnPState()
{
#if defined(DBG)
    if( this->getObjectProperty() ){
        
        assert( this->getObjectProperty()->dataU.property->pnpState == this->pnpState );
    }
#endif//DBG
        
    return this->pnpState;
}

//--------------------------------------------------------------------

bool
DldIOService::attach(
    __in DldIOService * provider
    )
{
    bool bAttached;
    
    //
    // check that the object has not been aready attached
    //
    if( this->isParent( provider, gDldDeviceTreePlan ) )
        return true;
    
    //
    // actually there is no need for locks here as the code is called
    // when the stack is being built and contention impossible
    //
    bAttached = this->attachToParent( provider, gDldDeviceTreePlan );
    assert( bAttached );
    if( bAttached ){
            
        //
        // if the parent is still not marked as started, then do this
        //
        if( kDldPnPStateStarted != provider->getPnPState() )
            provider->start( NULL, NULL );
        
        //
        // add the child property to the parent
        //
        provider->addChildProperty( this->getObjectProperty() );
        
        //
        // add properties for already attached descendants, this is required as 
        // an object might be a singleton object ( as IOHIDSystem instance ) that is
        // attached to multiple stacks and itself has attached objects
        //
        if( this->getChildProperties() ){
            
            const OSArray*   childProperties = this->getChildProperties();
            for( int i = 0x0; i < childProperties->getCount(); ++i ){
                
                DldObjectPropertyEntry*  childPropertyEntry = OSDynamicCast( DldObjectPropertyEntry, childProperties->getObject( i ) );
                assert( childPropertyEntry );
                if( !childPropertyEntry )
                    continue;
                
                provider->addChildProperty( childPropertyEntry );
                
                //
                // check for the case similar to IOHIDSystem that has preaatched user client when being attached to a next parent
                //
                assert( childPropertyEntry->dataU.property );
                if( childPropertyEntry->dataU.property->userClientAttached )
                    this->userClientAttached();
                    
            } // end for
        } // end if( this->getChildProperties() )
        
        //
        // add the parent property to the child
        //
        this->addParentProperty( provider->getObjectProperty() );
        
        //
        // add parent's ancestors properties
        //
        this->addParentProperties( provider->getParentProperties() );
        
        //
        // as the parent properties have changed update the descriptor
        //
        if( this->getObjectProperty() )
            this->getObjectProperty()->updateDescriptor( NULL, this, false );
        
        //
        // if this is IOMedia then consider it as having a user client,
        // in that case a user client hat is weared by IOMediaBSDClient,
        // the same for IOModemSerialStreamSync to which a client representing
        // a serial BSD device is attached by default ( IOSerialBSDClient )
        //
        if( this->getObjectProperty() &&
            ( DldObjectPopertyType_IOMedia == this->getObjectProperty()->dataU.property->typeDsc.type ||
              DldObjectPopertyType_Serial == this->getObjectProperty()->dataU.property->typeDsc.type ||
              DldObjectPopertyType_IODVDMedia == this->getObjectProperty()->dataU.property->typeDsc.type ||
              DldObjectPopertyType_IOCDMedia == this->getObjectProperty()->dataU.property->typeDsc.type ) )
            this->userClientAttached();
        
        //
        // increment the parents count
        //
        OSIncrementAtomic( &this->parentsCount );
        
    }
    
    this->logServiceOperation( kDldIOSO_attach );
    
    return bAttached;
}

//--------------------------------------------------------------------

void DldIOService::detach( DldIOService * provider )
{
    assert( preemption_enabled() );
    assert( DldRegistryEntriesHashTable::sHashTable );
    assert( this->service );
    //assert( this->parentsCount > 0x0 );
        
    //
    // remove the child property, do this before detaching
    // as the link to the parent must be alive for recursive
    // removing this property for all underlying parents
    //
    provider->removeChildProperty( this->getObjectProperty() );
    
    //
    // if there is a child properties left then remove them
    // from the underlying ancestors, this is a degenerative
    // case when an object is detached while having children
    //
    if( this->childProperties ){
        
        this->LockShared();
        {// start of the lock
            
            if( this->childProperties ){
                
                unsigned int count = this->childProperties->getCount();
                
                for( int i = 0x0; i < count; ++i ){
                    
                    OSObject* obj;
                    
                    obj = this->childProperties->getObject( i );
                    assert( obj && OSDynamicCast( DldObjectPropertyEntry, obj ) );
                    
                    provider->removeChildProperty( OSDynamicCast( DldObjectPropertyEntry, obj ) );
                    
                }// end for( int i = 0x0; i < count; ++i )
                
            }// end if( this->childProperties )
            
        }// end of the lock
        this->UnLockShared();
        
    }// end if( this->childProperties )
    
    
    OSArray*  parentPropertiesToRemove = NULL;
    
    if( provider->getParentProperties() || provider->getObjectProperty() ){
        
        provider->LockShared();
        {// start of the lock
            
            UInt32 arrayCapacity = 0x0;
            
            if( provider->getObjectProperty() )
                arrayCapacity += 0x1;
            
            if( provider->getParentProperties() )
                arrayCapacity += provider->getParentProperties()->getCount();
            
            parentPropertiesToRemove = OSArray::withCapacity( arrayCapacity );
            assert( parentPropertiesToRemove );
            if( parentPropertiesToRemove ){
                
                if( provider->getParentProperties() )
                    parentPropertiesToRemove->initWithArray( provider->getParentProperties(), provider->getParentProperties()->getCount() );
                
                if( provider->getObjectProperty() )
                    parentPropertiesToRemove->setObject( provider->getObjectProperty() );
                
            } // end if( parentPropertiesToRemove )
        } // end of the lock
        provider->UnLockShared();
               
    } // end if( provider->getParentProperties() || provider->getObjectProperty() )
    
    //
    // remove all parent's properties, including removing from all descendants
    //
    if( parentPropertiesToRemove ){
        
        this->removeParentProperties( parentPropertiesToRemove );
        parentPropertiesToRemove->release();
        DLD_DBG_MAKE_POINTER_INVALID( parentPropertiesToRemove );
    }
    
    //
    // detach from the parent
    //
    this->detachFromParent( provider, gDldDeviceTreePlan );
    
    //
    // a workaround for the case of virtual storages when IOMedia property for a physical
    // device are added and this property does not exist in the provider's properties
    //
    if( NULL == this->getParentEntry( gDldDeviceTreePlan ) ){
        
        //
        // the last parent has been removed
        //
        this->LockExclusive();
        {// start of the lock
            
            if( this->parentProperties )
                this->parentProperties->flushCollection();
            
        }// end of the lock
        this->UnLockExclusive();
        
    } // end if( NULL == this->getParentEntry( gDldDeviceTreePlan ) )
    
    //
    // as the entry is required for the didTerminate phase it is not removed
    // from the hash table,
    // the object is removed from the hash table in the free() hook, see
    // DldHookerCommonClass::free
    //
    
    this->ioServiceFlags.detached = 0x1;
    this->logServiceOperation( kDldIOSO_detach );
    
    //
    // decrement the parents count
    //
    OSDecrementAtomic( &this->parentsCount );
}

//--------------------------------------------------------------------

void DldIOService::removeFromHashTable()
{
    assert( preemption_enabled() );
    assert( DldRegistryEntriesHashTable::sHashTable );
    assert( this->service );
    
    //
    // remove all ancillary properties as they might contain back references to
    // the primary property thus creating a reference loop which will prevent
    // from both objects being destroyed as the reference counts don't drop to zero
    //
    
    if( this->property ){
        
        this->property->removeAncillaryProperties();
        
    }// end if( this->property )
    
    
    //
    // remove from the hash
    //
    
    DldIORegistryEntry* entry;
    
    DldRegistryEntriesHashTable::sHashTable->LockExclusive();
    { // start of the lock
        
        //
        // the removed entry is referenced
        //
        entry = DldRegistryEntriesHashTable::sHashTable->RemoveObject( this->service );
        
    }// end of the lock
    DldRegistryEntriesHashTable::sHashTable->UnLockExclusive();
    
    assert( entry && entry == this );
    if( entry )
        entry->release();
}

//--------------------------------------------------------------------

bool DldIOService::init()
{
    if( !super::init() ){
        
        assert( !"DldIOService::init()->super::init() failed" );
        return false;
    }
    
    this->logServiceOperation( kDldIOSO_init );
    
    return true;
    
}

//--------------------------------------------------------------------

unsigned int
DldIOService::getObjectArrayIndex(
    __in_opt OSArray* array,
    __in_opt OSObject* object
)
/*
 DLD_NOT_IN_ARRAY is returned if the object has not been found in the array
 */
{
    if( !array || !object )
        return DLD_NOT_IN_ARRAY;
    
    unsigned int  count;
    count = array->getCount();
    
    for( int i = 0x0; i < count; ++i ){
        
        if( array->getObject( i ) == object )
            return i;
    }
    
    return DLD_NOT_IN_ARRAY;
}

//--------------------------------------------------------------------

bool
DldIOService::isObjectInArrayWOLock(
    __in_opt OSArray* array,
    __in_opt OSObject* object
    )
{
    return ( DLD_NOT_IN_ARRAY != getObjectArrayIndex( array, object ) );
}

//--------------------------------------------------------------------

bool
DldIOService::addParentProperties(
    __in_opt const OSArray*  properties
    )
{
    bool   bAdded = true;
    
    assert( preemption_enabled() );
    
    if( !properties )
        return true;
    
    this->LockExclusive();
    {// start of the lock
        
        if( !this->parentProperties ){
            
            this->parentProperties = OSArray::withArray( properties );
            assert( this->parentProperties );
            if( !this->parentProperties ){
                
                DBG_PRINT_ERROR(( "DldIOService::addParentProperty OSArray::withCapacity failed\n" ));
                
                bAdded = false;
                goto __exit_with_lock;
            }
            
            this->parentProperties->setCapacityIncrement( 2 );
            
            bAdded = true;
            goto __exit_with_lock;
            
        }// end if( !this->parentsProperties )
        
        unsigned int  newPropertiesCount;
        unsigned int  numberOfDuplicatedEntries;
        
        newPropertiesCount = properties->getCount();
        numberOfDuplicatedEntries = 0x0;
        
        //
        // check for duplications, this is a required operation as the object might be attached
        // to different levels in the stack, for example IOHIDSystem's object is attached to
        // two parents on the same stack
        //
        for( unsigned int i = 0x0; i < newPropertiesCount; ++i ){
            
            OSObject* property;
            
            property = properties->getObject( i );
            assert( property );
            
            if( this->isObjectInArrayWOLock( this->parentProperties, property ) )
                ++numberOfDuplicatedEntries;
            
        }// end for
        
        
        if( 0x0 == numberOfDuplicatedEntries ){
            
            //
            // a simple case - no any duplications
            //
            bAdded = this->parentProperties->merge( properties );
            
        } else if( numberOfDuplicatedEntries != newPropertiesCount ){
            
            //
            // there is something new, add one entry to another to avoid duplications
            //
            for( unsigned int i = 0x0; i < newPropertiesCount; ++i ){
                
                OSObject* property;
                
                property = properties->getObject( i );
                assert( property );
                
                if( ! this->isObjectInArrayWOLock( this->parentProperties, property ) )
                    bAdded = this->parentProperties->setObject( property );
                
                assert( bAdded );
                if( !bAdded )
                    break;
                
            }// end for
            
        } else {
            
            //
            // all entries have been already added
            //
            assert( bAdded );
        }
        
    __exit_with_lock:;
    }// end of the lock
    this->UnLockExclusive();
    
    //
    // if there are children attached to the current object, then copy current object properties to the children,
    // the applier function merges the arrays so there is no need to add only new properties
    //
        
    this->applyToChildren( DldIOService::AddParentPropertiesApplierFunction,
                           (void*)properties,
                           gDldDeviceTreePlan );
    
    assert( bAdded );
    
    return bAdded;
}

//--------------------------------------------------------------------

bool
DldIOService::addParentProperty(
    __in_opt DldObjectPropertyEntry*  parentProperty
    )
{
    bool bAdded = false;
    
    assert( preemption_enabled() );
    
    if( !parentProperty )
        return true;
    
    this->LockExclusive();
    {// start of the lock
        
        if( !this->parentProperties ){
            
            this->parentProperties = OSArray::withCapacity( 3 );
            assert( this->parentProperties );
            if( !this->parentProperties ){
                
                DBG_PRINT_ERROR(( "DldIOService::addParentProperty OSArray::withCapacity failed\n" ));
                
                goto __exit_with_lock;
            }
            
            this->parentProperties->setCapacityIncrement( 2 );
            
        }// end if( !this->parentsProperties )
        
        //
        // if the assert failes then consider 
        //  - redesign your code as this is a bad behaviour to fire the same path several times
        //  - use isObjectInParentProperties()
        //
        assert( ! this->isObjectInArrayWOLock( this->parentProperties, parentProperty ) );
        
        if( ! this->isObjectInArrayWOLock( this->parentProperties, parentProperty ) )
            bAdded = this->parentProperties->setObject( parentProperty );
        else
            bAdded = true;

    __exit_with_lock:;
    }// end of the lock
    this->UnLockExclusive();

        
    //
    // if there are children attached to the current object, then copy object propertiy to the children,
    // the applier function merges the arrays so there is no need to add only new properties
    //
    
    this->applyToChildren( DldIOService::AddParentPropertyApplierFunction,
                           (void*)parentProperty,
                           gDldDeviceTreePlan );
    
    return bAdded;
}

//--------------------------------------------------------------------

bool
DldIOService::isObjectInParentProperties(
    __in_opt DldObjectPropertyEntry*  property
    )
{
    bool found = false;
    
    assert( preemption_enabled() );
    
    if( !property || !this->parentProperties )
        return false;
    
    this->LockShared();
    {// start of the lock
        
        if( this->parentProperties ){
            
            found = this->isObjectInArrayWOLock( this->parentProperties, property );
            
        }// end if( this->parentsProperties )
        
    }// end of the lock
    this->UnLockShared();
    
    return found;
}

//--------------------------------------------------------------------

void
DldIOService::AddParentPropertiesApplierFunction(
    __in IORegistryEntry * entry,
    __in void * context
    )
{
    OSArray*        properties = (OSArray*)context;
    DldIOService*   dldIOService = OSDynamicCast( DldIOService, entry );
    
    if( properties && dldIOService )
        dldIOService->addParentProperties( properties );
    
}

//--------------------------------------------------------------------

void
DldIOService::AddParentPropertyApplierFunction(
    __in IORegistryEntry * entry,
    __in void * context
    )
{
    DldObjectPropertyEntry*  property = (DldObjectPropertyEntry*)context;
    DldIOService*            dldIOService = OSDynamicCast( DldIOService, entry );
    
    if( property && dldIOService )
        dldIOService->addParentProperty( property );
    
}

//--------------------------------------------------------------------

void
DldIOService::RemoveParentPropertiesApplierFunction(
    __in IORegistryEntry * entry,
    __in void * context
    )
{
    assert( preemption_enabled() );
    
    OSArray*        properties = (OSArray*)context;
    DldIOService*   dldIOService = OSDynamicCast( DldIOService, entry );
    
    if( properties && dldIOService )
        dldIOService->removeParentProperties( properties );
    
}

//--------------------------------------------------------------------

void
DldIOService::removeParentProperties( __in OSArray* propertiesToRemove )
{
    assert( preemption_enabled() );
    
    if( !this->parentProperties || !propertiesToRemove )
        return;
    
    this->LockExclusive();
    {// start of the lock
        
        for( int i = 0x0; i < propertiesToRemove->getCount() && this->parentProperties; ++i ){
            
            int indexToRemove = this->getObjectArrayIndex( this->parentProperties, propertiesToRemove->getObject( i ) );
            if( DLD_NOT_IN_ARRAY == indexToRemove )
                continue;
            
            this->parentProperties->removeObject( indexToRemove );
        } // end for
        
    }// end of the lock
    this->UnLockExclusive();
    
    //
    // if there are children attached to the current object, then remove their parent object properties,
    // the applier function merges the arrays so there is no need to add only new properties
    //
    
    this->applyToChildren( DldIOService::RemoveParentPropertiesApplierFunction,
                           (void*)propertiesToRemove,
                           gDldDeviceTreePlan );
}

//--------------------------------------------------------------------

void
DldIOService::AddChildPropertyApplierFunction(
    __in IORegistryEntry * entry,
    __in void * context
    )
{
    DldObjectPropertyEntry*  property = (DldObjectPropertyEntry*)context;
    DldIOService*            dldIOService = OSDynamicCast( DldIOService, entry );
    
    //
    // we might bump into the root which is not of the DldIOService type
    //
    if( dldIOService )
        dldIOService->addChildProperty( property );
    
}

//--------------------------------------------------------------------

bool
DldIOService::addChildProperty(
    __in_opt DldObjectPropertyEntry*  childProperty
    )
{
    bool bAdded = false;
    
    assert( preemption_enabled() );
    
    if( !childProperty )
        return true;
    
    this->LockExclusive();
    {// start of the lock
        
        if( !this->childProperties ){
            
            this->childProperties = OSArray::withCapacity( 3 );
            assert( this->childProperties );
            if( !this->childProperties ){
                
                DBG_PRINT_ERROR(( "DldIOService::addChildrenProperty OSArray::withCapacity failed\n" ));
                
                goto __exit_with_lock;
            }
            
            this->childProperties->setCapacityIncrement( 2 );
            
        }// end if( !this->parentsProperties )
        
        
        //
        // an object can be attached to two or more parents
        // and this parents might be on different levels, 
        // this is a case for IOHIDSystem objects
        /*
         +-o IOBluetoothHIDDriver  <class IOBluetoothHIDDriver, id 0x10000040a, registered, matched, active, busy 0 (9 ms), retain 6>
         | |   |     | | |   +-o IOHIDInterface  <class IOHIDInterface, id 0x10000040d, registered, matched, active, busy 0 (6 ms), retain 6>
         | |   |     | | |     +-o IOHIDEventDriver  <class IOHIDEventDriver, id 0x10000040e, registered, matched, active, busy 0 (3 ms), retain 8>
         | |   |     | | |       +-o IOHIDPointing  <class IOHIDPointing, id 0x10000040f, registered, matched, active, busy 0 (1 ms), retain 7>
         | |   |     | | |       | +-o IOHIDSystem  <class IOHIDSystem, id 0x10000021f, registered, matched, active, busy 0 (35 ms), retain 18>
         | |   |     | | |       |   +-o IOHIDUserClient  <class IOHIDUserClient, id 0x1000003a9, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |       |   +-o IOHIDParamUserClient  <class IOHIDParamUserClient, id 0x1000003aa, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |       |   +-o IOHIDEventSystemUserClient  <class IOHIDEventSystemUserClient, id 0x1000003bd, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |       |   +-o IOHIDEventSystemUserClient  <class IOHIDEventSystemUserClient, id 0x1000003be, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |       +-o IOHIDSystem  <class IOHIDSystem, id 0x10000021f, registered, matched, active, busy 0 (35 ms), retain 17>
         | |   |     | | |         +-o IOHIDUserClient  <class IOHIDUserClient, id 0x1000003a9, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |         +-o IOHIDParamUserClient  <class IOHIDParamUserClient, id 0x1000003aa, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |         +-o IOHIDEventSystemUserClient  <class IOHIDEventSystemUserClient, id 0x1000003bd, !registered, !matched, active, busy 0, retain 5>
         | |   |     | | |         +-o IOHIDEventSystemUserClient  <class IOHIDEventSystemUserClient, id 0x1000003be, !registered, !matched, active, busy 0, retain 5>
         */
        //
        
        bAdded = this->isObjectInArrayWOLock( this->childProperties, childProperty );
        
        //
        // skipping already added objects for the multiple parents case( see above )
        // we are risking of losing a child property if object is being detached
        // from only one parent, but we will trade off this for simplicity
        //
        if( !bAdded )
            bAdded = this->childProperties->setObject( childProperty );
        
        assert( bAdded );
        assert( this->isObjectInArrayWOLock( this->childProperties, childProperty ) );
        
    __exit_with_lock:;
    }// start of the lock
    this->UnLockExclusive();
    
    if( !bAdded )
        return bAdded;
    
    //
    // mark the device as processing the boot device requests
    // if its children is on the boot path
    // OR
    // if the child is added to the boot device mark the child as boot also
    //
    if( childProperty->dataU.property->onBootDevicePath )
        this->deviceIsOnBootPath();
    else if( this->isOnBootDevicePath() )
        childProperty->dataU.property->onBootDevicePath = true;
    
    //
    // add the child property for this object's parents thus causing recursive
    // adding for all underlying layers, as you see this is a tail recursion
    // so it can be converted in a "for" loop
    //
    this->applyToParents( AddChildPropertyApplierFunction,
                          (void*)childProperty,
                          gDldDeviceTreePlan );
    
    return bAdded;
    
}

//--------------------------------------------------------------------

void
DldIOService::RemoveChildPropertyApplierFunction(
    __in IORegistryEntry * entry,
    __in void * context
    )
{
    assert( preemption_enabled() );
    
    DldObjectPropertyEntry*  property = (DldObjectPropertyEntry*)context;
    DldIOService*            dldIOService = OSDynamicCast( DldIOService, entry );
    
    //
    // we might bump into the root which is not of the DldIOService type
    //
    if( dldIOService )
        dldIOService->removeChildProperty( property );
    
}

//--------------------------------------------------------------------

void
DldIOService::removeChildProperty(
    __in_opt DldObjectPropertyEntry*  property
    )
{
    assert( preemption_enabled() );
    
    if( !property || !this->childProperties )
        return;
    
    this->LockExclusive();
    {// start of the lock
        
        if( this->childProperties ){
            
            unsigned int indx;
            
            indx = this->getObjectArrayIndex( this->childProperties, property );
            
            if( DLD_NOT_IN_ARRAY != indx )
                this->childProperties->removeObject( indx );
            
            assert( DLD_NOT_IN_ARRAY == this->getObjectArrayIndex( this->childProperties, property ) );
            
        } // end if( this->childProperties )
        
    }// start of the lock
    this->UnLockExclusive();
    
    //
    // remove the child property for this object's parents thus causing recursive
    // removing for all underlying layers, as you see this is a tail recursion
    // so it can be converted in a "for" loop
    //
    this->applyToParents( RemoveChildPropertyApplierFunction,
                          (void*)property,
                          gDldDeviceTreePlan );
    
}

//--------------------------------------------------------------------

DldObjectPropertyEntry*
DldIOService::retrievePropertyByTypeRef( __in DldObjectPopertyType  type, __in int previousIndex )
/*
 returns a referenced property from the property array for the object
 ( i.e. a parent, this object's property or a child property ) or
 returns an ancillary property if found before primary one(!)
 */
{
    assert( preemption_enabled() );
    
    //
    // the object's property can be touched w/o lock
    //
    if( this->property &&
        this->property->dataU.property &&
        this->property->dataU.property->typeDsc.type == type ){
        
        this->property->retain();
        return this->property;
    }
    
    DldObjectPropertyEntry* property = NULL;
    
    this->LockShared();
    {// start of the lock
    
        int      currentIndex = (-1);
        OSArray* propArrays[2];
        
        //
        // search among children's and parent's properties
        //
        propArrays[0] = this->childProperties;
        propArrays[1] = this->parentProperties;
        
        for( unsigned int k=0; !property && k<DLD_STATIC_ARRAY_SIZE(propArrays); ++k ){
            
            assert( !property );
            
            OSArray*  propArray = propArrays[ k ];
            if( propArray ){
                
                unsigned int  count;
                count = propArray->getCount();
                for( unsigned int i = 0x0; i < count; ++i ){
                    
                    assert( !property );
                    
                    DldObjectPropertyEntry*  p;
                    
                    //
                    // the function is called to often so do not use the safe cast in release
                    //
                    assert( OSDynamicCast( DldObjectPropertyEntry, propArray->getObject(i) ) );
                    
                    p = (DldObjectPropertyEntry*)(propArray->getObject(i));
                    if( p->dataU.property->typeDsc.type == type ){
                        property = p;
                        goto __compare_index;
                    }
                    
                    assert( !property );
                    
                    //
                    // check the ancillary property entries
                    //
                    if( p->ancillaryObjectPropertyEntries ){
                        
                        int ancCount;
                        
                        ancCount = p->ancillaryObjectPropertyEntries->getCount();
                        for( int m = 0x0; m < ancCount; ++m ){
                            
                            assert( !property );
                            
                            DldObjectPropertyEntry* ancProp;
                            
                            //
                            // the function is called to often so do not use the safe cast in release
                            //
                            assert( OSDynamicCast( DldObjectPropertyEntry, p->ancillaryObjectPropertyEntries->getObject( m ) ) );

                            ancProp = (DldObjectPropertyEntry*)p->ancillaryObjectPropertyEntries->getObject( m );
                            assert( ancProp );
                            
                            if( ancProp->dataU.property->typeDsc.type == type ){
                                property = ancProp;
                                goto __compare_index;
                            }
                            
                        }// end for( int m = 0x0; m < ancCount; ++m )
                        
                    }// end if( p->ancillaryObjectPropertyEntries )
                    
                __compare_index:
                    
                    if( property ){
                        
                        ++currentIndex;
                        
                        //
                        // should we continue the iteration?
                        //
                        if( currentIndex <= previousIndex )
                            property = NULL;
                    } // end if( property )
                    
                    if( property )
                        break;
                    
                }// end for( unsigned int i = 0x0; i < count; ++i )
                
            }// end if( propArray )
            
            if( property )
                break;
            
        }// end for( unsigned int k=0; !property && k<DLD_STATIC_ARRAY_SIZE(propArrays); ++k )
        
        if( property )
            property->retain();
        
    }// start of the lock
    this->UnLockShared();
    
    return property;
}

//--------------------------------------------------------------------

void DldIOService::userClientAttached()
{
    assert( preemption_enabled() );
    
    DldObjectPropertyEntry* serviceProperty;
    
    serviceProperty = this->getObjectProperty();
    if( serviceProperty ){
        
        if( serviceProperty->dataU.property->userClientAttached )
            return;// we have already visit this stack branch
        else
            serviceProperty->dataU.property->userClientAttached = true;
        
    }// end if( property )
    
    
    this->LockShared();
    {// start of the lock
        
        const OSArray*  parentProperties;
        
        parentProperties = this->getParentProperties();
        if( parentProperties ){
            
            for( unsigned int i = 0x0; i < parentProperties->getCount(); ++i ){
                
                OSObject* object = parentProperties->getObject( i );
                
                assert( object && OSDynamicCast( DldObjectPropertyEntry, object ) );
                
                if( !object || !OSDynamicCast( DldObjectPropertyEntry, object ) )
                    continue;
                
                DldObjectPropertyEntry* property = OSDynamicCast( DldObjectPropertyEntry, object );
                assert( property );
                
                if( property->dataU.property->userClientAttached )
                    continue;// we have already visit this part of the branch
                else
                    property->dataU.property->userClientAttached = true;
                
            }// end for
            
        }// end if( parentProperties )
        
    }// end of the lock
    this->UnLockShared();
    
}

//--------------------------------------------------------------------

void DldIOService::deviceIsOnBootPath()
{
    assert( preemption_enabled() );
    
    //
    // set the property
    //
    this->setProperty( DldStrPropertyBootDevice, "true" );
    
    DldObjectPropertyEntry* serviceProperty;
    
    serviceProperty = this->getObjectProperty();
    if( serviceProperty ){
        
        if( serviceProperty->dataU.property->onBootDevicePath )
            return;// we have already visited this stack branch
        else
            serviceProperty->dataU.property->onBootDevicePath = true;
        
    }// end if( property )
    
    
    this->LockShared();
    {// start of the lock
        
        const OSArray*  parentProperties;
        
        parentProperties = this->getParentProperties();
        if( parentProperties ){
            
            for( unsigned int i = 0x0; i < parentProperties->getCount(); ++i ){
                
                OSObject* object = parentProperties->getObject( i );
                
                assert( object && OSDynamicCast( DldObjectPropertyEntry, object ) );
                
                if( !object || !OSDynamicCast( DldObjectPropertyEntry, object ) )
                    continue;
                
                DldObjectPropertyEntry* property = OSDynamicCast( DldObjectPropertyEntry, object );
                assert( property );
                
                if( property->dataU.property->onBootDevicePath )
                    continue;// we have already visited this part of the branch
                else
                    property->dataU.property->onBootDevicePath = true;
                
            }// end for
            
        }// end if( parentProperties )
        
    }// end of the lock
    this->UnLockShared();
    
}

//--------------------------------------------------------------------

//
// returns true if the device is on the boot device path
//
Boolean DldIOService::isOnBootDevicePath()
{
    DldObjectPropertyEntry* serviceProperty;
    
    serviceProperty = this->getObjectProperty();
    if( serviceProperty ){
        
        if( serviceProperty->dataU.property->onBootDevicePath )
            return true;
    }// end if( property )
    
    return false;
}

//--------------------------------------------------------------------

bool DldIOService::init( __in OSDictionary * PropertyDictionary )
{
    if( !super::init( PropertyDictionary ) ){
        
        assert( !"DldIOService::init( PropertyDictionary )->super::init( PropertyDictionary ) failed" );
        return false;
    }
    
    this->logServiceOperation( kDldIOSO_init );
    
    return true;
}

//--------------------------------------------------------------------

void DldIOService::free( void )
{
    
    //assert( 0x0 == this->parentsCount );
    
    if( this->property )
        this->property->release();
    
    if( this->childProperties ){
        
        assert( 0x0 == this->childProperties->getCount() );
        this->childProperties->release();
    }
    
    if( this->parentProperties ){
        
        assert( 0x0 == this->parentProperties->getCount() );
        this->parentProperties->release();
    }
    
    if( this->rwLock ){
        
        IORWLockFree( this->rwLock );
    }
    
#if defined( DBG )
    OSDecrementAtomic( &gDldIOServiceCount );
#endif//DBG
    
    return super::free();
}

//--------------------------------------------------------------------

bool DldIOService::initDeviceLogData(
    __in     DldRequestedAccess* action,
    __in     DldFileOperation    operation, // a very limited use, valid only for IOMedia and its derivatives
    __in     int32_t        pid,// BSD process ID
    __in     kauth_cred_t   credential,
    __in     bool           accessDisabled,
    __inout  DldDriverDataLogInt*  intData// at least sizeof( intData->logData->device )
    )
{
    //
    // it is undesirable to lost usefull log information
    //
    assert( this->property );
    
    if( NULL == this->property )
        return false;
    
    DldObjectPropertyEntry*   propertyToAudit = NULL;
    
    //
    // look for actual CD/DVD properties
    //
    if( DLD_DEVICE_TYPE_CD_DVD == this->property->dataU.property->deviceType.type.major && 
        ( DldObjectPopertyType_IODVDMedia != this->property->dataU.property->typeDsc.type &&
          DldObjectPopertyType_IOCDMedia != this->property->dataU.property->typeDsc.type )){

        propertyToAudit = this->retrievePropertyByTypeRef( DldObjectPopertyType_IODVDMedia );
        if( NULL == propertyToAudit )
            propertyToAudit = this->retrievePropertyByTypeRef( DldObjectPopertyType_IOCDMedia );
    }
    
    if( NULL == propertyToAudit ){
        
        propertyToAudit = this->property;
        propertyToAudit->retain();
    }
    
    bool successful = propertyToAudit->initDeviceLogData( action, operation, pid, credential, accessDisabled, intData );
    
    propertyToAudit->release();
    return successful;
}

//--------------------------------------------------------------------

bool DldIOService::initParentDeviceLogData(
    __in     DldRequestedAccess* action,
    __in     DldFileOperation    operation, // a very limited use, valid only for IOMedia and its derivatives
    __in     int32_t        pid,// BSD process ID
    __in     kauth_cred_t   credential,
    __in     bool           accessDisabled,
                                           
    __inout  DldDriverDataLogInt*  intData// at least sizeof( intData->logData->device )
    )
{
    //
    // it is undesirable to lost usefull log information
    //
    assert( this->property && this->property->dataU.property->parentProperty );
    
    if( NULL == this->property || NULL == this->property->dataU.property->parentProperty )
        return false;
    
    return this->property->dataU.property->parentProperty->initDeviceLogData( 
                              action, operation, pid, credential, accessDisabled, intData );
}

//--------------------------------------------------------------------

IOService* DldIOService::getSystemServiceRef()
{
    IOService*   refToService = NULL;
    
    assert( preemption_enabled() );
    
    this->LockShared();
    {// start of the lock
        
        if( 0x0 == this->ioServiceFlags.terminate ){
            
            assert( this->service );
            
            refToService = this->service;
            refToService->retain();
            
            OSIncrementAtomic( &this->ioServiceRefCount );
        }
        
    }// end of the lock
    this->UnLockShared();
    
    return refToService;
}

//--------------------------------------------------------------------

void DldIOService::putSystemServiceRef( __in IOService* refService )
{
    assert( this->ioServiceRefCount > 0x0 );
    assert( refService == this->service );
    
    if( refService != this->service ){
        
        DBG_PRINT_ERROR(( "refService != this->service \n" ));
        return;
    }
    
    this->service->release();
    
    if( 0x1 == OSDecrementAtomic( &this->ioServiceRefCount ) ){
        
        //
        // somebody is waiting for the service releasing
        //
        this->zeroReferenceNotification();
        
    }// end if( 0x1 == OSDecrementAtomic( &this->ioServiceRefCount ) )
    
    return;
}

//--------------------------------------------------------------------

void DldIOService::zeroReferenceNotification()
{
    UInt32*  event;
        
    //
    // somebody is waiting for the service object releasing
    //
    do{
        
        event = this->ioServiceReleaseEvent;
        
    } while( !OSCompareAndSwapPtr( event, NULL, &this->ioServiceReleaseEvent ) );
    
    if( event )
        DldSetNotificationEvent( event );
    
}

//--------------------------------------------------------------------

void DldIOService::waitForZeroReferenceNotification()
{
    UInt32  event;
    bool    wait = false;
#if DBG
    int     loopCounter = 0x0;
#endif//DBG
    
    assert( preemption_enabled() );
    
    DldInitNotificationEvent( &event );
    
    assert( !wait );
    
    while( !wait && !OSCompareAndSwap( 0x0, 0x0, &this->ioServiceRefCount ) ){
        
        //
        // this->ioServiceRefCount might move to zero at this point so
        // the above check is not enough, the additional check must
        // be performed after the loop exit
        //
        
        wait = OSCompareAndSwapPtr( NULL, &event, &this->ioServiceReleaseEvent );
        
        if( !wait ){
            
            //
            // back off ( it is better to use a geometric series )
            //
            IOSleep( 5 );
        }
        
#if DBG
        assert( ++loopCounter < 0xFF );
#endif//DBG
        
    }// end while
    
    //
    // the check for this->ioServiceRefCount is required - see comments above
    // in the while loop
    //
    if( wait ){
        
        if( !OSCompareAndSwap( 0x0, 0x0, &this->ioServiceRefCount ) ){
            
            DldWaitForNotificationEvent( &event );
            
        } else if( !OSCompareAndSwapPtr( &event, NULL, &this->ioServiceReleaseEvent ) ){
            
            //
            // we were unable to remove the event pointer, so it
            // was removed by somebody else, this can only be
            // zeroReferenceNotification() so we have to wait
            // as the event is acquired by zeroReferenceNotification()
            //
            DldWaitForNotificationEvent( &event );
        }
        
        assert( &event != this->ioServiceReleaseEvent );
        
    }// end if( wait )
    
    //
    // this exchange must fail, we must not leave a pointer to nowhere
    //
    assert( !OSCompareAndSwapPtr( &event, NULL, &this->ioServiceReleaseEvent ) );
    
}

//--------------------------------------------------------------------

void DldIOService::logServiceOperation( __in DldIOServiceOperation  op )
{
    SInt32  index;
    
    index = OSIncrementAtomic( &this->serviceOperationslogValidEntries )%DLD_STATIC_ARRAY_SIZE(this->serviceOperationsLog);
    this->serviceOperationsLog[ index ] = op;
}

//--------------------------------------------------------------------

bool DldIOService::serializeProperties( OSSerialize * s ) const
{
    //
    // see the comments in the function declaration about the reasons we need to
    // change the default behaviour
    //
    if( this->ioServiceFlags.detached ){
        
        //
        // it is unsafe to query property objects as the object is way down to a recycle bin,
        // DldIOService shares property objects with a real IoService and this objects
        // might not be idempotent on their behavior and crush the system when they
        // patially deinitialized
        //
        
        if (s->previouslySerialized(this))
            return true;
        
        if (!s->addXMLStartTag(this, "string"))
            return false;
        
        if (!s->addString("!!unsafe to query!!"))
            return false; 
        
        return s->addXMLEndTag("string");
    }
    
    return super::serializeProperties( s );
}

//--------------------------------------------------------------------

void DldIOService::processBluetoothStack()
{   
    IOService*              ioService = NULL; // must be released by a call to putSystemServiceRef
    OSObject*               usbVendorID = NULL; // referenced
    OSObject*               usbProductID = NULL; // referenced
    const OSSymbol*         usbVendorIDKey = OSSymbol::withCString( kUSBVendorID ); // referenced
    const OSSymbol*         usbProductIDKey = OSSymbol::withCString( kUSBProductID ); // referenced
    OSDictionary*           matchingDictionairy = NULL; // referenced
    DldIOService*           dldUsbObject = NULL; // referenced
    OSObject*               usbObject = NULL; // referenced
    OSIterator*             objectsIterator = NULL; // referenced
    OSIterator *            parentIterator = NULL; // referenced
    IOService *             bluetoothHCI = NULL; // reefrenced
    
    assert( preemption_enabled() );
    assert( usbVendorIDKey && usbProductIDKey );
    if( NULL == usbVendorIDKey || NULL == usbProductIDKey )
        goto __exit;
    
    ioService = this->getSystemServiceRef();
    assert( ioService );
    if( ! ioService )
        goto __exit;
    
    if( ioService->metaCast( "IOBluetoothSerialManager" ) ){
        
        //
        // all bluetooth implementations have a separated serial manager stack,
        //  we will create an artificial connection in the DldDeviceTreePlan,
        // find an IOBluetoothHCIController object to collect USB VID/PID from it
        //
        OSIterator *  hciIterator = IOService::getMatchingServices( IOService::serviceMatching("IOBluetoothHCIController") );
        assert( hciIterator );
        if( !hciIterator ) // virtual machines normally do not have bth devices
            goto __exit;
        
        bluetoothHCI = OSDynamicCast( IOService, hciIterator->getNextObject() );
        assert( bluetoothHCI );
        if( bluetoothHCI )
            bluetoothHCI->retain(); // the object is retained while the iterator is pointing to it
        
        hciIterator->release();
        
    } else if( ioService->metaCast( "IOBluetoothHCIController" ) ){
        
        //
        // the new bluetooth stack does not have connection between IOUsbDevice and HCI controller objects,
        // we will create an artificial connection in the DldDeviceTreePlan
        //
        
        bluetoothHCI = ioService;
        bluetoothHCI->retain();
    }

    if( ! bluetoothHCI )
       goto __exit;

    //
    // get the USB VID and PID, the HCI class copies them from the usb device object
    //
    usbVendorID = bluetoothHCI->getProperty( kUSBVendorID );
    if( usbVendorID )
        usbVendorID->retain();
    
    usbProductID = bluetoothHCI->getProperty( kUSBProductID );
    if( usbProductID )
        usbProductID->retain();
    
    assert( usbVendorID && usbProductID );
    
    if( NULL==usbVendorID || NULL==usbProductID ){
        
        DBG_PRINT_ERROR(( "usbVendorID or usbProductID are missing for Bluetooth HCI\n" ));
        goto __exit;
    }
       
    //
    // find the corresponding USB device
    //
    matchingDictionairy = IOService::serviceMatching("IOUSBDevice");
    assert( matchingDictionairy );
    if( ! matchingDictionairy )
        goto __exit;
    
    IOService::propertyMatching( usbVendorIDKey, usbVendorID, matchingDictionairy );
    IOService::propertyMatching( usbProductIDKey, usbProductID, matchingDictionairy );
    
    objectsIterator = IOService::getMatchingServices(matchingDictionairy);
    assert( objectsIterator );
    if( ! objectsIterator )
        goto __exit;
    
    //
    // there should be only one found device
    //
    usbObject = objectsIterator->getNextObject();
    assert( usbObject );
    if( ! usbObject ){
        
        DBG_PRINT_ERROR(("unable to locate IOUSBDevice for IOBluetoothHCIController\n"));
        goto __exit;
        
    } else {
        
        //
        // the object is valid while the iterator is pointing to it
        //
        usbObject->retain();
        
        //
        // there should be only one found device
        //
        assert( NULL == objectsIterator->getNextObject() );
    }
    
    //
    // find the corresponding DLD object
    //
    dldUsbObject = RetrieveDldIOServiceForIOService( OSDynamicCast( IOService, usbObject ) );
    assert( dldUsbObject );
    if( ! dldUsbObject )
        goto __exit;
    
    assert( dldUsbObject->getObjectProperty() && 
            DldObjectPopertyType_UsbDevice == (dldUsbObject->getObjectProperty())->dataU.property->typeDsc.type );
    
    //
    // detach from the old USB parent
    //
    parentIterator = this->getParentIterator( gDldDeviceTreePlan );
    assert( parentIterator );
    if( parentIterator ){
        
        OSObject*  parent;
        
        while( NULL != ( parent = parentIterator->getNextObject() ) ){
            
            DldIOService*  dldParent = OSDynamicCast( DldIOService, parent );
            assert( dldParent );
            if( dldParent && dldParent != dldUsbObject ){
                
                DldObjectPropertyEntry* parentProperty = dldParent->getObjectProperty();
                if( parentProperty &&
                    DldObjectPopertyType_UsbDevice == parentProperty->dataU.property->typeDsc.type ){
                    
                    //
                    // detach from the old parent
                    //
                    this->detach( dldParent );
                    break;
                }
            } // end if( dldParent && dldParent != dldUsbObject )
            
        } // end while
        
    } // end f( parentIterator )
    
    //
    // attach to the parent
    //
    if( ! this->isParent( dldUsbObject, gDldDeviceTreePlan ) ){
        
        //
        // finally attach to it
        //
        this->attach( dldUsbObject );
    }
    
__exit:
    
    if( parentIterator )
        parentIterator->release();
    
    if( objectsIterator )
        objectsIterator->release();
    
    if( dldUsbObject )
        dldUsbObject->release();
    
    if( usbObject )
        usbObject->release();
    
    if( matchingDictionairy )
        matchingDictionairy->release();
    
    if( usbVendorIDKey )
        usbVendorIDKey->release();
    
    if( usbProductIDKey )
        usbProductIDKey->release();
    
    if( usbVendorID )
        usbVendorID->release();
    
    if( usbProductID )
        usbProductID->release();
       
    if( bluetoothHCI )
       bluetoothHCI->release();
       
    if( ioService )
       this->putSystemServiceRef( ioService );
}

//--------------------------------------------------------------------
