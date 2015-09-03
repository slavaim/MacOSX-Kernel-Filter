/*
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIORegistryEntry.h"

//--------------------------------------------------------------------

#define super IORegistryEntry

OSDefineMetaClassAndStructors( DldIORegistryEntry, IORegistryEntry )

//--------------------------------------------------------------------

DldRegistryEntriesHashTable* DldRegistryEntriesHashTable::sHashTable = NULL;

//--------------------------------------------------------------------

DldRegistryEntriesHashTable*
DldRegistryEntriesHashTable::withSize( int size, bool non_block )
{
    DldRegistryEntriesHashTable* objHashTable;
    
    assert( preemption_enabled() );
    
    objHashTable = new DldRegistryEntriesHashTable();
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
DldRegistryEntriesHashTable::CreateStaticTableWithSize( int size, bool non_block )
{
    assert( !DldRegistryEntriesHashTable::sHashTable );
    
    DldRegistryEntriesHashTable::sHashTable = DldRegistryEntriesHashTable::withSize( size, non_block );
    assert( DldRegistryEntriesHashTable::sHashTable );

    gDldDbgData.sRegistryEntriesHashTable = (void*)DldRegistryEntriesHashTable::sHashTable;
    
    return ( NULL != DldRegistryEntriesHashTable::sHashTable );
}

void
DldRegistryEntriesHashTable::DeleteStaticTable()
{
    if( !DldRegistryEntriesHashTable::sHashTable ){
        
        DldRegistryEntriesHashTable::sHashTable->free();
        delete DldRegistryEntriesHashTable::sHashTable;
    }// end if
}

//--------------------------------------------------------------------

void
DldRegistryEntriesHashTable::free()
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
        DBG_PRINT_ERROR( ("DldRegistryEntriesHashTable::free() found an entry for an object(0x%p)\n", *(void**)p_key ) );
        
        DldIORegistryEntry* objEntry = (DldIORegistryEntry*)p_e->p_data;
        assert( objEntry );
        objEntry->release();
        
        p_table->fn_free( p_e, p_e->size );
    }
    
    ght_finalize( p_table );
    
    IORWLockFree( this->RWLock );
    this->RWLock = NULL;
}

//--------------------------------------------------------------------

bool
DldRegistryEntriesHashTable::AddObject(
    __in OSObject* obj,
    __in DldIORegistryEntry* objEntry
    )
/*
 the caller must allocate space for the entry and
 free it only after removing the entry from the hash,
 the objEntry is referenced, so a caller can release it
 */
{
    GHT_STATUS_CODE RC;
    
    RC = ght_insert( this->HashTable, objEntry, sizeof( obj ), &obj );
    assert( GHT_OK == RC );
    if( GHT_OK != RC ){
        
        DBG_PRINT_ERROR( ( "DldRegistryEntriesHashTable::AddObject->ght_insert( 0x%p, 0x%p ) failed RC = 0x%X\n",
                          (void*)this->HashTable, (void*)objEntry, RC ) );
    } else {
        
        objEntry->retain();
    }
    
    return ( GHT_OK == RC );
}

//--------------------------------------------------------------------

DldIORegistryEntry*
DldRegistryEntriesHashTable::RemoveObject(
    __in OSObject* obj
    )
/*
 the returned object is referenced!
 */
{
    DldIORegistryEntry* objEntry;
    
    //
    // the entry was refernced when was added to the hash table
    //
    objEntry = (DldIORegistryEntry*)ght_remove( this->HashTable, sizeof( obj ), &obj );    
    return objEntry;
}

//--------------------------------------------------------------------

DldIORegistryEntry*
DldRegistryEntriesHashTable::RetrieveObjectEntry(
    __in OSObject* obj,
    __in bool reference
    )
/*
 the returned object is referenced if the refernce's value is "true"
 */
{
    DldIORegistryEntry* objEntry;
        
    objEntry = (DldIORegistryEntry*)ght_get( this->HashTable, sizeof( obj ), &obj );
    
    if( objEntry && reference )
        objEntry->retain();
    
    return objEntry;
}

//--------------------------------------------------------------------

