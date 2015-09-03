/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */
#include <AvailabilityMacros.h>
#include "DldHookerCommonClass.h"
#include "DldVmPmap.h"
#include "DldIOKitHookEngine.h"
#include "IOBSDSystem.h"
#include "DldUndocumentedQuirks.h"
#include "DldKernAuthorization.h"
#include "DldIOLog.h"
#include <sys/proc.h>

//--------------------------------------------------------------------

extern
bool
DldAddNewIORegistryEntryToDldPlane(
    __in IORegistryEntry *  object
    );

//--------------------------------------------------------------------

DldHookedObjectsHashTable* DldHookedObjectsHashTable::sHashTable = NULL;

//--------------------------------------------------------------------

OSDefineMetaClassAndStructors( DldHookedObjectEntry, OSObject )


DldHookedObjectEntry* DldHookedObjectEntry::allocateNew(){
    
    DldHookedObjectEntry*   newEntry = new DldHookedObjectEntry();
    assert( newEntry );
    if( !newEntry )
        return NULL;
    
    if( !newEntry->init() ){
        
        assert( !"newEntry->init() failed" );
        
        newEntry->release();
        return NULL;
    }
    
    return newEntry;
};


void DldHookedObjectEntry::free()
{
    assert( preemption_enabled() );
    
    switch( this->Type ) {
            
        case DldHookEntryTypeObject:
            {
            }
            break;
            
        case DldHookEntryTypeVtableObj:
            {
                //
                // release a reference to the VtableEntry object
                //
                if( Parameters.TypeVatbleObj.VtableEntry )
                    Parameters.TypeVatbleObj.VtableEntry->release();
                
            }
            break;
            
        case DldHookEntryTypeVtable:
            if( this->Parameters.Common.HookedVtableFunctionsInfo ){
                
                int i = 0x0;
                vm_size_t  size = 0x0; 
                
                //
                // calculate the array size
                //
                do {
                    size += sizeof( this->Parameters.Common.HookedVtableFunctionsInfo[ 0 ] );
                } while( (unsigned int)(-1) != this->Parameters.Common.HookedVtableFunctionsInfo[ i++ ].VtableIndex );
                
                assert( this->Parameters.TypeVtable.HookedVtableFunctionsInfoEntriesNumber == i );
                IOFree( (void*)this->Parameters.Common.HookedVtableFunctionsInfo, size );
                
            }// end if
            break;
            
        default:
            panic( "DldHookedObjectEntry::processAfterRemove() called for an unknown type" );
            break;
            
    }// end switch
}


//--------------------------------------------------------------------

DldHookedObjectsHashTable*
DldHookedObjectsHashTable::withSize( int size, bool non_block )
{
    DldHookedObjectsHashTable* objHashTable;
    
    assert( preemption_enabled() );
    
    objHashTable = new DldHookedObjectsHashTable();
    assert( objHashTable );
    if( !objHashTable )
        return NULL;
    
    objHashTable->RWLock = IORWLockAlloc();
    assert( objHashTable->RWLock );
    if( !objHashTable->RWLock ){
        
        delete objHashTable;
        return NULL;
    }
    
    objHashTable->HashTable = ght_create( size, non_block );
    assert( objHashTable->HashTable );
    if( !objHashTable->HashTable ){
        
        IORWLockFree( objHashTable->RWLock );
        objHashTable->RWLock = NULL;
        
        delete objHashTable;
        return NULL;
    }
    
    return objHashTable;
}

//--------------------------------------------------------------------

bool
DldHookedObjectsHashTable::CreateStaticTableWithSize( int size, bool non_block )
{
    assert( !DldHookedObjectsHashTable::sHashTable );
    
    DldHookedObjectsHashTable::sHashTable = withSize( size, non_block );
    assert( DldHookedObjectsHashTable::sHashTable );
    
    return ( NULL != DldHookedObjectsHashTable::sHashTable );
}

void
DldHookedObjectsHashTable::DeleteStaticTable()
{
    if( !DldHookedObjectsHashTable::sHashTable ){
        
        DldHookedObjectsHashTable::sHashTable->free();
        delete DldHookedObjectsHashTable::sHashTable;
    }// end if
    
}

//--------------------------------------------------------------------

void
DldHookedObjectsHashTable::free()
{
    ght_hash_table_t* p_table;
    ght_iterator_t iterator;
    void *p_key;
    ght_hash_entry_t *p_e;
    
    assert( preemption_enabled() );
    
    p_table = this->HashTable;
    assert( p_table );
    if( !p_table )
        return;
    
    this->HashTable = NULL;
    
    for( p_e = (ght_hash_entry_t*)ght_first( p_table, &iterator, (const void**)&p_key );
         NULL != p_e;
         p_e = (ght_hash_entry_t*)ght_next( p_table, &iterator, (const void**)&p_key ) ){
        
        assert( !"Non emprty hash!" );
        DBG_PRINT_ERROR( ("DldHookedObjectsHashTable::free() found an entry for an object(0x%p)\n", *(void**)p_key ) );
        
        p_table->fn_free( p_e, p_e->size );
    }
    
    ght_finalize( p_table );
    
    IORWLockFree( this->RWLock );
    this->RWLock = NULL;
}

//--------------------------------------------------------------------

bool
DldHookedObjectsHashTable::AddObject(
    __in OSObject* obj,
    __in DldHookedObjectEntry* objEntry,
    __in bool errorIfPresent
    )
/*
 the caller must alloacte space for the entry and
 free it only after removing the entry from the hash
 */
{
    GHT_STATUS_CODE RC;
    
    assert( DldHookedObjectEntry::DldHookEntryTypeObject == objEntry->Type );
    assert( objEntry->Parameters.Common.HookedVtableFunctionsInfo );
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    RC = ght_insert( this->HashTable, objEntry, sizeof( obj ), &obj );
    if( !errorIfPresent && GHT_ALREADY_IN_HASH == RC )
        return true;
    
    assert( GHT_OK == RC );
    if( GHT_OK != RC ){
        
        DBG_PRINT_ERROR( ( "DldHookedObjectsHashTable::AddObject->ght_insert( 0x%p, (OSObject)0x%p ) failed RC = 0x%X\n",
                          (void*)this->HashTable, (void*)obj, RC ) );
    } else {
        
        objEntry->retain();
    }

    return ( GHT_OK == RC );
}

//--------------------------------------------------------------------

bool
DldHookedObjectsHashTable::AddObject(
    __in DldHookTypeVtableObjKey* vtableHookObj,
    __in DldHookedObjectEntry* objEntry,
    __in bool errorIfPresent
    )
/*
 the caller must alloacte space for the entry and
 free it only after removing the entry from the hash
 */
{
    GHT_STATUS_CODE RC;
    
    assert( DldHookedObjectEntry::DldHookEntryTypeVtableObj == objEntry->Type );
    assert( objEntry->Parameters.Common.HookedVtableFunctionsInfo );
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    RC = ght_insert( this->HashTable, objEntry, sizeof( *vtableHookObj ), vtableHookObj );
    if( !errorIfPresent && GHT_ALREADY_IN_HASH == RC )
        return true;
    
    assert( GHT_OK == RC );
    if( GHT_OK != RC ){
        
        DBG_PRINT_ERROR( ( "DldHookedObjectsHashTable::AddObject->ght_insert( 0x%p, (DldHookTypeVtableObjKey::Object)0x%p ) failed RC = 0x%X\n",
                          (void*)this->HashTable, (void*)vtableHookObj->Object, RC ) );
    } else {
        
        objEntry->retain();
    }    
    
    return ( GHT_OK == RC );
}

//--------------------------------------------------------------------

bool
DldHookedObjectsHashTable::AddObject(
    __in DldHookTypeVtableKey* vtableHookVtable,
    __in DldHookedObjectEntry* objEntry,
    __in bool errorIfPresent
    )
/*
 the caller must alloacte space for the entry and
 free it only after removing the entry from the hash
 */
{
    GHT_STATUS_CODE RC;
    
    assert( DldHookedObjectEntry::DldHookEntryTypeVtable == objEntry->Type );
    assert( objEntry->Parameters.Common.HookedVtableFunctionsInfo );
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    RC = ght_insert( this->HashTable, objEntry, sizeof( *vtableHookVtable ), vtableHookVtable );
    if( !errorIfPresent && GHT_ALREADY_IN_HASH == RC )
        return true;
    
    assert( GHT_OK == RC );
    if( GHT_OK != RC ){
        
        DBG_PRINT_ERROR( ( "DldHookedObjectsHashTable::AddObject->ght_insert( 0x%p, (DldHookTypeVtableKey::Vtable)0x%p ) failed RC = 0x%X\n",
                          (void*)this->HashTable, (void*)vtableHookVtable->Vtable, RC ) );
    } else {
        
        objEntry->retain();
    }
    
    return ( GHT_OK == RC );
}

//--------------------------------------------------------------------

#if defined( DBG )
bool
DldHookedObjectsHashTable::AddObject(
    __in OSMetaClassBase::_ptf_t*    key,
    __in DldDbgVtableHookToObject*   DbgEntry,
    __in bool errorIfPresent
    )
/*
 the caller must alloacte space for the entry and
 free it only after removing the entry from the hash
 */
{
    GHT_STATUS_CODE RC;
    
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    RC = ght_insert( this->HashTable, DbgEntry, sizeof( key ), &key );
    if( !errorIfPresent && GHT_ALREADY_IN_HASH == RC )
        return true;
    
    assert( GHT_OK == RC );
    
    return ( GHT_OK == RC );
}
#endif//#if defined( DBG )

//--------------------------------------------------------------------

DldHookedObjectEntry*
DldHookedObjectsHashTable::RemoveObject(
    __in OSObject* obj
    )
{
    DldHookedObjectEntry* objEntry;
    
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    //
    // the object was referenced by AddObject
    //
    objEntry = (DldHookedObjectEntry*)ght_remove( this->HashTable, sizeof( obj ), &obj );
    if( objEntry ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeObject == objEntry->Type );
    }
    
    return objEntry;
}

//--------------------------------------------------------------------

DldHookedObjectEntry*
DldHookedObjectsHashTable::RemoveObject(
    __in DldHookTypeVtableObjKey* vtableHookObj
    )
{
    DldHookedObjectEntry* objEntry;
    
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    //
    // the object was referenced by AddObject
    //
    objEntry = (DldHookedObjectEntry*)ght_remove( this->HashTable, sizeof( *vtableHookObj ), vtableHookObj );
    if( objEntry ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeVtableObj == objEntry->Type );
    }
    
    return objEntry;
}

//--------------------------------------------------------------------

DldHookedObjectEntry*
DldHookedObjectsHashTable::RemoveObject(
    __in DldHookTypeVtableKey* vtableHookVtable
    )
{
    DldHookedObjectEntry* objEntry;
    
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
#endif//DBG
    
    //
    // the object was referenced by AddObject
    //
    objEntry = (DldHookedObjectEntry*)ght_remove( this->HashTable, sizeof( *vtableHookVtable ), vtableHookVtable );
    if( objEntry ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeVtable == objEntry->Type );
    }
    
    return objEntry;
}

//--------------------------------------------------------------------

DldHookedObjectEntry*
DldHookedObjectsHashTable::RetrieveObjectEntry(
    __in OSObject* obj,
    __in bool reference
    )
{
    DldHookedObjectEntry* objEntry;
    
    objEntry = (DldHookedObjectEntry*)ght_get( this->HashTable, sizeof( obj ), &obj );
    if( objEntry ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeObject == objEntry->Type );
        
        if( reference )
            objEntry->retain();
    }
    
    return objEntry;
}

//--------------------------------------------------------------------

DldHookedObjectEntry*
DldHookedObjectsHashTable::RetrieveObjectEntry(
    __in DldHookTypeVtableObjKey* vtableHookObj,
    __in bool reference
    )
{
    DldHookedObjectEntry* objEntry;
    
    assert( NULL != vtableHookObj->Object && NULL != vtableHookObj->metaClass );
    
    objEntry = (DldHookedObjectEntry*)ght_get( this->HashTable, sizeof( *vtableHookObj ), vtableHookObj );
    if( objEntry ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeVtableObj == objEntry->Type );
        
        if( reference )
            objEntry->retain();
    }
    
    return objEntry;
}

//--------------------------------------------------------------------

DldHookedObjectEntry*
DldHookedObjectsHashTable::RetrieveObjectEntry(
    __in DldHookTypeVtableKey* vtableHookVtable,
    __in bool reference
    )
{
    DldHookedObjectEntry* objEntry;
    
    assert( NULL != vtableHookVtable->metaClass );

    objEntry = (DldHookedObjectEntry*)ght_get( this->HashTable, sizeof( *vtableHookVtable ), vtableHookVtable );
    if( objEntry ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeVtable == objEntry->Type );
        
        if( reference )
            objEntry->retain();
    }
    
    return objEntry;
}
//--------------------------------------------------------------------

#if defined( DBG )
DldDbgVtableHookToObject*
DldHookedObjectsHashTable::RetrieveObjectEntry(
    __in OSMetaClassBase::_ptf_t*    key
    )
{
    DldDbgVtableHookToObject* dbgEntry;
    
    dbgEntry = (DldDbgVtableHookToObject*)ght_get( this->HashTable, sizeof( key ), &key );
        
    return dbgEntry;
}
#endif//#if defined( DBG )

//--------------------------------------------------------------------

DldHookerCommonClass::DldHookerCommonClass()
{
    
    this->ClassHookerObject = NULL;
    this->OriginalVtable    = NULL;
    this->HookClassVtable   = NULL;
    this->HookedObjectsCounter = 0x0;
    this->HookType = DldHookTypeUnknown;
    
};

DldHookerCommonClass::~DldHookerCommonClass()
{ 
    if( this->ClassName )
        this->ClassName->release();
    
    assert( 0x0 == this->HookedObjectsCounter );
};

//--------------------------------------------------------------------
    
//
// saves an object pointer, the object is not referenced
//
void
DldHookerCommonClass::SetClassHookerObject(
    __in DldHookerBaseInterface*  ClassHookerObject
   )
{
    assert( NULL == this->ClassHookerObject );
    this->ClassHookerObject = ClassHookerObject;
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::SetHookedVtableSize(
    __in unsigned int HookedVtableSize
    )
{
    assert( 0x0 == this->HookedVtableSize );
    assert( 0x0 < HookedVtableSize && HookedVtableSize < 0xFFFF );
    
    this->HookedVtableSize = HookedVtableSize;
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::SetInheritanceDepth(
    __in DldInheritanceDepth   Depth
    )
{
    assert( DldInheritanceDepth_0 == this->InheritanceDepth );
    this->InheritanceDepth = Depth;
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::SetHookedVtableFunctionsInfo(
    __in DldHookedFunctionInfo* HookedFunctonsInfo,
    __in unsigned int NumberOfEntries
    )
{
    assert( NULL == this->HookedFunctonsInfo );
    assert( NULL != HookedFunctonsInfo );
    
    this->HookedFunctonsInfo = HookedFunctonsInfo;
    this->HookedFunctonsInfoEntriesNumber = NumberOfEntries;
    
    assert( (unsigned int)(-1) ==  this->HookedFunctonsInfo[ this->HookedFunctonsInfoEntriesNumber - 0x1 ].VtableIndex );
}

//--------------------------------------------------------------------

//
// called after the original IOService starts successfully,
// so this si more  a start postcallback!
//
bool
DldHookerCommonClass::start(
    __in IOService* serviceObject,
    __in IOService* provider
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != provider );
    DBG_PRINT( ( "%s->%s::start( object=0x%p ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject ) );
    
    DldIOService* dldService;
    DldIOService* dldServiceProvider;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( !dldService ){
        
        return true;
    }
    
    dldServiceProvider = DldIOService::RetrieveDldIOServiceForIOService( provider );
    if( !dldServiceProvider ){
        
        dldService->release();
        return true;
    }
    
    dldService->start( serviceObject, dldServiceProvider );
    
    dldServiceProvider->release();
    dldService->release();
    return true;
}

//--------------------------------------------------------------------

bool DldHookerCommonClass::open(
    __in IOService*    serviceObject,
    __in IOService *   forClient,
    __in IOOptionBits  options,
    __in void *		   arg )
{
    assert( NULL != this->ClassHookerObject );
    DBG_PRINT( ( "%s->%s::open( object=0x%p ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( !dldService ){
        
        return true;
    }
    
    dldService->open( serviceObject, forClient, options, arg );
    dldService->release();
    return true;
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::free(
    __in IOService* serviceObject
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::free( object=0x%p ) \n",
                 this->ClassHookerObject->fGetClassName(),
                 serviceObject->getMetaClass()->getClassName(),
                 (void*)serviceObject ) );
    
    DldIOService* dldService;
    
    //
    // it's okay to get NULL for a chain of hooks - the first called hook does the job
    //
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        dldService->removeFromHashTable();
        dldService->release();
    }
    
    DldRemoveMediaEntriesFromMountCache( serviceObject );
    
    this->UnHookObject( serviceObject );
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::requestTerminate(
    __in IOService* serviceObject,
    __in IOService* provider,
    __in IOOptionBits options
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::requestTerminate( object=0x%p, provider=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)provider,
                provider->getMetaClass()->getClassName() ) );
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::willTerminate(
    __in IOService* serviceObject,
    __in IOService* provider,
    __in IOOptionBits options
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::willTerminate( object=0x%p, provider=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)provider,
                provider->getMetaClass()->getClassName() ) );
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::didTerminate(
    __in    IOService* serviceObject,
    __in    IOService* provider,
    __in    IOOptionBits options,
    __inout bool * defer
   )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::didTerminate( object=0x%p, provider=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)provider,
                provider->getMetaClass()->getClassName() ) );
    
    //
    // the probem with didTerminate is that some drivers process it incorrectly,
    // for example iokit_client_died() calls clientDied() which in turns calls
    // SCSITaskUserClient::didTerminate which calls SCSITaskUserClient::HandleTerminate
    // which in turn calls IOService::detach() which is incorrect for me
    //
    DldIOService* dldService;
    DldIOService* dldProvider;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    dldProvider = DldIOService::RetrieveDldIOServiceForIOService( provider );
    
    assert( dldService && dldProvider );
    
    if( dldService && dldProvider ){
        
        dldService->didTerminate( dldProvider, options, defer);
    }
    
    if( dldService ){
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService )
    }
    
    if( dldProvider ){
        
        dldProvider->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::terminate(
    __in IOService*   serviceObject,
    __in IOOptionBits options
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::terminate( object=0x%p ) \n",
                 this->ClassHookerObject->fGetClassName(),
                 serviceObject->getMetaClass()->getClassName(),
                 (void*)serviceObject ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    assert( dldService );
    
    if( dldService ){
      
        dldService->terminate( options );
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::terminateClient(
    __in IOService* serviceObject,
    __in IOService* client,
    __in IOOptionBits options
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::terminateClient( object=0x%p, client=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)client,
                client->getMetaClass()->getClassName() ) );
    
    return true;
}

//--------------------------------------------------------------------
    
bool
DldHookerCommonClass::finalize(
    __in IOService* serviceObject,
    __in IOOptionBits options
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::finalize( object=0x%p ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    assert( dldService );
    
    if( dldService ){
        
        dldService->finalize( options );
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::attach(
    __in IOService* serviceObject,
    __in IOService* provider
    )
/*
 this function has an idempotent symantic!
 */
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::attach( object=0x%p, provider=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)provider,
                provider->getMetaClass()->getClassName() ) );
    
    DldIOService* dldService;
    DldIOService* dldProvider;
    bool          bAttached = false;
    IOReturn      hooked;
    
    //
    // get the object if they have been added somehow
    //
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    dldProvider = DldIOService::RetrieveDldIOServiceForIOService( provider );
    
    //
    // create and hook the provider's object if this has not been done
    //
    if( !dldProvider ){
        
        dldProvider = DldIOService::withIOServiceAddToHash( provider );
        assert( dldProvider );
        if( !dldProvider ){
            
            bAttached = false;
            goto __exit;
        }
        
        hooked = gHookEngine->HookObject( provider );
        assert( kIOReturnSuccess == hooked );
        if( kIOReturnSuccess != hooked ){
            
            dldProvider->removeFromHashTable();
            bAttached = false;
            goto __exit;
        }
        
        //
        // attach a newly created provider to the root
        //
        dldProvider->attachToParent( gDldRootEntry, gDldDeviceTreePlan );
        
    }// end if( !dldProvider )
    
    //
    // create and hook the service's object if this has not been done
    //
    if( !dldService ){
        
        //
        // do not update property here ( the second parameter is true ) as
        // the object doesn't have parent properties initialized so the
        // object's property might be filled incorrectly, the parent properties
        // will be set by the following dldService->attach( dldProvider ) call
        //
        dldService = DldIOService::withIOServiceAddToHash( serviceObject, true );
        assert( dldService );
        if( !dldService ){
            
            bAttached = false;
            goto __exit;
        }
        
        hooked = gHookEngine->HookObject( serviceObject );
        assert( kIOReturnSuccess == hooked );
        if( kIOReturnSuccess != hooked ){
            
            dldService->removeFromHashTable();
            bAttached = false;
            goto __exit;
        }
        
    }// end if( !dldService )
    
    assert( dldProvider && dldService );
    
    //
    // attach
    //
    bAttached = dldService->attach( dldProvider );
    assert( bAttached );
    if( bAttached ){
        //
        // it is a possible situation when an attached object already has
        // an attached user client, for example this is a case of HID devices
        // when  windowsServer's user client is a global object that attached
        // to another global objects that are attached to all HID devices so if a
        // new HID device arrives it will be a paren( one of the many) for already
        // created HID stacks
        //
        DldObjectPropertyEntry* serviceProperty = dldService->getObjectProperty();
        if( serviceProperty ){
            
            if( serviceProperty->dataU.property->userClientAttached )
                dldProvider->userClientAttached();
                            
        }// end if( property )
    }
    
__exit:
    
    if( dldProvider )
        dldProvider->release();
    
    if( dldService )
        dldService->release();
    
    assert( bAttached );
    
    return bAttached;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::attachToChild(
    __in IOService* serviceObject,
    __in IORegistryEntry * child,
    __in const IORegistryPlane * plane
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::attachToChild( object=0x%p, child=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)child,
                child->getMetaClass()->getClassName() ) );
    
    if( plane != gIOServicePlane )
        return true;
    
    IOService* childObject;
    
    childObject = OSDynamicCast( IOService, child );
    if( !childObject )
        return true;
    
    //
    // attach is an idempotent function
    //
    return this->attach( childObject, serviceObject );
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::detach(
    __in IOService* serviceObject,
    __in IOService* provider
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::detach( object=0x%p, provider=0x%p (%s) ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, (void*)provider,
                provider->getMetaClass()->getClassName() ) );
    
    DldIOService* dldService;
    DldIOService* dldProvider;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    dldProvider = DldIOService::RetrieveDldIOServiceForIOService( provider );
    if( dldService && dldProvider ){
        
        dldService->detach( dldProvider );
    }
    
    if( dldService )
        dldService->release();
    
    if( dldProvider )
        dldProvider->release();

    DldRemoveMediaEntriesFromMountCache( serviceObject );
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::setPropertyTable(
    __in IOService* serviceObject,
    __in OSDictionary * dict
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty1( object=0x%p ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    
    if( dldService ){
        
        dldService->LockExclusive();
        {// start of the lock
            dldService->setPropertyTable( dict );
        }// end of the lock
        dldService->UnLockExclusive();
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
    
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty1(
    __in IOService* serviceObject,
    __in const OSSymbol * aKey,
    __in OSObject * anObject
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty1( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey->getCStringNoCopy() ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, anObject );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty2(
    __in IOService* serviceObject,
    __in const OSString * aKey,
    __in OSObject * anObject
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty2( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey->getCStringNoCopy() ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, anObject );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty3(
    __in IOService* serviceObject,
    __in const char * aKey,
    __in OSObject * anObject
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty3( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey ) );
    
    DldIOService* dldService;
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, anObject );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty4(
    __in IOService* serviceObject,
    __in const char * aKey,
    __in const char * aString
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty4( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, aString );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty5(
    __in IOService* serviceObject,
    __in const char * aKey,
    __in bool aBoolean
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty5( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, aBoolean );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty6(
    __in IOService* serviceObject,
    __in const char * aKey,
    __in unsigned long long aValue,
    __in unsigned int aNumberOfBits
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty6( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, aValue, aNumberOfBits );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        //
        // the property has changed so the descriptor must be updated
        //
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::setProperty7(
    __in IOService* serviceObject,
    __in const char * aKey,
    __in void * bytes,
    __in unsigned int length
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::setProperty7( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        bool bRet;
        
        dldService->LockExclusive();
        {// start of the lock
            bRet = dldService->setProperty( aKey, bytes, length );
        }// end of the lock
        dldService->UnLockExclusive();
        
        assert( bRet );
        
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
        
        return bRet;
    }
    
    return true;
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::removeProperty1(
    __in IOService* serviceObject,
    __in const OSSymbol * aKey
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::removeProperty1( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey->getCStringNoCopy() ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        dldService->LockExclusive();
        {// start of the lock
            dldService->removeProperty( aKey );
        }// end of the lock
        dldService->UnLockExclusive();
        
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
    
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::removeProperty2(
    __in IOService* serviceObject,
    __in const OSString * aKey
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::removeProperty2( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey->getCStringNoCopy() ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        dldService->LockExclusive();
        {// start of the lock
            dldService->removeProperty( aKey );
        }// end of the lock
        dldService->UnLockExclusive();
        
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::removeProperty3(
    __in IOService* serviceObject,
    __in const char * aKey
    )
{
    assert( NULL != this->ClassHookerObject );
    assert( NULL != serviceObject );
    DBG_PRINT( ( "%s->%s::removeProperty3( object=0x%p, aKey=%s ) \n",
                this->ClassHookerObject->fGetClassName(),
                serviceObject->getMetaClass()->getClassName(),
                (void*)serviceObject, aKey ) );
    
    DldIOService* dldService;
    dldService = DldIOService::RetrieveDldIOServiceForIOService( serviceObject );
    if( dldService ){
        
        dldService->LockExclusive();
        {// start of the lock
            dldService->removeProperty( aKey );
        }// end of the lock
        dldService->UnLockExclusive();
        
        if( dldService->getObjectProperty() )
            dldService->getObjectProperty()->updateDescriptor( serviceObject, dldService, false );
        
        dldService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldService );
    }
}

//--------------------------------------------------------------------

/*
 an excerpt of the call stack for newUserClient call
 #0  DldHookerCommonClass::newUserClient1 (this=0x59fdc08, serviceObject=0x5571200, owningTask=0xc5419c4, securityID=0xc5419c4, type=0, properties=0x0, handler=0x318a3d08) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldHookerCommonClass.cpp:1413
 #1  0x463f5e1f in DldHookerCommonClass2<IOServiceDldHook2<(_DldInheritanceDepth)1>, IOService>::newUserClient1_hook (this=0x5571200, owningTask=0xc5419c4, securityID=0xc5419c4, type=0, properties=0x0, handler=0x318a3d08) at DldHookerCommonClass2.h:1608
 #2  0x0064ae67 in is_io_service_open_extended (_service=0x5571200, owningTask=0xc5419c4, connect_type=0, ndr={mig_vers = 0 '\0', if_vers = 0 '\0', reserved1 = 0 '\0', mig_encoding = 0 '\0', int_rep = 1 '\001', char_rep = 0 '\0', float_rep = 0 '\0', reserved2 = 0 '\0'}, properties=0x0, propertiesCnt=0, result=0xc71c5b8, connection=0x318a3d80) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:2395
 #3  0x002c1bbb in _Xio_service_open_extended (InHeadP=0x6c17568, OutHeadP=0xc71c584) at device/device_server.c:14130
 #4  0x00226d74 in ipc_kobject_server (request=0x6c17500) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/ipc_kobject.c:339
 #5  0x002126b1 in ipc_kmsg_send (kmsg=0x6c17500, option=0, send_timeout=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/ipc_kmsg.c:1371
 #6  0x0021e193 in mach_msg_overwrite_trap (args=0x719d4c8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:505
 #7  0x0021e37d in mach_msg_trap (args=0x719d4c8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:572
 #8  0x002d8b06 in mach_call_munger64 (state=0x719d4c4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/bsd_i386.c:765 
 */
IOReturn
DldHookerCommonClass::newUserClient1( __in IOService* serviceObject, task_t owningTask, void * securityID,
                                     UInt32 type,  OSDictionary * properties, IOUserClient ** handler )
{
    DldRequestedAccess requestedAccess = {0x0};
    
    requestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA;
    
    return checkAndLogUserClientAccess( serviceObject, owningTask, &requestedAccess );
}

//--------------------------------------------------------------------

IOReturn
DldHookerCommonClass::checkAndLogUserClientAccess(
    __in     IOService* parentServiceObject,
    __in     pid_t pid,
    __in_opt kauth_cred_t credentialRef,
    __in     DldRequestedAccess* _requestedAccess
    )
/*
 parentServiceObject - an object to which a new client will be attached or has been attached,
 the returend value of kIOReturnSuccess means that the request must be allowed, any other
 value means that the request is not allowe and has been logged as denied
 */
{
    IOReturn            retStatus = kIOReturnSuccess;
    DldIOService*       dldService;
    bool                disable = false;
    
    assert( preemption_enabled() );
    
    dldService = DldIOService::RetrieveDldIOServiceForIOService( parentServiceObject );
    if( !dldService )
        return kIOReturnSuccess;
    
    assert( dldService );
    
    DldObjectPropertyEntry* hidDeviceProp = dldService->retrievePropertyByTypeRef( DldObjectPropertyType_IOHIDSystem );
    if( hidDeviceProp ){
        
        //
        // skip IOHIDSystem clients check as a single IOHIDSystem object serves multiple devices, the check will be made
        // at the level where a request to a particular device can be detected ( usually at a USB interface object )
        //
        hidDeviceProp->release();
        return kIOReturnSuccess;
    }
    
    if( DldIsDLServiceOrItsChild( pid ) ){
        
        //
        // this is a request from the DL Service process or its child, allow it to proceed
        //
        return kIOReturnSuccess;
    }
    
    //
    // check the object itself excluding its parents which will be checked later
    //
    if( dldService->getObjectProperty() ){
        
        DldAccessCheckParam    param;
        DldRequestedAccess     requestedAccess = *_requestedAccess;
        
        if( dldService->getObjectProperty()->dataU.property ){
            
            //
            // in case of CD/DVD apply only KAUTH_VNODE_READ_DATA to not have a false write in the audit
            // and to not reject benign requests, all write requests are caught at other levels ( FSD or SCSI )
            //
            if( DldObjectPopertyType_IODVDMedia == dldService->getObjectProperty()->dataU.property->typeDsc.type ||
                DldObjectPopertyType_IOCDMedia == dldService->getObjectProperty()->dataU.property->typeDsc.type ||
                DLD_DEVICE_TYPE_CD_DVD == dldService->getObjectProperty()->dataU.property->deviceType.type.major )
            {
                requestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA;
                requestedAccess.winRequestedAccess = DEVICE_READ;
            }
        }
        
        bzero( &param, sizeof( param ) );
        
        //
        // if credentialRef is not NULL then a user is known
        //
        param.userSelectionFlavor = credentialRef ? kDefaultUserSelectionFlavor : kActiveUserSelectionFlavor;
        param.aclType             = kDldAclTypeSecurity;
        param.checkParentType     = false; // all ancestors will be checked later
        param.dldRequestedAccess  = requestedAccess;
        param.credential          = credentialRef; // might be NULL
        param.service             = parentServiceObject;
        param.dldIOService        = dldService;
        param.userClient          = true;
#if defined(LOG_ACCESS)
        param.sourceFunction      = __PRETTY_FUNCTION__;
        param.sourceFile          = __FILE__;
        param.sourceLine          = __LINE__;
        param.dldVnode            = NULL;
#endif//#if defined(LOG_ACCESS)
        
        /*if( dldIOService->getObjectProperty() &&
         DLD_DEVICE_TYPE_REMOVABLE == dldIOService->getObjectProperty()->dataU.property->deviceType.type.major ){}*/
        
        ::DldAcquireResources( &param );
        ::isAccessAllowed( &param );
        
        //
        // we didn't check parent, it will be checked later
        //
        assert( !param.output.access.result[ DldParentTypeFlavor ].disable &&
                !param.output.access.result[ DldParentTypeFlavor ].log );
        
        disable = ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                    param.output.access.result[ DldMajorTypeFlavor ].disable );
        
        //
        // should we log?
        //
        if( param.output.access.result[ DldFullTypeFlavor ].log ||
            param.output.access.result[ DldMajorTypeFlavor ].log ){
            
            DldDriverDataLogInt intData;
            DldDriverDataLog    data;
            bool                logDataValid;
            kauth_cred_t        credentials;
            
            intData.logDataSize = sizeof( data );
            intData.logData = &data;
            
            credentials = credentialRef;
            if( NULL == credentials )
                credentials = param.decisionKauthCredEntry ? param.decisionKauthCredEntry->getCred() : param.activeKauthCredEntry->getCred();
            
            assert( credentials );
            
            logDataValid = dldService->initDeviceLogData( &requestedAccess,
                                                          (ALL_WRITE & requestedAccess.winRequestedAccess) ? DldFileOperationWrite : DldFileOperationRead,
                                                          pid,
                                                          credentials,
                                                          ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                                                            param.output.access.result[ DldMajorTypeFlavor ].disable ),
                                                          &intData );
            
            assert( logDataValid );
            if( logDataValid ){
                
                gLog->logData( &intData );
                
            } else {
                
                DBG_PRINT_ERROR(("device log data is invalid\n"));
            }
            
        }// end log
        
        ::DldReleaseResources( &param );
    }// end if( dldService->getObjectProperty() )
    
    
    bool    locked = false;
    
    //
    // check the parents by starting from the top to bottom
    // and checking whether any device denies access for a user
    //
    dldService->LockShared();
    locked = true;
    for( unsigned int i=0;
         !disable &&
         NULL != dldService->getParentProperties() &&
         i < dldService->getParentProperties()->getCount();
         ++i )
    {
        OSObject*                object;
        DldObjectPropertyEntry*  property;
        
        assert( locked );
        
        object = dldService->getParentProperties()->getObject( i );
        assert( object );
        if( !object )
            continue;
        
        assert( OSDynamicCast( DldObjectPropertyEntry, object ) );
        
        property = (DldObjectPropertyEntry*)object;
        
        assert( property->dataU.property );
        
        //
        // we are not interested in a device w/o type, as type defines security
        //
        if( 0x0 == property->dataU.property->deviceType.combined )
            continue;
        
        //
        // continue with the parent property
        //
        property->retain();
        //
        // release the lock as the following code might take a significant amount of time
        //
        dldService->UnLockShared();
        locked = false;
        {// entering a lock free block
            
            assert( !locked );
            
            DldAccessCheckParam     param;
            DldIOService*           dldParentService;
            kauth_ace_rights_t      kauthRequestedAccess = KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA; // we don't know the nature of request, so guess it
            
            bzero( &param, sizeof( param ) );
            
            assert( property->dataU.property->service );
            
            dldParentService = DldIOService::RetrieveDldIOServiceForIOService( property->dataU.property->service );
            assert( dldParentService );// can fail in a very rare case which depends on
            // an OS concurrency model for objects management,
            // I can envision a case when there is a race between
            // a stack tear down and user's requests
            if( !dldParentService )
                goto __exit_wo_lock;
            
            if( property->dataU.property ){
                
                //
                // in case of CD/DVD apply only KAUTH_VNODE_READ_DATA to not have a false write in the audit
                // and to not reject benign requests, all write requests are caught at other levels ( FSD or SCSI )
                //
                if( DldObjectPopertyType_IODVDMedia == property->dataU.property->typeDsc.type ||
                    DldObjectPopertyType_IOCDMedia == property->dataU.property->typeDsc.type ||
                    DLD_DEVICE_TYPE_CD_DVD == property->dataU.property->deviceType.type.major )
                        kauthRequestedAccess = KAUTH_VNODE_READ_DATA;
            }
            
            //
            // if credentialRef is not NULL then a user is known
            //
            param.userSelectionFlavor = credentialRef ? kDefaultUserSelectionFlavor : kActiveUserSelectionFlavor;
            param.aclType             = kDldAclTypeSecurity;
            param.checkParentType     = false; // we are now checking the parents, do not check grandparent right now
            param.dldRequestedAccess.kauthRequestedAccess = kauthRequestedAccess;
            param.credential          = credentialRef; // might be NULL
            param.service             = NULL;
            param.dldIOService        = dldParentService;
            param.dldIOServiceRequestOwner = dldService;
            param.userClient          = true;
            
#if defined(LOG_ACCESS)
            param.sourceFunction      = __PRETTY_FUNCTION__;
            param.sourceFile          = __FILE__;
            param.sourceLine          = __LINE__;
            param.dldVnode            = NULL;
#endif//#if defined(LOG_ACCESS)
            
            //
            // let others know that this is not a genuine call at the parent's level
            //
            param.calledOnTopDownTraversing = true;
            
            /*if( dldIOService->getObjectProperty() &&
             DLD_DEVICE_TYPE_REMOVABLE == dldIOService->getObjectProperty()->dataU.property->deviceType.type.major ){}*/
            
            ::DldAcquireResources( &param );
            ::isAccessAllowed( &param );
            
            //
            // we didn't check parent
            //
            assert( !param.output.access.result[ DldParentTypeFlavor ].disable &&
                    !param.output.access.result[ DldParentTypeFlavor ].log );
            
            disable = ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                        param.output.access.result[ DldMajorTypeFlavor ].disable );
            
            
            //
            // should we log?
            //
            if( param.output.access.result[ DldFullTypeFlavor ].log ||
                param.output.access.result[ DldMajorTypeFlavor ].log ){
                
                DldDriverDataLogInt intData;
                DldDriverDataLog    data;
                bool                logDataValid;
                kauth_cred_t        credentials;
                
                intData.logDataSize = sizeof( data );
                intData.logData = &data;
                
                credentials = credentialRef;
                if( NULL == credentials )
                    credentials = param.decisionKauthCredEntry ? param.decisionKauthCredEntry->getCred() : param.activeKauthCredEntry->getCred();
                
                assert( credentials );
                
                logDataValid = dldParentService->initDeviceLogData( &param.dldRequestedAccess,
                                                                    (ALL_WRITE & param.dldRequestedAccess.winRequestedAccess) ? DldFileOperationWrite : DldFileOperationRead,
                                                                    pid,
                                                                    credentials,
                                                                    ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                                                                      param.output.access.result[ DldMajorTypeFlavor ].disable ),
                                                                    &intData );
                
                assert( logDataValid );
                if( logDataValid ){
                    
                    gLog->logData( &intData );
                    
                } else {
                    
                    DBG_PRINT_ERROR(("device log data is invalid\n"));
                }
                
            }// end log
            
            dldParentService->release();
            DLD_DBG_MAKE_POINTER_INVALID( dldParentService );
            
            ::DldReleaseResources( &param );
            
        __exit_wo_lock:;
#if defined(DBG)
            assert( param.resourcesReleased == param.resourcesAcquired );
#endif//DBG
        }// exiting the lock free block
        property->release();
        DLD_DBG_MAKE_POINTER_INVALID( property );
        DLD_DBG_MAKE_POINTER_INVALID( object );
        //
        // reacquire the lock
        //
        assert( !locked );
        dldService->LockShared();
        locked = true;
        
    }// end for()
    assert( locked );
    dldService->UnLockShared();
    
    retStatus = disable? kIOReturnNotPrivileged : kIOReturnSuccess;
    
    dldService->userClientAttached();
    dldService->release();
    DLD_DBG_MAKE_POINTER_INVALID( dldService );
    
    return retStatus;
}

//--------------------------------------------------------------------

IOReturn
DldHookerCommonClass::checkAndLogUserClientAccess(
    __in IOService* parentServiceObject,
    __in task_t owningTask,
    __in DldRequestedAccess* requestedAccess
    )
/*
 parentServiceObject - an object to which a new client will be attached or has been attached,
 the returend value of kIOReturnSuccess means that the request must be allowed, any other
 value means that the request is not allowe and has been logged as denied
 */
{
    IOReturn            retStatus = kIOReturnSuccess;
    
    //
    // check the security if there is a corresponding user's BSD process,
    // TO DO - there might be a race condition if the BSD process will exit and zeros the
    // bsd_info field of the task structure, but I believe that the process
    // will wait for a system call completion before exit ( verify this assumption! )
    //
    if( DldTaskToBsdProc( owningTask ) ){
    
        kauth_cred_t    credentialRef; 
        
        assert( DldTaskToBsdProc( owningTask ) );
        assert( DldBsdProcToTask( DldTaskToBsdProc( owningTask ) ) == owningTask );
        
        credentialRef = kauth_cred_proc_ref( DldTaskToBsdProc( owningTask ) );
        assert( credentialRef );
        if( !credentialRef )
            credentialRef = kauth_cred_get_with_ref();// resort to a current thread's credentials
        
        assert( credentialRef );
        
        retStatus = this->checkAndLogUserClientAccess( parentServiceObject,
                                                       proc_pid( DldTaskToBsdProc( owningTask ) ),
                                                       credentialRef,
                                                       requestedAccess );
        
        kauth_cred_unref( &credentialRef );
    }// end DldTaskToBsdProc( owningTask )

    return retStatus;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::ObjectFirstPublishCallback(
    __in IOService* newService
    )
{
    DBG_PRINT( ( "%s->DldHookerCommonClass::ObjectFirstPublishCallback( object=0x%p ) \n",
                this->ClassHookerObject->fGetClassName(),
                (void*)newService ) );
    
    return true;
}

//--------------------------------------------------------------------

bool
DldHookerCommonClass::ObjectTerminatedCallback(
    __in IOService* terminatedService
    )
{
    DBG_PRINT( ( "%s->DldHookerCommonClass::ObjectTerminatedCallback( object=0x%p ) \n",
                this->ClassHookerObject->fGetClassName(),
                (void*)terminatedService ) );
    
    return true;
}

//--------------------------------------------------------------------

//static bool gEnterDebugger = false;

void
DldHookerCommonClass::DldHookVtableFunctions(
    __in    OSObject*                     object,
    __inout DldHookedFunctionInfo*        HookedFunctonsInfo,
    __inout OSMetaClassBase::_ptf_t*      VtableToHook,
    __inout OSMetaClassBase::_ptf_t*      NewVtable
)
{
    
    DldHookedFunctionInfo* PtrHookInfo = HookedFunctonsInfo;
    
    while( (unsigned int)(-1) != PtrHookInfo->VtableIndex ){
        
        assert( 0x0  != PtrHookInfo->VtableIndex );
        assert( NULL != PtrHookInfo->HookingFunction );
        assert( NULL == PtrHookInfo->OriginalFunction );
        
        unsigned int   Bytes;
        
#if defined( DBG )
        /*
        if( VtableToHook[ PtrHookInfo->VtableIndex - 1] == PtrHookInfo->HookingFunction ){
            //
            // an attempt to rehook
            //
            DldDbgVtableHookToObject*   DbgEntry;
            
            DbgEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableToHook[ PtrHookInfo->VtableIndex - 1] );
            
            if( gEnterDebugger ){
                
                __asm__ volatile( "int $0x3" );
            }
        }
         */
#endif//#if defined( DBG )
        
        //
        // save an original value so the hooking function can call it
        //
        PtrHookInfo->OriginalFunction = VtableToHook[ PtrHookInfo->VtableIndex - 1];
        
        //
        // check for double hook
        //
        assert( PtrHookInfo->OriginalFunction != PtrHookInfo->HookingFunction );
        
        //
        // set a hooking function address
        //
        //NewVtable[ PtrHookInfo->VtableIndex - 1] = PtrHookInfo->HookingFunction;
        
        
        Bytes = DldWriteWiredSrcToWiredDst( (vm_offset_t)&PtrHookInfo->HookingFunction,
                                            (vm_offset_t)&NewVtable[ PtrHookInfo->VtableIndex - 1],
                                            sizeof( PtrHookInfo->HookingFunction ) );
        
        assert( sizeof( PtrHookInfo->HookingFunction ) == Bytes );
        assert( NewVtable[ PtrHookInfo->VtableIndex - 1] == PtrHookInfo->HookingFunction );
        
#if defined( DBG )
        
        /*
        //
        // restore pointer back, the test
        //
        Bytes = DldWriteWiredSrcToWiredDst( (vm_offset_t)&PtrHookInfo->OriginalFunction,
                                            (vm_offset_t)&NewVtable[ PtrHookInfo->VtableIndex - 1],
                                            sizeof( PtrHookInfo->HookingFunction ) );
         */
#endif//#if defined( DBG )
        
#if defined( DBG )
        /*
        DldDbgVtableHookToObject*   DbgEntry = (DldDbgVtableHookToObject*)IOMalloc( sizeof( *DbgEntry ) );
        assert( DbgEntry );
        if( DbgEntry ){
            
            DbgEntry->Object      = object;
            DbgEntry->MetaClass   = object->getMetaClass();
            DbgEntry->Vtable      = *(OSMetaClassBase::_ptf_t**)object;
            DbgEntry->VtableEntry = &NewVtable[ PtrHookInfo->VtableIndex - 1];// also a key
            DbgEntry->HookInfo    = PtrHookInfo;
            
            DbgEntry->copy = (DldDbgVtableHookToObject*)IOMalloc( sizeof( *DbgEntry ) );
            if( DbgEntry->copy ){
                
                *DbgEntry->copy      = *DbgEntry;
                DbgEntry->copy->copy = DbgEntry;
            }
            
            
            DldHookedObjectsHashTable::sHashTable->AddObject( DbgEntry->VtableEntry, DbgEntry, false );
            //assert( DbgEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( DbgEntry->VtableEntry ) );
            
            if( gEnterDebugger ){
                
                //__asm__ volatile( "int $0x3" );
            }
            
        }
         */
#endif//#if defined( DBG )
                                    
        ++PtrHookInfo;
        
    }// end while
    
}

//--------------------------------------------------------------------

void
DldHookerCommonClass::DldUnHookVtableFunctions(
    __inout DldHookedFunctionInfo*        HookedFunctonsInfo,
    __inout OSMetaClassBase::_ptf_t*      VtableToUnHook
    )
{
    
    DldHookedFunctionInfo* PtrHookInfo = HookedFunctonsInfo;
    
    while( (unsigned int)(-1) != PtrHookInfo->VtableIndex ){
        
        assert( 0x0  != PtrHookInfo->VtableIndex );
        assert( NULL != PtrHookInfo->HookingFunction );
        assert( NULL != PtrHookInfo->OriginalFunction );
        
        unsigned int   Bytes;
        
        //
        // restore the original value
        //
        //VtableToUnHook[ PtrHookInfo->VtableIndex - 1] = PtrHookInfo->OriginalFunction;
        
        Bytes = DldWriteWiredSrcToWiredDst( (vm_offset_t)&PtrHookInfo->OriginalFunction,
                                            (vm_offset_t)&VtableToUnHook[ PtrHookInfo->VtableIndex - 1],
                                            sizeof( PtrHookInfo->OriginalFunction ) );
        
        assert( sizeof( PtrHookInfo->OriginalFunction ) == Bytes );
        assert( VtableToUnHook[ PtrHookInfo->VtableIndex - 1] == PtrHookInfo->OriginalFunction );
        
        ++PtrHookInfo;
        
    }// end while
    
}

//--------------------------------------------------------------------

OSMetaClassBase::_ptf_t
DldHookerCommonClass::GetOriginalFunction(
    __in OSObject* hookedObject,
    __in unsigned int indx
    )
{   
    
    assert( this->HookedFunctonsInfo );
    assert( indx < this->HookedFunctonsInfoEntriesNumber );
    
    
    OSMetaClassBase::_ptf_t    OriginalFunction;
    
    if( DldHookTypeObject == this->HookType ){
        
        assert( DldInheritanceDepth_0 == this->InheritanceDepth );
               
        //
        // this is an optimization, the same can be achieved by finding
        // an entry of the DldHookEntryTypeObject type in the hash table
        // and taking the Common.HookedVtableFunctionsInfo[ indx ] value
        //
        OriginalFunction = this->HookedFunctonsInfo[ indx ].OriginalFunction;
        assert( OriginalFunction );
        if( NULL != OriginalFunction )
            return OriginalFunction;
        
    }
    
    //
    // a direct vtable hook saves the original addresses in the hasth table entries
    //
    assert( NULL == this->HookedFunctonsInfo[ indx ].OriginalFunction );
    
    //
    // the class doesn't contain the original functons, it is pretty normal
    // if for example a vtable was hooked directly instead of vtable
    // replacement for the object
    //
    
    //
    // check the cache table
    //
    DldSingleInheritingClassObjectPtr ObjU;
    const OSMetaClass*  objectMetaClass;
    const OSMetaClass*  hookedMetaClass;
    const OSMetaClass*  parentMetaClass;
    const OSMetaClass*  keyMetaClass;
    unsigned int currentObjectDepth;
    
    ObjU.fObj = hookedObject;
    objectMetaClass = hookedObject->getMetaClass();
    assert( objectMetaClass );
    
    //
    // if the checked meta class id not at the inheritance depth then this is a call to
    // super::Fun() from a class on the inheritance chain, the corresponding meta class
    // should be used to pick up an entry from the hash table
    //
    parentMetaClass = objectMetaClass;
    
    assert( this->MetaClass && this->ClassName );
    
    currentObjectDepth = 0x0;
    hookedMetaClass = objectMetaClass;
    while( hookedMetaClass && hookedMetaClass != this->MetaClass ){
        
        hookedMetaClass = hookedMetaClass->getSuperClass();
        ++currentObjectDepth;
    }
    
    assert( hookedMetaClass == this->MetaClass );
    assert( objectMetaClass == parentMetaClass );
    
#if defined(DBG)
    { 
        //
        // a check for consistency
        //
        const OSMetaClass*  nextMetaClass = objectMetaClass;
        unsigned int        nextObjectDepth = 0x0;
        
        //
        // 10.6 and 10.7 lacks getClassNameSymbol(), it happened 10.8 does not export this symbol though it exists in the kernel
        //
/*#if (defined(MAC_OS_X_VERSION_10_8) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
        while( nextMetaClass && ( ! this->ClassName->isEqualTo( nextMetaClass->getClassNameSymbol() ) ) ){
            
            nextMetaClass = nextMetaClass->getSuperClass();
            ++nextObjectDepth;
        }
#else*/
        const OSSymbol*  className = OSSymbol::withCString( nextMetaClass->getClassName() );
        while( nextMetaClass && ( ! this->ClassName->isEqualTo( className ) ) ){
            
            nextMetaClass = nextMetaClass->getSuperClass();
            ++nextObjectDepth;
            if( ! nextMetaClass )
                break;
            
            className->release();
            className = OSSymbol::withCString( nextMetaClass->getClassName() );
        }
        
        className->release();
//#endif
        assert( nextObjectDepth == currentObjectDepth && nextMetaClass == hookedMetaClass );
    }
#endif
    
    //
    // this actually a desperate attempt to allow the system to continue
    // after encountering a nearly fatal error
    //
    if( NULL == hookedMetaClass ){
        
        assert( !"trying to restore an integrity after a fatal bug in DldHookerCommonClass::GetOriginalFunction, this->MetaClass has an incorrect value" );
        DBG_PRINT_ERROR(("FATAL ERROR!!! Trying to restore an integrity after a fatal bug in DldHookerCommonClass::GetOriginalFunction, this->MetaClass has an incorrect value\n"));
        
        hookedMetaClass = objectMetaClass;
        currentObjectDepth = 0x0;

        if( this->ClassName ){
            
            //
            // 10.6 and 10.7 lacks getClassNameSymbol(), it happened 10.8 does not export this symbol though it exists in the kernel
            //
/*#if (defined(MAC_OS_X_VERSION_10_8) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
            while( hookedMetaClass && ( ! this->ClassName->isEqualTo( hookedMetaClass->getClassNameSymbol() ) ) ){
                
                hookedMetaClass = hookedMetaClass->getSuperClass();
                ++currentObjectDepth;
            }
#else*/
            const OSSymbol*  className = OSSymbol::withCString( hookedMetaClass->getClassName() );
            while( hookedMetaClass && ( ! this->ClassName->isEqualTo( className ) ) ){
                
                hookedMetaClass = hookedMetaClass->getSuperClass();
                ++currentObjectDepth;
                if( ! hookedMetaClass )
                    break;
                
                className->release();
                className = OSSymbol::withCString( hookedMetaClass->getClassName() );
            }
            
            className->release();
//#endif
        } // end if( this->ClassName )
    } // end if( NULL == hookedMetaClass )
    
    if( currentObjectDepth != this->InheritanceDepth ){
        
        //
        // a derived class from a class which vtable has been hooked, i.e. a super::Foo() call
        //
        unsigned int delta;
        
        assert( currentObjectDepth > this->InheritanceDepth );
        
        //
        // find an underlying parent object's meta class
        //
        delta = currentObjectDepth - this->InheritanceDepth;
        assert( delta < 0xFFF );
        
        assert( parentMetaClass == objectMetaClass );
        while( 0x0 != delta ){
            
            parentMetaClass = parentMetaClass->getSuperClass();
            assert( parentMetaClass );
            
            --delta;
        }
        
        keyMetaClass = parentMetaClass;
        
        assert( objectMetaClass != parentMetaClass );
        
    } else {
        
        //
        // the leaf class function call this->Foo()
        //
        assert( hookedMetaClass == this->MetaClass );
        assert( objectMetaClass == parentMetaClass );
        
        keyMetaClass = objectMetaClass;
    }
    
    assert( keyMetaClass == parentMetaClass );
    
    DldHookTypeVtableObjKey  VtableHookObjKey;
    bzero( &VtableHookObjKey, sizeof( VtableHookObjKey ) );// see Comment 1:
    VtableHookObjKey.Object = ObjU.fObj;
    VtableHookObjKey.metaClass = keyMetaClass;
    VtableHookObjKey.InheritanceDepth = this->InheritanceDepth;
    
    DldHookedObjectEntry*    VtableHookEntry;
    
    DldHookedObjectsHashTable::sHashTable->LockShared();
    {// start of the lock
        
        //
        // there are a lot possibilities for an optimization, see assert()s
        //
        
        VtableHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey );
        assert( !( parentMetaClass != objectMetaClass && NULL != VtableHookEntry ) );
        if( NULL == VtableHookEntry ){
            
            //
            // the first check is for non null table entry
            //
            DldHookTypeVtableKey  VtableHookKey1;
            bzero( &VtableHookKey1, sizeof( VtableHookKey1 ) );// see Comment 1:
            VtableHookKey1.Vtable = *ObjU.vtablep;
            VtableHookKey1.metaClass = keyMetaClass;
            VtableHookKey1.InheritanceDepth = this->InheritanceDepth;
            
            
            VtableHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey1 );
            assert( !( parentMetaClass != objectMetaClass && NULL != VtableHookEntry ) );
            if( NULL == VtableHookEntry ){
                
                //
                // resort to a null vtable entry, this can happen only when the object table has been replaced
                // or this is a super::Foo() call
                //
                DldHookTypeVtableKey  VtableHookKey2;
                bzero( &VtableHookKey2, sizeof( VtableHookKey2 ) );// see Comment 1:
                VtableHookKey2.Vtable = NULL;
                VtableHookKey2.metaClass = keyMetaClass;
                VtableHookKey2.InheritanceDepth = this->InheritanceDepth;
                
                
                VtableHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey2 );
                
            }// end if( NULL == VtableHookEntry )
            
        }// end if( NULL == VtableHookEntry )
        
        //assert( VtableHookEntry );
        if( NULL == VtableHookEntry ){
            
            //
            // this is a special case for an object about which the driver was not aware
            // but the object's the vtable had been hooked directly
            // and unhooked after the hooking function had been called - the hooking
            // function waited on the lock when the vtable was being unhooked,
            // in that case we have an already unhooked object and can use a value
            // from the current object's vtable, the hooking object is never removed
            // so we can safely use "this" pointer,
            // N.B. this vtable access must be done under the lock to prevent
            // a race conditions on the same vtable from a concurrent hooking thread
            //
            unsigned int VtableIndx = this->HookedFunctonsInfo[ indx ].VtableIndex;
            assert( 0x0 != VtableIndx );
            OriginalFunction = (*ObjU.vtablep)[ VtableIndx - 0x1 ];
            
            assert( OriginalFunction != this->HookedFunctonsInfo[ indx ].HookingFunction && 
                    NULL == this->HookedFunctonsInfo[ indx ].OriginalFunction );
            
        }//end if( NULL == VtableHookEntry )
        
    }// end of the lock
    DldHookedObjectsHashTable::sHashTable->UnLockShared();
    
    if( NULL != VtableHookEntry ){
        
        assert( VtableHookEntry->InheritanceDepth == this->InheritanceDepth );
        assert( indx < VtableHookEntry->Parameters.TypeVtable.HookedVtableFunctionsInfoEntriesNumber );
        assert( VtableHookEntry->Parameters.Common.HookedVtableFunctionsInfo[ indx ].OriginalFunction !=
                VtableHookEntry->Parameters.Common.HookedVtableFunctionsInfo[ indx ].HookingFunction );
        
        OriginalFunction = VtableHookEntry->Parameters.Common.HookedVtableFunctionsInfo[ indx ].OriginalFunction;
        VtableHookEntry->release();
        
    }// end if( NULL != VtableHookObjEntry )
    
    
    assert( OriginalFunction );
    return OriginalFunction;
}

//--------------------------------------------------------------------

#if defined(DBG)
OSObject** gHookedObjects = NULL;
unsigned int gObjCount = 0x0;
unsigned int gObjCapacity = 0x0;
#endif//defined(DBG)

//
// a zero starting and terminating padding is used for safety if there will be access
// to a zeroed data at the end or start of the vtable as this data is accessible
// before hooking and must remain accessible after hooking, and for example on 64 bit
// the pointer to vtable in the object points inside the vtable skipping the header
// which is 16 bytes set to zero
//
#define DLD_VTABLE_ZERO_PADDING_SIZE  ( 32*sizeof( OSMetaClassBase::_ptf_t ) )
#define DLD_MAX_NUMBER_OF_ADDED_VIRTUAL_FUNCS  (1024)

IOReturn
DldHookerCommonClass::HookObjectIntWoLock(
    __inout OSObject* object
    )
{
    IOReturn RC = kIOReturnSuccess;
    
    assert( NULL != this->ClassHookerObject );
    assert( NULL != object );
    assert( preemption_enabled() );
    assert( DldHookedObjectsHashTable::sHashTable );
    //assert( !DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( object, false ) );
    
    //
    // check that the object has not been already hooked ( a rare case )
    //
    if( DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( object, false ) ){
        
        RC = kIOReturnSuccess;
        return RC;
    }
    
    DldSingleInheritingClassObjectPtr       ObjU;
    DldSingleInheritingHookerClassObjectPtr HookerObjU;
    
    ObjU.fObj = object;
    HookerObjU.fObj = this->ClassHookerObject;
    
    //
    // perform the delayed intialization
    //
    if( NULL == this->MetaClass ){
        
        this->MetaClass = object->getMetaClass();
    }
    
    assert( this->MetaClass == object->getMetaClass() );
    
    //
    // the this->NewVtable pointer might be zeroed several times during the hooking object
    // life, the zeroing is done when the last object has been unhooked, so do not assume
    // that the following code is called only once!
    //
    if( NULL == this->NewVtable ){
    
        assert( 0x0 == this->HookedVtableSize%sizeof( OSMetaClassBase::_ptf_t ) );
        
        //
        // define the actual vtable size if this is a class hook defined through
        // a parent of the hooked class usually this happens when the leaf class is
        // not exported for third party developers ( as in the case of AppleUSBEHCI )
        //
            
        unsigned int              virtualsAdded  = 0x0;// a counter of virtual functions added by children
        unsigned int              virtualsParent = this->HookedVtableSize/sizeof( OSMetaClassBase::_ptf_t );
        OSMetaClassBase::_ptf_t*  vtable = *ObjU.vtablep;
        
        //
        // as there can't be pure virtual functions the vtable ends with when the first
        // zero entry is found ( this entry is either a padding, some data or a start of the next vtable ),
        // so continue up to the first zero or invalid memory - any of this two conditions
        // guarantees that a full vtable will be covered by a found range as usually vtables ends with
        // NULL pointers and are packed one after another and vtable starts with the 0x0 64 bit value
        //
        while( DldVirtToPhys( (vm_offset_t)vtable+virtualsParent+virtualsAdded ) &&
               DldVirtToPhys( (vm_offset_t)vtable+virtualsParent+virtualsAdded + sizeof( OSMetaClassBase::_ptf_t ) - 0x1 ) &&
               NULL != vtable[ virtualsParent+virtualsAdded-0x1 ] &&
               virtualsAdded < DLD_MAX_NUMBER_OF_ADDED_VIRTUAL_FUNCS ){
            
            ++virtualsAdded;
            
        }// end while
        
        assert( virtualsAdded < DLD_MAX_NUMBER_OF_ADDED_VIRTUAL_FUNCS );
        
        if( DLD_MAX_NUMBER_OF_ADDED_VIRTUAL_FUNCS == virtualsAdded ){
            
            //
            // we were unable to reasonably determine the range covering the vtable
            //
            DBG_PRINT_ERROR(("Vtable range definition failed for '%s' with HookedVtableSize=%u and virtualsAdded=%u\n",
                             object->getMetaClass()->getClassName(), this->HookedVtableSize, virtualsAdded));
            
            return kIOReturnError;
        }
        
        this->VirtualsAddedSize = virtualsAdded*sizeof( OSMetaClassBase::_ptf_t );
        
        this->BufferSize = this->HookedVtableSize + this->VirtualsAddedSize + 2*DLD_VTABLE_ZERO_PADDING_SIZE;
        this->Buffer = IOMalloc( this->BufferSize );
        assert( this->Buffer );
        if( NULL == this->Buffer ){
            
            DBG_PRINT_ERROR(("IOMalloc() failed for '%s' with HookedVtableSize=%u and VirtualsAddedSize=%u\n",
                             object->getMetaClass()->getClassName(), this->HookedVtableSize, this->VirtualsAddedSize));
            
            return kIOReturnNoMemory;
        }
        
        bzero( (char*)this->Buffer , DLD_VTABLE_ZERO_PADDING_SIZE );
        bzero( (char*)this->Buffer + DLD_VTABLE_ZERO_PADDING_SIZE + this->HookedVtableSize + this->VirtualsAddedSize , DLD_VTABLE_ZERO_PADDING_SIZE );
        
        this->NewVtable = (OSMetaClassBase::_ptf_t*)( (vm_offset_t)this->Buffer + DLD_VTABLE_ZERO_PADDING_SIZE );
        this->HookClassVtable = *HookerObjU.vtablep;
        this->OriginalVtable = *ObjU.vtablep;
        
#if APPLE_KEXT_LEGACY_ABI
        
        //
        // copy the vtable header and the pointers table, HookedVtableSize + VirtualsAddedSize
        // takes into account the size of the header
        //
        memcpy( (void*)this->NewVtable,
                (void*)this->OriginalVtable,
                this->HookedVtableSize + this->VirtualsAddedSize );
        
#else/* !APPLE_KEXT_LEGACY_ABI */
        
        //
        // copy the vtable header which is 2*sizeof( OSMetaClassBase::_ptf_t )
        //
        assert( (2*sizeof( OSMetaClassBase::_ptf_t )) <= DLD_VTABLE_ZERO_PADDING_SIZE );
        memcpy( (char*)this->NewVtable - 2*sizeof( OSMetaClassBase::_ptf_t ),
                (char*)this->OriginalVtable - 2*sizeof( OSMetaClassBase::_ptf_t ),
                2*sizeof( OSMetaClassBase::_ptf_t ) );
        
        //
        // copy the pointers table
        //
        memcpy( (void*)this->NewVtable,
                (void*)this->OriginalVtable,
                this->HookedVtableSize + this->VirtualsAddedSize );
        
#endif/* !APPLE_KEXT_LEGACY_ABI */
        
        DldHookerCommonClass::DldHookVtableFunctions( object,
                                                      this->HookedFunctonsInfo,
                                                      this->OriginalVtable,
                                                      this->NewVtable );
        
    }// end if( NULL == this->HookClassVtable )
    
    
    if( this->NewVtable != *ObjU.vtablep ){
        
        DBG_PRINT( ( "%s->DldHookerCommonClass::HookObject( object=0x%p ) \n",
                     this->ClassHookerObject->fGetClassName(),
                     (void*)object ) );
        
        DldHookedObjectEntry*    HookEntry;
        HookEntry = DldHookedObjectEntry::allocateNew();
        assert( HookEntry );
        if( HookEntry ){
            
            HookEntry->Type = DldHookedObjectEntry::DldHookEntryTypeObject;
            HookEntry->ClassHookerObject = this->ClassHookerObject;
            HookEntry->InheritanceDepth = this->InheritanceDepth;
            HookEntry->Parameters.Common.HookedVtableFunctionsInfo = this->HookedFunctonsInfo;
            HookEntry->Parameters.TypeObject.OriginalVtable = *ObjU.vtablep;
            HookEntry->Key.Object = object;
            
            assert( DldInheritanceDepth_0 == HookEntry->InheritanceDepth );
            
            //
            // insert in the hash
            //
            if( DldHookedObjectsHashTable::sHashTable->AddObject( object, HookEntry ) ){
          
                //
                // hook the object, this is the place where the vtable is replaced
                //
                *ObjU.vtablep = this->NewVtable;
                HookEntry->release();
                
            } else {
                
                DBG_PRINT_ERROR( ( "%s->DldHookerCommonClass::HookObject( object=0x%p )->DldHookedObjectsHashTable::sHashTable->AddObject( 0x%p, 0x%p ) failed\n",
                                  this->ClassHookerObject->fGetClassName(),
                                  (void*)object,
                                  (void*)object, (void*)HookEntry ) );
                
                HookEntry->release();
                HookEntry = NULL;
            }
            
        }// end if( HookEntry )
        
        if( !HookEntry )
            RC = kIOReturnNoMemory;
        
        if( kIOReturnSuccess == RC )
            this->HookedObjectsCounter += 0x1;
        
    }// end if( this->HookClassVtable != *(void**)object )

    assert( kIOReturnSuccess == RC );
    
#if defined(DBG)
    /*
    if( kIOReturnSuccess == RC ){
        
        if( NULL == gHookedObjects ){
            
            gObjCount = 0x0;
            gObjCapacity = 1024;
            gHookedObjects = (OSObject**)IOMalloc( gObjCapacity*sizeof(OSObject*) );
            
            if( gHookedObjects )
                gHookedObjects[ 0 ] = NULL;
            else
                gObjCapacity = 0x0;
            
        }// end if( NULL == gHookedObjects )
        
        
        if( NULL != gHookedObjects && gObjCount < gObjCapacity ){
            
            int i;
            
            for( i = 0x0; i < gObjCount; ++i ){
                
                if( object <= gHookedObjects[ i ] )
                    break;
            }// end for
            
            if( object != gHookedObjects[ i ] ){
                
                if( i != gObjCount && gObjCount > 0x0 ){
                    
                    assert( i < gObjCount );
                    memmove( &gHookedObjects[ i+1 ], &gHookedObjects[ i ], ( gObjCount - i )*sizeof(OSObject*) );
                }
                
                gHookedObjects[ i ] = object;
                ++gObjCount;
                
            }// end if( object != gHookedObjects[ i ] )
            
        }// end if( NULL != gHookedObjects )
        
    }
     */
#endif//defined(DBG)
    
    return RC;
}

//--------------------------------------------------------------------

#if defined(DBG)
typedef struct _DldVtObjHook{
    OSObject*              object;
    DldHookTypeVtableKey   vtableKey;
} DldVtObjHook;

DldVtObjHook* gVtableHookedObjects = NULL;
unsigned int gVtCount = 0x0;
unsigned int gVtCapacity = 0x0;
#endif//defined(DBG)

/*
 FYI the most common call stack
 #0  DldHookerCommonClass::HookVtableIntWoLock (this=0xffffff8012486c10, object=0xffffff8011ad4a00) at DldHookerCommonClass.cpp:2671
 #1  0xffffff7f81f7ea27 in DldHookerCommonClass::HookObject (this=0xffffff8012486c10, object=0xffffff8011ad4a00, type=DldHookTypeVtable) at DldHookerCommonClass.cpp:3217
 #2  0xffffff7f82136bd3 in DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<(_DldInheritanceDepth)0>, IOSCSIPeripheralDeviceType05>::fHookObject (object=0xffffff8011ad4a00, type=DldHookTypeVtable) at DldHookerCommonClass2.h:1676
 #3  0xffffff7f81fb5732 in DldIOKitHookEngine::HookObject (this=0xffffff8012b5d600, object=0xffffff8011ad4a00) at DldIOKitHookEngine.cpp:582
 #4  0xffffff7f81f7934d in DldHookerCommonClass::attach (this=0xffffff8013dc8810, serviceObject=0xffffff8011ad4a00, provider=0xffffff8017d4d800) at DldHookerCommonClass.cpp:918
 #5  0xffffff7f81f795cf in DldHookerCommonClass::attachToChild (this=0xffffff8013dc8810, serviceObject=0xffffff8017d4d800, child=0xffffff8011ad4a00, plane=0xffffff8011854300) at DldHookerCommonClass.cpp:996
 #6  0xffffff7f81fdac66 in attachToChild_hook (this=0xffffff8017d4d800, child=0xffffff8011ad4a00, plane=0xffffff8011854300) at DldHookerCommonClass2.h:1239
 #7  0xffffff800062913e in IORegistryEntry::attachToParent (this=<value temporarily unavailable, due to optimizations>, parent=<value temporarily unavailable, due to optimizations>, plane=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-2050.22.13/iokit/Kernel/IORegistryEntry.cpp:1675
 #8  0xffffff800062b898 in IOService::attach (this=0xffffff8011ad4a00, provider=0xffffff8017d4d800) at /SourceCache/xnu/xnu-2050.22.13/iokit/Kernel/IOService.cpp:460
 #9  0xffffff80006301d5 in IOService::probeCandidates (this=0xffffff8017d4d800, matches=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-2050.22.13/iokit/Kernel/IOService.cpp:2761
 #10 0xffffff800062c390 in IOService::doServiceMatch (this=0xffffff8017d4d800, options=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-2050.22.13/iokit/Kernel/IOService.cpp:3143
 #11 0xffffff8000630f59 in _IOConfigThread::main (arg=0xffffff80160a97e0, result=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-2050.22.13/iokit/Kernel/IOService.cpp:3407 
 */
IOReturn
DldHookerCommonClass::HookVtableIntWoLock(
    __inout OSObject* object
    )
{
    IOReturn RC = kIOReturnSuccess;
    
    assert( NULL != this->ClassHookerObject );
    assert( NULL != object );
    assert( preemption_enabled() );
    assert( DldHookedObjectsHashTable::sHashTable );
    assert( this->HookedFunctonsInfoEntriesNumber );
    
    DldSingleInheritingClassObjectPtr ObjU;
    const OSMetaClass*  keyMetaClass;
    
    ObjU.fObj = object;
    keyMetaClass = object->getMetaClass();
    
    //
    // perform the delayed intialization or reinitialization in case the system
    // deleted meta class object after deleting the last object of the type
    //
    if( NULL == this->MetaClass || 0x0 == this->HookedObjectsCounter ){
        
        unsigned int i;
        const OSMetaClass*  hookedMetaClass;
        
        hookedMetaClass = object->getMetaClass();
        assert( hookedMetaClass );
        for( i = 0x0; i < this->InheritanceDepth && hookedMetaClass; ++i ){
            
            hookedMetaClass = hookedMetaClass->getSuperClass();
            assert( hookedMetaClass );
            
        }// end for
        
        assert( this->InheritanceDepth == i && hookedMetaClass );
        
        //
        // save the meta class pointer ( not retained ) and the class name pointer
        //
        
        this->MetaClass = hookedMetaClass;
        
        if( NULL != this->MetaClass && NULL == this->ClassName ){
            
            //
            // 10.6 and 10.7 lacks getClassNameSymbol(),  it happened 10.8 does not export this symbol though it exists in the kernel
            //
/*#if (defined(MAC_OS_X_VERSION_10_8) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
            this->ClassName = this->MetaClass->getClassNameSymbol();
            assert( this->ClassName );
            if( this->ClassName )
                this->ClassName->retain();
#else*/
            this->ClassName = OSSymbol::withCString( this->MetaClass->getClassName() ? this->MetaClass->getClassName() : "Unknown" );
//#endif
        }
    }
    
#if defined(DBG)
    /*
    int hookedObjVtIndx;
    int hookedObjObjIndx;
    int currentVtableObjIndx = (-1);
    
    {
        unsigned int i;
        const OSMetaClass*  hookedMetaClass;
        
        hookedMetaClass = object->getMetaClass();
        assert( hookedMetaClass );
        for( i = 0x0; i < this->InheritanceDepth && hookedMetaClass; ++i ){
            
            hookedMetaClass = hookedMetaClass->getSuperClass();
            assert( hookedMetaClass );
            
        }// end for
        
        assert( this->InheritanceDepth == i && hookedMetaClass );
        assert( this->MetaClass == hookedMetaClass );
    }
    
    for( hookedObjVtIndx = 0x0; hookedObjVtIndx < gVtCount; ++hookedObjVtIndx ){
        
        if( gVtableHookedObjects[ hookedObjVtIndx ].vtableKey.Vtable == *(OSMetaClassBase::_ptf_t**)object )
            currentVtableObjIndx = hookedObjVtIndx;
        
        if( object == gVtableHookedObjects[ hookedObjVtIndx ].object )
            break;
    }// end for
    
    for( hookedObjObjIndx = 0x0; hookedObjObjIndx < gObjCount; ++hookedObjObjIndx ){
        
        if( object == gHookedObjects[ hookedObjObjIndx ] )
            break;
    }// end for
    */
#endif//defined(DBG)
    
    //
    // check that the vtable has not been already hooked
    //
    
    //
    // for the Vtable there are two keys as the object Vtable might be replaced, so
    // there is a second key with Vtable set to NULL
    //
    
    //
    // Comment 1:
    // You will never guess why you MUST zero these stack variables despite initilizing
    // every field directly, the correct answer is - alignment and padding - the compiler
    // alignes data to natural offsets thus adding holes containing stack garbage and
    // then the whole object is used as a plane data to represent a key for the hash table
    // thus the garbage acquires some meaning and can't be neglected as is done when
    // each field accessed individually and the compiler extractes the required data removing
    // the garbage
    //
    
    DldHookTypeVtableKey  VtableHookKey1;
    bzero( &VtableHookKey1, sizeof( VtableHookKey1 ) );// see Comment 1:
    VtableHookKey1.Vtable = *ObjU.vtablep;
    VtableHookKey1.metaClass = keyMetaClass;
    VtableHookKey1.InheritanceDepth = this->InheritanceDepth;
    
    
    DldHookTypeVtableKey  VtableHookKey2;
    bzero( &VtableHookKey2, sizeof( VtableHookKey2 ) );// see Comment 1:
    VtableHookKey2.Vtable = NULL;
    VtableHookKey2.metaClass = keyMetaClass;
    VtableHookKey2.InheritanceDepth = this->InheritanceDepth;
    
    
    DldHookTypeVtableObjKey  VtableHookObjKey;
    bzero( &VtableHookObjKey, sizeof( VtableHookObjKey ) );// see Comment 1:
    VtableHookObjKey.Object = ObjU.fObj;
    VtableHookObjKey.metaClass = keyMetaClass;
    VtableHookObjKey.InheritanceDepth = this->InheritanceDepth;
        
    //
    // check that the object has not been already hooked( a rare case )
    //
    //assert( !DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey, false ) );
    
    if( DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey, false ) ){
        
        RC = kIOReturnSuccess;
        return RC;
    }
    
    //
    // search the entry by the first key, if an entry was not found then either the default
    // vtabe has been replaced and a new entry is needed for a new vtable or the vtable has
    // not been hooked yet
    //
    DldHookedObjectEntry*    VtableHookEntry;
    if( NULL != ( VtableHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey1 ) ) ){
        
        assert( DldHookedObjectEntry::DldHookEntryTypeVtable == VtableHookEntry->Type );
        assert( kIOReturnSuccess == RC );
        assert( 0x0 != VtableHookEntry->Parameters.TypeVtable.ReferenceCount );
        
        //
        // the vtable has been hooked, add an entry for the object
        //
        DldHookedObjectEntry*   newObjectEntry;
        newObjectEntry = DldHookedObjectEntry::allocateNew();
        assert( newObjectEntry );
        if( newObjectEntry ){
            
            bool InHash = false;
            
            newObjectEntry->Type = DldHookedObjectEntry::DldHookEntryTypeVtableObj;
            newObjectEntry->ClassHookerObject = this->ClassHookerObject;
            newObjectEntry->InheritanceDepth = this->InheritanceDepth;
            newObjectEntry->Parameters.Common.HookedVtableFunctionsInfo = VtableHookEntry->Parameters.Common.HookedVtableFunctionsInfo;
            newObjectEntry->Parameters.TypeVatbleObj.OriginalVtable = *ObjU.vtablep;
            newObjectEntry->Parameters.TypeVatbleObj.VtableEntry = VtableHookEntry;
            newObjectEntry->Parameters.TypeVatbleObj.VtableEntry->retain();
            newObjectEntry->Key.VtableHookObj = VtableHookObjKey;
            
            VtableHookEntry->Parameters.TypeVtable.ReferenceCount += 0x1;
            
            //
            // be cautious! In case of vtable hooks the HookedObjectsCounter only counts
            // the number of objects we observed, there might be object that did not get
            // under our radar with the same vtable, in case of vtable hooks the
            // HookedObjectsCounter value is used as a heuristic to update the MetaClass
            // pointer
            //
            this->HookedObjectsCounter += 0x1;
            
            InHash = DldHookedObjectsHashTable::sHashTable->AddObject( &VtableHookObjKey, newObjectEntry );
            assert( InHash );
            if( !InHash ){
                
                //
                // decrement the reference count and set pointer to NULL
                // to avoid a normal case processing in free() which
                // assumes that the entries have been inserted successfully
                //
                
                newObjectEntry->Parameters.TypeVatbleObj.VtableEntry->release();
                newObjectEntry->Parameters.TypeVatbleObj.VtableEntry = NULL;
                
                VtableHookEntry->Parameters.TypeVtable.ReferenceCount -= 0x1;
                this->HookedObjectsCounter -= 0x1;
                assert( 0x0 != VtableHookEntry->Parameters.TypeVtable.ReferenceCount );
                
                RC = kIOReturnNoMemory;
                
            } else {
                
                assert( newObjectEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey, false ) );
            }
            
            //
            // release in any case, if the entry is in the hash it has been retained by the AddObject
            // else it must be removed
            //
            newObjectEntry->release();
            
        } else {
            
            RC = kIOReturnNoMemory;
        }
        
        //
        // was referenced by RetrieveObjectEntry
        //
        VtableHookEntry->release();
        
        assert( kIOReturnSuccess == RC );
        return RC;
        
    }// end if( VtableHookEntry = ...
    
    
    assert( NULL == VtableHookEntry );
    assert( kIOReturnSuccess == RC );
    
    
    //
    // the vtable must be hooked
    //
    
    //
    // check for an erroneous double hook, the reason might be a destroyed of the hash table consistency
    //
    if( this->HookedFunctonsInfoEntriesNumber >0x0 &&
        (-1) != this->HookedFunctonsInfo[0].VtableIndex &&
        this->HookedFunctonsInfo[0].HookingFunction == (*ObjU.vtablep)[ this->HookedFunctonsInfo[0].VtableIndex - 1] ){
        
#if defined( DBG )
        //
        // a double hook, an entry for VtableHookKey1 should have been found!
        // call a debugger and trace the RetrieveObjectEntry function
        //
        __asm__ volatile( "int $0x3" );
        VtableHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey1, false );
        VtableHookEntry = NULL;
#endif//DBG
        
        DBG_PRINT_ERROR(("A double hook for 0x%p(%s), an extremely serious error!\n", object, object->getMetaClass()->getClassName()));
        
        //
        // exit and pray that the system will carry on
        //
        return kIOReturnError;
    }
    
    //
    // create an object and table entries
    //
    DldHookedObjectEntry*   newObjectEntry;
    DldHookedObjectEntry*   newVtableEntry;
    DldHookedFunctionInfo*  HookedFunctonsInfo;
    
    //
    // allocate a private function info for the hooked vtable, the common
    // hook class can be used for hooking more than one class so it's an
    // error to assume that the class' HookedFunctonsInfo array can be used
    // to save the hooked function info such as original address
    //
    HookedFunctonsInfo = (DldHookedFunctionInfo*)IOMalloc( this->HookedFunctonsInfoEntriesNumber*sizeof( HookedFunctonsInfo[ 0 ] ) );
    assert( HookedFunctonsInfo );
    if( !HookedFunctonsInfo ){
        
        RC = kIOReturnNoMemory;
        return RC;
    }
    
    //
    // copy a function info from the common class
    //
    memcpy( HookedFunctonsInfo,
            this->HookedFunctonsInfo,
            this->HookedFunctonsInfoEntriesNumber*sizeof( HookedFunctonsInfo[ 0 ] ) );
    
    //
    // allocate new entries
    //
    newObjectEntry = DldHookedObjectEntry::allocateNew();
    newVtableEntry = DldHookedObjectEntry::allocateNew();
    assert( newObjectEntry && newVtableEntry );
    if( !( newObjectEntry && newVtableEntry ) ){
        
        if( newObjectEntry )
            newObjectEntry->release();
        
        if( newVtableEntry )
            newVtableEntry->release();
        
        IOFree( HookedFunctonsInfo, this->HookedFunctonsInfoEntriesNumber*sizeof( HookedFunctonsInfo[ 0 ] ) );
        
        RC = kIOReturnNoMemory;
        return RC;
        
    }// end if
    
    //
    // initialize the new hash entries
    //
    
    newVtableEntry->Type = DldHookedObjectEntry::DldHookEntryTypeVtable;
    newVtableEntry->ClassHookerObject = this->ClassHookerObject;
    newVtableEntry->InheritanceDepth = this->InheritanceDepth;
    newVtableEntry->Parameters.Common.HookedVtableFunctionsInfo = HookedFunctonsInfo;
    newVtableEntry->Parameters.TypeVtable.HookedVtableFunctionsInfoEntriesNumber = this->HookedFunctonsInfoEntriesNumber;
    newVtableEntry->Parameters.TypeVtable.ReferenceCount = 0x1;
    newVtableEntry->Key.VtableHookVtable = VtableHookKey1;
    
    
    newObjectEntry->Type = DldHookedObjectEntry::DldHookEntryTypeVtableObj;
    newObjectEntry->ClassHookerObject = this->ClassHookerObject;
    newObjectEntry->InheritanceDepth = this->InheritanceDepth;
    newObjectEntry->Parameters.Common.HookedVtableFunctionsInfo = HookedFunctonsInfo;
    newObjectEntry->Parameters.TypeVatbleObj.OriginalVtable = *ObjU.vtablep;
    newObjectEntry->Parameters.TypeVatbleObj.VtableEntry = newVtableEntry;
    newObjectEntry->Parameters.TypeVatbleObj.VtableEntry->retain();
    newObjectEntry->Key.VtableHookObj = VtableHookObjKey;
    
    //
    // add the new entries in the hash, the first is for the vtable, then the objets's one
    //
    
    bool InHash = false;
    DldHookedObjectEntry* ExistingSecondKeyEntry;
    
    //
    // the entry associated with the first key must not be in the hash, the entry associated
    // with the second key might be in the has if the object vtable has been changed
    //
    ExistingSecondKeyEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey2 );
    if( ExistingSecondKeyEntry ){
     
        assert( 0x0 != ExistingSecondKeyEntry->Parameters.TypeVtable.ReferenceCount );
        ExistingSecondKeyEntry->Parameters.TypeVtable.ReferenceCount += 0x1;
        InHash = true;
        
    } else {
        
        //
        // add a new null vtable entry
        //
        InHash = DldHookedObjectsHashTable::sHashTable->AddObject( &VtableHookKey2, newVtableEntry );
        assert( newVtableEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey2, false ) );
    }
    
    assert( InHash );
    
    //
    // add a new non null vtable entry
    //
    if( InHash ){
        
        InHash = DldHookedObjectsHashTable::sHashTable->AddObject( &VtableHookKey1, newVtableEntry );
        assert( newVtableEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey1, false ) );
        
        if( InHash ){
            //
            // be cautious! In case of vtable hooks the HookedObjectsCounter only counts
            // the number of objects we observed, there might be object that did not get
            // under our radar with the same vtable, in case of vtable hooks the
            // HookedObjectsCounter value is used as a heuristic to update the MetaClass
            // pointer
            //
            this->HookedObjectsCounter += 0x1;
        }
    }
    
    assert( InHash );
    
    //
    // add the object entry
    //
    if( InHash ){
        
        InHash = DldHookedObjectsHashTable::sHashTable->AddObject( &VtableHookObjKey, newObjectEntry );
        assert( newObjectEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey, false ) );
    }
    
    assert( InHash );
    
    if( InHash ){
        
        DldHookerCommonClass::DldHookVtableFunctions( object,
                                                      HookedFunctonsInfo,
                                                      *ObjU.vtablep,
                                                      *ObjU.vtablep );
        
    }// end if( InHash )
    

    if( !InHash ){
        
        //
        // something went wrong, undo
        //
        
        DldHookedObjectEntry*  removedVtableEntry;
        
        RC = kIOReturnError;
        
        //
        // decrement the reference count and set the pointer to NULL
        // to avoid a normal case processing in free() which
        // assumes that the entries have been inserted successfully
        //
        
        newVtableEntry->Parameters.TypeVtable.ReferenceCount = 0x0;
        
        newObjectEntry->Parameters.TypeVatbleObj.VtableEntry->release();
        newObjectEntry->Parameters.TypeVatbleObj.VtableEntry = NULL;
        
        //
        // remove the vtable entries for non null and null vtable entries( if required )
        //
        removedVtableEntry = DldHookedObjectsHashTable::sHashTable->RemoveObject( &VtableHookKey1 );
        if( removedVtableEntry ){
            
            assert( newVtableEntry == removedVtableEntry );
            removedVtableEntry->release();
            this->HookedObjectsCounter -= 0x1;
            
        }// end if( removedVtableEntry )
        
        
        if( !ExistingSecondKeyEntry ){
            
            removedVtableEntry = DldHookedObjectsHashTable::sHashTable->RemoveObject( &VtableHookKey2 );
            if( removedVtableEntry ){
                
                assert( newVtableEntry == removedVtableEntry );
                removedVtableEntry->release();
                
            }// end if( removedVtableEntry )
            
        } else {
            
            //
            // the vtable entry referencing the null vtable entry has been removed from the hash
            //
            ExistingSecondKeyEntry->Parameters.TypeVtable.ReferenceCount -= 0x1;
            assert( 0x0 != ExistingSecondKeyEntry->Parameters.TypeVtable.ReferenceCount );
        }

        
    } else {
        
        assert( InHash );
        assert( newVtableEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey2, false ) ||
                ExistingSecondKeyEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey2, false ) );
        assert( newVtableEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey1, false ) );
        assert( newObjectEntry == DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey, false ) );
        
    }// end else for if( InHash )
    
    
    if( ExistingSecondKeyEntry )
        ExistingSecondKeyEntry->release();
    
    
    newObjectEntry->release();
    newVtableEntry->release();
    
    assert( kIOReturnSuccess == RC );
    
#if defined(DBG)
    /*
    if( kIOReturnSuccess == RC ){
        
        if( NULL == gVtableHookedObjects ){
            
            gVtCount = 0x0;
            gVtCapacity = 1024;
            gVtableHookedObjects = (DldVtObjHook*)IOMalloc( gVtCapacity*sizeof(gVtableHookedObjects[0]) );
            
            if( gVtableHookedObjects )
                gVtableHookedObjects[ 0 ].object = NULL;
            else
                gVtCapacity = 0x0;

        }// end if( NULL == gVtableHookedObjects )
        
        
        if( NULL != gVtableHookedObjects && gVtCount < gVtCapacity ){
            
            int i;
            
            for( i = 0x0; i < gVtCount; ++i ){
                
                if( object <= gVtableHookedObjects[ i ].object )
                    break;
            }// end for
            
            if( object != gVtableHookedObjects[ i ].object ){
                
                if( i != gVtCount && gVtCount > 0x0 ){
                    
                    assert( i < gVtCount );
                    memmove( &gVtableHookedObjects[ i+1 ], &gVtableHookedObjects[ i ], ( gVtCount - i )*sizeof(gVtableHookedObjects[0]) );
                }
                
                gVtableHookedObjects[ i ].object = object;
                gVtableHookedObjects[i].vtableKey = VtableHookKey1;
                ++gVtCount;
                
            }// end if( object != gVtableHookedObjects[ i ] )
            
        }// end if( NULL != gVtableHookedObjects )
        
    }
     */
#endif//defined(DBG)
    
    return RC;
}
    

//--------------------------------------------------------------------

IOReturn
DldHookerCommonClass::HookObject(
    __inout OSObject* object,
    __in DldHookType type
    )
{
    IOReturn  RC = kIOReturnSuccess;
    
    assert( ( DldHookTypeUnknown < type ) && ( type < DldHookTypeMaximum ) );
    
    if( DldHookTypeUnknown == this->HookType ){
        
        this->HookType = type;
    }
    
    //
    // the class can't wear multiple hats
    //
    assert( type == this->HookType );
    
    DldHookedObjectsHashTable::sHashTable->LockExclusive();
    {// start of the lock
        
        switch( type ){
                
            case DldHookTypeObject:
                RC = this->HookObjectIntWoLock( object );
                assert( kIOReturnSuccess == RC );
                break;
            case DldHookTypeVtable:
                RC = this->HookVtableIntWoLock( object );
                assert( kIOReturnSuccess == RC );
                break;
            default:
                panic( "HookObject( 0x%p, 0x%X ) for an unimplemented type", (void*)object, (int)type );
                break;
    }
        
    }// end of the lock
    DldHookedObjectsHashTable::sHashTable->UnLockExclusive();
    
    return RC;
}

//--------------------------------------------------------------------

IOReturn
DldHookerCommonClass::UnHookObjectIntWoLock(
    __inout OSObject* object
    )
{
    DldHookType          type  = this->HookType;
    DldInheritanceDepth  Depth = this->InheritanceDepth;
    
    assert( DldHookedObjectsHashTable::sHashTable );
    assert( ( DldHookTypeUnknown < type ) && ( type < DldHookTypeMaximum ) );
    
    DBG_PRINT( ( "DldHookerCommonClass::UnHookObject( object=0x%p ( %s ) ) \n",
                 (void*)object,
                 object->getMetaClass()->getClassName() ) );

    DldSingleInheritingClassObjectPtr ObjU;
    
    ObjU.fObj = object;
    
    //
    // unhook all possible hooks for the object, it is assumed
    // that we never hook our private vtable for an object, but
    // we can hook an original vtable before hooking an object
    // by replacing its vtable
    //
            
    //
    // the vtable replacement hook must be unhooked first before the direct vtable hook as
    // its hooking object might refer to a hooking functions of the direct vtable hooking object
    //
    DldHookedObjectEntry* ObjHookEntry = DldHookedObjectsHashTable::sHashTable->RemoveObject( object );
    assert( !( DldHookTypeObject == type && NULL == ObjHookEntry ) );
    assert( !( DldHookTypeVtable == type && NULL != ObjHookEntry ) );// must be unhooked and remobed by a per-object hook
    if( ObjHookEntry ){
        
        //
        // one of the reason for having a zero counter ( beside the reference count management errors )
        // is when an object hook didn't hook the free() routine and the control flow
        // has been redirected to the vtable hook which discovered that there is a residual 
        // per-object hook which should have been removed by a per-object free() hook
        //
        assert( this->HookedObjectsCounter > 0x0 );
        
        //
        // return the original vtable to the object
        //
        assert( ObjHookEntry->Parameters.TypeObject.OriginalVtable );
        *ObjU.vtablep = ObjHookEntry->Parameters.TypeObject.OriginalVtable;
        
        this->HookedObjectsCounter -= 0x1;
        if( 0x0 == this->HookedObjectsCounter ){
            
            //
            // the last object has been unhooked ( this must be a done in a free function hook ),
            // so the object's class can be removed with a kext which contains it
            // and then reloaded by a new address and even a new version for kext,
            // the vtable's functions should not be called anymore by a caller who calls
            // free() as free() is a terminating function, inside free() the vtable
            // functions still can be called but in that case the vtable pointer will
            // be retrieved from the object as there is no a saved one as in the caller's
            // case where the caller can save the vtable address in a registry or on a stack
            //
            
            IOFree( this->Buffer, this->BufferSize );
            this->Buffer         = NULL;
            this->BufferSize     = 0x0;
            
            this->NewVtable      = NULL;
            
            this->MetaClass      = NULL;
            this->OriginalVtable = NULL;
            
            DldHookedFunctionInfo* PtrHookInfo = this->HookedFunctonsInfo;
            
            while( (unsigned int)(-1) != PtrHookInfo->VtableIndex ){
                
                assert( 0x0  != PtrHookInfo->VtableIndex );
                assert( NULL != PtrHookInfo->HookingFunction );
                assert( NULL != PtrHookInfo->OriginalFunction );
                
                PtrHookInfo->OriginalFunction = NULL;
                ++PtrHookInfo;
                
            }// end while
            
        }
        
        ObjHookEntry->release();
        DLD_DBG_MAKE_POINTER_INVALID( ObjHookEntry );
        
    }// end if( ObjHookEntry )
    
    if( DldHookTypeObject == type ){
        
        //
        // if this is a hook by a vtable replacement for an object then
        // do not unhook the direct vtable hooking as its functions will
        // be called and will require the hash entries presence in the
        // hash table ( e.g. this unhook called from the free() routine
        // calls the free_hook() for the direct vtable hook which will try
        // to find an original function using the hash table's entries )
        //
        
        return kIOReturnSuccess;
    }
    
    //
    // unhook the vtable hook ( actually the vtable is unhooked when this function
    // is called for the last object we are aware about, though there might be objects
    // we are not aware about - I don't care about them, as if the object is not in the
    // hash table no any meaningfull operation was done with it with reagrd to the data
    // processed by this object )
    //
    
    //
    // for the Vtable there are two keys as the object Vtable might be replaced, so
    // there is a second key with Vtable set to NULL
    //
    
    DldHookedObjectEntry*  NonNullVtableHookEntry = NULL;
    DldHookedObjectEntry*  NullVtableHookEntry = NULL;
    
    
    DldHookTypeVtableObjKey  VtableHookObjKey;
    bzero( &VtableHookObjKey, sizeof( VtableHookObjKey ) );// see Comment 1:
    VtableHookObjKey.Object = ObjU.fObj;
    VtableHookObjKey.metaClass = ObjU.fObj->getMetaClass();
    VtableHookObjKey.InheritanceDepth = Depth;

    
    ObjHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookObjKey );
    if( ObjHookEntry ){
        
        NonNullVtableHookEntry = ObjHookEntry->Parameters.TypeVatbleObj.VtableEntry;
        assert( NonNullVtableHookEntry );
        
        NonNullVtableHookEntry->retain();
        
    }
    
    
    if( NonNullVtableHookEntry ){
        
        //
        // get a null vtable hook entry
        //
        
        DldHookTypeVtableKey  VtableHookKey2;
        bzero( &VtableHookKey2, sizeof( VtableHookKey2 ) );// see Comment 1:
        VtableHookKey2.Vtable = NULL;
        VtableHookKey2.metaClass = NonNullVtableHookEntry->Key.VtableHookVtable.metaClass;
        VtableHookKey2.InheritanceDepth = Depth;
        
        NullVtableHookEntry = DldHookedObjectsHashTable::sHashTable->RetrieveObjectEntry( &VtableHookKey2 );
        assert( NullVtableHookEntry );

    }
    
    //
    // at this point all non-null hook table entries have been referenced and must be dereferenced later
    //
    
    if( NonNullVtableHookEntry ){
        
        //
        // unhook the vtable if the last hooked object will be removed
        //
        
        assert( NonNullVtableHookEntry->Parameters.Common.HookedVtableFunctionsInfo );
        assert( NonNullVtableHookEntry->Key.VtableHookVtable.Vtable );
        assert( this->HookedObjectsCounter > 0x0 );
        
        this->HookedObjectsCounter -= 0x1;
        NonNullVtableHookEntry->Parameters.TypeVtable.ReferenceCount -= 0x1;
        if( 0x0 == NonNullVtableHookEntry->Parameters.TypeVtable.ReferenceCount ){
            
            assert( ((SInt32)this->HookedObjectsCounter) >= 0 );
            
            //
            // there is no object hooks refrencing this entry,
            // unhook the vtable
            //
            DldHookerCommonClass::DldUnHookVtableFunctions( NonNullVtableHookEntry->Parameters.Common.HookedVtableFunctionsInfo,
                                                            NonNullVtableHookEntry->Key.VtableHookVtable.Vtable );
            
        }
        
    }
    
    
    if( ObjHookEntry ){
        
        //
        // remove the object entry, this is done after unhooking though this should
        // not matter
        //
        
        DldHookedObjectEntry* RemovedObjHookEntry;
        
        RemovedObjHookEntry = DldHookedObjectsHashTable::sHashTable->RemoveObject( &ObjHookEntry->Key.VtableHookObj );
        assert( RemovedObjHookEntry == ObjHookEntry );
        
        if( RemovedObjHookEntry )
            RemovedObjHookEntry->release();
    }
    
    
    if( NonNullVtableHookEntry && 0x0 == NonNullVtableHookEntry->Parameters.TypeVtable.ReferenceCount ){
        
        //
        // remove the non null hook entry as there is no any referencing entry
        //
        
        DldHookedObjectEntry*    RemovedNonNullVtableEntry;
        RemovedNonNullVtableEntry = DldHookedObjectsHashTable::sHashTable->RemoveObject( &NonNullVtableHookEntry->Key.VtableHookVtable );
        assert( RemovedNonNullVtableEntry == NonNullVtableHookEntry );
        
        if( RemovedNonNullVtableEntry ){
            
            //
            // decrement the refrence counter for null vtable hook entry if this entry
            // is not the same as non null entry
            //
            if( NullVtableHookEntry != NonNullVtableHookEntry ){
                
                assert( 0x0 != NullVtableHookEntry->Parameters.TypeVtable.ReferenceCount );
                
                NullVtableHookEntry->Parameters.TypeVtable.ReferenceCount -= 0x1;
                
            }
            
            RemovedNonNullVtableEntry->release();
            
        }// end if( RemovedNonNullVtableEntry )
        
    }
    
    
    if( NullVtableHookEntry && 0x0 == NullVtableHookEntry->Parameters.TypeVtable.ReferenceCount ){
        
        //
        // the last non null vtable has been removed ( it might be the same as NullVtableHookEntry )
        // so the null hook entry must be also removed
        //
        
        DldHookedObjectEntry*    RemovedNullVtableHookEntry;
        
        DldHookTypeVtableKey  VtableHookKey2;
        bzero( &VtableHookKey2, sizeof( VtableHookKey2 ) );// see Comment 1:
        VtableHookKey2.Vtable = NULL;
        VtableHookKey2.metaClass = NullVtableHookEntry->Key.VtableHookObj.metaClass;
        VtableHookKey2.InheritanceDepth = Depth;
        
        RemovedNullVtableHookEntry = DldHookedObjectsHashTable::sHashTable->RemoveObject( &VtableHookKey2 );
        assert( RemovedNullVtableHookEntry == NullVtableHookEntry );
        
        if( RemovedNullVtableHookEntry )
            RemovedNullVtableHookEntry->release();
        
    }
    
    
    //
    // release all retained entries
    //
    if( ObjHookEntry )
        ObjHookEntry->release();
    
    if( NonNullVtableHookEntry )
        NonNullVtableHookEntry->release();
    
    if( NullVtableHookEntry )
        NullVtableHookEntry->release();

    
    return kIOReturnSuccess;
}


//--------------------------------------------------------------------

IOReturn
DldHookerCommonClass::UnHookObject(
    __inout OSObject* object
    )
{
    
    IOReturn RC;
    
    DldHookedObjectsHashTable::sHashTable->LockExclusive();
    {// start of the lock
        
        RC = this->UnHookObjectIntWoLock( object );
        
    }//end of the lock
    DldHookedObjectsHashTable::sHashTable->UnLockExclusive();
    
    return RC;
}

//--------------------------------------------------------------------
