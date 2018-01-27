/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#ifdef _DLD_MACOSX_CAWL

#include <vfs/vfs_support.h>
#include <sys/ubc.h>
#include <sys/buf.h>

#include <IOKit/IOMemoryDescriptor.h>

#include "DldIOShadow.h"
#include "DldCoveringVnode.h"
#include "DldVnodeHashTable.h"
#include "DldVNodeHook.h"
#include "DldUndocumentedQuirks.h"
#include "DldDiskCAWL.h"
#include "DldVfsMntHook.h"
#include "md5_hash.h"

//--------------------------------------------------------------------

extern vfs_context_t gVfsContextSuser;

SInt32  gCawlNameGenerator = 0x0;
SInt64  gCawlWriteTimeStampSeed = 0x0;

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldCoveringFsd, OSObject )

//--------------------------------------------------------------------

DldCoveringFsd*  DldCoveringFsd::GlobalCoveringFsd = NULL;

//--------------------------------------------------------------------

bool DldCoveringFsd::InitCoveringFsd()
{
    DldCoveringFsd::GlobalCoveringFsd = DldCoveringFsd::withMount( NULL );
    assert( DldCoveringFsd::GlobalCoveringFsd );
    if( !DldCoveringFsd::GlobalCoveringFsd ){
        
        DBG_PRINT_ERROR(("withMount( NULL ) failed"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

DldCoveringFsd* DldCoveringFsd::GetCoveringFsd( __in mount_t mnt )
{
    assert( DldCoveringFsd::GlobalCoveringFsd );
    
    //
    // the returned object is not refernced as it is global
    //
    return DldCoveringFsd::GlobalCoveringFsd;
    
}

//--------------------------------------------------------------------

void DldCoveringFsd::PutCoveringFsd( __in DldCoveringFsd* coveringFsd )
{
    //
    // nothing to do, the object has not been referenced
    //
}

//--------------------------------------------------------------------

DldCoveringFsd*
DldCoveringFsd::withMount( __in_opt mount_t mnt )
{
    DldCoveringFsd*   newCoveringFsd;
    
    newCoveringFsd = new DldCoveringFsd();
    assert( newCoveringFsd );
    if( !newCoveringFsd ){
        
        DBG_PRINT_ERROR(("new newCoveringFsd() failed\n"));
        return NULL;
    }
    
    if( !newCoveringFsd->init() ){
        
        DBG_PRINT_ERROR(("newCoveringFsd->init() failed\n"));
        newCoveringFsd->release();
        return NULL;
    }
    
    newCoveringFsd->mnt = mnt;
    
    return newCoveringFsd;
}

//--------------------------------------------------------------------

bool DldCoveringFsd::init()
{
    this->lock = IORecursiveLockAlloc();
    assert( this->lock );
    if( !this->lock ){
        
        DBG_PRINT_ERROR(("IORecursiveLockAlloc failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

void DldCoveringFsd::free()
{
    if( this->lock )
        IORecursiveLockFree( this->lock );
}

//--------------------------------------------------------------------

errno_t
DldCoveringFsd::DldFsdHookCreateCoveringVnodeInternal(
    __inout DldVnodCreateParams*  ap
    )
/*
 the returned vnode has the iocount set to 0x1
 */
{
    assert( preemption_enabled() );
    assert( ap->dvp && ap->coveredVnode );
    assert( vnode_isreg( ap->coveredVnode ) );
    assert( (VOPFUNC*)DLD_VOP_UNKNOWN_OFFSET != DldGetVnodeOpVector( ap->coveredVnode ) );
    
	errno_t 				error    = 0;
	struct vnode_fsparam	vfsp;
	
    //
	// Zero the FS param structure
    //
	bzero ( &vfsp, sizeof ( vfsp ) );
	
    //
    // it seems okay to use the covered fsd mount structure as all requests
    // to the covered fsd will be intercepted so it will be unaware about
    // attached vnodes, the system traverses all vnodes for a mounted
    // fsd in functions such as vflush() and iterate_vnodes() - the latter
    // is the most dangerous as it allows the covered fsd to see covering vnodes
    // but there is a trick it skips vnodes with NULL v_data fields, so if we
    // set this field to NULL our vnode will not be visible to underlying FSD as
    // there is no another offical method to iterate vnodes
    //
	vfsp.vnfs_mp		 = vnode_mount( ap->coveredVnode );
	vfsp.vnfs_vtype 	 = vnode_vtype( ap->coveredVnode );
	vfsp.vnfs_str 		 = "DldCoverVnode"; // this is only for debug purposes
	vfsp.vnfs_dvp 		 = ap->dvp;
	vfsp.vnfs_fsnode 	 = NULL; // this is the most important trick
	vfsp.vnfs_cnp 		 = NULL; // do not add to the name cache to avoid two entries with the same name,
    // as the covered FSD have already done this
	vfsp.vnfs_vops 		 = DldGetVnodeOpVector( ap->coveredVnode ); // TO DO - get our own opvector!
	vfsp.vnfs_rdev 		 = 0;
    vfsp.vnfs_flags      = ap->vnfs_flags | (VNFS_NOCACHE | VNFS_CANTCACHE); // do not add to the name cache
	vfsp.vnfs_marksystem = vnode_issystem( ap->coveredVnode );
    vfsp.vnfs_markroot   = vnode_isvroot( ap->coveredVnode );
	
    //
    // TO DO
    // What about file size used for UBC ??
    //
    
    //
	// Note that vnode_create ( ) returns the vnode with an iocount of +1;
	// this routine returns the newly created vnode with this positive iocount.
    //
	error = vnode_create( VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &ap->coveringVnode );
    assert( !error );
	if( error != 0 ){
		
		assert ( "DldFsdHookCreateCoverVnode failed" );
		goto __exit;
		
	}
    
__exit:
    
    return error;
}

//--------------------------------------------------------------------

bool
DldCoveringFsd::DldIsCoveringVnode( __in vnode_t vnode )
{
    DldIOVnode::VnodeType   vnType = DldIOVnode::kVnodeType_Native;
    DldIOVnode*             dldVnode;
    
    dldVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( vnode );
    if( dldVnode ){
        
        vnType = dldVnode->dldVnodeType;
        dldVnode->release();
    }
    
    //
    // covering vnodes have NULL private data pointer
    //
    assert( !( (DldIOVnode::kVnodeType_CoveringFSD == vnType) && NULL != vnode_fsnode( vnode ) ) );
    
    return ( DldIOVnode::kVnodeType_CoveringFSD == vnType );
}

//--------------------------------------------------------------------

vnode_t
DldCoveringFsd::DldGetCoveredVnode( __in vnode_t coveringVnode )
/*
 the returned vnode is not referenced, returns NULL if there is no coveredVnode
 */
{
    DldIOVnode*             dldVnode;
    vnode_t                 coveredVnode = NULL;
    
    dldVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( coveringVnode );
    if( dldVnode ){
        
        if( DldIOVnode::kVnodeType_CoveringFSD == dldVnode->dldVnodeType ){
            
            assert( dldVnode->coveredVnode );
            coveredVnode = dldVnode->coveredVnode;
        }
        
        dldVnode->release();
    }
    
    return coveredVnode;
}

//--------------------------------------------------------------------

void
DldCoveringFsd::DldSetUbcFileSize(
    __in DldCoveredVnode*  vnodesDscr,
    __in off_t   size,
    __in bool    forceSetSize
    )
{
    bool  purgeCawlData = false;
    
    assert( preemption_enabled() );
    assert( vnodesDscr->coveringVnode );
    assert( vnodesDscr->dldCoveringVnode );
    
    if( !vnodesDscr->coveringVnode || !vnodesDscr->dldCoveringVnode )
        return;
    
    assert( DldIOVnode::kVnodeType_CoveringFSD == vnodesDscr->dldCoveringVnode->dldVnodeType );
    
    vnodesDscr->dldCoveringVnode->LockExclusive();
    { // start of the locked region
        
        if( ( vnodesDscr->dldCoveringVnode->fileDataSize < size ) || forceSetSize ){
            
            //
            // if the file is being truncated the CAWL related cached data
            // past the new size must be purgedas it will never be flushed
            // and so will keep the file marked as dirty ( having unflushed
            // CAWL data ) and prevent the FSD unmounting
            //
            purgeCawlData = ( vnodesDscr->dldCoveringVnode->fileDataSize > size );
            
            ubc_setsize( vnodesDscr->coveringVnode, size );
            vnodesDscr->dldCoveringVnode->fileDataSize = size;
            
        } // end if( dldVnode->fileDataSize < size )
        
    } // end of the locked region
    vnodesDscr->dldCoveringVnode->UnLockExclusive();
    
    if( purgeCawlData ){
        
        DldSparseFile*  sparseFile = NULL;
        
        assert( vnodesDscr->dldCoveringVnode->spinLock );
        IOSimpleLockLock( vnodesDscr->dldCoveringVnode->spinLock );
        { // start of the lock
            
            assert( !preemption_enabled() );
            
            sparseFile = vnodesDscr->dldCoveringVnode->sparseFile;
            if( NULL != sparseFile )
                sparseFile->retain();
            
        } // end of the lock
        IOSimpleLockUnlock( vnodesDscr->dldCoveringVnode->spinLock );
        
        if( sparseFile ){
            
            sparseFile->purgeFromOffset( size );
            sparseFile->release();
        }
        
    } // end if( purgeCawlData )
}

//--------------------------------------------------------------------

//
// BIG TO DO  -- REPLACE vnode_get to vnode_getwithref where appropriate in the code!
// 

int
DldCoveringFsd::DldReplaceVnodeByCovering(
    __inout vnode_t* vnodeToCover,
    __in    vnode_t dvp
    )
/*
 vnodeToCover's io reference must be bumped by the caller and released by vnode_put
 on covering vnode, normally this is done by the system
 */
{
    int          error = KERN_SUCCESS;
    /*
    DldIOVnode*  dldVnodeToSetSparseFile = NULL;
    bool         createSparseFile = false;
     */
    
    assert( preemption_enabled() );
    
    bool repeat;
    
    do{
        
        repeat = false;
        
        //this->LockExclusive();
        {// start of the lock
            
            DldIOVnode*  dldVnode;
            
            //
            // TO DO - remove from the lock!
            // TO DO if added at lookup or create the vnode doesn't have the name attached, the name 
            // and the parent are updated later by vnode_update_identity(), we only can be sure that when
            // a KAUTH callback is called a vnode has a valid name and parent
            //
            dldVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode( *vnodeToCover );
            assert( dldVnode && 0x1 == dldVnode->flags.vopHooked );
            if( dldVnode && 0x1 == dldVnode->flags.vopHooked ){
                
                bool waitForCoveringVnode;
                bool createCoveringVnode;
                    
                waitForCoveringVnode = false;
                createCoveringVnode = false;
                    
                IOSimpleLockLock( dldVnode->spinLock );
                { // start of the lock
                    
                    assert( !preemption_enabled() );
                    
                    if( 0x1 == dldVnode->flags.coveringVnodeIsBeingCreated ) {
                        
                        //
                        // a concurrent thread is creating a covering vnode or has already created one
                        //
                        waitForCoveringVnode = ( THREAD_WAITING == assert_wait( &dldVnode->coveringVnode, THREAD_UNINT ) );
                        
                    } else if( NULL == dldVnode->coveringVnode ){
                        
                        assert( 0x0 == dldVnode->flags.coveringVnodeIsBeingCreated );
                        dldVnode->flags.coveringVnodeIsBeingCreated = 0x1;
                        createCoveringVnode = true;
                    }
                    
                } // end of the lock
                IOSimpleLockUnlock( dldVnode->spinLock );
                
                if( waitForCoveringVnode ){
                    
                    thread_block( THREAD_CONTINUE_NULL );
                    
                    //
                    // there is a subtle moment here - the covering vnode reported by another thread
                    // might be in the reclaiming stage as the system doesn't know that we are going
                    // to use it here again, so we can get NULL or reclaimed vnode, so the vnode is
                    // being checked by vnode_getwithref below
                    //
                    assert( 0x0 == dldVnode->flags.coveringVnodeIsBeingCreated ); // might fail because of concurrent reclaim and recreation
                    assert( NULL != dldVnode->coveringVnode || 0x1 == dldVnode->flags.coveringVnodeReclaimed );
                }
                
                //
                // TO DO - SOLVE RACE CONDITIONS
                //  - multiple callers!
                //  - simultaneous lookup and close-inactive-reclaim!
                // ?? solved by the lock ???
                //
                if( createCoveringVnode ){
                    
                    assert( NULL == dldVnode->coveringVnode );
                    assert( 0x1 == dldVnode->flags.coveringVnodeIsBeingCreated );
                    
                    DldVnodCreateParams   createParams;
                    
                    bzero( &createParams, sizeof( createParams ) );
                    
                    createParams.dvp = dvp;
                    createParams.coveredVnode = *vnodeToCover;
                    
                    error = DldFsdHookCreateCoveringVnodeInternal( &createParams );
                    assert( KERN_SUCCESS == error );
                    if( KERN_SUCCESS == error ){
                        
                        assert( createParams.coveringVnode );
                        
                        //
                        // add the created vnode in the hash
                        //
                        DldIOVnode*  dldCoveringVnode;
                        
                        //
                        // TO DO - solve the name problem, the created DldIOVnode will have empty name, the covered
                        // DldIOVnode must be used to retrieve the name for log and etc
                        //
                        dldCoveringVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode(createParams.coveringVnode,
                                                                                                             DldIOVnode::kVnodeType_CoveringFSD );
                        assert( dldCoveringVnode && 0x1 == dldCoveringVnode->flags.vopHooked );
                        if( dldCoveringVnode ){
                            
                            //
                            // the covered and covering vnodes have been just created
                            //
                            assert( NULL == dldVnode->coveringVnode );

                            dldVnode->coveringVnode = createParams.coveringVnode;
                            
                            //
                            // get the covered file size and report it to UBC
                            //
                            vnode_attr va = { 0x0 };
                            VATTR_INIT( &va );
                            VATTR_WANTED( &va, va_data_size );
                            //
                            // TO DO - replace vfs_context_current()
                            // TO DO - remove from the lock
                            // TO DO - the call might be replaced with vnode_size()
                            //
                            if( 0x0 == vnode_getattr( *vnodeToCover, &va, vfs_context_current() ) ){
                                
                                dldCoveringVnode->fileDataSize = va.va_data_size;
                                ubc_setsize( dldCoveringVnode->vnode, dldCoveringVnode->fileDataSize );
                            }
                            
                            //
                            // Mark a vnode as having multiple hard links.
                            // This will cause the name cache to force a VNOP_LOOKUP on the vnode
                            // so that covering FSD can post-process the lookup.  Also, volfs will call
                            // VNOP_GETATTR2 to determine the parent, instead of using v_parent.
                            // So we will not allow the vnode cache to substitue the covered vnode
                            // instead the covering one.
                            //
                            vnode_setmultipath( *vnodeToCover );
                            
                            //
                            // increment the user reference count to retain the vnode
                            //
                            vnode_ref( *vnodeToCover );
                            
                            //
                            // decrement the iocount which if nonzero prevents fsd from unmounting
                            //
                            vnode_put( *vnodeToCover );
                            
                            //
                            // save the covered vnode before replacement
                            //
                            dldCoveringVnode->coveredVnode = *vnodeToCover;
                            
                            //
                            // copy some flags from the covered vnode
                            //
                            dldCoveringVnode->flags.controlledByCAWL = dldVnode->flags.controlledByCAWL;
                            
                            //
                            // replace the vnode
                            //
                            *vnodeToCover = createParams.coveringVnode;
                            
#if DBG
                            {
                                //
                                // check that the covering vnode is hooked so the covered FSD will not see it
                                //
                                DldVnodeHooksHashTable::sVnodeHooksHashTable->LockShared();
                                {// start of the lock
                                    assert( DldVnodeHooksHashTable::sVnodeHooksHashTable->RetrieveEntry( DldGetVnodeOpVector( dldCoveringVnode->vnode ), false ) );
                                }// end of the lock
                                DldVnodeHooksHashTable::sVnodeHooksHashTable->UnLockShared();
                            }
#endif//DBG
                            
                            dldCoveringVnode->release();
                            
                        } else {
                            
                            //
                            // delete the vnode
                            //
                            
                            // TO DO - as there is no corresponding DldIOVnode the vnode will be passed
                            // to the covered FSD's reclaim(), this will result in a crash
                            //
                            assert( !"this will crash the system" );
                            vnode_put( createParams.coveringVnode );
                            
                            error = ENOMEM;
                            
                        }// end if( dldCoveringVnode )
                        
                    }// end if( KERN_SUCCESS == error )
                    
                    IOSimpleLockLock( dldVnode->spinLock );
                    { // start of the lock
                        dldVnode->flags.coveringVnodeIsBeingCreated = 0x0;
                        dldVnode->flags.coveringVnodeReclaimed = 0x0;
                    } // end of the lock
                    IOSimpleLockUnlock( dldVnode->spinLock );
                    
                    thread_wakeup( &dldVnode->coveringVnode );
                    
                } else {
                    
                    errno_t  error;
                    
                    //
                    // the covered and covering vnode have been found,
                    // replace the vnode to already existing covering vnode
                    //
                    vnode_t   coveringVnodeWithIOCount = NULL;
                    
                    //
                    // vnode_getwithref can block so do a trick to
                    // have a valid vnode pointer after the lock
                    // is released - use vnode_put which doesn't block
                    //
                    
                    //
                    // synchronize with the reclaim
                    //
                    dldVnode->LockShared();
                    { // strat of the locked region
                        
                        if( dldVnode->coveringVnode )
                            error = vnode_get( dldVnode->coveringVnode );
                        else
                            error = ENOENT;
                        
                        if( !error )
                            coveringVnodeWithIOCount = dldVnode->coveringVnode;
                            
                    } // end of the locked region
                    dldVnode->UnLockShared();
                    
                    //
                    // increment an io count reference yet again with vnode_getwithref ( useless I believe )
                    //
                    if( coveringVnodeWithIOCount && KERN_SUCCESS == vnode_getwithref( coveringVnodeWithIOCount ) ){
                        
                        //
                        // the covering vnode holds a user reference to a covered vnode
                        //
                        assert( vnode_isinuse( *vnodeToCover, 0x0 ) );
                        
                        //
                        // release an io reference, we are holding a user reference
                        //
                        vnode_put( *vnodeToCover );
                        
                        //
                        // replace, the io count has been bumped
                        //
                        *vnodeToCover = coveringVnodeWithIOCount;
                        
                    } else {
                        
                        //
                        // vnode is being terminated, open a new one
                        //
                        repeat = true;
                    }
                    
                    //
                    // release the user IO count
                    //
                    if( coveringVnodeWithIOCount )
                        vnode_put( coveringVnodeWithIOCount );
                    
                }// else if( NULL == dldVnode->coveringVnode )
                
                dldVnode->release();
            }// if( dldVnode )
            
        }// end of the lock
        //this->UnLockExclusive();
        
    } while( repeat );
        
    return error;
}

//--------------------------------------------------------------------

// TO DO - this only for the test!
#define TEST_CAWL_DIR   "/work/cawl/"
#define TEST_CAWL_FILE_NAME   TEST_CAWL_DIR"XXXXXXXX_XXXXXXXX_XXXXXXXX_XXXXXXXX_AAAAAAAA"

errno_t
DldCoveringFsd::createCawlSparseFile( __in DldCoveredVnode*  coveredVnode, __in vfs_context_t  vfsContext )
{
    DldSparseFile*  sparseFile;
    errno_t         error = KERN_SUCCESS;
    DldIOVnode*     dldCoveringVnode = coveredVnode->dldCoveringVnode;
    
    assert( preemption_enabled() );
    assert( dldCoveringVnode );
    assert( vfsContext );
    
    if( NULL == dldCoveringVnode->sparseFile ){
        
        bool    wait = false;
        
        assert( preemption_enabled() );
                
        //
        // spin lock is used only for the sparse file creation synchronization
        // as it is impossible to use IORecursiveLock with assert_wait
        // because of possible reschedlung after or inside assert_wait
        // resulting in the retaining IORecursiveLock by a thread that is
        // not in the running queue and a possible deadlock
        //
        assert( dldCoveringVnode->spinLock );
        IOSimpleLockLock( dldCoveringVnode->spinLock );
        { // start of the lock
            
            //
            // we should call IOLockUnlock after removing
            // the thread from the running queue
            //
            assert( !preemption_enabled() );
            
            if( NULL == dldCoveringVnode->sparseFile && 0x1 == dldCoveringVnode->flags.sparseFileIsBeingCreated )
                wait = (THREAD_WAITING == assert_wait( &dldCoveringVnode->sparseFile, THREAD_UNINT ) );
            else if( NULL == dldCoveringVnode->sparseFile )
                dldCoveringVnode->flags.sparseFileIsBeingCreated = 0x1;
            
        } // end of the lock
        IOSimpleLockUnlock( dldCoveringVnode->spinLock );
        
        if( wait )
            thread_block( THREAD_CONTINUE_NULL );
        
    } // end if( NULL == dldCoveringVnode->sparseFile )
    
    if( NULL != dldCoveringVnode->sparseFile )
        return KERN_SUCCESS;
    
    //
    // do not be adamant and tenacious in file creation
    //
    if( 0x1 == dldCoveringVnode->flags.sparseFileCreationHasFailed )
        return ENOMEM;
    
    //
    // get the unique ID for the data stream,
    // if the file is of the zero size then 
    // write one bit of data just to allocate
    // the disk space for the filr as some
    // FSD generate unique ID using the file
    // disk map, if there is no map ( a case
    // of zero data file ) the returned value
    // is not unique
    //
    vnode_attr va = { 0x0 };
    
    do{
        
        VATTR_INIT( &va );
        VATTR_WANTED( &va, va_data_size );
        VATTR_WANTED( &va, va_fileid );
        VATTR_WANTED( &va, va_fsid );
        
        error = vnode_getwithref( coveredVnode->coveredVnode );
        assert( KERN_SUCCESS == error );
        if( KERN_SUCCESS == error ){
            
            error = vnode_getattr( coveredVnode->coveredVnode, &va, vfsContext );
            if( KERN_SUCCESS == error && 0x0 == va.va_data_size ){
                
                //
                // write one bit of data just to allocate
                // the disk space for the filr as some
                // FSD generate unique ID using the file
                // disk map, if there is no map ( a case
                // of zero data file ) the returned value
                // is not unique
                //
                error = DldVnodeSetsize( coveredVnode->coveredVnode, 0x1, 0x0, vfsContext);
                assert( KERN_SUCCESS == error );
                if( KERN_SUCCESS != error ){
                    
                    DBG_PRINT_ERROR(("DldVnodeSetsize() failed with an error(%u)\n", error));
                }
                
            } else {
                
                DBG_PRINT_ERROR(("vnode_getattr() failed with an error(%u)\n", error));
            }
            
            vnode_put( coveredVnode->coveredVnode );
            
            assert( KERN_SUCCESS == error );
            if( KERN_SUCCESS != error ){
                
                DBG_PRINT_ERROR(("vnode_getattr() failed with an error(%u)\n", error));
                return error;
            }
            
        } else {
            
            //
            // the vnode is being reclaimed
            //
            return error;
        }
        
    } while( 0x0 == va.va_data_size );
    
    //
    // get the name hash
    //
    OSString* nameStrRef = dldCoveringVnode->getNameRef();
    assert( nameStrRef );
    if( !nameStrRef )
        return ENOMEM;
    
    MD5_CTX     mdContext;
    bool        addInHashTable = false;
    bool        usersCountIsIncremented = false;
    
    DldMD5Init( &mdContext );
    //DldMD5Update ( &mdContext, (unsigned char*)nameStrRef->getCStringNoCopy(), nameStrRef->getLength() );
    DldMD5Update( &mdContext, (unsigned char*)&va.va_fileid, sizeof(va.va_fileid) );
    DldMD5Update( &mdContext, (unsigned char*)&va.va_fsid, sizeof(va.va_fsid) );
    DldMD5Final( &mdContext );
    
    assert( 0x1 == dldCoveringVnode->flags.sparseFileIsBeingCreated );
    
    //
    // try to retrieve from the hash
    //
    DldSparseFilesHashTable::sSparseFilesHashTable->LockShared();
    {
        sparseFile = DldSparseFilesHashTable::sSparseFilesHashTable->RetrieveEntry( mdContext.digest, true );
    }
    DldSparseFilesHashTable::sSparseFilesHashTable->UnLockShared();
    
    //
    // account for a new possible sparse file user,
    // this protects from the race when a covering vnode is being reclaimed
    // and another vnode for the same data stream is being created
    //
    if( sparseFile ){
        
        usersCountIsIncremented = sparseFile->incrementUsersCount();
        if( !usersCountIsIncremented ){
            
            //
            // the sparse file is being removed and has passed the stage from which it can't be resurected
            //
            sparseFile->release();
            sparseFile = NULL;
        }
    } // end if( sparseFile )
    
    if( !sparseFile ){
        
        char    name[ sizeof(TEST_CAWL_FILE_NAME) ];
        
        assert( !usersCountIsIncremented );
        
        //
        // create a new
        //
        snprintf( name, sizeof( name ), "%s%08x_%08x_%08x_%08x", TEST_CAWL_DIR, 
                 *(int*)&mdContext.digest[0],*(int *)&mdContext.digest[4],*(int *)&mdContext.digest[8],*(int *)&mdContext.digest[12]);
        
        //
        // create the sparse file
        //
        sparseFile = DldSparseFile::withPath( (const char*)name,
                                              coveredVnode->coveringVnode,
                                              mdContext.digest,
                                              nameStrRef->getCStringNoCopy() );
        assert( sparseFile );
        if( sparseFile ){
            
            addInHashTable = true;
            usersCountIsIncremented = true; // the object is created with a 0x1 reference count
        } // if( sparseFile )
        
    } // end if( !sparseFile )
    
    if( sparseFile ){
        
        bool deleteSparseFile = false;
        
        //
        // mark the mount as controlled by the CAWL, do it before setting the dldCoveringVnode->sparseFile value
        //
        assert( NULL != vnode_mount( dldCoveringVnode->coveringVnode ) );
        assert( vnode_mount( dldCoveringVnode->coveringVnode ) == vnode_mount( dldCoveringVnode->coveredVnode ) );
        
        DldVfsMntHook*  mnt = DldVfsMntHook::withVfsMnt( vnode_mount( dldCoveringVnode->coveringVnode ) );
        assert( mnt );
        if( mnt ){
            
            mnt->setAsControlledByCAWL();
            mnt->release();
        }
        
        sparseFile->exchangeCawlRelatedVnode( dldCoveringVnode->vnode );
        
        assert( dldCoveringVnode->spinLock );
        IOSimpleLockLock( dldCoveringVnode->spinLock );
        { // start of the lock
            
            assert( !preemption_enabled() );
            
            if( NULL == dldCoveringVnode->sparseFile ){
                
                dldCoveringVnode->sparseFile = sparseFile;
                
                //
                // take a reference for the sparse file hash
                //
                if( addInHashTable )
                    sparseFile->retain();
                
            } else {
                
                deleteSparseFile = true;
            }
            
        } // end of the lock
        IOSimpleLockUnlock( dldCoveringVnode->spinLock );
        
        if( deleteSparseFile ){
            
            assert( sparseFile != dldCoveringVnode->sparseFile );
            
            if( usersCountIsIncremented ){
                
                sparseFile->decrementUsersCount();
                usersCountIsIncremented = false;
            } // end if( usersCountIsIncremented )
            
            sparseFile->release();
            
        } else if( addInHashTable ){
            
            //
            // insert in the sparse file hash, as here we are guaranteed that
            // there is a single instance of the sparse file for the key
            //
            assert( DldSparseFilesHashTable::sSparseFilesHashTable );
            DldSparseFilesHashTable::sSparseFilesHashTable->LockExclusive();
            { // strat of the lock
                
                bool added;
                
                assert( sizeof( mdContext.digest ) == 16 );
                assert( NULL == DldSparseFilesHashTable::sSparseFilesHashTable->RetrieveEntry( mdContext.digest, false ) );
                
                added = DldSparseFilesHashTable::sSparseFilesHashTable->AddEntry( mdContext.digest, sparseFile );
                assert( added );
                if( !added ){
                    
                    DBG_PRINT_ERROR(( "DldSparseFilesHashTable::sSparseFilesHashTable->AddEntry() failed for %s\n", nameStrRef->getCStringNoCopy() ));
                }
                
            } // end of the lock
            DldSparseFilesHashTable::sSparseFilesHashTable->UnLockExclusive();
            
            //
            // the object was referenced while dldCoveringVnode->spinLock was held
            //
            sparseFile->release();
        }
        
        sparseFile = NULL;
        
    } else {
        
        IOSimpleLockLock( dldCoveringVnode->spinLock );
        { // start of the lock
            dldCoveringVnode->flags.sparseFileCreationHasFailed = 0x1;
        } // end of the lock
        IOSimpleLockUnlock( dldCoveringVnode->spinLock );
        
        error = ENOMEM;
        
    }// end if( sparseFile )
    
    //
    // we finished
    //
    IOSimpleLockLock( dldCoveringVnode->spinLock );
    { // start of the lock
        dldCoveringVnode->flags.sparseFileIsBeingCreated = 0x0;
    } // end of the lock
    IOSimpleLockUnlock( dldCoveringVnode->spinLock );
    
    nameStrRef->release();
    DLD_DBG_MAKE_POINTER_INVALID( nameStrRef );
    
    //
    // wake up waiters( if any ), the waiter should check
    // for NULL sparseFile value after being woken up
    //
    thread_wakeup( &dldCoveringVnode->sparseFile );
    
    return error;
}

//--------------------------------------------------------------------

int
DldCoveringFsd::DldReclaimCoveringVnode(
    __in vnode_t  coveringVnode
    )
{
    //
    // free the covered vnode ( it seems the following  call to RemoveIOVnodeByBSDVnode
    // is also doing this if the DldIOVnode's reference count drops to zero
    //
    
    assert( preemption_enabled() );
    assert( this->DldIsCoveringVnode( coveringVnode ) );
    
    //
    // free the covered vnode ( it seems the following  call to RemoveIOVnodeByBSDVnode
    // is also doing this if the DldIOVnode's reference count drops to zero
    //
    
    vnode_t         coveredVnode = NULL;
    DldSparseFile*  sparseFile = NULL;
    DldIOVnode*     dldVnode;
    
    dldVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( coveringVnode );
    assert( dldVnode );
    if( dldVnode ){
        
        assert( DldIOVnode::kVnodeType_CoveringFSD == dldVnode->dldVnodeType );
        
        if( dldVnode->coveredVnode ){
            
            coveredVnode = dldVnode->coveredVnode;
            
            dldVnode->coveredVnode = NULL;
            
            //
            // remeber - this is a debug flag! do not synchronize on it, do not make any decisions using it!
            //
            dldVnode->flags.coveringVnodeReclaimed = 0x1;
            
            //
            // we are the sole reference holder ( might fail if the vnode had been opened before the driver was loaded )
            //
            // assert( !vnode_isinuse( coveredVnode, 0x1 ) );
            
            DldIOVnode*  dldCoveredVnode;
            
            dldCoveredVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( coveredVnode );
            if( dldCoveredVnode ){
                
                assert( DldIOVnode::kVnodeType_Native == dldCoveredVnode->dldVnodeType );
                
                //
                // synchronize with the create and lookup
                //
                dldCoveredVnode->LockExclusive();
                {
                    dldCoveredVnode->coveringVnode = NULL;
                }
                dldCoveredVnode->UnLockExclusive();
                
                dldCoveredVnode->release();
            }
            
        }// end if( dldVnode->coveredVnode )
        
        assert( dldVnode->spinLock );
        IOSimpleLockLock( dldVnode->spinLock );
        {
            if( dldVnode->sparseFile ){
                
                sparseFile = dldVnode->sparseFile;
                dldVnode->sparseFile = NULL;
                
            } // end if( dldVnode->sparseFile )
        }
        IOSimpleLockUnlock( dldVnode->spinLock );
        
        dldVnode->release();
        
    }// end if( dldVnode )
    
    
    //
    // remove the corresponding DldIOVnode
    //
    DldVnodeHashTable::sVnodesHashTable->RemoveIOVnodeByBSDVnode( coveringVnode );
    
    //
    // release the covered vnode, do this outside the lock (  TO DO - might be in a separate thread ?? )
    // to avoid deadlock like in the following stack where a covered vnode was release under the lock
    // and the HFS waited for another operation completion which was blocked on the lock
    //
    /*
     #0  machine_switch_context (old=0x5b15010, continuation=0, new=0x5ae53f4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/pcb.c:843
     #1  0x0023cb50 in thread_invoke (self=0x5b15010, thread=0x5ae53f4, reason=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/sched_prim.c:1618
     #2  0x0023ce19 in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/sched_prim.c:1853
     #3  0x0023cea7 in thread_block (continuation=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/sched_prim.c:1870
     #4  0x002cef2b in lck_rw_lock_exclusive_gen (lck=0x5bf6d90) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/locks_i386.c:1094
     #5  0x00471f36 in hfs_lock (cp=0x5bf6d90, locktype=HFS_EXCLUSIVE_LOCK) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/hfs/hfs_cnode.c:1063
     #6  0x0048efce in hfs_systemfile_lock (hfsmp=0x5b58014, flags=1, locktype=HFS_EXCLUSIVE_LOCK) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/hfs/hfs_vfsutils.c:901
     #7  0x00494be2 in hfs_update (vp=0x6a8fd84, waitfor=1) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/hfs/hfs_vnops.c:4082
     #8  0x00480e26 in do_hfs_truncate (vp=0x6a8fd84, length=0, flags=<value temporarily unavailable, due to optimizations>, skipupdate=0, context=0x5ca2f14) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/hfs/hfs_readwrite.c:2811
     #9  0x004810be in hfs_truncate (vp=0x6a8fd84, length=0, flags=16, skipsetsize=1, skipupdate=0, context=0x5ca2f14) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/hfs/hfs_readwrite.c:2918
     #10 0x00472b07 in hfs_vnop_inactive (ap=0x3218bae0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/hfs/hfs_cnode.c:188
     #11 0x46eadd59 in DldFsdInactiveHook (ap=0x3218bae0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldVNodeHook.cpp:453
     #12 0x0032df2b in VNOP_INACTIVE (vp=0x6a8fd84, ctx=0x5ca2f14) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:5082
     #13 0x00310862 in vnode_lock_spin [inlined] () at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:1729
     #14 0x00310862 in vnode_rele_internal (vp=0x6a8fd84, fmode=0, dont_reenter=0, locked=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:1731
     #15 0x0031094e in vnode_rele (vp=0x6a8fd84) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:1648
     #16 0x46f3bf30 in DldCoveringFsd::DldReclaimCoveringVnode (this=0x5f26790, coveringVnode=0x7f277c0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldCoveringVnode.cpp:522
     #17 0x46ea3e7d in DldFsdReclaimHook (ap=0x3218bc14) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldVNodeHook.cpp:532
     #18 0x00329f8d in VNOP_RECLAIM (vp=0x7f277c0, ctx=0x5ca2f14) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:5141
     #19 0x0030ffa1 in vclean (vp=0x7f277c0, flags=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:2067
     #20 0x00310100 in vgone [inlined] () at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:2203
     #21 0x00310100 in vnode_reclaim_internal (vp=0x7f277c0, locked=1, reuse=1, flags=8) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:4026
     #22 0x003103c1 in vnode_put_locked (vp=0x7f277c0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:3811
     #23 0x003103f7 in vnode_put (vp=0x7f277c0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:3766
     #24 0x0031d441 in unlink1 (ctx=0x5ca2f14, ndp=0x3218bdcc, nodelbusy=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:3740
     #25 0x0031d539 in unlink (p=0x60e2820, uap=0x56eb2b8, retval=0x5ca2e54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:3754
     #26 0x0054e9fd in unix_syscall64 (state=0x56eb2b4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365
     */
    
    if( coveredVnode ){
        
        //
        // allow the node to be reusesd immediately by calling vnode_recycle
        // which triggers 
        // vnode_reclaim_internal()->vgone()->vclean()->VNOP_RECLAIM()
        // when iocounts and userio count drop to zero,
        // the reclaim hook removes vnode from the hash table so there
        // is no need to do this here
        //
        vnode_recycle( coveredVnode );
        
        //
        // decrement the user IO count , the vnode might be removed by this call
        // resulting in a recursive FSD entering!
        // TO DO - remove from the lock!
        //
        vnode_rele( coveredVnode );
        
    } // end if( coveredVnode )
    
    
    if( sparseFile ){
        
        sparseFile->prepareForReclaim();
        sparseFile->release();
    }
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

void
DldCoveringFsd::releaseIOArgs( __in DldCfsdIOArgs* args)
{
    if( args->memDscArray ){
        
        args->memDscArray->flushCollection();
        args->memDscArray->release();
        args->memDscArray = NULL;
    }
    
    if( args->rangeDscArray ){
        
        args->rangeDscArray->flushCollection();
        args->rangeDscArray->release();
        args->rangeDscArray = NULL;
    }
    
    if( args->upl ){
        
        //
        // destroy the range, the pages will be moved to the zeroed or
        // inactive list!
        //
        ubc_upl_abort( args->upl, UPL_ABORT_FREE_ON_EMPTY );
        args->upl = NULL;
        
    }
    
    if( args->contextRef )
        vfs_context_rele( args->contextRef );
}

//--------------------------------------------------------------------

kern_return_t
DldCoveringFsd::rwData(
    __in DldCoveredVnode*  coveredVnode,
    __in DldCfsdIOArgs* args // a caller releases all associated resources
    )
{
    
    IOReturn RC = KERN_SUCCESS;
    
    assert( preemption_enabled() );
    
    assert( coveredVnode->coveringVnode && coveredVnode->coveredVnode );
    assert( coveredVnode->dldCoveringVnode && coveredVnode->dldCoveredVnode );
    
    assert( args->memDscArray && args->rangeDscArray &&
            args->memDscArray->getCount() == args->rangeDscArray->getCount() );
    
    assert( args->flags.write != args->flags.read );
    
#if defined( DBG )
    assert( DLD_CFSD_WRVN_SIGNATURE == args->signature );
#endif//DBG
    
    bool prepareForIO = false;
    
    if( NULL == coveredVnode->dldCoveringVnode->sparseFile ){
        
        //
        // create a sparse file to support IO
        //
        RC = this->createCawlSparseFile( coveredVnode, ( args->contextRef ? args->contextRef : vfs_context_current() ) );
        assert( KERN_SUCCESS == RC );
        if( KERN_SUCCESS != RC )
            return RC;
    }
    
    DldSparseFile* sparseFile;
    sparseFile = coveredVnode->dldCoveringVnode->getSparseFileRef();
    
    if( 0x1 == args->flags.pagingIO ){
        
        //
        // the physical memory should be "prepared" by wiring it
        // as there is no target task to provide vm objects
        //
        prepareForIO = true;
    }
    
    bool        writeIO = ( 0x1 == args->flags.write );
    SInt64      timeStamp = OSIncrementAtomic64( &gCawlWriteTimeStampSeed );
    
    unsigned int count = args->memDscArray->getCount();
    
    for( unsigned int i = 0; i < count; ++i ){
        
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
            // wire out or read in the physical pages
            //
            RC = memDscr->prepare( writeIO ? kIODirectionOut : kIODirectionIn );
            assert( kIOReturnSuccess == RC );
            if( kIOReturnSuccess != RC ){
                
                DBG_PRINT_ERROR(("memDscr->prepare( kIODirectionOut )failed with the 0x%X code\n", RC));
                break;
            }
            
            prepared = true;
            
        } // end if( prepareForIO )
        
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
            off_t             fileOffset;
            
            fileOffset = (off_t)(rangeDsc.fileOffset + (rangeDsc.rangeSize - residual));
            
            //
            // map up to the mapLimit as the kernel
            // space might be severely fragmented,
            // in case of a upl the maximum size
            // of he provided buffer is 1MB so the
            // limits are not an issue
            //
#if defined( DBG )
            mapLimit = 0x1000*0x1000;// 16 MB
#else
            mapLimit = 0x2*0x1000*0x1000;// 32 MB this is in correspondence with the MAX_UPL_SIZE = 8192*4096
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
            // map user data in the kernel space
            //
            IOOptionBits  options = kIOMapAnywhere;
            if( writeIO )
                options |= kIOMapReadOnly; // only read access is required as the data is written FROM the memory
                
            IOMemoryMap* map = memDscr->createMappingInTask( kernel_task, // we are mapping into the kernel task space!
                                                             0x0,            // mach_vm_address_t  atAddress
                                                             options,        // IOOptionBits		options
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
                                                    options,        // IOOptionBits		options
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
            
            assert( map->getLength() == lengthToMap );
            
            //
            // this should not fail
            //
            IOVirtualAddress mappedAddr = map->getVirtualAddress();
            assert( mappedAddr );
            if( (IOVirtualAddress)NULL == mappedAddr ){
                
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
            assert( mappedLength <= args->residual );
            
            //
            // read or write using data in sparse file
            //
            assert( coveredVnode->dldCoveringVnode );
            
            assert( sparseFile );
            if( sparseFile ){
                
                //
                // use the sparse file to store and retrieve data
                //
                DldSparseFile::IoBlockDescriptor*  dscrs = NULL;
                unsigned int                       dscrsCount;                     
                
                dscrsCount = ( DldSparseFile::alignToBlock( fileOffset + mappedLength + (DldSparseFile::BlockSize - 0x1)) -
                               DldSparseFile::alignToBlock( fileOffset ) ) / DldSparseFile::BlockSize;
                
                dscrs = (DldSparseFile::IoBlockDescriptor*)IOMalloc( dscrsCount * sizeof(*dscrs) );
                assert( dscrs );
                if( !dscrs ){
                    
                    DBG_PRINT_ERROR(("dscrs = IOMalloc() failed\n"));
                    RC = kIOReturnNoMemory;
                    goto __exit_sparse;
                }
                
                bzero( dscrs, dscrsCount * sizeof(*dscrs) );
                
                for( int i = 0x0; i < dscrsCount; ++i ){
                    
                    dscrs[i].flags.write = writeIO ? 0x1 : 0x0;
                    dscrs[i].tag.timeStamp = timeStamp;
                    
                    //
                    // set the offset
                    //
                    if( 0x0 == i ){
                        
                        //
                        // the first entry
                        //
                        dscrs[i].fileOffset = fileOffset;
                        
                    } else {
                        
                        dscrs[i].fileOffset = DldSparseFile::alignToBlock( fileOffset + i*DldSparseFile::BlockSize );
                    }

                    //
                    // set the size
                    //
                    if( i < (dscrsCount - 0x1) ){
                        
                        //
                        // not the last entry
                        //
                        off_t  nextBlock = DldSparseFile::alignToBlock(dscrs[i].fileOffset + DldSparseFile::BlockSize);
                        
                        dscrs[i].size = (unsigned int)(nextBlock - dscrs[i].fileOffset);
                        assert( !( 0x0 != i && DldSparseFile::BlockSize != dscrs[i].size ) );
                        
                    } else {
                        
                        //
                        // the last entry ( or the first if there is only one )
                        //
                        off_t  accumulatedOffset = dscrs[i].fileOffset - dscrs[0].fileOffset;
                        
                        assert( mappedLength > accumulatedOffset );
                        
                        dscrs[i].size = (unsigned int)( mappedLength - accumulatedOffset );
                    }
                    
                    //
                    // set the buffer
                    //
                    dscrs[i].buffer = (char*)mappedAddr + (dscrs[i].fileOffset - dscrs[0].fileOffset);
                    
                } // end for( int i = 0x0; i < dscrsCount; ++i )
                
                assert( (( dscrs[dscrsCount-1].fileOffset + dscrs[dscrsCount-1].size ) - dscrs[0].fileOffset ) == mappedLength );
                
                //
                // do a sparse file IO
                //
                sparseFile->performIO( dscrs, dscrsCount );
                
                //
                // check the result
                //
                for( int i = 0x0; i < dscrsCount; ++i ){
                    
                    if( KERN_SUCCESS == dscrs[i].error ){
                        
                        assert( args->residual >= dscrs[i].size );
                        args->residual -= dscrs[i].size;
                        
                        //
                        // invalidate the entry
                        //
                        dscrs[i].fileOffset = DldSparseFile::InvalidOffset;
                        dscrs[i].size = 0x0;
                        
                        assert( KERN_SUCCESS == RC );
                        continue;
                    }
                    
                    //
                    // there is an error, define its nature
                    //
                    
                    if( 0x0 == dscrs[i].flags.blockIsNotPresent ){
                        
                        //
                        // an unrecoverable error,
                        // IO fails and this is not because a valid data block was not found
                        //
                        RC = dscrs[i].error;
                        break;
                    }
                        
                    //
                    // try to remove the error condition
                    //
                    
                    if( 0x1 == dscrs[i].flags.write && 0x0 != i && (dscrsCount-1) != i){
                        
                        assert( 0x0 == i || (dscrsCount-1) == i );
                        
                        //
                        // this should have happen with the first or last descriptors as they
                        // might not describe full block but not with the internal range descriptor
                        //
                        
                        //
                        // an unrecoverable error on write
                        //
                        RC = dscrs[i].error;
                        break;
                        
                    } // if( 0x1 == dscrs[i].flags.write )
        
                    
                    //
                    // try to recover from the error
                    // cases
                    //  1 write on an unaligned offset ( solution: read aligned block, write in it, write to sparse file )
                    //  2 write on aligned offset but less that a block size ( solution: the same as for 1 )
                    //  3 read from an unaligned offset ( solution: read data from the covered vnode )
                    //  4 read from aligned offset but less than a block size ( solution: the same as for 3 )
                    //  5 read from aligned offset and the size is the size of block ( solution: the same as for 3 )
                    //
                    
                    void*   bufferForRead = NULL;
                    off_t   offsetForRead;
                    int     lengthForRead;
                    
                    if( 0x1 == dscrs[i].flags.write ){
                        
                        //
                        // write, the cases 1 and 2
                        // allocate an intermediate buffer to fill it with the data and write to the sparse file
                        //
                        
                        offsetForRead = DldSparseFile::alignToBlock( dscrs[i].fileOffset );
                        lengthForRead = DldSparseFile::BlockSize;
                        
                        bufferForRead = IOMalloc( lengthForRead );
                        assert( bufferForRead );
                        if( !bufferForRead ){
                            
                            DBG_PRINT_ERROR(("bufferForRead = IOMalloc( DldSparseFile::BlockSize ) failed\n"));
                            
                            RC = ENOMEM;
                            break;
                        }
                        
                    } else {
                        
                        //
                        // read, the cases 3, 4 and 5
                        //
                        lengthForRead = dscrs[i].size;
                        offsetForRead = dscrs[i].fileOffset;
                        bufferForRead = dscrs[i].buffer;
                    }
                    
                    //
                    // read in the data
                    //
                    int bytesLeft;
                    int ioflag = IO_NOAUTH | IO_SYNC; // | args->ioflag;
                    int bytesProcessed = 0x0;
                    
                    RC = vn_rdwr( UIO_READ,
                                  coveredVnode->coveredVnode,
                                  (char*)bufferForRead,
                                  lengthForRead,
                                  offsetForRead,
                                  UIO_SYSSPACE,
                                  ioflag,
                                  args->contextRef ? vfs_context_ucred(args->contextRef) : kauth_cred_get(),
                                  &bytesLeft,
                                  args->contextRef ? vfs_context_proc(args->contextRef) : current_proc() );
                    if( KERN_SUCCESS == RC && bytesLeft != lengthForRead ){
                        
                        assert( bytesLeft < lengthForRead );
                        
                        //
                        // zero the left bytes
                        //
                        if( 0x0 != bytesLeft )
                            bzero( (char*)bufferForRead + (lengthForRead - bytesLeft), bytesLeft );
                        
                        if( 0x1 == dscrs[i].flags.write ){
                            
                            //
                            // write the new data in the buffer,
                            // two cases
                            //  - write from an unaligned offset
                            //  - write from an aligned offset less than a block size
                            //
                            if( dscrs[i].fileOffset != offsetForRead ){
                                
                                off_t  delta;
                                
                                delta = dscrs[i].fileOffset - offsetForRead;
                                
                                assert( dscrs[i].fileOffset > offsetForRead );
                                assert( (delta + dscrs[i].size) <= lengthForRead );
                                
                                memcpy( (char*)bufferForRead + delta, dscrs[i].buffer, dscrs[i].size );
                                
                            } else {
                                
                                memcpy( (char*)bufferForRead, dscrs[i].buffer, dscrs[i].size );
                            }
                            
                            //
                            // write the buffer in the sparse file
                            //
                            DldSparseFile::IoBlockDescriptor  writeDscr;
                            
                            bzero( &writeDscr, sizeof( writeDscr ) );
                            
                            writeDscr.fileOffset    = offsetForRead;
                            writeDscr.tag.timeStamp = timeStamp;
                            writeDscr.buffer        = bufferForRead;
                            writeDscr.size          = lengthForRead;
                            writeDscr.flags.write   = 0x1;
                            
                            sparseFile->performIO( &writeDscr, 0x1 );
                            
                            RC = writeDscr.error;
                            
                            bytesProcessed = dscrs[i].size;
                            
                        } else {
                            
                            //
                            // this is a read from the file
                            //
                            bytesProcessed = lengthForRead - bytesLeft;
                            
                        }// end if if( 0x1 == dscrs[i].flags.write )
                        
                    } else if( KERN_SUCCESS == RC && bytesLeft == lengthForRead  ){
                        
                        //
                        // reading or writing beyound the end of the file
                        //
                        bytesProcessed = 0x0;
                        
                    } else {
                        
                        assert( KERN_SUCCESS != RC );
                        
                        //
                        // an unrecovarable error,
                        // do not break here as the bufferForRead
                        // might require to be freed
                        //
                    }
                    
                    if( bufferForRead != dscrs[i].buffer ){
                        
                        assert( lengthForRead == DldSparseFile::BlockSize );
                        IOFree( bufferForRead, lengthForRead );
                        bufferForRead = NULL;
                    }
                    
                    if( KERN_SUCCESS != RC ){
                        
                        //
                        // an unrecoverable error
                        //
                        break;
                    }
                    
                    assert( args->residual >= dscrs[i].size );
                    args->residual -= bytesProcessed;
                    
                    //
                    // invalidate the entry
                    //
                    dscrs[i].fileOffset = DldSparseFile::InvalidOffset;
                    dscrs[i].size = 0x0;
                    
                } // end for( int i = 0x0; i < dscrsCount; ++i )
                
            __exit_sparse:
                
                if( dscrs )
                    IOFree( dscrs, dscrsCount * sizeof(*dscrs) );
                
            } else {
                
                //
                // there is no sparse file,
                // read or write directly from/to the covered file
                //
                
                int bytesLeft;
                int ioflag = IO_NOAUTH | IO_SYNC | args->ioflag;
                
                //
                // write or read data ( addr, length )
                //
                RC = vn_rdwr( writeIO ? UIO_WRITE : UIO_READ,
                              coveredVnode->coveredVnode,
                              (char*)mappedAddr, // map->getVirtualAddress(),
                              mappedLength, // map->getLength(),
                              fileOffset,
                              UIO_SYSSPACE,
                              ioflag,
                              kauth_cred_get(),
                              &bytesLeft,
                              current_proc() );
                
                //assert( KERN_SUCCESS == RC );
                if( KERN_SUCCESS == RC ){
                    
                    assert( mappedLength >= bytesLeft );
                    args->residual -= (mappedLength - bytesLeft);
                }
                
            } // end for else for if( sparseFile )
            
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
            // unwire the physical pages
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
        // if the write was successful then notify the service,
        // currently the only client is the CAWL subsystem so
        // the assertion for the CAWL
        //
        assert( coveredVnode->dldCoveringVnode && coveredVnode->dldCoveringVnode->isControlledByServiceCAWL() );
        if( writeIO && coveredVnode->dldCoveringVnode && coveredVnode->dldCoveringVnode->isControlledByServiceCAWL() ){
            
            IOReturn   cawlError;
            DldDiskCAWL::NotificationData  notificationData;
            
            notificationData.write.timeStamp = timeStamp;
            
            //
            // (offset+size) might be beyond the end of the file
            // but the CAWL subsystem normally rechecks the file size
            //
            notificationData.write.offset    = rangeDsc.fileOffset;
            notificationData.write.size      = rangeDsc.rangeSize;
            
            memcpy( notificationData.write.backingSparseFileID,
                    sparseFile->sparseFileID(),
                    sizeof( notificationData.write.backingSparseFileID ) );
            
            //
            // notify the service
            //
            assert( gDiskCAWL );
            cawlError = gDiskCAWL->diskCawlNotification( coveredVnode->dldCoveringVnode,
                                                  kDldCawlOpFileWrite,
                                                  NULL,
                                                  &notificationData );
            
            assert( KERN_SUCCESS == cawlError );
            if( KERN_SUCCESS == cawlError ){
                
                DBG_PRINT_ERROR(( "diskCawlNotification() failed with an error(%u)\n", cawlError ));
            }
            
            if( !gSecuritySettings.securitySettings.disableAccessOnCawlError ){
                
                //
                // the CAWL errors must be suppressed in this case
                //
                cawlError = KERN_SUCCESS;
                
            } else {
                
                //
                // this might render the previous error, but it is pkay, we just need to know that there is any error
                //
                RC = cawlError;
            }
            
            //
            // a test flush test, this is a test which takes a long path to test all data consistency
            //
            if( gGlobalSettings.flushCAWL ){
                
                vnode_t         coveringVnodeRef;
                DldIOVnode*     dldCoveringVnode;
                
                //
                // check that the sparse file related vnode can be found by sparse file ID
                // and check the whole chain of references
                //
                coveringVnodeRef = DldSparseFile::getCoveringVnodeRefBySparseFileID( sparseFile->sparseFileID() );
                assert( coveringVnodeRef == coveredVnode->coveringVnode ); // we inside a write VNOP, so vnode must be found

                dldCoveringVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( coveringVnodeRef );
                assert( dldCoveringVnode );
                if( dldCoveringVnode ){
                    
                    DldSparseFile*  sparseFileRef;
                    
                    sparseFileRef = dldCoveringVnode->getSparseFileRef();
                    assert( sparseFile == sparseFileRef );
                    if( sparseFileRef ){
                        
                        DldIOVnode* dldCoveredVnode;
                        
                        dldCoveredVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( dldCoveringVnode->coveredVnode );
                        assert( dldCoveredVnode == coveredVnode->dldCoveredVnode );
                        if( dldCoveredVnode ){
                            
                            vnode_t     coveredVnodeRef;
                            
                            coveredVnodeRef = dldCoveredVnode->getReferencedVnode();
                            assert( coveredVnodeRef == coveredVnode->coveredVnode );
                            if( coveredVnodeRef ){
                                
                                errno_t  error;
                                
                                //
                                // dldCoveringVnode->fileDataSize has not yet been updated here as it is updated after IO completion,
                                // so rangeDsc is used
                                //
                                error = sparseFileRef->flushUpToTimeStamp( timeStamp,
                                                                           coveredVnodeRef,
                                                                           rangeDsc.fileOffset + rangeDsc.rangeSize,
                                                                           vfs_context_current() );
                                assert( !error );
                                
                                vnode_put( coveredVnodeRef );
                            } // end if( coveredVnodeRef )
                            
                            dldCoveredVnode->release();
                        } // end if( dldCoveredVnode )
                        
                        sparseFileRef->release();
                    } // end if( sparseFileRef )
                    
                    dldCoveringVnode->release();
                } // end if( dldCoveringVnode )             
                
                if( coveringVnodeRef )
                    vnode_put( coveringVnodeRef );
              
                //
                // we should flush all and we suppose that there is no concurrent writers
                //
                assert( DldSparseFile::InvalidOffset == sparseFile->getOldestWrittenBlockOffset() );
                
            } // end if( gGlobalSettings.flushCAWL ) of the flush test

        } // end if( writeIO && coveredVnode->dldCoveringVnode ... )
        
        //
        // if there was an error and the request must not be failed then
        // flush all data and repeat the request using the covered file,
        // if the sparse file is present then this is a notification or sparse file related error
        //
        if( KERN_SUCCESS != RC &&
            coveredVnode->dldCoveringVnode && coveredVnode->dldCoveringVnode->isControlledByServiceCAWL() &&
            !gSecuritySettings.securitySettings.disableAccessOnCawlError &&
            sparseFile ){
            
            assert( !"CAWL is being bypassed" );
            DBG_PRINT_ERROR(("CAWL is being bypassed\n"));
            
            //
            // flush the backing sparse file, we do not check for errors here
            //
            sparseFile->flushUpToTimeStamp( timeStamp,
                                            coveredVnode->coveredVnode,
                                            coveredVnode->dldCoveringVnode->fileDataSize, // flush the whole file
                                            vfs_context_current() );
            
            //
            // we should not fail requests because of the CAWL errors
            //
            RC = KERN_SUCCESS;
            
            //
            // switch the processing to the covered vnode
            //
            sparseFile->release();
            sparseFile = NULL;
            
            //
            // repeat the request using the native file
            //
            i -= 0x1;
            
        } // end if( KERN_SUCCESS != RC && !gSecuritySettings.securitySettings.disableAccessOnCawlError )
        
        //
        // exit the for loop in case of error
        //
        assert( KERN_SUCCESS == RC );
        if( KERN_SUCCESS != RC )
            break;
        
    } // end for(unsigned int = 0; i < capacity; ++i)

    if( sparseFile )
        sparseFile->release();
                   
    //
    // convert the IOKit error to a BSD error
    //
    if( kIOReturnSuccess != RC )
        RC = ENOMEM;
    
    return RC;
}

//--------------------------------------------------------------------

vnode_t
DldCoveringFsd::getCoveringVnodeRefByCoveredFileName( __in char* coveredFileName )
{
    errno_t       error;
    vnode_t       coveringVnode;
    vfs_context_t vfsContext;
    
    assert( preemption_enabled() );
    
    vfsContext = vfs_context_create( NULL );
    assert( vfsContext );
    if( !vfsContext ){
        
        DBG_PRINT_ERROR(( "vfs_context_create( NULL ) faile\n" ));
        return NULL;
    }
    
    //
    // the request will be intercepted by the hooks
    // so the covering vnode will be returned if the
    // disk is under the CAWL controll
    //
    error = vnode_open( coveredFileName,
                        O_RDWR, // fmode
                        0644,// cmode
                        0x0,// flags
                        &coveringVnode,
                        vfsContext );
    if( error ){
        
        DBG_PRINT_ERROR(( "vnode_open( %s ) failed with an error(%u)\n", coveredFileName, error ));
        coveringVnode = NULL;
    }
    
    vfs_context_rele( vfsContext );
    
    //
    // covering vnode doesn't have attached data
    //
    assert( !( coveringVnode && NULL != vnode_fsnode( coveringVnode ) ) );
    
    return coveringVnode;
}

//--------------------------------------------------------------------

void
DldCoveringFsd::processWrite(
    __in DldCoveredVnode*  coverdVnode,
    __in struct vnop_write_args *ap,
    __inout DldCoveringFsdResult* result
    )
{
    DldCfsdIOArgs        args;
    bool                 zeroLengthBuffer = false;
    int                  numberOfBuffers;
    vfs_context_t        context;
    kern_return_t        RC = KERN_SUCCESS;
    user_ssize_t         originalResid;
    off_t                originalSize;
    off_t                originalOffset;
    off_t                offset;
    off_t                accumulatedBufferOffset;
    
    assert( preemption_enabled() );
    assert( coverdVnode->dldCoveringVnode );
    
    context = ap->a_context;
    zeroLengthBuffer = ( 0x0 == uio_iovcnt( ap->a_uio ) || uio_resid( ap->a_uio ) <= 0x0 );
    numberOfBuffers = uio_iovcnt( ap->a_uio );
    
    bzero( &args, sizeof( args ) );
    
#if defined( DBG )
    args.signature = DLD_CFSD_WRVN_SIGNATURE;
#endif//DBG
    
    args.contextRef = vfs_context_create( context );
    
    coverdVnode->dldCoveringVnode->LockExclusive();
    { // strat of the locked region
        
        //
        // Remember some values in case the write fails.
        //
        originalResid  = uio_resid( ap->a_uio );
        originalSize   = coverdVnode->dldCoveringVnode->fileDataSize;
        originalOffset = uio_offset( ap->a_uio );
        offset         = originalOffset;
        accumulatedBufferOffset = 0x0;
        
        if( ap->a_ioflag & IO_APPEND ){
            
            uio_setoffset(ap->a_uio, coverdVnode->dldCoveringVnode->fileDataSize);
            offset = coverdVnode->dldCoveringVnode->fileDataSize;
            
        } // end if( ap->a_ioflag & IO_APPEND )
        
        if( offset < 0 ){
            
            RC = EFBIG;
            goto __exit;
        }
        
        if( zeroLengthBuffer ){
            
            assert( 0x0 == originalResid );
            
            //
            // nothing to do
            //
            RC = KERN_SUCCESS;
            goto __exit;
        }
        
        if( offset + originalResid > coverdVnode->dldCoveringVnode->fileDataSize ){
            
            //
            // set the new file size if the write extends beyound the end of the file,
            // this causes a recursive FSD entering which will be caught and processed
            // appropriately, for performance reasons the size is increased w/o zeroing -
            // this will create a security hole as the data from deleted files can be visible
            // for a user if he deletes a drive before the CAWL processed the data, but I
            // believe that this is not an issue with the removeabale drives for many
            // reasons one of which is a proliferation of FAT32
            //
            RC = DldVnodeSetsize( coverdVnode->coveredVnode, offset + originalResid, IO_NOZEROFILL | IO_NOAUTH, gVfsContextSuser );
            if( KERN_SUCCESS != RC ){
                
                //
                // try with the real user credentials, in that case IO_NOZEROFILL can't be used as only super user is
                // allowed to use this flag
                //
                RC = DldVnodeSetsize( coverdVnode->coveredVnode, offset + originalResid, 0x0, ap->a_context );
                if( KERN_SUCCESS != RC ){
                    
                    DBG_PRINT_ERROR(("DldVnodeSetsize() failed\n"));
                    goto __exit;
                } // if( KERN_SUCCESS != RC )
            } // if( KERN_SUCCESS != RC )
            
        } // end if( offset + originalResid > coverdVnode->dldCoveringVnode->fileDataSize )
        
        args.flags.write = 0x1;
        args.residual = originalResid;
        
        //
        // all resources allocated or retained for the args structure
        // will be released here in case of error or by worker routine
        // if the request is sent to a worker thread
        //
        
        args.memDscArray = OSArray::withCapacity( numberOfBuffers );
        assert( args.memDscArray );
        if( !args.memDscArray ){
            
            RC = ENOMEM;
            goto __exit;
        }
        
        args.rangeDscArray = OSArray::withCapacity( numberOfBuffers );
        assert( args.rangeDscArray );
        if( !args.rangeDscArray ){
            
            RC = ENOMEM;
            goto __exit;
        }
        
        //
        // create a memory descriptor for each memory range
        //
        for( int i = 0x0; i < numberOfBuffers; ++i ){
            
            int                   ret;
            IOMemoryDescriptor*   memDscr = NULL;
            DldRangeDescriptor    rangeDsc = { 0x0 };
            OSData*               rangeDscAsData;
            user_addr_t           baseaddr;
            user_size_t           length;
            task_t                writerTask;
            bool                  success;
            
            assert( UIO_WRITE == uio_rw(ap->a_uio) );
            
            ret = uio_getiov( ap->a_uio, i, &baseaddr, &length );
            assert( (-1) != ret );
            
            rangeDsc.fileOffset = offset + accumulatedBufferOffset;
            rangeDsc.rangeSize  = length;
            
            //
            // account for accumulated offset in the IO buffer
            //
            accumulatedBufferOffset += length;
            
            //
            // use vfs_context_proc( ap->a_a_context ) to get the task,
            // the current task can't be used as for an asyncronous IO
            // the user task's map is attached to a kernel thread but
            // this doesn't change the task which is the kernel task,
            // for reference see aio_work_thread()
            //
            if( uio_isuserspace( ap->a_uio ) )
                writerTask = vfs_context_proc( context )? DldBsdProcToTask( vfs_context_proc( context ) ): current_task();
            else
                writerTask = kernel_task;
            
            memDscr = IOMemoryDescriptor::withAddressRange( baseaddr,
                                                           length,
                                                           kIODirectionOut,
                                                           writerTask );
            assert( memDscr );
            if( !memDscr ){
                
                RC = ENOMEM;
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
            
            if( !success ){
                
                RC = ENOMEM;
                break;
            }        
            
            rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
            assert( rangeDscAsData );
            if( !rangeDscAsData ){
                
                RC = ENOMEM;
                break;
            }
            
            success = args.rangeDscArray->setObject( rangeDscAsData );
            assert( success );
            //
            // do not break before releasing the rangeDscAsData object
            //
            
            rangeDscAsData->release();
            if( !success ){
                
                RC = ENOMEM;
                break;
            }
            
        } // for( int i = 0x0; i < numberOfBuffers; ++i )
        
        assert( KERN_SUCCESS == RC );
        assert( 0x0 == result->flags.passThrough );
        
        if( KERN_SUCCESS == RC ){
            
            //
            // write down the buffers
            //
            RC = this->rwData( coverdVnode, &args );
            //assert( KERN_SUCCESS == RC );
            assert( args.residual <= originalResid );
            
            //
            // set the new residual and the offset
            //
            uio_setresid( ap->a_uio, args.residual );
            uio_setoffset( ap->a_uio, originalOffset + (originalResid - args.residual) );
            
            if( KERN_SUCCESS != RC ){
                
                if( ap->a_ioflag & IO_UNIT ){
                    
                    //
                    // the write is atomic and it failed, restore original values
                    //
                    uio_setoffset( ap->a_uio, originalOffset );
                    uio_setresid( ap->a_uio, originalResid );
                    
                } else {
                    
                    //
                    // if not atomic and managed to write some data - this is a success
                    //
                    if( args.residual != originalResid )
                        RC = KERN_SUCCESS;
                }
                
            } // end if( KERN_SUCCESS != RC )
            
        } // if( KERN_SUCCESS == RC )
        
    __exit:;
        
    } // end of the locked region
    coverdVnode->dldCoveringVnode->UnLockExclusive();
    
    result->flags.completeWithStatus = 0x1;
    result->status = RC;
    
    //
    // the current processing is synchronous
    //
    this->releaseIOArgs( &args );
}

//--------------------------------------------------------------------

void
DldCoveringFsd::processRead(
    __in DldCoveredVnode*  coverdVnode,
    __in struct vnop_read_args *ap,
    __inout DldCoveringFsdResult* result
    )
{
    DldCfsdIOArgs        args;
    bool                 zeroLengthBuffer = false;
    int                  numberOfBuffers;
    vfs_context_t        context;
    kern_return_t        RC = KERN_SUCCESS;
    user_ssize_t         originalResid;
    off_t                originalSize;
    off_t                originalOffset;
    off_t                offset;
    off_t                accumulatedBufferOffset;
    
    context = ap->a_context;
    zeroLengthBuffer = ( 0x0 == uio_iovcnt( ap->a_uio ) || uio_resid( ap->a_uio ) <= 0x0 );
    numberOfBuffers  = uio_iovcnt( ap->a_uio );
    
    bzero( &args, sizeof( args ) );
    
#if defined( DBG )
    args.signature = DLD_CFSD_WRVN_SIGNATURE;
#endif//DBG
    
    args.contextRef = vfs_context_create( context );
    
    coverdVnode->dldCoveringVnode->LockShared();
    { // start of the locked region
        
        //
        // Remember some values in case the write fails.
        //
        originalResid  = uio_resid( ap->a_uio );
        originalSize   = coverdVnode->dldCoveringVnode->fileDataSize;
        originalOffset = uio_offset( ap->a_uio );
        offset         = originalOffset;
        accumulatedBufferOffset = 0x0;
        
        if( offset < 0 ){
            
            RC = EFBIG;
            goto __exit;
        }
        
        if( zeroLengthBuffer ){
            
            //
            // nothing to do
            //
            RC = KERN_SUCCESS;
            goto __exit;
        }
        
        if( offset >= coverdVnode->dldCoveringVnode->fileDataSize ){
            
            //
            // nothing to do, return successfull code and zero bytes processed
            //
            RC = KERN_SUCCESS;
            goto __exit;
        }
        
        args.flags.read = 0x1;
        args.residual = originalResid;
        
        //
        // all resources allocated or retained for the args structure
        // will be released here in case of error or by worker routine
        // if the request is sent to a worker thread
        //
        
        args.memDscArray = OSArray::withCapacity( numberOfBuffers );
        assert( args.memDscArray );
        if( !args.memDscArray ){
            
            RC = ENOMEM;
            goto __exit;
        }
        
        args.rangeDscArray = OSArray::withCapacity( numberOfBuffers );
        assert( args.rangeDscArray );
        if( !args.rangeDscArray ){
            
            RC = ENOMEM;
            goto __exit;
        }
        
        //
        // create a memory descriptor for each memory range
        //
        for( int i = 0x0; i < numberOfBuffers; ++i ){
            
            int                   ret;
            IOMemoryDescriptor*   memDscr = NULL;
            DldRangeDescriptor    rangeDsc = { 0x0 };
            OSData*               rangeDscAsData;
            user_addr_t           baseaddr;
            user_size_t           length;
            task_t                writerTask;
            bool                  success;
            
            assert( UIO_READ == uio_rw(ap->a_uio) );
            
            ret = uio_getiov( ap->a_uio, i, &baseaddr, &length );
            assert( (-1) != ret );
            
            rangeDsc.fileOffset = offset + accumulatedBufferOffset;
            rangeDsc.rangeSize  = length;
            
            //
            // account for accumulated offset in the IO buffer
            //
            accumulatedBufferOffset += length;
            
            //
            // use vfs_context_proc( ap->a_a_context ) to get the task,
            // the current task can't be used as for an asyncronous IO
            // the user task's map is attached to a kernel thread but
            // this doesn't change the task which is the kernel task,
            // for reference see aio_work_thread()
            //
            if( uio_isuserspace( ap->a_uio ) )
                writerTask = vfs_context_proc( context )? DldBsdProcToTask( vfs_context_proc( context ) ): current_task();
            else
                writerTask = kernel_task;
            
            memDscr = IOMemoryDescriptor::withAddressRange( baseaddr,
                                                           length,
                                                           kIODirectionIn,
                                                           writerTask );
            
            assert( memDscr );
            if( !memDscr ){
                
                RC = ENOMEM;
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
            
            if( !success ){
                
                RC = ENOMEM;
                break;
            }        
            
            rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
            assert( rangeDscAsData );
            if( !rangeDscAsData ){
                
                RC = ENOMEM;
                break;
            }
            
            success = args.rangeDscArray->setObject( rangeDscAsData );
            assert( success );
            //
            // do not break before releasing the rangeDscAsData object
            //
            
            rangeDscAsData->release();
            if( !success ){
                
                RC = ENOMEM;
                break;
            }
            
        } // for( int i = 0x0; i < numberOfBuffers; ++i )
        
        assert( KERN_SUCCESS == RC );
        assert( 0x0 == result->flags.passThrough );
        
        if( KERN_SUCCESS == RC ){
            
            //
            // read data from the sparse file and covered vnode
            //
            RC = this->rwData( coverdVnode, &args );
            //assert( KERN_SUCCESS == RC );
            assert( args.residual <= originalResid );
            
            //
            // if the read was beyound the file data size then
            // truncate the returned data, we can't do this before
            // processing the read because of historic reasons,
            // so yes we might have wasted time by mapping a big user biffer
            //
            user_ssize_t   adjustedResidual;
            if( ( originalOffset + (originalResid - args.residual) ) > coverdVnode->dldCoveringVnode->fileDataSize )
                adjustedResidual = (originalOffset + originalResid) - coverdVnode->dldCoveringVnode->fileDataSize;
            else
                adjustedResidual = args.residual;
            
            //
            // set the new residual and the offset
            //
            uio_setresid( ap->a_uio, adjustedResidual );
            uio_setoffset( ap->a_uio, originalOffset + (originalResid - adjustedResidual) );
            
            
            if( KERN_SUCCESS != RC ){
                
                if( ap->a_ioflag & IO_UNIT ){
                    
                    //
                    // the write is atomic and it failed, restore original values
                    //
                    uio_setoffset( ap->a_uio, originalOffset );
                    uio_setresid( ap->a_uio, originalResid );
                    
                }/* else {
                    
                    //
                    // if not atomic and managed to write some data - this is a success
                    //
                    if( args.residual != originalResid )
                        RC = KERN_SUCCESS;
                }*/
                
            } // end if( KERN_SUCCESS != RC )
        }
        
    __exit:;
    } // end of the locked region
    coverdVnode->dldCoveringVnode->UnLockShared();
    
    result->flags.completeWithStatus = 0x1;
    result->status = RC;
    
    //
    // the current processing is synchronous
    //
    this->releaseIOArgs( &args );
}

//--------------------------------------------------------------------

void
DldCoveringFsd::processPagingIO(
    __in DldCoveredVnode* coverdVnode,
    __in DldPagingIoArgs* ap,
    __inout DldCoveringFsdResult* result
    )
{
    //
    // see cluster_pagein_ext for pagein processing example
    //
    
    DldCfsdIOArgs       args;
    int                 numberOfBuffers;
    upl_t               upl;
    size_t              size;
    u_int               ioSize;
	int                 roundedSize;
    off_t               maxSize;
    off_t               fileSize;
    bool                commitUPL;
    upl_offset_t        uplOffset;
    off_t               fileOffset;
    kern_return_t       RC = KERN_SUCCESS;
    
    assert( preemption_enabled() );
    assert( coverdVnode->coveringVnode && coverdVnode->coveredVnode );
    assert( coverdVnode->dldCoveringVnode && coverdVnode->dldCoveredVnode );
    assert( ap->a_vp == coverdVnode->coveredVnode );
    
    assert( 0x0 == result->status );
    assert( 0x0 == result->flags.passThrough );
    assert( 0x0 == result->flags.completeWithStatus );
    assert( ap->flags.read != ap->flags.write );
    
    bzero( &args, sizeof( args ) );
    
#if defined( DBG )
    args.signature = DLD_CFSD_WRVN_SIGNATURE;
#endif//DBG
    
    args.contextRef = vfs_context_create( ap->a_context );
    
    coverdVnode->dldCoveringVnode->LockExclusive();
    { // strat of the locked region
        
        upl             = ap->a_pl;
        size            = ap->a_size;
        commitUPL       = !(ap->a_flags & UPL_NOCOMMIT);
        uplOffset       = ap->a_pl_offset;
        fileOffset      = ap->a_f_offset;
        fileSize        = coverdVnode->dldCoveringVnode->fileDataSize;
        
        //
        // the memory subsystem should know its business
        //
        assert( fileOffset < coverdVnode->dldCoveringVnode->fileDataSize );
        
        if( size < 0 ){
            
            DBG_PRINT_ERROR(("invalid size %ld", size));
            
            if( commitUPL ){
                
                assert( upl );
                assert( NULL == args.upl );
                
                ubc_upl_abort( upl, ( 0x1 == ap->flags.read ) ?  UPL_ABORT_ERROR : UPL_ABORT_DUMP_PAGES );
                
                upl = NULL;
                commitUPL = false; // already done
            }
            
            RC = EINVAL;
            goto __exit;
        }
        
        //
        // can't page-in from a negative offset
        // or if we're starting beyond the EOF
        // or if the file offset isn't page aligned
        // or the size requested isn't a multiple of PAGE_SIZE
        //
        if (fileOffset < 0 || fileOffset >= fileSize ||
            (fileOffset & PAGE_MASK_64) || (size & PAGE_MASK) || (uplOffset & PAGE_MASK)){
            
            //
            // we are freeing now so do not free upl in releaseIOArgs()
            //
            assert( NULL == args.upl );
            
            if (commitUPL) {
                
                int ubcAbortError;
                
                assert(upl);
                ubcAbortError = ubc_upl_abort_range( upl, uplOffset, size, UPL_ABORT_FREE_ON_EMPTY |
                                                    (( 0x1 == ap->flags.read ) ?  UPL_ABORT_ERROR : UPL_ABORT_DUMP_PAGES) );
                commitUPL = false; // already done
                
                assert( !ubcAbortError );
            }
            
            upl = NULL;
            RC = EINVAL;
            goto __exit;
        } // end if (fileOffset < 0 || fileOffset >= filesize ||
        
        maxSize = fileSize - fileOffset;
        
        if( size < maxSize )
            ioSize = size;
        else
            ioSize = maxSize;
        
        numberOfBuffers  = 0x1;
        
        if( 0x0 == ioSize ){
            
            //
            // nothing to do
            //
            
            //
            // we are freeing now so do not free upl in releaseIOArgs()
            //
            assert( NULL == args.upl );
            
            if( commitUPL ){
                
                int ubcAbortError;
                
                ubcAbortError = ubc_upl_abort_range( upl, uplOffset, size, UPL_ABORT_FREE_ON_EMPTY );
                
                assert( !ubcAbortError );
                commitUPL = false;
            }
            
            upl = NULL;
            
            RC = KERN_SUCCESS;
            goto __exit;
        }
        
        //
        // TO DO - look at the cluster_io where some magic for rounding is done,
        // for example the volumes with cluster size 1 do not require rounding
        //
        roundedSize = (ioSize + (PAGE_SIZE - 1)) & ~PAGE_MASK;
        
        if( size > roundedSize && commitUPL ){
            
            int ubcAbortError;
            
            assert( upl );
            
            ubcAbortError = ubc_upl_abort_range( upl, uplOffset + roundedSize,
                                                 size - roundedSize, UPL_ABORT_FREE_ON_EMPTY );
            
            assert( !ubcAbortError );
        }// end if( size > roundedSize && commitUPL )
        
        args.flags.pagingIO = 0x1;
        args.residual       = ioSize;
        
        if( 0x1 == ap->flags.read )
            args.flags.read = 0x1;
        else
            args.flags.write = 0x1;
        
        //
        // all resources allocated or retained for the args structure
        // will be released here in case of error or by worker routine
        // if the request is sent to a worker thread
        //
        
        args.memDscArray = OSArray::withCapacity( numberOfBuffers );
        assert( args.memDscArray );
        if( !args.memDscArray ){
            
            RC = ENOMEM;
            goto __exit;
        }
        
        args.rangeDscArray = OSArray::withCapacity( numberOfBuffers );
        assert( args.rangeDscArray );
        if( !args.rangeDscArray ){
            
            RC = ENOMEM;
            goto __exit;
        }
        
        for( int i = 0x0; i < numberOfBuffers; ++i ){
            
            upl_page_info_t*  pl;
            bool              is_pagingv2;
            
            //
            // we can tell if we're getting the new or old behavior from the UPL
            //
            is_pagingv2 = ( NULL == upl );
            
            pl = NULL;
            
            if( is_pagingv2 ){
                
                //
                // look at vnode_pagein() processing for
                // FSD with VFC_VFSVNOP_PAGEINV2 flag set - 
                // NULL upl is sent in this case, the same for pageout
                //
                
                off_t f_offset;
                int   offset;
                int   isize; 
                int   pg_index;
                int   i = 0x0;
                int   upl_flags = UPL_FLAGS_NONE;
                
                //
                // we're in control of any UPL we commit
                // make sure someone hasn't accidentally passed in UPL_NOCOMMIT,
                // also set the offset to zero, sometimes the system passes non zero
                // upl offset, it must be ignored, the nature of the non zero upl
                // offset is revealed by the vnode_pager_cluster_write()'s code
                // setting a cluster's base_offset
                //
                commitUPL = true;
                uplOffset = 0x0;
                
                assert( commitUPL );
                assert( 0x0 == uplOffset );
                
                isize    = ioSize;
                f_offset = fileOffset;
                
                if( 0x1 == ap->flags.write ){
                    
                    if( ap->a_flags & UPL_MSYNC )
                        upl_flags |= UPL_UBC_MSYNC | UPL_RET_ONLY_DIRTY;
                    else
                        upl_flags |= UPL_UBC_PAGEOUT | UPL_RET_ONLY_DIRTY;
                    
                } else {
                    
                    upl_flags = UPL_UBC_PAGEIN | UPL_RET_ONLY_ABSENT;
                    
                }
                
                //
                // UPL_SET_IO_WIRE is useless as not taken into account,
                // consider UPL_SET_LITE
                //
                
                RC = ubc_create_upl( coverdVnode->coveringVnode,
                                     fileOffset,
                                     roundedSize,
                                     &upl,
                                     &pl,
                                     upl_flags ); 
                assert( upl && pl );
                assert( KERN_SUCCESS == RC );
                if( KERN_SUCCESS != RC ){
                    
                    OSString* fileNameRef = coverdVnode->dldCoveredVnode->getNameRef();
                    assert( fileNameRef );
                    
                    DBG_PRINT_ERROR(( "ubc_create_upl() failed with RC=0x%X for %s file\n", RC, fileNameRef->getCStringNoCopy() ));
                    
                    fileNameRef->release();
                    
                    upl = NULL;
                    pl  = NULL;
                    
                    break;
                }
                
                //
                // we are the UPL owner
                //
                commitUPL = true;
                
                //
                // change a size so it reflects the actual upl size
                //
                size = roundedSize;
                
                pg_index = ((roundedSize) / PAGE_SIZE);
                assert(pg_index > 0);
                if( 0x1 == ap->flags.write ){
                    
                    // 
                    // Scan from the back to find the last page in the UPL
                    //
                    bool dirtyPages = true;
                    while( pg_index > 0 ){
                        
                        if( upl_page_present(pl, --pg_index) )
                            break;
                        
                        if (pg_index == 0) {
                            
                            //
                            // oops, no dirty pages were found
                            //
                            dirtyPages = false;
                            break;
                        }
                        
                    }
                    
                    if( !dirtyPages )
                        break;
                    
                } else { // end if( 0x1 == ap->flags.write )
                    //
                    // Scan from the back to find the last page in the UPL
                    //
                    bool pagesPresent = true;
                    while( pg_index > 0 ){
                        
                        if( upl_page_present(pl, --pg_index) )
                            break;
                        
                        if (pg_index == 0) {
                            
                            //
                            // oops, no pages were found
                            //
                            pagesPresent = false;
                            break;
                        }
                        
                    }
                    
                    if( !pagesPresent )
                        break;
                }
                
                // 
                // initialize the offset variables before we touch the UPL.
                // a_f_offset is the position into the file, in bytes
                // offset is the position into the UPL, in bytes
                // pg_index is the pg# of the UPL we're operating on.
                // isize is the offset into the UPL after the last non-clean page
                // in case of the write or the end of the range to pagein for read.
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
                    
                    //
                    // pg_index is an index of the first present page
                    //
                    
                    if( 0x1 == ap->flags.write ){
                        
                        if( !upl_dirty_page( pl, pg_index ) ){
                            
                            assert( !"unforeseen clean page" );
                            DBG_PRINT_ERROR(("unforeseen clean page @ index %d for UPL %p\n", pg_index, upl));
                            //
                            // continue processing as the page is present and there won't be a page fault,
                            // but HFS FSD panics in this case, so in any case we are on the road to hell
                            //
                        }
                        
                    } // end if( 0x1 == args->flags.write )
                    
                    //
                    // We know that we have at least one (dirty or present) page.
                    // Now checking to see how many in a row we have
                    //
                    num_of_pages = 1;
                    xsize = isize - PAGE_SIZE;
                    
                    while( 0x0 != xsize ){
                        
                        //
                        // upl_dirty_page is for pageout as we must not flush clean pages
                        // upl_page_present is for pagein as we can pagein into resident pages only
                        //
                        if( 0x1 == ap->flags.write && !upl_dirty_page( pl, pg_index + num_of_pages) )
                            break;
                        
                        if( 0x1 == ap->flags.read && !upl_page_present(pl, pg_index + num_of_pages) )
                            break;
                        
                        num_of_pages++;
                        xsize -= PAGE_SIZE;
                        
                    }
                    xsize = num_of_pages * PAGE_SIZE;
                    
                    //
                    // so the dirty range has been found
                    //
                    
                    rangeDsc.fileOffset = f_offset;
                    
                    if( f_offset + xsize > (fileOffset + ioSize) )
                        rangeDsc.rangeSize = (fileOffset + ioSize) - f_offset;
                    else
                        rangeDsc.rangeSize = xsize;
                    
                    //
                    // create a memory descriptor for the range
                    //
                    
                    options = kIOMemoryTypeUPL | ( (0x1 == ap->flags.read) ? kIODirectionIn : kIODirectionOut );
                    
                    memDscr = IOMemoryDescriptor::withOptions( upl,
                                                               rangeDsc.rangeSize,  // range size
                                                               offset, // offset in upl
                                                               0,
                                                               options );
                    assert( memDscr );
                    if( !memDscr ){
                        
                        OSString* fileNameRef = coverdVnode->dldCoveredVnode->getNameRef();
                        assert( fileNameRef );
                        
                        DBG_PRINT_ERROR(("IOMemoryDescriptor::withOptions() for %s file\n", fileNameRef->getCStringNoCopy() ));
                        
                        fileNameRef->release();
                        
                        RC = ENOMEM;
                        break;
                    }
                    
                    bool wasAdded;
                    
                    wasAdded = args.memDscArray->setObject( memDscr );
                    assert( wasAdded );
                    //
                    // do not break here as the memDscr object must be released
                    //
                    
                    //
                    // if added the object is retained
                    // if not added the allocated resources must be released
                    //
                    memDscr->release();
                    DLD_DBG_MAKE_POINTER_INVALID( memDscr );
                    
                    if( !wasAdded ){
                        
                        RC = ENOMEM;
                        break;
                    }
                    
                    rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
                    assert( rangeDscAsData );
                    if( !rangeDscAsData ){
                        
                        RC = ENOMEM;
                        break;
                    }
                    
                    wasAdded = args.rangeDscArray->setObject( rangeDscAsData );
                    assert( wasAdded );
                    //
                    // do not break before releasing the rangeDscAsData object
                    //
                    
                    rangeDscAsData->release();
                    
                    if( !wasAdded ){
                        
                        RC = ENOMEM;
                        break;
                    }
                    
                    //
                    // move to the next range( clean ) in the upl
                    //
                    f_offset += xsize;
                    offset   += xsize;
                    isize    -= xsize;
                    pg_index += num_of_pages;
                    ++i;
                    
                }// end while( 0x0 != isize )
                
                assert( KERN_SUCCESS == RC );
                if( KERN_SUCCESS != RC )
                    break;
                
            } // end block for v2 paging behavior
            else
            {
                
                IOMemoryDescriptor*   memDscr;
                DldRangeDescriptor    rangeDsc  = { 0x0 };
                OSData*               rangeDscAsData;
                IOOptionBits          options;
                
                rangeDsc.fileOffset = fileOffset;
                rangeDsc.rangeSize  = ioSize;
                
                //
                // create a memory descriptor for the range
                //
                
                options = kIOMemoryTypeUPL | kIODirectionIn;
                
                memDscr = IOMemoryDescriptor::withOptions( upl,
                                                          ioSize,        // range size
                                                          uplOffset,     // offset in upl
                                                          0,             // no task is allowed for UPL!
                                                          options );
                assert( memDscr );
                if( !memDscr ){
                    
                    OSString*    nameStr = coverdVnode->dldCoveredVnode->getNameRef();
                    assert( nameStr );
                    
                    DBG_PRINT_ERROR(("IOMemoryDescriptor::withOptions() for %s file\n", nameStr->getCStringNoCopy() ));
                    nameStr->release();
                    
                    RC = ENOMEM;
                    break;
                }
                
                bool wasAdded;
                
                wasAdded = args.memDscArray->setObject( memDscr );
                assert( wasAdded );
                //
                // do not break here as the memDscr object must be released
                //
                
                //
                // if added the object is retained
                // if not added the allocated resources must be released
                //
                memDscr->release();
                DLD_DBG_MAKE_POINTER_INVALID( memDscr );
                
                if( !wasAdded ){
                    
                    RC = ENOMEM;
                    break;
                }
                
                rangeDscAsData = OSData::withBytes( &rangeDsc, sizeof( rangeDsc ) );
                assert( rangeDscAsData );
                if( !rangeDscAsData ){
                    
                    RC = ENOMEM;
                    break;
                }
                
                wasAdded = args.rangeDscArray->setObject( rangeDscAsData );
                assert( wasAdded );
                //
                // do not break before releasing the rangeDscAsData object
                //
                
                rangeDscAsData->release();
                
                if( !wasAdded ){
                    
                    RC = ENOMEM;
                    break;
                }
                
            } // end else
            
        } // for( int i = 0x0; i < numberOfBuffers; ++i )
        
        assert( KERN_SUCCESS == RC );
        assert( 0x0 == result->flags.passThrough );
        
        if( KERN_SUCCESS == RC ){
            
            //
            // write down the buffers
            //
            RC = this->rwData( coverdVnode, &args );
            assert( KERN_SUCCESS == RC );
        }
        
    __exit:;
    } // end of the locked region
    coverdVnode->dldCoveringVnode->UnLockExclusive();
    
    if( commitUPL && upl ){
        
        int ubcError;
        
        //
        // we are freeing now so do not free upl in releaseIOArgs()
        //
        assert( NULL == args.upl );
        
        upl_size_t uplSizeLeft = (size > roundedSize) ? roundedSize : size;
        
		if( KERN_SUCCESS != RC ){
            
			ubcError = ubc_upl_abort_range( upl, uplOffset, uplSizeLeft, UPL_ABORT_FREE_ON_EMPTY );
            
		} else {
            
            int commitFlags = 0x0;
            
            if( 0x1 == ap->flags.read )
                commitFlags |= UPL_COMMIT_CLEAR_DIRTY;
            
			ubcError = ubc_upl_commit_range( upl, uplOffset, uplSizeLeft,
                                             UPL_COMMIT_FREE_ON_EMPTY | commitFlags );
        }
        
        assert( !ubcError );
        
    } // end if( commitUPL )
    
    result->flags.completeWithStatus = 0x1;
    result->status = RC;
    
#if defined(DBG)
    {
        upl_t               tmpUpl;
        upl_page_info_t*    tmpPl;
        
        //
        // check for dirty pages in the range
        //
        RC = ubc_create_upl( coverdVnode->coveringVnode,
                             fileOffset,
                             round_page_64(ap->a_size),
                             &tmpUpl,
                             &tmpPl,
                             UPL_UBC_PAGEOUT | UPL_RET_ONLY_DIRTY );
        assert( KERN_SUCCESS == RC );
        if( KERN_SUCCESS == RC ){

            int tmpPgIndex;
            
            tmpPgIndex = ((round_page_64(ap->a_size)) / PAGE_SIZE);
            
            // 
            // Scan from the back to find the last page in the UPL
            //
            bool dirtyPages = true;
            while( tmpPgIndex > 0 ){
                
                if( upl_page_present(tmpPl, --tmpPgIndex) ){
                    
                    //
                    // a dirty page !
                    //
                    __asm__ volatile( "int $0x3" );
                }
                
                if (tmpPgIndex == 0) {
                    
                    //
                    // no dirty pages were found
                    //
                    dirtyPages = false;
                    break;
                }
                
            }
            
            ubc_upl_abort( tmpUpl, UPL_ABORT_FREE_ON_EMPTY );
            
        } // if( KERN_SUCCESS == RC )
    }
#endif // DBG
    
    //
    // the current processing is synchronous
    //
    this->releaseIOArgs( &args );
    
}

//--------------------------------------------------------------------

/*
 struct vnop_pagein_args {
 struct vnodeop_desc *a_desc;
 vnode_t         a_vp;
 upl_t           a_pl;
 upl_offset_t    a_pl_offset;
 off_t           a_f_offset;
 size_t          a_size;
 int             a_flags;
 vfs_context_t   a_context;
 };
 */

void
DldCoveringFsd::processPagein(
    __in DldCoveredVnode*  coverdVnode,
    __in struct vnop_pagein_args *ap,
    __inout DldCoveringFsdResult* result
    )
{
    
    DldPagingIoArgs ioArgs = { 0x0 };
    
    ioArgs.a_vp        = ap->a_vp;
    ioArgs.a_pl        = ap->a_pl;
    ioArgs.a_pl_offset = ap->a_pl_offset;
    ioArgs.a_f_offset  = ap->a_f_offset;
    ioArgs.a_size      = ap->a_size;
    ioArgs.a_flags     = ap->a_flags;
    ioArgs.a_context   = ap->a_context;
    
    ioArgs.flags.read  = 0x1;
    
    this->processPagingIO( coverdVnode, &ioArgs, result );
}

//--------------------------------------------------------------------

/*
 FYI, an interesting example of a call stack, pay attention that vnode_put is a dangerous operation as 
 might trigger a pageout operation
 
 #0  machine_switch_context (old=0x708c000, continuation=0, new=0x68e17a8) at /SourceCache/xnu/xnu-1504.7.4/osfmk/i386/pcb.c:869
 #1  0x00226e57 in thread_invoke (self=0x708c000, thread=0x68e17a8, reason=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1628
 #2  0x002270f6 in thread_block_reason (continuation=0, parameter=0x0, reason=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1863
 #3  0x00227184 in thread_block (continuation=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/sched_prim.c:1880
 #4  0x00221566 in lck_rw_sleep (lck=0x735c5d0, lck_sleep_action=0, event=0x4606784, interruptible=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/kern/locks.c:861
 #5  0x00274b88 in vm_object_upl_request (object=0x735c5c8, offset=0, size=12288, upl_ptr=0x322eada8, user_page_list=0x8c610b4, page_list_count=0x0, cntrl_flags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_pageout.c:3552
 #6  0x0049f651 in ubc_create_upl (vp=0x8ba13a8, f_offset=0, bufsize=12288, uplp=0x322eada8, plp=0x322eadac, uplflags=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/ubc_subr.c:1869
 #7  0x002d3cbc in cluster_write_copy (vp=0x8ba13a8, uio=0x322eb21c, io_req_size=<value temporarily unavailable, due to optimizations>, oldEOF=9103, newEOF=12288, headOff=0, tailOff=0, flags=32772, callback=0, callback_arg=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_cluster.c:2778
 #8  0x002d4ee4 in cluster_write_ext (vp=0x8ba13a8, uio=0x322eb21c, oldEOF=9103, newEOF=12288, headOff=0, tailOff=0, xflags=<value temporarily unavailable, due to optimizations>, callback=0, callback_arg=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_cluster.c:1919
 #9  0x002d5922 in cluster_write (vp=0x8ba13a8, uio=0x322eb21c, oldEOF=0, newEOF=0, headOff=0, tailOff=0, xflags=32772) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_cluster.c:1811
 #10 0x3227ba9a in ?? () DLDriver
 #11 0x473c35dd in ?? () DLDriver
 #12 0x002faeb4 in VNOP_WRITE (vp=0x8ba13a8, uio=0x322eb21c, ioflag=32772, ctx=0x322eb258) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:3520
 #13 0x002f07ed in vn_rdwr_64 (rw=UIO_WRITE, vp=0x8ba13a8, base=14082048, len=12288, offset=0, segflg=UIO_SYSSPACE, ioflg=32772, cred=0x5b82d4c, aresid=0x322eb2c8, p=0x69cb000) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_vnops.c:689
 #14 0x002f0899 in vn_rdwr (rw=UIO_WRITE, vp=0x8ba13a8, base=0xd6e000  len=12288, offset=0, segflg=UIO_SYSSPACE, ioflg=32772, cred=0x5b82d4c, aresid=0x322eb3fc, p=0x69cb000) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_vnops.c:615
 #15 0x472dbe35 in ?? () DLDriver
 #16 0x472de878 in ?? () DLDriver
 #17 0x473c2714 in ?? () DLDriver
 #18 0x002f66a0 in VNOP_PAGEOUT (vp=0x3000, pl=0x8c61280, pl_offset=0, f_offset=0, size=12288, flags=24, ctx=0x70abb24) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:5444
 #19 0x004cdf0f in vnode_pageout (vp=0x8ba1314, upl=0x8c61280, upl_offset=0, f_offset=0, size=12288, flags=24, errorp=0x322eb8dc) at /SourceCache/xnu/xnu-1504.7.4/bsd/vm/vnode_pager.c:368
 #20 0x002503a5 in vnode_pager_cluster_write (vnode_object=0x8a456f4, offset=0, cnt=12288, resid_offset=0x0, io_error=0x0, upl_flags=24) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/bsd_vm.c:991
 #21 0x00250ba5 in vnode_pager_data_return (mem_obj=0x8a456f4, offset=0, data_cnt=12288, resid_offset=0x0, io_error=0x0, dirty=1, kernel_copy=1, upl_flags=16) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/bsd_vm.c:687
 #22 0x00252a3c in vm_object_update (object=0x8adf970, offset=0, size=12288, resid_offset=0x0, io_errno=0x0, should_return=2, flags=<value temporarily unavailable, due to optimizations>, protection=8) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/memory_object.c:2152
 #23 0x00252d91 in memory_object_lock_request (control=0x8a12870, offset=0, size=12288, resid_offset=0x0, io_errno=0x0, should_return=2, flags=<value temporarily unavailable, due to optimizations>, prot=8) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/memory_object.c:423
 #24 0x0049f791 in ubc_msync_internal (vp=<value temporarily unavailable, due to optimizations>, beg_off=0, end_off=<value temporarily unavailable, due to optimizations>, resid_off=0x0, flags=2, io_errno=0x0) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/ubc_subr.c:1294
 #25 0x002dc33a in vclean (vp=0x8ba1314, flags=0) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_subr.c:2029
 #26 0x002dc519 in vgone [inlined] () at :2203
 #27 0x002dc519 in vnode_reclaim_internal (vp=0x8ba1314, locked=1, reuse=1, flags=8) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_subr.c:4026
 #28 0x002dc7da in vnode_put_locked (vp=0x8ba1314) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_subr.c:3811
 #29 0x002dc810 in vnode_put (vp=0x8ba1314) at /SourceCache/xnu/xnu-1504.7.4/bsd/vfs/vfs_subr.c:3766
 #30 0x002508fd in vnode_pager_last_unmap (mem_obj=0x8a456f4) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/bsd_vm.c:959
 #31 0x0026dd9c in vm_object_deallocate (object=0x8adf970) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_object.c:834
 #32 0x00260c92 in vm_map_delete (map=0x6a16b88, start=<value temporarily unavailable, due to optimizations>, end=4912168960, flags=4, zap_map=0x0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_map.c:4715
 #33 0x00260e89 in vm_map_remove (map=0x6a16b88, start=4912156672, end=4912168960, flags=0) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_map.c:5292
 #34 0x00282096 in mach_vm_deallocate (map=0x6a16b88, start=4912156672, size=9103) at /SourceCache/xnu/xnu-1504.7.4/osfmk/vm/vm_user.c:276
 #35 0x0047a6f3 in munmap (p=0x69cb000, uap=0x68e3dc8, retval=0x70aba64) at /SourceCache/xnu/xnu-1504.7.4/bsd/kern/kern_mman.c:668
 #36 0x004edaf8 in unix_syscall64 (state=0x68e3dc4) at /SourceCache/xnu/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:433
 */

/*
 struct vnop_pageout_args {
    struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    upl_t a_pl;
    upl_offset_t a_pl_offset;
    off_t a_f_offset;
    size_t a_size;
    int a_flags;
    vfs_context_t a_context;
 };
*/

void
DldCoveringFsd::processPageout(
    __in DldCoveredVnode*  coverdVnode,
    __in struct vnop_pageout_args *ap,
    __inout DldCoveringFsdResult* result
    )
{
    
    DldPagingIoArgs ioArgs = { 0x0 };
    
    ioArgs.a_vp        = ap->a_vp;
    ioArgs.a_pl        = ap->a_pl;
    ioArgs.a_pl_offset = ap->a_pl_offset;
    ioArgs.a_f_offset  = ap->a_f_offset;
    ioArgs.a_size      = ap->a_size;
    ioArgs.a_flags     = ap->a_flags;
    ioArgs.a_context   = ap->a_context;
    
    ioArgs.flags.write = 0x1;
    
    this->processPagingIO( coverdVnode, &ioArgs, result );
}

//--------------------------------------------------------------------

void
DldCoveringFsd::LockShared()
{
    assert( preemption_enabled() );
    assert( this->lock );
    IORecursiveLockLock( this->lock );
}

void
DldCoveringFsd::UnLockShared()
{
    assert( preemption_enabled() );
    assert( this->lock );
    IORecursiveLockUnlock( this->lock );
}

void
DldCoveringFsd::LockExclusive()
{
    assert( preemption_enabled() );
    assert( this->lock );
    IORecursiveLockLock( this->lock );
}

void
DldCoveringFsd::UnLockExclusive()
{
    assert( preemption_enabled() );
    assert( this->lock );
    IORecursiveLockUnlock( this->lock );
}

//--------------------------------------------------------------------

errno_t
DldCoveredVnode::getVnodeInfo( __in vnode_t coveringVnode )
{
    
    errno_t  error = KERN_SUCCESS;
    
    assert( preemption_enabled() );
    assert( NULL == this->coveringVnode &&
            NULL == this->coveredVnode &&
            NULL == this->dldCoveringVnode &&
            NULL == this->dldCoveredVnode );
    
    DldCoveringFsd*  fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( coveringVnode ) );
    assert( fsd );
    
    fsd->LockShared();
    {// start of the lock
        
        this->dldCoveringVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( coveringVnode );
        if( this->dldCoveringVnode ){
            
            if( DldIOVnode::kVnodeType_CoveringFSD == this->dldCoveringVnode->dldVnodeType ){
                
                assert( NULL == vnode_fsnode( coveringVnode ) );
                assert( !( NULL == this->dldCoveringVnode->coveredVnode && !vnode_isrecycled( coveringVnode ) ) );
                
                if( !this->dldCoveringVnode->coveredVnode ){
                    
                    //
                    // this can happen if a concurrent thread calls vclean() which calls
                    // VNOP_RECLAIM
                    //
                    assert( vnode_isrecycled( coveringVnode ) );
                    DBG_PRINT_ERROR(( "a covering vnode without a covered one" ));
                    
                    error = ENOENT;
                    goto __exit;
                }
                
                //
                // reference the covered vnode and then save the pointer in the structure
                // as the structure must be consisten because of error handling by calling
                // putVnodeInfo()
                //
                error = vnode_get( this->dldCoveringVnode->coveredVnode ) ;
                if( KERN_SUCCESS != error ){
                    
                    assert( !"vnode_get() failed" );
                    goto __exit;
                } // end if( KERN_SUCCESS != error )
                
                //
                // the covered vnode was referenced so save its address
                //
                this->coveredVnode = this->dldCoveringVnode->coveredVnode;
                
                this->dldCoveredVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( this->dldCoveringVnode->coveredVnode );
                assert( this->dldCoveredVnode );
                if( !this->dldCoveredVnode ){
                    
                    assert( !"retrieving dldCoveredVnode failed" );
                    error = ENOENT;
                    goto __exit;
                } // end if( !this->dldCoveredVnode )
                
                //
                // the covering vnode is not referenced
                //
                this->coveringVnode = coveringVnode;
                
                
                assert( DldIOVnode::kVnodeType_Native == this->dldCoveredVnode->dldVnodeType );
                
                assert( this->coveringVnode );
                assert( this->coveredVnode );
                assert( this->dldCoveringVnode );
                assert( this->dldCoveredVnode );
                
                //
                // check for the flags consistency
                //
                assert( this->dldCoveringVnode->flags.controlledByCAWL == this->dldCoveringVnode->flags.controlledByCAWL );
                
                //
                // covering vnode doesn't have an attached data
                //
                assert( NULL == vnode_fsnode( this->coveringVnode ) );
                
            } else {
                
                //
                // this is not a covering vnode
                // and this is not an error
                //
                assert( KERN_SUCCESS == error );
                
                //
                // release all objects
                //
                this->putVnodeInfo();
            }
            
        } // end if( this->dldCoveringVnode )
        
    __exit:
        
        if( KERN_SUCCESS != error ){
            
            //
            // release all resources
            //
            this->putVnodeInfo();
            
            assert(  NULL == this->coveringVnode &&
                     NULL == this->coveredVnode &&
                     NULL == this->dldCoveringVnode &&
                     NULL == this->dldCoveredVnode );
        } else {
            
            assert(( this->coveringVnode &&
                     this->coveredVnode &&
                     this->dldCoveringVnode &&
                     this->dldCoveredVnode &&
                     this->coveredVnode != this->coveringVnode &&
                     0x1 == this->dldCoveringVnode->flags.vopHooked &&
                     0x1 == this->dldCoveredVnode->flags.vopHooked )
                   ||
                   ( NULL == this->coveringVnode &&
                     NULL == this->coveredVnode &&
                     NULL == this->dldCoveringVnode &&
                     NULL == this->dldCoveredVnode ) );
        }
        
    }// end of the lock
    fsd->UnLockShared();
    
    DldCoveringFsd::PutCoveringFsd( fsd );
    
    return error;
}

//--------------------------------------------------------------------

void
DldCoveredVnode::putVnodeInfo()
{
    if( this->dldCoveredVnode ){
        
        this->dldCoveredVnode->release();
        this->dldCoveredVnode = NULL;
    }
    
    if( this->dldCoveringVnode ){
        
        this->dldCoveringVnode->release();
        this->dldCoveringVnode = NULL;
    }
    
    if( this->coveredVnode ){
        
        vnode_put( this->coveredVnode );
        this->coveredVnode = NULL;
    }
    
    //
    // coveringVnode is not referenced
    //
    this->coveringVnode = NULL;
}

//--------------------------------------------------------------------

//
// putInputArguments template and its specializations must be defined
// before getInputArguments template and specializations as the former
// are called by the latter and GCC compiler must instantiate them
// before the first call and it instantiates functions in order of
// definitions
//

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_strategy_args_class>::putInputArguments( __inout vnop_strategy_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || buf_vnode( apWrapper->ap->a_bp) == this->vnode1.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        buf_setvnode( apWrapper->ap->a_bp, this->vnode1.coveringVnode );
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_bwrite_args_class>::putInputArguments( __inout vnop_bwrite_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || buf_vnode( apWrapper->ap->a_bp) == this->vnode1.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        buf_setvnode( apWrapper->ap->a_bp, this->vnode1.coveringVnode );
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_exchange_args_class>::putInputArguments( __inout vnop_exchange_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_fvp == this->vnode1.coveredVnode );
    assert( NULL == this->vnode2.coveringVnode || apWrapper->ap->a_tvp == this->vnode2.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_fvp = this->vnode1.coveringVnode;
    
    if( this->vnode2.coveringVnode )
        apWrapper->ap->a_tvp = this->vnode2.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}


//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_copyfile_args_class>::putInputArguments( __inout vnop_copyfile_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_fvp == this->vnode1.coveredVnode );
    assert( NULL == this->vnode2.coveringVnode || apWrapper->ap->a_tvp == this->vnode2.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_fvp = this->vnode1.coveringVnode;
    
    if( this->vnode2.coveringVnode )
        apWrapper->ap->a_tvp = this->vnode2.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_mkdir_args_class>::putInputArguments( __inout vnop_mkdir_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_dvp == this->vnode1.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_mknod_args_class>::putInputArguments( __inout vnop_mknod_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_dvp == this->vnode1.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}


//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_rename_args_class>::putInputArguments( __inout vnop_rename_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_fvp == this->vnode1.coveredVnode );
    assert( NULL == this->vnode2.coveringVnode || apWrapper->ap->a_tvp == this->vnode2.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_fvp = this->vnode1.coveringVnode;
    
    if( this->vnode2.coveringVnode )
        apWrapper->ap->a_tvp = this->vnode2.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_symlink_args_class>::putInputArguments( __inout vnop_symlink_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_dvp == this->vnode1.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------

template<>
void
DldVnopArgs<vnop_whiteout_args_class>::putInputArguments( __inout vnop_whiteout_args_class* apWrapper )
{
    if( 0x1 == this->flags.released )
        return;
    
    assert( NULL == this->vnode1.coveringVnode || apWrapper->ap->a_dvp == this->vnode1.coveredVnode );
    
    //
    // restore the original
    //
    if( this->vnode1.coveringVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveringVnode;
    
    //
    // release all vnodes
    //
    this->putAllVnodes();
    
    if( this->fsd ){
        
        DldCoveringFsd::PutCoveringFsd( this->fsd );
        this->fsd = NULL;
    }
    
    this->flags.released = 0x1;
}

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_strategy_args_class>::getInputArguments( __inout vnop_strategy_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    vnode_t   vnode;
    
    assert( apWrapper->ap );
    assert( NULL == this->fsd );
    
    vnode = buf_vnode( apWrapper->ap->a_bp );
    assert( vnode );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( vnode ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( vnode );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnode to covered one
    //
    if( this->vnode1.coveredVnode )
        buf_setvnode( apWrapper->ap->a_bp, this->vnode1.coveredVnode );
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_strategy_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_bwrite_args_class>::getInputArguments( __inout vnop_bwrite_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    vnode_t   vnode;
    
    assert( apWrapper->ap );
    assert( NULL == this->fsd );
    
    vnode = buf_vnode( apWrapper->ap->a_bp );
    assert( vnode );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( vnode ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( vnode );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnode to covered one
    //
    if( this->vnode1.coveredVnode )
        buf_setvnode( apWrapper->ap->a_bp, this->vnode1.coveredVnode );
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_bwrite_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_exchange_args_class>::getInputArguments( __inout vnop_exchange_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_fvp && apWrapper->ap->a_tvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_fvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_fvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    error = this->vnode2.getVnodeInfo( apWrapper->ap->a_tvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_fvp = this->vnode1.coveredVnode;
    
    if( this->vnode2.coveredVnode )
        apWrapper->ap->a_tvp = this->vnode2.coveredVnode;
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_exchange_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_copyfile_args_class>::getInputArguments( __inout vnop_copyfile_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_fvp && apWrapper->ap->a_tvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_fvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_fvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    error = this->vnode2.getVnodeInfo( apWrapper->ap->a_tvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_fvp = this->vnode1.coveredVnode;
    
    if( this->vnode2.coveredVnode )
        apWrapper->ap->a_tvp = this->vnode2.coveredVnode;
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_copyfile_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_mkdir_args_class>::getInputArguments( __inout vnop_mkdir_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_dvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_dvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_dvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveredVnode;
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_mkdir_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_mknod_args_class>::getInputArguments( __inout vnop_mknod_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_dvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_dvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_dvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveredVnode;
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_mknod_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_rename_args_class>::getInputArguments( __inout vnop_rename_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_fvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_fvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_fvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    if( apWrapper->ap->a_tvp ){
        
        error = this->vnode2.getVnodeInfo( apWrapper->ap->a_tvp );
        assert( KERN_SUCCESS == error );
        if( KERN_SUCCESS != error )
            goto __exit;
        
    }
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_fvp = this->vnode1.coveredVnode;
    
    if( this->vnode2.coveredVnode ){
     
        assert( this->vnode2.coveringVnode == apWrapper->ap->a_tvp );
        apWrapper->ap->a_tvp = this->vnode2.coveredVnode;
    }
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_rename_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_symlink_args_class>::getInputArguments( __inout vnop_symlink_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_dvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_dvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_dvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveredVnode;
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_symlink_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------

template<>
errno_t
DldVnopArgs<vnop_whiteout_args_class>::getInputArguments( __inout vnop_whiteout_args_class* apWrapper )
{
    
    errno_t   error = KERN_SUCCESS;
    
    assert( apWrapper->ap && apWrapper->ap->a_dvp );
    assert( NULL == this->fsd );
    
    this->fsd = DldCoveringFsd::GetCoveringFsd( vnode_mount( apWrapper->ap->a_dvp ) );
    assert( this->fsd );
    if( !this->fsd )
        goto __exit;
    
    error = this->vnode1.getVnodeInfo( apWrapper->ap->a_dvp );
    assert( KERN_SUCCESS == error );
    if( KERN_SUCCESS != error )
        goto __exit;
    
    //
    // replace the covering vnodes to covered ones
    //
    if( this->vnode1.coveredVnode )
        apWrapper->ap->a_dvp = this->vnode1.coveredVnode;
    
    this->flags.released = 0x0;
    
__exit:
    
    if( error )
        this->DldVnopArgs<vnop_whiteout_args_class>::putInputArguments( apWrapper );
    
    return error;
}

//--------------------------------------------------------------------
#endif // _DLD_MACOSX_CAWL
