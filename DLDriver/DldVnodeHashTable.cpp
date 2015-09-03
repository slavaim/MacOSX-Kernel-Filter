/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldVnodeHashTable.h"
#include "DldVNodeHook.h"

//--------------------------------------------------------------------

void DldVnodeHashTableListHead::LockShared()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
    
    assert( NULL == this->exclusiveThread );
    
}

void DldVnodeHashTableListHead::UnlockShared()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    assert( NULL == this->exclusiveThread );
    
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

void DldVnodeHashTableListHead::LockExclusive()
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

void DldVnodeHashTableListHead::UnlockExclusive()
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

DldVnodeHashTableListHead::DldVnodeHashTableListHead()
{
    assert( preemption_enabled() );
    
    CIRCLEQ_INIT_WITH_TYPE( &this->listHead, DldIOVnode );
    
    //
    // the error can't be reported from the c++ constructor in the kernel
    //
    while( NULL == ( this->rwLock = IORWLockAlloc() ) ){
        
        //
        // sleep for a second and repeat
        //
        IOSleep( 1000 );
    }
    
    assert( this->rwLock );
}

//--------------------------------------------------------------------

DldVnodeHashTableListHead::~DldVnodeHashTableListHead()
{
    DldIOVnode* entry;
    
    assert( preemption_enabled() );
    assert( CIRCLEQ_EMPTY( &this->listHead ) );
    
    CIRCLEQ_FOREACH( entry, &this->listHead, listEntry ){
        // start CIRCLEQ_FOREACH
        
        DldIOVnode* currentEntry = entry;
        
        //
        // move to the next entry before removing the current
        //
        entry = CIRCLEQ_NEXT( currentEntry, listEntry );
        
        //
        // remove the current entry
        //
        CIRCLEQ_REMOVE( &this->listHead, currentEntry, listEntry);
        
        //
        // release the current entry
        //
        currentEntry->release();
        DLD_DBG_MAKE_POINTER_INVALID( currentEntry );
        
    }// end CIRCLEQ_FOREACH
    
    IORWLockFree( this->rwLock );
}

//--------------------------------------------------------------------

DldIOVnode*
DldVnodeHashTableListHead::RetrieveReferencedIOVnodByBSDVnode(
    __in vnode_t vnode
    )
/*
 the returned object is retained
 */
{
    DldIOVnode* entry;
    
    assert( vnode );
    
    CIRCLEQ_FOREACH( entry, &this->listHead, listEntry ){
        // start CIRCLEQ_FOREACH
        
        if( entry->vnode == vnode ){
            
            entry->retain();
            return entry;
            
        }// end if( entry->vnode == vnode )
        
    }// end CIRCLEQ_FOREACH
    
    return NULL;
}

//--------------------------------------------------------------------

//
// the caller must release the returned entry
//
DldIOVnode*
DldVnodeHashTableListHead::RemoveIOVnodeByBSDVnode(
    __in vnode_t vnode
    )
{
    DldIOVnode* entry;
    
    assert( preemption_enabled() );
    
    CIRCLEQ_FOREACH( entry, &this->listHead, listEntry ){
        // start CIRCLEQ_FOREACH
        
        if( entry->vnode == vnode ){
            
            //
            // remove the current entry
            //
            CIRCLEQ_REMOVE( &this->listHead, entry, listEntry);
#if DBG
            entry->listEntry.cqe_next = entry->listEntry.cqe_prev = NULL;
#endif//DBG
            
            //
            // the entry was retained when was added
            //
            return entry;
            
        }// end if( entry->vnode == vnode )
        
    }// end CIRCLEQ_FOREACH
    
    return NULL;
}

//--------------------------------------------------------------------

void
DldVnodeHashTableListHead::AddIOVnode(
    __in DldIOVnode* ioVnode
    )
{
    assert( ioVnode->vnode );
    assert( NULL == this->RetrieveReferencedIOVnodByBSDVnode( ioVnode->vnode ) );
    
    //
    // retain the entry
    //
    ioVnode->retain();
    
    //
    // add the entry in the list
    //
    CIRCLEQ_INSERT_HEAD_WITH_TYPE( &this->listHead, ioVnode, listEntry, DldIOVnode );
}

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldVnodeHashTable, OSObject )

//--------------------------------------------------------------------

DldVnodeHashTable*    DldVnodeHashTable::sVnodesHashTable;

//--------------------------------------------------------------------

void
DldVnodeHashTable::LockLineSharedByBSDVnode( __in vnode_t vnode )
{
    this->head[ BSDVnodeToHashTableLine( vnode ) ].LockShared();
}

void
DldVnodeHashTable::UnlockLineSharedByBSDVnode( __in vnode_t vnode )
{
    
    this->head[ BSDVnodeToHashTableLine( vnode ) ].UnlockShared();
}

//--------------------------------------------------------------------

void
DldVnodeHashTable::LockLineExclusiveByBSDVnode( __in vnode_t vnode )
{
    this->head[ BSDVnodeToHashTableLine( vnode ) ].LockExclusive();
}

void
DldVnodeHashTable::UnlockLineExclusiveByBSDVnode( __in vnode_t vnode )
{
    this->head[ BSDVnodeToHashTableLine( vnode ) ].UnlockExclusive();
}

//--------------------------------------------------------------------

//
// returns a referenced object
//
DldIOVnode*
DldVnodeHashTable::RetrieveReferencedIOVnodByBSDVnodeWOLock( __in vnode_t vnode )
{
    return this->head[ BSDVnodeToHashTableLine( vnode ) ].RetrieveReferencedIOVnodByBSDVnode( vnode );
}

//--------------------------------------------------------------------

//
// remove an object from the table if it exists
//
DldIOVnode*
DldVnodeHashTable::RemoveIOVnodeByBSDVnodeWOLock( __in vnode_t vnode )
{
    return this->head[ BSDVnodeToHashTableLine( vnode ) ].RemoveIOVnodeByBSDVnode( vnode );
}

//--------------------------------------------------------------------

//
// adds an object to the table and references the object
//
void
DldVnodeHashTable::AddIOVnodeWOLock( __in DldIOVnode* ioVnode )
{
    assert( ioVnode->vnode );
    
    this->head[ BSDVnodeToHashTableLine( ioVnode->vnode ) ].AddIOVnode( ioVnode );
}

//--------------------------------------------------------------------

//
// the returned object is retained, the function is idempotent in its behaviour
// but this behaviour should not be abused by overusing
//
DldIOVnode*
DldVnodeHashTable::CreateAndAddIOVnodByBSDVnode(
    __in vnode_t vnode,
    __in DldIOVnode::VnodeType vnodeType
    )
{
    DldIOVnode* ioVnode;
    DldIOVnode* ioVnodeNew;
    DldIOVnode* ioVnodeOld;
    
    assert( preemption_enabled() );
    
    if( vnode_isrecycled( vnode ) ){
        
        //
        // the vnode has been put in a dead state
        // by vclean()
        // 	vp->v_mount = dead_mountp;
        //  vp->v_op = dead_vnodeop_p;
        //  vp->v_tag = VT_NON;
        //  vp->v_data = NULL;
        //
        // we should not hook dead_vnodeop_p as hooking
        // it results in processing vnode as a normal one
        // which is not true
        //
        return NULL;
        
    } // if( vnode_isrecycled( vnode ) )
    
    
    //
    // as the routine is called from the Kauth callbacks the same
    // vnode might be encountered dozens of times, so the common
    // scenario is when a vnode is already in the hash table
    //
    ioVnodeOld = this->RetrieveReferencedIOVnodByBSDVnode( vnode );
    if( ioVnodeOld )
        return ioVnodeOld;
    
    //
    // create a new vnode before acquiring the deadlock
    //
    ioVnodeNew = DldIOVnode::withBSDVnode( vnode );
    assert( ioVnodeNew );
    if( !ioVnodeNew )
        return NULL;
    
    ioVnodeNew->dldVnodeType = vnodeType;
    
    this->LockLineExclusiveByBSDVnode( vnode );
    {// start of the lock
        
        //
        // search the hash table for the ioVnode for the vnode
        //
        ioVnodeOld = this->RetrieveReferencedIOVnodByBSDVnodeWOLock( vnode );
        if( !ioVnodeOld ){
            
            this->AddIOVnodeWOLock( ioVnodeNew );
            
        }// end if( !ioVnode )
        
    }// end of the lock
    this->UnlockLineExclusiveByBSDVnode( vnode );
    
    
    if( ioVnodeOld ){
        
        //
        // destroy the object as it has not been added in the hash table
        //
        if( ioVnodeNew )
            ioVnodeNew->release();
        
        ioVnode = ioVnodeOld;
        
    } else {
        
        ioVnode = ioVnodeNew;
        
        //
        // hook all vnodes including covered ones as
        // they might have been created with nonhooked voptable
        // as voptable is extracted from the native vnode just
        // after it has been created, if we change this to our
        // own voptable in the future the hook for
        // covered vnodes must be removed and left only for
        // native vnodes
        //
        
        IOReturn   RC;
        bool       vopHooked = false;
        
        RC = DldHookVnodeVop( vnode, &vopHooked );
        assert( kIOReturnSuccess == RC || kIOReturnNoDevice == RC );
        if( kIOReturnSuccess != RC ){
            
            DBG_PRINT_ERROR(("DldHookVnodeVop() failed\n"));
            
            this->RemoveIOVnodeByBSDVnode( vnode );
            ioVnode->release();
            ioVnode = NULL;
            
        } else {
            
            ioVnode->flags.vopHooked = vopHooked? 0x1 : 0x0;
        }
        
    }
        
    assert( ioVnode );
    return ioVnode;
}

//--------------------------------------------------------------------

//
// returns a referenced object from the hash table or NULL if
// the corresponding object doesn't exist
//
DldIOVnode*
DldVnodeHashTable::RetrieveReferencedIOVnodByBSDVnode( __in vnode_t vnode )
{
    
    DldIOVnode* ioVnode;
    
    assert( preemption_enabled() );
    
    this->LockLineSharedByBSDVnode( vnode );
    {// start of the lock
        
        //
        // search the hash table for the ioVnode for the vnode
        //
        ioVnode = this->RetrieveReferencedIOVnodByBSDVnodeWOLock( vnode );
        
    }// end of the lock
    this->UnlockLineSharedByBSDVnode( vnode );
    
    return ioVnode;
}

//--------------------------------------------------------------------

//
// removes the corresponding object from the hash table
//
void
DldVnodeHashTable::RemoveIOVnodeByBSDVnode( __in vnode_t vnode )
{
    DldIOVnode* ioVnode;
    
    this->LockLineExclusiveByBSDVnode( vnode );
    {// start of the lock
        
        //
        // the returned object is retained
        //
        ioVnode = this->RemoveIOVnodeByBSDVnodeWOLock( vnode );
        
    }// end of the lock
    this->UnlockLineExclusiveByBSDVnode( vnode );
    
    if( ioVnode ){
        
        bool unhook = ( 0x1 == ioVnode->flags.vopHooked );
        
        ioVnode->release();
        DLD_DBG_MAKE_POINTER_INVALID( ioVnode );
        
        //
        // the vnode optable is being unhooked if this is the last hooked vnode
        //
        if( unhook )
            DldUnHookVnodeVop( vnode );
    }
}

//--------------------------------------------------------------------

bool
DldVnodeHashTable::CreateStaticTable()
{
    assert( !sVnodesHashTable );
    
    sVnodesHashTable = new DldVnodeHashTable();
    assert( sVnodesHashTable );
    if( !sVnodesHashTable )
        return false;
    
    if( !sVnodesHashTable->init() ){
        
        assert( !"sVnodesHashTable->init() failed" );
        
        sVnodesHashTable->release();
        sVnodesHashTable = NULL;
        
        return false;
    }
    
    gDldDbgData.sVnodesHashTable = (void*)sVnodesHashTable;
    
    return true;
}

//--------------------------------------------------------------------

void
DldVnodeHashTable::DeleteStaticTable()
{
    if( NULL != sVnodesHashTable );
        sVnodesHashTable->release();
    
    sVnodesHashTable = NULL;
}

//--------------------------------------------------------------------

