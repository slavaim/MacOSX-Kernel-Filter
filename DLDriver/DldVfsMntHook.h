/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */
    
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <libkern/c++/OSArray.h>
#include "DldFakeFSD.h"

//--------------------------------------------------------------------

class DldVfsMntHook: public OSObject{
    
    OSDeclareDefaultStructors( DldVfsMntHook )

private:
    
    class DldVfsHook{
        
        friend class DldVfsMntHook;
        
    private:
        
        //
        // the structure is recycled when the reference count drops to zero
        //
        SInt32          referenceCount;
        
        //
        // addresses of original functions
        //
        VFSFUNC         originalVfsOps[ kDldVfsOpMax ];
        
        //
        // mnt->mnt_op value
        //
        VFSFUNC*        vfsOpsVector;
        
    protected:
        
        static DldVfsHook*   withVfsOpsVector( __in VFSFUNC*  vfsOpsVector );
        
        virtual void retain();
        virtual void release();
        
        virtual VFSFUNC*  getVfsOpsVector() { return vfsOpsVector; }
        
        virtual VFSFUNC  getOriginalVfsOps( __in DldVfsOperation op )
        { 
            assert( op < kDldVfsOpMax );
            return this->originalVfsOps[ op ];
        }
        
    };
    
private:
    
    //
    // a hooked mount structure
    //
    mount_t       mnt;
    
    //
    // several mount structures can share the same vfs operation structure,
    // the object is referenced
    //
    DldVfsHook*   vfsHook;
    
    //
    // number of "dirty" CAWL files, i.e. the number of CAWL backed files with unflushed data
    //
    SInt32        dirtyCawlFilesCnt;
    
    //
    // true if the volume is under the CAWL control
    //
    bool          controlledByCAWL;
    
    //
    // if true the object is a second duplicate as a concurrent thread managed to create
    // an object for the mounted volume structure before this object was created
    //
    bool         duplicateObject;
    
    //
    // a static array of DldVfsMntHook objects
    //
    static OSArray*  sMntsArray;
    
    //
    // a lock for sMntsArray protection
    //
    static IORWLock* sRWLock;
    
    //
    // the returned object is not referenced, a caller must hold the lock
    //
    static DldVfsMntHook* findInArrayByMount( __in mount_t mnt );
    
    //
    // the returned object is not referenced, a caller must hold the lock
    //
    static DldVfsHook*  findInArrayByVfsOpVector( __in VFSFUNC* vfsVector );
    
    //
    // returns an address for a VFS operation entry in a mount structure's VFS vector
    //
    static VFSFUNC* getVfsOperationEntryForMount( __in mount_t mnt, __in DldVfsOperation op );
    static VFSFUNC* getVfsOperationEntryForVfsVector( __in VFSFUNC* vfsOpVector, __in DldVfsOperation op );
    
protected:
    
    virtual void free();
    
public:
    
    virtual void incrementDirtyCawlFilesCounter(){ OSIncrementAtomic( &this->dirtyCawlFilesCnt ); }
    virtual void decrementDirtyCawlFilesCounter(){ assert( this->dirtyCawlFilesCnt > 0x0 ); OSDecrementAtomic( &this->dirtyCawlFilesCnt ); }
    virtual SInt32 getDirtyCawlFilesCounter(){ return this->dirtyCawlFilesCnt; }
    
    virtual void setAsControlledByCAWL() { this->controlledByCAWL = true; }
    virtual bool isAsControlledByCAWL() { return this->controlledByCAWL; }
    
    static int  DldVfsUnmountHook(struct mount *mp, int mntflags, vfs_context_t context);
    
    //
    // returns a referenced object, if there is no object for
    // the mnt structure it will be created, the caller must
    // call release() when the object is no longer needed, the
    // function is idempotent in behaviour
    //
    static DldVfsMntHook*  withVfsMnt( __in mount_t mnt );
    
    //
    // a hook function, actually a wrapper for withVfsMnt,
    // there is no unhook function as unhook is performed
    // in the DldVfsUnmountHook routine
    //
    static IOReturn HookVfsMnt( __in mount_t mnt );
    
    //
    // static members initialization and destruction
    //
    static IOReturn CreateVfsMntHook();
    static void DestructVfsMntHook();

};

//--------------------------------------------------------------------
