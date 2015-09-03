/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDVNODEHASH_H
#define _DLDVNODEHASH_H

#include <libkern/c++/OSObject.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include "DldCommon.h"
#include "DldIOVnode.h"


class DldVnodeHashTable;

//--------------------------------------------------------------------

class DldVnodeHashTableListHead{
    
    friend class DldVnodeHashTable;
    
private:
    
    DldVnodeHashTableListHead();
    ~DldVnodeHashTableListHead();
    
    void LockShared();
    void UnlockShared();
    
    void LockExclusive();
    void UnlockExclusive();
    
    //
    // returns a referenced object
    //
    DldIOVnode* RetrieveReferencedIOVnodByBSDVnode( __in vnode_t vnode );
    
    //
    // remove an object from the table if it exists and returns
    // a retained entry for the removed object
    //
    DldIOVnode* RemoveIOVnodeByBSDVnode( __in vnode_t vnode );
    
    //
    // adds an object to the table and references the object
    //
    void AddIOVnode( __in DldIOVnode* ioVnode );
    
    //
    // a head of the IOVnodes list
    //
    CIRCLEQ_HEAD( ListHead, DldIOVnode ) listHead;
    
    //
    // a lock to protect the list access
    //
    IORWLock*    rwLock;
    
#if defined( DBG )
    thread_t exclusiveThread;
#endif//DBG
    
};

//--------------------------------------------------------------------


class DldVnodeHashTable : public OSObject
{    
    OSDeclareDefaultStructors( DldVnodeHashTable )
    
private:
    
    //
    // it is better to choose the prime number which is not too close
    // to a power of 2
    //
    const static unsigned long VNODE_HASH_SIZE = 97;
    
    DldVnodeHashTableListHead   head[ VNODE_HASH_SIZE ];
    
    static unsigned long BSDVnodeToHashTableLine( __in vnode_t vnode )
    { return (((unsigned long)vnode)>>0x5)%VNODE_HASH_SIZE; }
    
    void LockLineSharedByBSDVnode( __in vnode_t vnode );
    void UnlockLineSharedByBSDVnode( __in vnode_t vnode );
    
    void LockLineExclusiveByBSDVnode( __in vnode_t vnode );
    void UnlockLineExclusiveByBSDVnode( __in vnode_t vnode );
    
    //
    // returns a referenced object
    //
    DldIOVnode* RetrieveReferencedIOVnodByBSDVnodeWOLock( __in vnode_t vnode );
    
    //
    // removes an object from the table if it exists and returns
    // a retained entry for the removed object
    //
    DldIOVnode* RemoveIOVnodeByBSDVnodeWOLock( __in vnode_t vnode );
    
    //
    // adds an object to the table and references the object
    //
    void AddIOVnodeWOLock( __in DldIOVnode* ioVnode );
    
public:
    
    //
    // the returned object is retained, the function is idempotent in its behaviour
    // but this behaviour should not be abused by overusing
    //
    DldIOVnode* CreateAndAddIOVnodByBSDVnode( __in vnode_t vnode,
                                             __in DldIOVnode::VnodeType vnodeType = DldIOVnode::kVnodeType_Native );
    
    //
    // returns a referenced object from the hash table or NULL if
    // the corresponding object doesn't exist
    //
    DldIOVnode* RetrieveReferencedIOVnodByBSDVnode( __in vnode_t vnode );
    
    //
    // removes the corresponding object from the hash table
    //
    void RemoveIOVnodeByBSDVnode( __in vnode_t vnode );
    
    static bool CreateStaticTable();
    static void DeleteStaticTable();
    
    static DldVnodeHashTable*      sVnodesHashTable;
};

//--------------------------------------------------------------------

#endif//_DLDVNODEHASH_H

