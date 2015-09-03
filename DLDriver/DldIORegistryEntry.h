/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOREGISTRYENTRY_H
#define _DLDIOREGISTRYENTRY_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldCommonHashTable.h"

extern IORegistryPlane* gDldDeviceTreePlan;

//--------------------------------------------------------------------

class DldIORegistryEntry : public IORegistryEntry
{    
    OSDeclareDefaultStructors( DldIORegistryEntry )
    
public:
    
};

extern DldIORegistryEntry*   gDldRootEntry;

//--------------------------------------------------------------------

class DldRegistryEntriesHashTable
{
    
private:
    
    ght_hash_table_t* HashTable;
    IORWLock*         RWLock;
    
#if defined(DBG)
    thread_t           ExclusiveThread;
#endif//DBG
    
    //
    // returns an allocated hash table object
    //
    static DldRegistryEntriesHashTable* withSize( int size, bool non_block );
    
    //
    // free must be called before the hash table object is deleted
    //
    void free();
    
    //
    // as usual for IOKit the desctructor and constructor do nothing
    // as it is impossible to return an error from the constructor
    // in the kernel mode
    //
    DldRegistryEntriesHashTable()
    { 
        this->HashTable = NULL;
        this->RWLock = NULL;
#if defined(DBG)
        this->ExclusiveThread = NULL;
#endif//DBG
    }
    
    //
    // the destructor checks that the free() has been called
    //
    ~DldRegistryEntriesHashTable(){ assert( !this->HashTable && !this->RWLock ); };
    
public:
    
    static bool CreateStaticTableWithSize( int size, bool non_block );
    static void DeleteStaticTable();
    
    //
    // adds an entry to the hash table and refernces it, so the caller must derefrence the entry
    // when it is not longer needed
    //
    bool   AddObject( __in OSObject* obj, __in DldIORegistryEntry* objEntry );
    
    //
    // removes the entry from the hash and returns the removed entry, NULL if there
    // is no entry for an object, the returned entry is referenced
    //
    DldIORegistryEntry*   RemoveObject( __in OSObject* obj );
    
    //
    // returns an entry from the hash table, the returned entry is referenced
    // if the refrence's value is "true"
    //
    DldIORegistryEntry*   RetrieveObjectEntry( __in OSObject* obj, __in bool reference = true );
    
    
    void
    LockShared()
    {   assert( this->RWLock );
        assert( preemption_enabled() );
        
        IORWLockRead( this->RWLock );
    };
    
    
    void
    UnLockShared()
    {   assert( this->RWLock );
        assert( preemption_enabled() );
        
        IORWLockUnlock( this->RWLock );
    };
    
    
    void
    LockExclusive()
    {
        assert( this->RWLock );
        assert( preemption_enabled() );
        
#if defined(DBG)
        assert( current_thread() != this->ExclusiveThread );
#endif//DBG
        
        IORWLockWrite( this->RWLock );
        
#if defined(DBG)
        assert( NULL == this->ExclusiveThread );
        this->ExclusiveThread = current_thread();
#endif//DBG
        
    };
    
    
    void
    UnLockExclusive()
    {
        assert( this->RWLock );
        assert( preemption_enabled() );
        
#if defined(DBG)
        assert( current_thread() == this->ExclusiveThread );
        this->ExclusiveThread = NULL;
#endif//DBG
        
        IORWLockUnlock( this->RWLock );
    };
    
    static DldRegistryEntriesHashTable* sHashTable;
};

#endif//_DLDIOREGISTRYENTRY_H