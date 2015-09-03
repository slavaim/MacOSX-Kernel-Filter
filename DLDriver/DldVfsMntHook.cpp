/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#include "DldVfsMntHook.h"
#include "DldVmPmap.h"

//--------------------------------------------------------------------

OSArray*  DldVfsMntHook::sMntsArray = NULL;
IORWLock* DldVfsMntHook::sRWLock = NULL;

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldVfsMntHook, OSObject )

//--------------------------------------------------------------------

IOReturn DldVfsMntHook::CreateVfsMntHook()
{
    DldVfsMntHook::sRWLock = IORWLockAlloc();
    assert( DldVfsMntHook::sRWLock );
    if( !DldVfsMntHook::sRWLock ){
        
        DBG_PRINT_ERROR(( "IORWLockAlloc() failed\n" ));
        return kIOReturnNoMemory;
    }
    
    DldVfsMntHook::sMntsArray = OSArray::withCapacity( 4 );
    assert( DldVfsMntHook::sMntsArray );
    if( !DldVfsMntHook::sMntsArray ){
        
        DBG_PRINT_ERROR(( "OSArray::withCapacity() failed\n" ));
        return kIOReturnNoMemory;
    }
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

void DldVfsMntHook::DestructVfsMntHook()
{
    if( DldVfsMntHook::sRWLock )
        IORWLockFree( DldVfsMntHook::sRWLock );
    
    if( DldVfsMntHook::sMntsArray )
        DldVfsMntHook::sMntsArray->release();
}

//--------------------------------------------------------------------

VFSFUNC* DldVfsMntHook::getVfsOperationEntryForVfsVector( __in VFSFUNC* vfsOpVector, __in DldVfsOperation op )
{
    
    assert( op < kDldVfsOpMax );
    
    DldVfsOperationDesc*  dsc;
    
    dsc = DldGetVfsOperationDesc( op );
    if( !dsc )
        return NULL;
    
    assert( DLD_VOP_UNKNOWN_OFFSET != dsc->offset );
    
    return (VFSFUNC*)((vm_address_t)vfsOpVector + dsc->offset);
}

//--------------------------------------------------------------------

VFSFUNC* DldVfsMntHook::getVfsOperationEntryForMount( __in mount_t mnt, __in DldVfsOperation op )
{
    VFSFUNC* vfsOpVector;
    
    assert( op < kDldVfsOpMax );
    
    vfsOpVector = DldGetVfsOperations( mnt );
    assert( vfsOpVector );
    if( !vfsOpVector )
        return NULL;
    
    return DldVfsMntHook::getVfsOperationEntryForVfsVector( vfsOpVector, op );
}

//--------------------------------------------------------------------

//
// the returned object is not referenced, a caller must hold the lock
//
DldVfsMntHook*  DldVfsMntHook::findInArrayByMount( __in mount_t mnt )
{
    int count = DldVfsMntHook::sMntsArray->getCount();
    
    //
    // it is unlikely that there will be so many mounted FS
    //
    assert( count < 0xFF );
    
    for( int i = 0x0; i < count; ++i ){
        
        DldVfsMntHook*  currentHookObj = OSDynamicCast( DldVfsMntHook, DldVfsMntHook::sMntsArray->getObject( i ) );
        assert( currentHookObj );
        
        if( currentHookObj->mnt != mnt )
            continue;
        
        return currentHookObj;
    } // end for
    
    return NULL;
}

//--------------------------------------------------------------------

//
// the returned object is not referenced, a caller must hold the lock
//
DldVfsMntHook::DldVfsHook*  DldVfsMntHook::findInArrayByVfsOpVector( __in VFSFUNC* vfsVector )
{
    int count = DldVfsMntHook::sMntsArray->getCount();
    
    //
    // it is unlikely that there will be so many mounted FS
    //
    assert( count < 0xFF );
    
    for( int i = 0x0; i < count; ++i ){
        
        DldVfsMntHook*  currentHookObj = OSDynamicCast( DldVfsMntHook, DldVfsMntHook::sMntsArray->getObject( i ) );
        assert( currentHookObj );
        
        assert( currentHookObj->vfsHook && currentHookObj->vfsHook->getVfsOpsVector() );
        
        if( currentHookObj->vfsHook->getVfsOpsVector() != vfsVector )
            continue;
        
        return currentHookObj->vfsHook;
    } // end for
    
    return NULL;
}

//--------------------------------------------------------------------

void DldVfsMntHook::DldVfsHook::retain()
{
    assert( this->referenceCount > 0x0 );
    OSIncrementAtomic( &this->referenceCount );
}

//--------------------------------------------------------------------

void DldVfsMntHook::DldVfsHook::release()
{
    assert( preemption_enabled() );
    assert( this->referenceCount > 0x0 );
    
    if( 0x1 != OSDecrementAtomic( &this->referenceCount ) )
        return;
    
    //
    // the last reference has gone,
    // unhook all VFS operations
    //
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( this->originalVfsOps ); ++i ){
        
        if( NULL == this->originalVfsOps[ i ] ){
            
            //
            // not hooked
            //
            continue;
        }
        
        assert( i != kDldVfsOpUnknown );
        
        VFSFUNC*  funcToUnhookEntry;
        
        //
        // hook and add in the array, currently only unmount is hooked to support CAWL
        //
        funcToUnhookEntry = DldVfsMntHook::getVfsOperationEntryForVfsVector( (VFSFUNC*)this->vfsOpsVector, (DldVfsOperation)i );
        assert( funcToUnhookEntry );
        if( funcToUnhookEntry ){
            
            unsigned int bytes;
            
            //
            // replace the function address with the original one
            //
            
            bytes = DldWriteWiredSrcToWiredDst( (vm_offset_t)&this->originalVfsOps[ i ],
                                                (vm_offset_t)funcToUnhookEntry,
                                                sizeof( VFSFUNC ) );
            
            assert( sizeof( VFSFUNC ) == bytes );
            
            this->originalVfsOps[ i ] = NULL;
            
        } else {
            
            DBG_PRINT_ERROR(( "getVfsOperationEntryForVfsVector(%u) failed\n", (unsigned int)i ));
        }
        
    } // end for
    
    delete this;
}

//--------------------------------------------------------------------

typedef struct _DldVfsHookDscr{
    DldVfsOperation  operation;
    VFSFUNC          hookingFunction;
} DldVfsHookDscr;

DldVfsHookDscr gDldVfsHookDscrs[] = 
{
    { kDldVfsOpUnmount, (VFSFUNC)DldVfsMntHook::DldVfsUnmountHook },
    
    //
    // the last termintaing entry
    //
    { kDldVfsOpUnknown, (VFSFUNC)NULL }
};

//--------------------------------------------------------------------

DldVfsMntHook::DldVfsHook*  DldVfsMntHook::DldVfsHook::withVfsOpsVector( __in VFSFUNC*  vfsOpsVector )
{
    //
    // THE CALLER MUST HOLD THE LOCK EXCLUSIVELY
    //
    
    DldVfsMntHook::DldVfsHook*  vfsObj = NULL;
    
    assert( vfsOpsVector );
    if( !vfsOpsVector )
        return NULL;
    
    //
    // first try to search in the array
    //
    vfsObj = DldVfsMntHook::findInArrayByVfsOpVector( vfsOpsVector );
    if( vfsObj ){
        
        vfsObj->retain();
        return vfsObj;
        
    }
    
    vfsObj = new DldVfsMntHook::DldVfsHook();
    assert( vfsObj );
    if( !vfsObj ){
        
        DBG_PRINT_ERROR(( "new DldVfsMntHook::DldVfsHook() faile\n" ));
        return NULL;
    }
    
    vfsObj->vfsOpsVector = vfsOpsVector;
    vfsObj->referenceCount = 0x1;
    
    for( int i = 0x0; kDldVfsOpUnknown != gDldVfsHookDscrs[ i ].operation; ++i ){
        
        VFSFUNC*  funcToHookEntry;
        
        assert( gDldVfsHookDscrs[ i ].hookingFunction );
        
        //
        // hook and add in the array, currently only unmount is hooked to support CAWL
        //
        funcToHookEntry = DldVfsMntHook::getVfsOperationEntryForVfsVector( (VFSFUNC*)vfsOpsVector, gDldVfsHookDscrs[ i ].operation );
        assert( funcToHookEntry );
        if( funcToHookEntry ){
            
            unsigned int bytes;
            
            assert( gDldVfsHookDscrs[ i ].operation < DLD_STATIC_ARRAY_SIZE( vfsObj->originalVfsOps ) );
            assert( gDldVfsHookDscrs[ i ].hookingFunction != *funcToHookEntry );
            
            //
            // exchange the functions
            //
            vfsObj->originalVfsOps[ gDldVfsHookDscrs[ i ].operation ] = *funcToHookEntry;
            
            bytes = DldWriteWiredSrcToWiredDst( (vm_offset_t)&gDldVfsHookDscrs[ i ].hookingFunction,
                                                (vm_offset_t)funcToHookEntry,
                                                sizeof( VFSFUNC ) );
            
            assert( sizeof( VFSFUNC ) == bytes );
            
        } else {
            
            DBG_PRINT_ERROR(( "getVfsOperationEntryForVfsVector(%u) failed\n", (unsigned int)gDldVfsHookDscrs[ i ].operation  ));
        }
        
    } // end for
    
    return vfsObj;
}

//--------------------------------------------------------------------

DldVfsMntHook*  DldVfsMntHook::withVfsMnt( __in mount_t mnt )
{
    assert( preemption_enabled() );
    assert( DldVfsMntHook::sRWLock );
    assert( DldVfsMntHook::sMntsArray );
    assert( mnt );
    
    DldVfsMntHook*  mntHookObj = NULL;
    DldVfsMntHook*  objToRelease = NULL;
    
    if( !mnt )
        return NULL;
    
    //
    // first try to search in the array
    //
    IORWLockRead( DldVfsMntHook::sRWLock );
    { // start of the lock
        
        mntHookObj = DldVfsMntHook::findInArrayByMount( mnt );
        if( mntHookObj )
            mntHookObj->retain();
        
    } // end of the lock
    IORWLockUnlock( DldVfsMntHook::sRWLock );
    
    if( mntHookObj )
        return mntHookObj;
    
    //
    // create a new object
    //
    mntHookObj = new DldVfsMntHook();
    assert( mntHookObj );
    if( !mntHookObj ){
        
        DBG_PRINT_ERROR(( "new DldVfsMntHook() fauled\n" ));
        return NULL;
    }
    
    if( !mntHookObj->init() ){
        
        DBG_PRINT_ERROR(( "mntHookObj->init() failed\n" ));
        mntHookObj->release();
        return NULL;
    }
    
    mntHookObj->mnt = mnt;
    
    //
    // hook the VFS operations, there should not be two hooking thread simultaneously
    //
    IORWLockWrite( DldVfsMntHook::sRWLock );
    { // start of the lock
        
        //
        // first check thet it has not been already hooked
        //
        if( DldVfsMntHook::findInArrayByMount( mnt ) ){
            
            //
            // releasing under the lock is not allowed as results in the deadlock
            //
            objToRelease = mntHookObj;
            objToRelease->duplicateObject = true;
            mntHookObj = DldVfsMntHook::findInArrayByMount( mnt );
            
            assert( mntHookObj && mntHookObj->vfsHook );
            mntHookObj->retain();
            
        } else {
            
            mntHookObj->vfsHook = DldVfsMntHook::DldVfsHook::withVfsOpsVector( ::DldGetVfsOperations( mnt ) );
            assert( mntHookObj->vfsHook );
            if( mntHookObj->vfsHook ){
                
                //
                // insert in the array, the array retains the object!
                //
                if( !DldVfsMntHook::sMntsArray->setObject( mntHookObj ) ){
                                       
                    DBG_PRINT_ERROR(( "DldVfsMntHook::sMntsArray->setObject( mntHookObj ) failed\n" ));
                    assert( NULL == objToRelease );
                    objToRelease = mntHookObj; // do not release under the lock!
                    mntHookObj = NULL;
                }
                
            } else {
                
                
                DBG_PRINT_ERROR(( "DldVfsMntHook::DldVfsHook::withVfsOpsVector failed\n" ));
                assert( NULL == objToRelease );
                objToRelease = mntHookObj; // do not release under the lock!
                mntHookObj = NULL;
            }
            
        }
        
    } // end of the lock
    IORWLockUnlock( DldVfsMntHook::sRWLock );
    
    if( objToRelease )
        objToRelease->release();
    
    return mntHookObj;
}

//--------------------------------------------------------------------

void DldVfsMntHook::free()
{
    //
    // it is okay for the duplicate object to be removed
    //
    assert(!( false == this->duplicateObject  && NULL != DldVfsMntHook::findInArrayByMount( this->mnt ) ) );
    assert( this != DldVfsMntHook::findInArrayByMount( this->mnt ) );
    
    if( this->vfsHook )
        this->vfsHook->release();
    
    super::free();
}

//--------------------------------------------------------------------

int  DldVfsMntHook::DldVfsUnmountHook(struct mount *mp, int mntflags, vfs_context_t context)
{
    DldVfsMntHook*  mntHookObj = NULL;
    typedef int  (*VfsUnmount)(struct mount *mp, int mntflags, vfs_context_t context);
    VfsUnmount      vfsUnmountOriginal;
    bool            allowUnmount = true;
    bool            forcedUnmount;
    
    forcedUnmount = ( 0x0 != (mntflags & MNT_FORCE) );
    
    //
    // search in the array and remove if found
    //
    IORWLockWrite( DldVfsMntHook::sRWLock );
    { // start of the lock
        
        int count = DldVfsMntHook::sMntsArray->getCount();
        
        //
        // it is unlikely that there will be so many mounted FS
        //
        assert( count < 0xFF );
        
        for( int i = 0x0; i < count; ++i ){
            
            assert( !mntHookObj );
            
            DldVfsMntHook*  currentHookObj = OSDynamicCast( DldVfsMntHook, DldVfsMntHook::sMntsArray->getObject( i ) );
            assert( currentHookObj );
            
            if( currentHookObj->mnt != mp )
                continue;
            
            mntHookObj = currentHookObj;
            
            //
            // if there no dirty files it is okay to remove the object
            // and allow the unmount, also the forced unmount is always
            // obeyed
            //
            allowUnmount = ( forcedUnmount || 0x0 == mntHookObj->dirtyCawlFilesCnt );
            if( allowUnmount ){
                
                //
                // remove from the array, this also removes a reference, so retain before removing
                //
                mntHookObj->retain();
                DldVfsMntHook::sMntsArray->removeObject( i );
            }
            
            break;
        } // end for
        
        
    } // end of the lock
    IORWLockUnlock( DldVfsMntHook::sRWLock );
    
    assert( mntHookObj );
    if( !mntHookObj || !allowUnmount ){
        
        DBG_PRINT_ERROR(( "mntHookObj is %p, allowUnmount = %u\n", mntHookObj, (int)allowUnmount ));
        
        //
        // disable unmount to not damage the system
        //
        return EBUSY;
    }
    
    assert( mntHookObj->mnt == mp );
    assert( mntHookObj->vfsHook );
    
    vfsUnmountOriginal = (VfsUnmount)mntHookObj->vfsHook->getOriginalVfsOps( kDldVfsOpUnmount );
    assert( vfsUnmountOriginal );
    if( !vfsUnmountOriginal ){
        
        mntHookObj->release();
        
        DBG_PRINT_ERROR(( "vfsUnmountOriginal is NULL\n" ));
        return EBUSY;
        
    } // end if( !vfsUnmountOriginal )
    
    mntHookObj->vfsHook->release();
    mntHookObj->vfsHook = NULL;
    mntHookObj->release();
    
    return vfsUnmountOriginal( mp, mntflags, context );
}


//--------------------------------------------------------------------

IOReturn DldVfsMntHook::HookVfsMnt( __in mount_t mp )
{
    DldVfsMntHook*  mntHookObj;
    
    mntHookObj = DldVfsMntHook::withVfsMnt( mp );
    assert( mntHookObj );
    if( !mntHookObj ){
        
        DBG_PRINT_ERROR(( "withVfsMnt() failed to return an object\n" ));
        return kIOReturnError;
    }
    
    //
    // the object is retained by the array
    //
    mntHookObj->release();
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------
