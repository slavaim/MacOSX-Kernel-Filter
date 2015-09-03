/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDVNODEHOOK_H
#define _DLDVNODEHOOK_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
    
#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------

#include <libkern/c++/OSObject.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include "DldCommon.h"
#include "DldFakeFSD.h"
#include "DldCommonHashTable.h"

//--------------------------------------------------------------------

typedef enum _DldVopEnum{
    DldVopEnum_Unknown = 0x0,
    
    DldVopEnum_access,
    DldVopEnum_advlock,
    DldVopEnum_allocate,
    DldVopEnum_blktooff,
    DldVopEnum_blockmap,
    DldVopEnum_bwrite,
    DldVopEnum_close,
    DldVopEnum_copyfile,
    DldVopEnum_create,
    DldVopEnum_default,
    DldVopEnum_exchange,
    DldVopEnum_fsync,
    DldVopEnum_getattr,
    DldVopEnum_getxattr,
    DldVopEnum_inactive,
    DldVopEnum_ioctl,
    DldVopEnum_link,
    DldVopEnum_listxattr,
    DldVopEnum_lookup,
    DldVopEnum_kqfilt_add,
    DldVopEnum_kqfilt_remove,
    DldVopEnum_mkdir,
    DldVopEnum_mknod,
    DldVopEnum_mmap,
    DldVopEnum_mnomap,
    DldVopEnum_offtoblock,
    DldVopEnum_open,
    DldVopEnum_pagein,
    DldVopEnum_pageout,
    DldVopEnum_pathconf,
    DldVopEnum_read,
    DldVopEnum_readdir,
    DldVopEnum_readdirattr,
    DldVopEnum_readlink,
    DldVopEnum_reclaim,
    DldVopEnum_remove,
    DldVopEnum_removexattr,
    DldVopEnum_rename,
    DldVopEnum_revoke,
    DldVopEnum_rmdir,
    DldVopEnum_searchfs,
    DldVopEnum_select,
    DldVopEnum_setattr,
    DldVopEnum_setxattr,
    DldVopEnum_strategy,
    DldVopEnum_symlink,
    DldVopEnum_whiteout,
    DldVopEnum_write,
    DldVopEnum_getnamedstreamHook,
    DldVopEnum_makenamedstreamHook,
    DldVopEnum_removenamedstreamHook,
    
    DldVopEnum_Max
} DldVopEnum;

//--------------------------------------------------------------------

class DldVnodeHookEntry: public OSObject
{
    
    OSDeclareDefaultStructors( DldVnodeHookEntry )
    
#if defined( DBG )
    friend class DldVnodeHooksHashTable;
#endif//DBG
    
private:
    
    //
    // the number of vnodes which we are aware of for this v_op vector
    //
    SInt32 vNodeCounter;
    
    //
    // the value is used to mark the origVop's entry as 
    // corresponding to not hooked function ( i.e. skipped deliberately )
    //
    static VOPFUNC  vopNotHooked;
    
    //
    // an original functions array,
    // for not present functons the values are set to NULL( notional assertion, 
    // it was niether checked no taken into account by the code ),
    // for functions which hooking was skipped deliberately the
    // value is set to vopNotHooked
    //
    VOPFUNC  origVop[ DldVopEnum_Max ];
    
#if defined( DBG )
    bool   inHash;
#endif
    
protected:
    
    virtual bool init();
    virtual void free();
    
public:
    
    //
    // allocates the new entry
    //
    static DldVnodeHookEntry* newEntry()
    {
        DldVnodeHookEntry* entry;
        
        assert( preemption_enabled() );
        
        entry = new DldVnodeHookEntry();
        assert( entry ) ;
        if( !entry )
            return NULL;
        
        //
        // the init is very simple and must alvays succeed
        //
        entry->init();
        
        return entry;
    }
    
    
    VOPFUNC
    getOrignalVop( __in DldVopEnum   indx ){
        
        assert( indx < DldVopEnum_Max );
        return this->origVop[ (int)indx ];
    }
    
    void
    setOriginalVop( __in DldVopEnum   indx, __in VOPFUNC orig ){
        
        assert( indx < DldVopEnum_Max );
        assert( NULL == this->origVop[ (int)indx ] );
        
        this->origVop[ (int)indx ] = orig;
    }
    
    void
    setOriginalVopAsNotHooked( __in DldVopEnum   indx ){
        
        this->setOriginalVop( indx, this->vopNotHooked );
    }
    
    bool
    isHooked( __in DldVopEnum indx ){
        
        //
        // NULL is invalid, vopNotHooked means not hooked deliberately
        //
        return ( this->vopNotHooked != this->origVop[ (int)indx ] );
    }
    
    //
    // returns te value before the increment
    //
    SInt32
    incrementVnodeCounter(){
        
        assert( this->vNodeCounter < 0x80000000 );
        return OSIncrementAtomic( &this->vNodeCounter );
    }
    
    //
    // returns te value before the decrement
    //
    SInt32
    decrementVnodeCounter(){
        
        assert( this->vNodeCounter > 0x0 && this->vNodeCounter < 0x80000000);
        return OSDecrementAtomic( &this->vNodeCounter );
    }
    
    SInt32
    getVnodeCounter(){
        
        return this->vNodeCounter;
    }
    
};

//--------------------------------------------------------------------

class DldVnodeHooksHashTable
{
    
private:
    
    ght_hash_table_t*  HashTable;
    IORWLock*          RWLock;
    
#if defined(DBG)
    thread_t           ExclusiveThread;
#endif//DBG
    
    //
    // returns an allocated hash table object
    //
    static DldVnodeHooksHashTable* withSize( int size, bool non_block );
    
    //
    // free must be called before the hash table object is deleted
    //
    void free();
    
    //
    // as usual for IOKit the desctructor and constructor do nothing
    // as it is impossible to return an error from the constructor
    // in the kernel mode
    //
    DldVnodeHooksHashTable()
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
    ~DldVnodeHooksHashTable()
    {
        
        assert( !this->HashTable && !this->RWLock );
    };
    
public:
    
    static bool CreateStaticTableWithSize( int size, bool non_block );
    static void DeleteStaticTable();
    
    //
    // adds an entry to the hash table, the entry is referenced so the caller must
    // dereference the entry if it has been referenced
    //
    bool   AddEntry( __in VOPFUNC* v_op, __in DldVnodeHookEntry* entry );
    
    //
    // removes the entry from the hash and returns the removed entry, NULL if there
    // is no entry for an object, the returned entry is referenced
    //
    DldVnodeHookEntry*   RemoveEntry( __in VOPFUNC* v_op );
    
    //
    // returns an entry from the hash table, the returned entry is referenced
    // if the refrence's value is "true"
    //
    DldVnodeHookEntry*   RetrieveEntry( __in VOPFUNC* v_op, __in bool reference = true );
    
    
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
    
    static DldVnodeHooksHashTable* sVnodeHooksHashTable;
};

//--------------------------------------------------------------------

//
// success doen't mean that a vnode's operations table has
// been hooked, it can be skipped as ia not interested for us
//
extern
IOReturn
DldHookVnodeVop(
    __inout vnode_t vnode,
    __inout bool* isVopHooked
    );

extern
VOPFUNC
DldGetOriginalVnodeOp(
    __in vnode_t      vnode,
    __in DldVopEnum   indx
    );

extern
void
DldUnHookVnodeVop(
    __inout vnode_t vnode
    );

//--------------------------------------------------------------------

#endif//_DLDVNODEHOOK_H

