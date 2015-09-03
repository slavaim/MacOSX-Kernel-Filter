/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/vnode_if.h>
#include <libkern/OSAtomic.h>
#include "DldSparseFile.h"
#include "DldIOVnode.h"
#include "DldVnodeHashTable.h"
#include "DldVfsMntHook.h"

//--------------------------------------------------------------------

extern vfs_context_t gVfsContextSuser;

SInt32  gBTreeNodeAllocationCounter = 0x0;

#if defined(DBG)
#define DldDeclareReeferenceTaken      int referencesTaken = 0x0;
#define DldIncrementReferenceTaken()   do{ ++referencesTaken; } while(false);
#define DldDecrementReferenceTaken()   do{ assert( referencesTaken > 0 ); --referencesTaken; } while(false);
#define DldCheckReferenceTaken( R )    do{ assert( (R) == referencesTaken); } while(false);
#else
#define DldDeclareReeferenceTaken
#define DldIncrementReferenceTaken()
#define DldDecrementReferenceTaken()
#define DldCheckReferenceTaken( R )
#endif // DBG

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldSparseFile, OSObject )

//--------------------------------------------------------------------

LIST_ENTRY   DldSparseFile::sSparseFileListHead;
IORWLock*    DldSparseFile::sSparseFileListLock = NULL;
thread_t     DldSparseFile::sCachedNodesSetTrimmerThread = NULL;
UInt32       DldSparseFile::sCachedNodeReaperEvent;
UInt32       DldSparseFile::sCachedNodeReaperTerminatedEvent;
bool         DldSparseFile::sTerminateCachedNodeReaper = false;
UInt32       DldSparseFile::sCacheUsageThreshold = 0x4;
int          DldSparseFile::sGrowDivisor = 0x2;

//--------------------------------------------------------------------

IOReturn DldSparseFile::sInitSparseFileSubsystem()
{
    IOReturn  RC = kIOReturnSuccess;
    
    InitializeListHead( &DldSparseFile::sSparseFileListHead );
    DldInitSynchronizationEvent( &DldSparseFile::sCachedNodeReaperEvent );
    DldInitSynchronizationEvent( &DldSparseFile::sCachedNodeReaperTerminatedEvent );
    
    DldSparseFile::sSparseFileListLock = IORWLockAlloc();
    assert( DldSparseFile::sSparseFileListLock );
    if( !DldSparseFile::sSparseFileListLock ){
        
        DBG_PRINT_ERROR(("IORWLockAlloc() failed\n"));
        RC = kIOReturnNoMemory;
        
        goto __exit;
    }
    
#ifndef _DLD_MACOSX_CAWL
    return RC;
#endif
    
    //
    // start the thread that takes care of unused cached nodes freeing,
    // actually the function returns the Mach error
    //
    RC = kernel_thread_start ( ( thread_continue_t ) &DldSparseFile::sCachedNodesSetTrimmer,
                               NULL,
                               &DldSparseFile::sCachedNodesSetTrimmerThread );
	assert( KERN_SUCCESS == RC );
    if ( KERN_SUCCESS != RC ) {
        
        DBG_PRINT_ERROR(("kernel_thread_start() failed with an error(%u)\n", RC));
        goto __exit;
    }
    
__exit:
    
    //
    // in case of error the caller must call DldSparseFile::sFreeSparseFileSubsystem()
    // to free alocated resources
    //
    return RC;
}

//--------------------------------------------------------------------

void DldSparseFile::sFreeSparseFileSubsystem()
{
    
    if( DldSparseFile::sCachedNodesSetTrimmerThread ){
        
        //
        // stop the thread
        //
        DldSparseFile::sTerminateCachedNodeReaper = true;
        
        //
        // wake up the thread
        //
        DldSetSynchronizationEvent( &DldSparseFile::sCachedNodeReaperEvent );
        
        //
        // release the thread object
        //
        thread_deallocate( DldSparseFile::sCachedNodesSetTrimmerThread );
        
        //
        // wait for the termination
        //
        DldWaitForSynchronizationEvent( &DldSparseFile::sCachedNodeReaperTerminatedEvent );
        
    } // end if( DldSparseFile::sCachedNodesSetTrimmerThread )
    
    if( DldSparseFile::sSparseFileListLock )
        IORWLockFree( DldSparseFile::sSparseFileListLock );
}

//--------------------------------------------------------------------

void DldSparseFile::sCachedNodesSetTrimmer( void* )
{
    assert( DldSparseFile::sSparseFileListLock );
    
    while( true ){
        
        LIST_ENTRY   staleObjectsListHead;
        
        //
        // wait on the event with a timeout
        //
        DldWaitForSynchronizationEventWithTimeout( &DldSparseFile::sCachedNodeReaperEvent, 10000 ); // 10 secs timeout
        if( DldSparseFile::sTerminateCachedNodeReaper )
            break;
        
        InitializeListHead( &staleObjectsListHead );
        
        IORWLockWrite( DldSparseFile::sSparseFileListLock );
        { // start of the locked region
            
            for( PLIST_ENTRY entry = DldSparseFile::sSparseFileListHead.Flink;
                 entry != &DldSparseFile::sSparseFileListHead;
                 entry = entry->Flink )
            {
                
                UInt32           cacheUnderuse;
                DldSparseFile*   sparseFile = CONTAINING_RECORD( entry, DldSparseFile, listEntry );
                
                //
                // at least the root must be in the cache
                //
                assert( sparseFile->numberOfCachedNodes >= 0x1 );
                
                //
                // some sanity check, the operation is not an exact comparision
                // with the accessedCachedNodesCount value as it is incremented w/o any lock
                //
                assert( (2 * sparseFile->numberOfCachedNodes) > sparseFile->accessedCachedNodesCount );
                
                cacheUnderuse = 1 + sparseFile->numberOfCachedNodes/(sparseFile->accessedCachedNodesCount + 0x1);
                
                assert( cacheUnderuse < 25*DldSparseFile::sCacheUsageThreshold ); // check that the cache shrinking is functional
                assert( DldSparseFile::sCacheUsageThreshold > 1 );
                
                if( sparseFile->accessed && cacheUnderuse < DldSparseFile::sCacheUsageThreshold ){
                    
                    //
                    // mark as not accessed, the nodes will be reaped if the file
                    // is not accessed on the next time interval
                    //
                    sparseFile->accessed = false;
                    
                    //
                    // set the access count to zero, so the next time all unsued nodes
                    // will be purged, the used ones will be preserved
                    //
                    sparseFile->accessedCachedNodesCount = 0x0;
                    
                } else {
                    
                    InsertTailList( &staleObjectsListHead, &sparseFile->staleListEntry );
                    sparseFile->retain();
                }
            } // end for
            
        } // end of the locked region
        IORWLockUnlock( DldSparseFile::sSparseFileListLock );
        
        while( !IsListEmpty( &staleObjectsListHead ) ){
            
            //
            // remove all cached nodes except the root
            //
            DldSparseFile*   sparseFile = CONTAINING_RECORD( RemoveHeadList( &staleObjectsListHead ), DldSparseFile, staleListEntry );
            
            sparseFile->flushAndPurgeCachedNodes( false );
            sparseFile->release();
        }
        
    } // end while( true )
    
    //
    // wake up a thread waiting for termination
    //
    DldSetSynchronizationEvent( &DldSparseFile::sCachedNodeReaperTerminatedEvent );
    
    thread_terminate( current_thread() );
}

//--------------------------------------------------------------------

bool
DldSparseFile::init()
{
    /*
     for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( this->offsetMapCache ); ++i )
        this->offsetMapCache[ i ].sparseFileOffset = DldSparseFile::InvalidOffset;
     */
    
    InitializeListHead( &this->cachedBTreeNodesList );
    
    this->oldestWrittenBlock = DldSparseFile::InvalidOffset;
    
    //
    // there is no valid map cache
    //
    // this->validDataMapCacheOffset = DldSparseFile::InvalidOffset;
    
    //bzero( &this->validDataMapCache, sizeof( this->validDataMapCache ) );
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( !this->rwLock )
        return false;
    
    return true;
}

//--------------------------------------------------------------------

void DldSparseFile::free()
{
    assert( preemption_enabled() );
    assert( IsListEmpty( &this->listEntry ) );
    
    //
    // save the data and B-Trees so the file can be used
    // to retrieve data
    //
    this->flags.preserveDataFile = 0x1;
    
#if defined(DBG)
    DldSparseFilesHashTable::sSparseFilesHashTable->LockShared();
    { // strat of the lock
        
        //
        // check for undereference, when the entry is left in the hash table
        //
        assert( this != DldSparseFilesHashTable::sSparseFilesHashTable->RetrieveEntry( this->identificationInfo.cawlCoveredVnodeID, false ) );
        
    } // end of the lock
    DldSparseFilesHashTable::sSparseFilesHashTable->UnLockShared();
#endif // DBG
    
    if( this->dataFile.freeChunksDescriptor ){
        
        errno_t error;
        
        //
        // flush the free chunks descriptor
        //
        assert( DldSparseFile::InvalidOffset != this->dataFile.firstFreeChunk );
        
        error = this->writeChunkToDataFile( this->dataFile.freeChunksDescriptor, this->dataFile.firstFreeChunk );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(( "this->writeChunkToDataFile( this->dataFile.freeChunksDescriptor, this->dataFile.firstFreeChunk ) failed with an error(%u)\n", error ));
            
            //
            // in case of error remember at least the already flushed descriptor
            //
            this->dataFile.firstFreeChunk = this->dataFile.freeChunksDescriptor->flink;
        } // end if( error )
        
        IOFree( this->dataFile.freeChunksDescriptor, DldSparseFile::BlockSize );
        
    } // end if( this->dataFile.freeChunksDescriptor )
    
    //
    // flush the header
    //
    void* block = IOMalloc( DldSparseFile::BlockSize );
    assert( block );
    if( block ){
        
        errno_t     error;
        FileHeader*  header = (FileHeader*)block;
        
        //
        // read in the current header to retrieve the identification info
        //
        error = this->readChunkFromDataFile( block, DldSparseFile::HeaderOffset );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("readChunkFromDataFile() failed with an error(%u)\n", error));
            // continue even if there was an error
            
        } else {
            
            assert( DLD_SPARSE_FILE_SIGNATURE == header->signature );
        }
        
        header->signature = DLD_SPARSE_FILE_SIGNATURE;
        
        header->dataFile = this->dataFile;
        
        if( this->sparseFileOffsetBTree && this->sparseFileOffsetBTree->root )
            header->sparseFileOffsetBTreeRoot = this->sparseFileOffsetBTree->root->h.nodeOffset;
        else
            header->sparseFileOffsetBTreeRoot = DldSparseFile::InvalidOffset;
        
        header->oldestWrittenBlock = this->oldestWrittenBlock;
        
        error = this->writeChunkToDataFile( block, DldSparseFile::HeaderOffset );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("writeChunkToDataFile() failed with an error(%u)\n", error));
        }
        
        IOFree( block, DldSparseFile::BlockSize );
        
    } else {
        
        DBG_PRINT_ERROR(("block = IOMalloc( DldSparseFile::BlockSize ) failed\n"));
    }
    
    if( this->sparseFileOffsetBTree )
        this->deleteBTree( this->sparseFileOffsetBTree, false );
    
    //
    // flush the cached data and cached nodes,
    // close the data file
    //
    this->freeDataFile();
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
    assert( IsListEmpty( &this->cachedBTreeNodesList ) );
    assert( !this->inHashTable );
}

//--------------------------------------------------------------------

void DldSparseFile::insertInList()
{
    assert( preemption_enabled() );
    assert( IsListEmpty( &this->listEntry ) );
    
    if( !IsListEmpty( &this->listEntry ) )
        return;
    
    //
    // take a reference before inserting in the list
    //
    this->retain();
    
    IORWLockWrite( DldSparseFile::sSparseFileListLock );
    { // start of the locked region
        
        InsertHeadList( &DldSparseFile::sSparseFileListHead, &this->listEntry );
        
    } // end of the locked region
    IORWLockUnlock( DldSparseFile::sSparseFileListLock );
}

//--------------------------------------------------------------------

void DldSparseFile::removeFromList()
{
    assert( !IsListEmpty( &this->listEntry ) );
    
    if( IsListEmpty( &this->listEntry ) )
        return;
    
    IORWLockWrite( DldSparseFile::sSparseFileListLock );
    { // start of the locked region
        
        RemoveEntryList( &this->listEntry );
        
    } // end of the locked region
    IORWLockUnlock( DldSparseFile::sSparseFileListLock );
    
    InitializeListHead( &this->listEntry );
    
    //
    // the object was referenced when was inserted in the list
    //
    this->release();
}

//--------------------------------------------------------------------

void DldSparseFile::prepareForReclaim()
{
    this->decrementUsersCount();
}

//--------------------------------------------------------------------

bool DldSparseFile::incrementUsersCount()
{
    UInt32  currentCount = this->activeUsersCount;
    
    assert( currentCount >= 0x0 );
    
    if( 0x0 == currentCount )
        return false;
    
    while( !OSCompareAndSwap( currentCount, currentCount+0x1, &this->activeUsersCount ) ){
        
        currentCount = this->activeUsersCount;
        if( 0x0 == currentCount )
            return false;
        
    } // end while
    
    return true;
}

//--------------------------------------------------------------------

void DldSparseFile::decrementUsersCount()
{
    assert( this->activeUsersCount > 0x0 );
    
    if( 0x1 != OSDecrementAtomic( &this->activeUsersCount) )
        return;
    
    //
    // the last user has gone
    //
    
    //
    // the list takes a reference
    //
    this->removeFromList();
    
    //
    // remove from the hash
    //
    assert( DldSparseFilesHashTable::sSparseFilesHashTable );
    DldSparseFilesHashTable::sSparseFilesHashTable->RemoveEntryByObject( this );
    
    this->exchangeCawlRelatedVnode( NULL );
}

//--------------------------------------------------------------------

/*
 
 FYI a call stack for an executable image loading when the data is in the sparse file
 
 #0  DldSparseFile::withPath (sparseFilePath=0x320c3634 "/work/cawl/d9ed7232_7c6e2397_8315e82f_bc7eb819", cawlCoveringVnode=0xad5a7c0, cawlCoveredVnodeID=0x320c3624 "2r?ٗ#n|/?\025?\031?~?/work/cawl/d9ed7232_7c6e2397_8315e82f_bc7eb819", identificationString=0xa283bf0 "/Volumes/NO NAME/test5/build/Debug/test5") at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:224
 #1  0x470f0511 in DldCoveringFsd::createCawlSparseFile (this=0x7cbb8f0, coveredVnode=0x320c3ad8, vfsContext=0x7c8c870) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldCoveringVnode.cpp:736
 #2  0x470f0d69 in DldCoveringFsd::rwData (this=0x7cbb8f0, coveredVnode=0x320c3ad8, args=0x320c39e4) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldCoveringVnode.cpp:1064
 #3  0x470f542c in DldCoveringFsd::processRead (this=0x7cbb8f0, coverdVnode=0x320c3ad8, ap=0x320c3b6c, result=0x320c3b0c) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldCoveringVnode.cpp:2263
 #4  0x471dcb5f in DldFsdReadHook (ap=0x320c3b6c) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldVNodeHook.cpp:776
 #5  0x0032ec0c in VNOP_READ (vp=0xad5a7c0, uio=0x320c3bdc, ioflag=8, ctx=0x320c3c18) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:3458
 #6  0x00324412 in vn_rdwr_64 (rw=UIO_READ, vp=0xad5a7c0, base=755036160, len=4096, offset=0, segflg=UIO_SYSSPACE, ioflg=8, cred=0x5a429ac, aresid=0x320c3c88, p=0x8d67010) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_vnops.c:686
 #7  0x003244dd in vn_rdwr (rw=UIO_READ, vp=0xad5a7c0, base=0x2d00f000 "????\a", len=4096, offset=170786352, segflg=UIO_SYSSPACE, ioflg=8, cred=0x5a429ac, aresid=0x320c3e5c, p=0x8d67010) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_vnops.c:615
 #8  0x004cd2c5 in exec_activate_image (imgp=0xa04d014) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_exec.c:1285
 #9  0x004cd577 in __mac_execve (p=0x8d67010, uap=0x320c3f00, retval=0x72c2e54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_exec.c:2182
 #10 0x004cd6db in execve (p=0x8d67010, uap=0x72f6c58, retval=0x72c2e54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_exec.c:2103
 #11 0x0054e9fd in unix_syscall64 (state=0x72f6c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365 
 */

DldSparseFile* 
DldSparseFile::withPath( __in const char* sparseFilePath,
                         __in_opt vnode_t cawlCoveringVnode,
                         __in_opt unsigned char* cawlCoveredVnodeID, // char bytes[16]
                         __in_opt const char* identificationString )
{
    DldSparseFile*  sparseFile;
    
    assert( preemption_enabled() );
    
    sparseFile = new DldSparseFile();
    assert( sparseFile );
    if( !sparseFile ){
        
        DBG_PRINT_ERROR(("sparseFile = new DldSparseFile() failed\n"));
        return NULL;
    }
    
    InitializeListHead( &sparseFile->listEntry );
    
    sparseFile->mnt = vnode_mount( cawlCoveringVnode );
    assert( sparseFile->mnt );
    
    sparseFile->dataFile.vnode = NULLVP;
    //sparseFile->validDataMapFile = NULLVP;
    
    if( !sparseFile->init() ){
        
        assert( !"sparseFile->init()" );
        DBG_PRINT_ERROR(("sparseFile->init() failed\n"));
        
        sparseFile->release();
        return NULL;
    }
    
    //
    // create the data file which contains both the data and B-Tree
    //
    IdentificatioInfo  info;
    
    bzero( &info, sizeof( info ) );
    
    info.identificationString = identificationString;
    info.cawlCoveringVnode    = cawlCoveringVnode;
    if( cawlCoveredVnodeID )
        memcpy( info.cawlCoveredVnodeID, cawlCoveredVnodeID, sizeof(info.cawlCoveredVnodeID) );
    
    if( KERN_SUCCESS != sparseFile->createDataFileByName( sparseFilePath, &info ) ){
        
        assert( !"createBackingFileByName" );
        DBG_PRINT_ERROR(("createBackingFileByName( %s ) failed\n", sparseFilePath));
        
        sparseFile->release();
        return NULL;
    }
    
    //
    // create the B-Tree for sparse file offsets
    //
    sparseFile->sparseFileOffsetBTree = sparseFile->createBTree( compareOffsets,
                                                                 sparseFile->sparseFileOffsetBTreeRootInit,
                                                                 false );
    assert( sparseFile->sparseFileOffsetBTree );
    if( !sparseFile->sparseFileOffsetBTree ){
        
        DBG_PRINT_ERROR(("sparseFileOffsetBTree = sparseFile->createBTree( compareOffsets, false ) failed\n"));
        
        sparseFile->release();
        return NULL;
    }
    
    assert( 0x0 == sparseFile->activeUsersCount );
    sparseFile->activeUsersCount = 0x1;
    
    //
    // all sparse files are anchored in the global list
    //
    sparseFile->insertInList();
    
    return sparseFile;
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::createDataFileByName( __in const char* sparseFilePath, __in IdentificatioInfo*  identificationInfo )
{    
    errno_t     error;
    void*       block = NULL;
    
    assert( preemption_enabled() );
    
    bzero( &this->dataFile, sizeof(this->dataFile) );
    
    //
    // allocated a block for the header
    //
    block = IOMalloc( DldSparseFile::BlockSize );
    assert( block );
    if( NULL == block ){
        
        DBG_PRINT_ERROR(("block = IOMalloc( DldSparseFile::BlockSize ) failed\n"));
        return ENOMEM;
    }
    
    this->dataFile.vfsContext = vfs_context_create( gVfsContextSuser ); // I want to be a superuser
    assert( this->dataFile.vfsContext );
    if( !this->dataFile.vfsContext ){
        
        DBG_PRINT_ERROR(("vfs_context_create( %s ) failed \n", sparseFilePath));
        error = ENOMEM;
        
        goto __exit;
    }
        
    //
    // a first attempt,
    // open or create a file, if the file exists it should be verified ( TO DO )
    //
    error = vnode_open( sparseFilePath,
                        O_EXLOCK | O_RDWR | O_SYNC, // fmode
                        0644,// cmode
                        0x0,// flags
                        &this->dataFile.vnode,
                        this->dataFile.vfsContext );
    if( !error ){
        
        //
        // the file exists
        //
        
        FileHeader*  header = NULL;
        
        assert( sizeof( DldSparseFile::FileHeader ) <= DldSparseFile::BlockSize );
        
        //
        // get the covered file size and report it to UBC
        //
        vnode_attr va = { 0x0 };
        VATTR_INIT( &va );
        VATTR_WANTED( &va, va_data_size );

        error = vnode_getattr( this->dataFile.vnode, &va, this->dataFile.vfsContext );
        if( error ){
            
            DBG_PRINT_ERROR(("vnode_getattr() failed with an error(%u)\n", error));
            goto __eixt_file_exist;
        }
        
        this->dataFile.fileSize = va.va_data_size;
        assert( this->dataFile.fileSize > DldSparseFile::BlockSize && 0x0 == this->dataFile.fileSize % (DldSparseFile::BlockSize) );
        
        //
        // set the brake to the file end and the free block descriptor to invalid data,
        // this preparations are required for readChunkFromDataFile correctly
        // processes the request and doesn't damage the data in the file
        //
        this->dataFile.breakOffset    = this->dataFile.fileSize;
        this->dataFile.firstFreeChunk = DldSparseFile::InvalidOffset;
        
        //
        // read the header
        //
        error = this->readChunkFromDataFile( block, DldSparseFile::HeaderOffset );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("readChunkFromDataFile() failed with an error(%u)\n", error));
            goto __eixt_file_exist;
        }
        
        //
        // read the parameters from the header
        //
        header = (FileHeader*)block;
        
#if defined(DBG)
        assert( DLD_SPARSE_FILE_SIGNATURE == header->signature );
#endif // DBG
        assert( 0x0 == memcmp( header->identificationInfo.cawlCoveredVnodeID,
                               identificationInfo->cawlCoveredVnodeID,
                               sizeof( identificationInfo->cawlCoveredVnodeID ) ) );
        
        if( DldSparseFile::InvalidOffset != header->dataFile.firstFreeChunk ){
            
            //
            // allocated a block for the free chunks descriptor
            //
            this->dataFile.freeChunksDescriptor = (FreeChunkHeader*)IOMalloc( DldSparseFile::BlockSize );
            assert( this->dataFile.freeChunksDescriptor );
            if( NULL == this->dataFile.freeChunksDescriptor ){
                
                DBG_PRINT_ERROR(("this->dataFile.freeChunksDescriptor = IOMalloc( DldSparseFile::BlockSize ) failed\n"));
                error = ENOMEM;
                goto __eixt_file_exist;
            }

            error = this->readChunkFromDataFile( block, header->dataFile.firstFreeChunk );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("readChunkFromDataFile() failed with an error(%u)\n", error));
                goto __eixt_file_exist;
            }
            
            assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries > 0x0 );
            assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries <= DldSparseFile::sFreeChunksArrayCapacity );
            
        } else {
            
            this->dataFile.firstFreeChunk = DldSparseFile::InvalidOffset;
        }
        
        this->dataFile.breakOffset          = header->dataFile.breakOffset;
        this->sparseFileOffsetBTreeRootInit = header->sparseFileOffsetBTreeRoot;
        this->oldestWrittenBlock            = header->oldestWrittenBlock;
        this->identificationInfo            = header->identificationInfo;
        
        assert( this->dataFile.firstFreeChunk <= this->dataFile.fileSize ||
                DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk );
        assert( this->dataFile.breakOffset <= this->dataFile.fileSize );
        assert( this->sparseFileOffsetBTreeRootInit <= this->dataFile.fileSize &&
                DldSparseFile::InvalidOffset != this->sparseFileOffsetBTreeRootInit );
        
    __eixt_file_exist:;
        
        //
        // if there is an error continue - truncate the file and update its data
        //
        
    } else {
        
        this->dataFile.vnode = NULL;
    }// end for else for if( !error )
    
    
    //
    // a second attempt
    //
    if( error ){
        
        //
        // the file doesn't exist or should be recreated ( i.e. truncated )
        //
        error = vnode_open( sparseFilePath,
                           O_EXLOCK | O_RDWR | O_CREAT | O_TRUNC | O_SYNC, // fmode
                           0644,// cmode
                           0x0,// flags
                           &this->dataFile.vnode,
                           this->dataFile.vfsContext );
        assert( !error );
        if( !error ){
            
            //
            // set initial file size to 3 chunks, at least we need one chunk
            // for the header, see the breakOffset initialization below
            //
            this->dataFile.fileSize = 3*DldSparseFile::BlockSize;
            error =	DldVnodeSetsize( this->dataFile.vnode,
                                    this->dataFile.fileSize,
                                    IO_NOZEROFILL, // do not zero, requires super user credentials
                                    this->dataFile.vfsContext );
            
            if( !error ){
                
                this->dataFile.firstFreeChunk       = DldSparseFile::InvalidOffset; // start the allocation from the pool
                this->dataFile.breakOffset          = DldSparseFile::BlockSize; // reserve the space for the header
                this->sparseFileOffsetBTreeRootInit = DldSparseFile::InvalidOffset; // create a new root
                this->identificationInfo            = *identificationInfo;
                
                //
                // write the header
                //
                FileHeader*  header =(FileHeader*)block;
                
                assert( header );
                
                header->signature = DLD_SPARSE_FILE_SIGNATURE;
                
                header->identificationInfo = *identificationInfo;
                if( identificationInfo->identificationString ){
                    
                    strncpy( header->identificationString,
                             identificationInfo->identificationString,
                             sizeof(header->identificationString) );
                }
                
                header->dataFile = this->dataFile;
                header->sparseFileOffsetBTreeRoot = DldSparseFile::InvalidOffset;
                header->oldestWrittenBlock        = DldSparseFile::InvalidOffset;
                
                error = this->writeChunkToDataFile( block, DldSparseFile::HeaderOffset );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("writeChunkToDataFile() failed with an error(%u)\n", error));
                }
                
            } else {
                
                DBG_PRINT_ERROR(("DldVnodeSetsize( %s ) failed with the %u error\n", sparseFilePath, error));
            }
            
            
        } else {
            
            this->dataFile.vnode = NULL;
            DBG_PRINT_ERROR(("vnode_open( %s ) failed with the %u error\n", sparseFilePath, error));
        }
        
    } // end if( error )
    
__exit:
    
    if( this->dataFile.vnode ){
        
        //
        // vn_open returns with both a use_count
        // and an io_count on the found vnode
        // drop the io_count, but keep the use_count
        //
        
        //
        // decrement the io count, the user count is still bumped,
        // this prevents from stalling on shutdown
        //
        vnode_put( this->dataFile.vnode );
    }
    
    if( block )
        IOFree( block, DldSparseFile::BlockSize );
    
    return error;
}

//--------------------------------------------------------------------

void
DldSparseFile::freeDataFile()
{
    assert( preemption_enabled() );
    
    if( NULLVP == this->dataFile.vnode ){
        
        assert( !this->dataFile.vfsContext );
        assert( IsListEmpty( &this->cachedBTreeNodesList ) );
        return;
    }
    
    //
    // delete the B-Tree cached nodes, B-Trees have been already removed
    //
    assert( NULL == this->sparseFileOffsetBTree->root );
    this->flushAndPurgeCachedNodes( true );
    assert( IsListEmpty( &this->cachedBTreeNodesList ) );
    assert( 0x0 == this->numberOfCachedNodes );
    
    //
    // the io_count was dropped just after the vnode creation,
    // so we need to bump it as vnode_close() drops
    // it again, in case of shutdown the vnode has been drained
    // to this moment so vnode_getwithref() fails
    //
    if( KERN_SUCCESS == vnode_getwithref( this->dataFile.vnode ) ){
        
        //
        // TO DO - remove the file, it seems the apple doesn't provide any usable
        // interface from the kernel, also we can reuse vnodes by putting
        // them to a list
        //
        
        assert( this->dataFile.vfsContext );
        vnode_close( this->dataFile.vnode, 0x0, this->dataFile.vfsContext );
    }
    
    if( this->dataFile.vfsContext )
        vfs_context_rele( this->dataFile.vfsContext );
}

//--------------------------------------------------------------------

//
// DldSparseFile::InvalidOffset is returned in case of a failure
//
off_t 
DldSparseFile::allocateChunkFromDataFile( __in bool acquireLock )
{
    errno_t error;
    off_t   allocatedChunk = DldSparseFile::InvalidOffset;
    
    assert( preemption_enabled() );
    assert( NULLVP != this->dataFile.vnode );
    
    assert( 0x0 == (this->dataFile.breakOffset % DldSparseFile::BlockSize) );
    assert( 0x0 == (this->dataFile.fileSize % DldSparseFile::BlockSize) );
    
    assert( (DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk) || 
            0x0 == (this->dataFile.firstFreeChunk % DldSparseFile::BlockSize) );
    
    //
    // bump the io count
    //
    error = vnode_getwithref( this->dataFile.vnode );
    assert( !error );
    if(  KERN_SUCCESS != error ){
        
        DBG_PRINT_ERROR(("vnode_getwithref() failed with an error(%u)\n", error));
        return DldSparseFile::InvalidOffset;
    }
    
    
    if( acquireLock )
        this->LockExclusive();
    { // start of the locked region
        
        //
        // try to retrieve a free chunk from the free chunks descriptor
        //
        if( NULL != this->dataFile.freeChunksDescriptor ){
            
            assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries > 0x0 );
            assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries <= DldSparseFile::sFreeChunksArrayCapacity );
            
            allocatedChunk = this->dataFile.freeChunksDescriptor->entries[ this->dataFile.freeChunksDescriptor->numberOfValidEntries - 0x1 ];
            
            this->dataFile.freeChunksDescriptor->numberOfValidEntries -= 0x1;
            
            if( 0x0 == this->dataFile.freeChunksDescriptor->numberOfValidEntries ){
                
                //
                // the last free entry has gone, we don't need this descriptor any more
                //
                
                off_t  nextDescriptor = this->dataFile.freeChunksDescriptor->flink;
                
                if( DldSparseFile::InvalidOffset != nextDescriptor ){
                    
                    //
                    // reuse the empty descriptor's memory
                    //
                    error = this->readChunkFromDataFile( this->dataFile.freeChunksDescriptor, nextDescriptor );
                    assert( !error );
                    if( error ){
                        
                        DBG_PRINT_ERROR(("readChunkFromDataFile() failed with an error(%u)\n", error));
                        
                        //
                        // we will try to allocate from the break offset
                        //
                        nextDescriptor = DldSparseFile::InvalidOffset;
                    } // end if( error )
                } // end if( DldSparseFile::InvalidOffset != nextDescriptor )
                
                this->dataFile.firstFreeChunk = nextDescriptor;
                
                if( DldSparseFile::InvalidOffset == nextDescriptor ){
                    
                    //
                    // this was the last descriptor
                    //
                    IOFree( this->dataFile.freeChunksDescriptor, DldSparseFile::BlockSize );
                    this->dataFile.freeChunksDescriptor = NULL;
                }
                
            } // end if( 0x0 == this->dataFile.freeChunksDescriptor->numberOfValidEntries )
            
        } // end if( NULL != this->dataFile.freeChunksDescriptor )
        
        assert( DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk ||
                this->dataFile.firstFreeChunk <= (this->dataFile.fileSize - DldSparseFile::BlockSize) );
        
        
        if( DldSparseFile::InvalidOffset == allocatedChunk ){
            
            //
            // allocate from the pool at the end of the data file
            //
            assert( this->dataFile.fileSize >= DldSparseFile::BlockSize );
            assert( this->dataFile.breakOffset <= this->dataFile.fileSize );
            
            if( this->dataFile.breakOffset <= (this->dataFile.fileSize - DldSparseFile::BlockSize) ){
                
                //
                // simply move the break pointer
                //
                allocatedChunk = this->dataFile.breakOffset;
                this->dataFile.breakOffset += DldSparseFile::BlockSize;
                
            } else {
                
                off_t       newSize;
                off_t       bytesToAdd;
                const int   upperLimit = 128*1024*1024; // do not add more than 128 MB at once
                
                //
                // expand the file at fileSize/(DldSparseFile::sGrowDivisor) or BlockSize whatever is bigger,
                // round to the block size
                //
                if( this->dataFile.fileSize/(DldSparseFile::sGrowDivisor) < DldSparseFile::BlockSize )
                    bytesToAdd = DldSparseFile::BlockSize;
                else if( this->dataFile.fileSize/(DldSparseFile::sGrowDivisor) < upperLimit )
                    bytesToAdd = DldSparseFile::roundToBlock( this->dataFile.fileSize/(DldSparseFile::sGrowDivisor) );
                else
                    bytesToAdd = upperLimit;

                assert( 0x0 != bytesToAdd && bytesToAdd == DldSparseFile::alignToBlock( bytesToAdd ) );
                
                newSize = DldSparseFile::roundToBlock( this->dataFile.fileSize + bytesToAdd );
                
                error = DldVnodeSetsize( this->dataFile.vnode,
                                         newSize,
                                         IO_NOZEROFILL | IO_NOAUTH, // do not zero, do not authenticate
                                         this->dataFile.vfsContext );
                assert( !error );
                if( !error ){
                    
                    this->dataFile.fileSize = newSize;
                    
                    //
                    // simply move the break pointer
                    //
                    allocatedChunk = this->dataFile.breakOffset;
                    this->dataFile.breakOffset += DldSparseFile::BlockSize;
                    
                    assert( this->dataFile.breakOffset <= this->dataFile.fileSize );
                    
                } else {
                    
                    assert( DldSparseFile::InvalidOffset == allocatedChunk );
                    DBG_PRINT_ERROR(("vnode_setsize( %lu ) failed\n", (long unsigned int)newSize));
                    
                }// end for else for if( !error )
                
            }// end for else for if( this->dataFile.breakOffset <= (
            
        } // if( DldSparseFile::InvalidOffset == allocatedChunk )
        
        assert( this->dataFile.breakOffset <= this->dataFile.fileSize );
    
    } // end of the locjked region
    if( acquireLock )
        this->UnLockExclusive();
    
    assert( 0x0 == (this->dataFile.breakOffset % DldSparseFile::BlockSize) );
    assert( 0x0 == (this->dataFile.fileSize % DldSparseFile::BlockSize) );
    
    assert( (DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk) ||
            0x0 == (this->dataFile.firstFreeChunk % DldSparseFile::BlockSize) );
    
    assert( (DldSparseFile::InvalidOffset == allocatedChunk) || 0x0 == (allocatedChunk % DldSparseFile::BlockSize) ); 
    
    //
    // decrement the io count
    //
    vnode_put( this->dataFile.vnode );
    
    return allocatedChunk;
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::returnChunkToFreeList( __in off_t chunkOffset, __in bool acquireLock )
{
    errno_t error = KERN_SUCCESS;
    
    assert( preemption_enabled() );
    assert( NULLVP != this->dataFile.vnode );
    
    assert( 0x0 == (this->dataFile.breakOffset % DldSparseFile::BlockSize) );
    assert( 0x0 == (this->dataFile.fileSize % DldSparseFile::BlockSize) );
    
    assert( (DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk) ||
            0x0 == (this->dataFile.firstFreeChunk % DldSparseFile::BlockSize) );
    
    assert( (DldSparseFile::InvalidOffset == chunkOffset) || 0x0 == (chunkOffset % DldSparseFile::BlockSize) ); 
    
    //
    // bump the io count
    //
    error = vnode_getwithref( this->dataFile.vnode );
    assert( !error );
    if(  KERN_SUCCESS != error ){
        
        DBG_PRINT_ERROR(("vnode_getwithref() failed with an error(%u)\n", error));
        return error;
    }
    
    if( acquireLock )
        this->LockExclusive();
    { // start of the locked region
        
        if( this->dataFile.breakOffset == (chunkOffset + DldSparseFile::BlockSize) ){
            
            //
            // the chunk can be returned to the pool as it is a chunk adjacent to the break position
            //
            this->dataFile.breakOffset -= DldSparseFile::BlockSize;
            
        } else {
            
            //
            // return to the free list
            //
            
            if( NULL != this->dataFile.freeChunksDescriptor ){
                
                assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries > 0x0 );
                assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries <= DldSparseFile::sFreeChunksArrayCapacity );
                
                if( DldSparseFile::sFreeChunksArrayCapacity == this->dataFile.freeChunksDescriptor->numberOfValidEntries ){
                    
                    //
                    // there is no room for the new entry
                    //
                    
                    off_t  newDescriptorOffset;
                    
                    //
                    // borrow the last entry to back a new descriptor
                    //
                    newDescriptorOffset = this->dataFile.freeChunksDescriptor->entries[ this->dataFile.freeChunksDescriptor->numberOfValidEntries - 0x1 ];
                    this->dataFile.freeChunksDescriptor->numberOfValidEntries -= 0x1;
                    
                    //
                    // flush the nearly full descriptor
                    //
                    error = this->writeChunkToDataFile( this->dataFile.freeChunksDescriptor, this->dataFile.firstFreeChunk );
                    assert( !error );
                    if( error ){
                        
                        DBG_PRINT_ERROR(( "this->writeChunkToDataFile( this->dataFile.freeChunksDescriptor, this->dataFile.firstFreeChunk ) failed with an error(%u)\n", error ));
                        goto __exit;
                    } // end if( error )
                    
                    //
                    // replace with the new one, chain the old and the new descriptors
                    //
                    this->dataFile.freeChunksDescriptor->numberOfValidEntries = 0x0;
                    this->dataFile.freeChunksDescriptor->flink = this->dataFile.firstFreeChunk;
                    this->dataFile.firstFreeChunk = newDescriptorOffset;
                }
                
            } else {
                
                //
                // there is no descriptor
                //
                assert( NULL == this->dataFile.freeChunksDescriptor );
                assert( DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk );
                
                //
                // allocate the block to support the descriptor
                //
                this->dataFile.firstFreeChunk = this->allocateChunkFromDataFile( false );
                assert( DldSparseFile::InvalidOffset != this->dataFile.firstFreeChunk );
                if( DldSparseFile::InvalidOffset == this->dataFile.firstFreeChunk ){
                    
                    DBG_PRINT_ERROR(( "this->dataFile.firstFreeChunk = this->allocateChunkFromDataFile( false ) failed\n" ));
                    error = ENOMEM;
                    goto __exit;
                }
                
                //
                // allocate the memory for a descriptor
                //
                this->dataFile.freeChunksDescriptor = (FreeChunkHeader*)IOMalloc( DldSparseFile::BlockSize );
                assert( this->dataFile.freeChunksDescriptor );
                if( !this->dataFile.freeChunksDescriptor ){
                    
                    //
                    // return it back, it is safe as the allocation was made from the break offset and will be returned
                    // back w/o allocating a descriptor - because of the lock being held there was no any new allocations
                    //
                    this->returnChunkToFreeList( this->dataFile.firstFreeChunk , false );
                    this->dataFile.firstFreeChunk = DldSparseFile::InvalidOffset;
                    DBG_PRINT_ERROR(( "newDescriptor = (FreeChunkHeader*)IOMalloc( DldSparseFile::BlockSize ) failed\n" ));
                    error = ENOMEM;
                    goto __exit;
                }
                
                this->dataFile.freeChunksDescriptor->numberOfValidEntries = 0x0;
                this->dataFile.freeChunksDescriptor->flink = DldSparseFile::InvalidOffset;
            }
            
            //
            // there is a room for a new entry,
            // simply add the new entry
            //
            
            assert( this->dataFile.freeChunksDescriptor );
            assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries >= 0x0 );
            assert( this->dataFile.freeChunksDescriptor->numberOfValidEntries < DldSparseFile::sFreeChunksArrayCapacity );
            
            this->dataFile.freeChunksDescriptor->entries[ this->dataFile.freeChunksDescriptor->numberOfValidEntries ] = chunkOffset;
            ++this->dataFile.freeChunksDescriptor->numberOfValidEntries;
            
        } // end for else
        
    __exit:;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    //
    // decrement the io count
    //
    vnode_put( this->dataFile.vnode );
    
    return error;
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::readChunkFromDataFile( __in void* buffer, __in off_t offsetInDataFile )
{
    errno_t error;
    
    assert( preemption_enabled() );
    assert( 0x0 == (offsetInDataFile % DldSparseFile::BlockSize) );
    assert( this->dataFile.breakOffset <= this->dataFile.fileSize );
    assert( offsetInDataFile < this->dataFile.breakOffset );
    
    //
    // bump the io count
    //
    error = vnode_getwithref( this->dataFile.vnode );
    assert( !error );
    if(  KERN_SUCCESS != error ){
        
        DBG_PRINT_ERROR(("vnode_getwithref() failed with an error(%u)\n", error));
        return error;
    }
    
    //
    // no nedd to acquire the lock as the space has been reserved
    //
    
    error = vn_rdwr( UIO_READ,
                     this->dataFile.vnode,
                     (char*)buffer, 
                     DldSparseFile::BlockSize,
                     offsetInDataFile,
                     UIO_SYSSPACE,
                     IO_NOAUTH | IO_SYNC,
                     vfs_context_ucred( this->dataFile.vfsContext ),
                     NULL,
                     vfs_context_proc( this->dataFile.vfsContext ) );
    assert( !error );
    if( error ){
        
        DBG_PRINT_ERROR(("vn_rdwr() faile with error(%u)\n", error));
    }
    
    vnode_put( this->dataFile.vnode );
    
    return error;
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::writeChunkToDataFile( __in void* buffer, __in off_t offsetInDataFile )
{
    errno_t error;
    
    assert( preemption_enabled() );
    assert( 0x0 == (offsetInDataFile % DldSparseFile::BlockSize) );
    assert( this->dataFile.breakOffset <= this->dataFile.fileSize );
    assert( offsetInDataFile < this->dataFile.breakOffset );
    
    //
    // bump the io count
    //
    error = vnode_getwithref( this->dataFile.vnode );
    assert( !error );
    if( KERN_SUCCESS != error ){
        
        DBG_PRINT_ERROR(("vnode_getwithref() failed with an error(%u)\n", error));
        return error;
    }
    
    //
    // no nedd to acquire the lock as the space has been reserved
    //
    
    error = vn_rdwr( UIO_WRITE,
                     this->dataFile.vnode,
                     (char*)buffer, 
                     DldSparseFile::BlockSize,
                     offsetInDataFile,
                     UIO_SYSSPACE,
                     IO_NOAUTH | IO_SYNC,
                     vfs_context_ucred( this->dataFile.vfsContext ),
                     NULL,
                     vfs_context_proc( this->dataFile.vfsContext ) );
    assert( !error );
    if( error ){
        
        DBG_PRINT_ERROR(("vn_rdwr() faile with error(%u)\n", error));
    }
    
    vnode_put( this->dataFile.vnode );
    
    return error;
}

//--------------------------------------------------------------------

void
DldSparseFile::LockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

void
DldSparseFile::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldSparseFile::LockExclusive()
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
DldSparseFile::UnLockExclusive()
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

//
//	The function creates a btree with just the root node
//	@return The an empty B-tree
//
DldSparseFile::BTree*
DldSparseFile::createBTree( __in CompareKeysFtr  comparator, __in off_t rootNodeOffset, __in bool acquireLock )
{
    
    BTree* newBTree;
    
    assert( preemption_enabled() );
    
    //
    // a node is stashed in a chunk
    //
    assert( sizeof( BTreeNode ) <= DldSparseFile::BlockSize );
    
    newBTree = (BTree*)IOMalloc(sizeof(*newBTree));
    assert( newBTree );
    if( !newBTree ){
        
        DBG_PRINT_ERROR(("IOMalloc(sizeof(*newBTree)) failed\n"));
        return NULL;
    }
    
    bzero( newBTree, sizeof(*newBTree) );

    newBTree->compareKeys = comparator;
    newBTree->damaged = false;
    
    //
    // reserve the space for the root node in the data file
    // OR
    // retrieve the existing root
    //
    newBTree->root = this->getBTreeNodeReference( rootNodeOffset, acquireLock );
    assert( newBTree->root );
    assert( DLD_BTNODE_SIGNATURE == newBTree->root->h.signature );
    if( !newBTree->root ){
        
        DBG_PRINT_ERROR(("getBTreeNodeReference( DldSparseFile::InvalidOffset, true ) failed\n"));
        
        IOFree( newBTree, sizeof(*newBTree) );
        return NULL;
    }
    
    if( DldSparseFile::InvalidOffset == rootNodeOffset ){
        
        //
        // init a new root
        //
        
        newBTree->root->h.leaf = true;
        newBTree->root->h.nrActive = 0;
        newBTree->root->h.level = 0;
        
        //
        // write in the root
        //
        errno_t error;
        error = this->writeChunkToDataFile( newBTree->root, newBTree->root->h.nodeOffset );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("writeChunkToDataFile( newBTree->root, chunk ) failed with an error(%u)\n", error));
            
            //
            // return the chunk back
            //
            this->returnChunkToFreeList( newBTree->root->h.nodeOffset, acquireLock );
            this->dereferenceBTreeNode( newBTree->root, acquireLock );
            
            IOFree( newBTree, sizeof(*newBTree) );
            return NULL;
        }
    } // end if( DldSparseFile::InvalidOffset == rootNodeOffset )
    
    return newBTree;
}

//--------------------------------------------------------------------

void
DldSparseFile::deleteBTree( __in BTree* btree, __in bool acquireLock )
{
    if( btree->root ){
        
        //
        // if 0x1 == this->flags.preserveDataFile then do not remove the B-Tree from the
        // data file, only free the memory allocated for the cached node data
        //
        if( 0x0 == this->flags.preserveDataFile )
            this->returnChunkToFreeList( btree->root->h.nodeOffset, acquireLock );
        
        this->dereferenceBTreeNode( btree->root, acquireLock );
        btree->root = NULL;
    }
    
    IOFree( btree, sizeof(*btree) );
}

//--------------------------------------------------------------------

void
DldSparseFile::referenceBTreeNode( __in BTreeNode* node )
{
    assert( node->h.referenceCount > 0x0 );
    assert( DLD_BTNODE_SIGNATURE == node->h.signature );
    
    OSIncrementAtomic( &node->h.referenceCount );
}

//--------------------------------------------------------------------

void DldSparseFile::flushAndPurgeCachedNodes( __in bool prepareForReclaim )
{
    
    this->LockExclusive();
    { // start of the locked region
        
        assert( preemption_enabled() );
        
        for( PLIST_ENTRY entry = this->cachedBTreeNodesList.Flink;
             entry != &this->cachedBTreeNodesList; ){
            
            BTreeNode* cachedNode;
            
            cachedNode = CONTAINING_RECORD( entry, BTreeNode, h.listEntry );
            assert( DLD_BTNODE_SIGNATURE == cachedNode->h.signature );
            
            //
            // move to the next entry here as the current entry can be removed
            //
            entry = entry->Flink;
            
            assert( cachedNode->h.referenceCount >= 0x1 );
            
            if( !prepareForReclaim ){
                
                //
                // this is a call to flush and purge unused nodes
                //
                
                if( 0x1 != cachedNode->h.referenceCount || cachedNode->h.accessed ){
                    
                    //
                    // the node is in use( referenced or has been accessed ) or this is a root node,
                    // the root is always has a reference count at least 0x2
                    //
                    
                    cachedNode->h.accessed = false;
                    
                    continue;
                }
                
            } // if( !prepareForReclaim && 0x1 != cachedNode->h.referenceCount )
            
            RemoveEntryList( &cachedNode->h.listEntry );
            
            assert( this->numberOfCachedNodes > 0x0 );
            OSDecrementAtomic( &this->numberOfCachedNodes );
            
            //
            // mark as not in the cached list, the node is still referenced by the cached list
            //
            InitializeListHead( &cachedNode->h.listEntry );
            
            //
            // flush it if dirty
            //
            if( cachedNode->h.dirty ){
                
                this->BTreeFlushNode( this->sparseFileOffsetBTree, cachedNode );
                assert( !cachedNode->h.dirty );
                assert( 0x1 == cachedNode->h.referenceCount );
            } // end if( cachedNode->h.dirty )
            
            //
            // the nodes were referenced when were inserted in the list
            //
            assert( 0x1 == cachedNode->h.referenceCount );
            
            //
            // set the reference count to zero, just to behaive conssitently
            // the freeBTreeNodeMemory function must see only zero referenced nodes
            //
            cachedNode->h.referenceCount = 0x0;
            this->freeBTreeNodeMemory( cachedNode );
            
        } // end for()
        
        //
        // prepare for the next round
        //
        this->accessedCachedNodesCount = 0x0;
        
    } // end of the locked region
    this->UnLockExclusive();
}

//--------------------------------------------------------------------

DldSparseFile::BTreeNode*
DldSparseFile::referenceCachedBTreeNodeByOffset( __in off_t offset, __in bool acquireLock )
{
    assert( preemption_enabled() );
    
    BTreeNode* foundNode = NULL;
    
    if( acquireLock )
        this->LockShared();
    { // start of the lock
        
        //
        // search through the list for a cached node
        //
        for( LIST_ENTRY* entry = this->cachedBTreeNodesList.Flink; entry != &this->cachedBTreeNodesList; entry = entry->Flink ){
            
            BTreeNode* cachedNode;
            
            cachedNode = CONTAINING_RECORD( entry, BTreeNode, h.listEntry );
            
            assert( DLD_BTNODE_SIGNATURE == cachedNode->h.signature );
            
            if( cachedNode->h.nodeOffset == offset ){
                
                //
                // reference and return the node
                //
                this->referenceBTreeNode( cachedNode );
                foundNode = cachedNode;
                break;
                
            } // end if( cachedNode->h.nodeOffset == offset )
            
        } // end for( LIST_ENTRY* entry = btree->cachedListHead;
        
    } // end of the lock
    if( acquireLock )
        this->UnLockShared();
    
    assert( !( foundNode && 0x0 == foundNode->h.referenceCount ) );
    
    //
    // increment the accessed nodes counter if this
    // is a first time when the node is accessed
    //
    if( foundNode && !foundNode->h.accessed ){
        
        //
        // do not bother about concurrency, this is not an exact operation,
        // the data is used for a heuristic algorithm
        //
        ++this->accessedCachedNodesCount;
        foundNode->h.accessed = true;
    }
    
    return foundNode;
}

//--------------------------------------------------------------------

//
// Function is used to allocate memory for the btree node
// OR
// read in the node from a secondary storage
// @param leaf boolean set true for a leaf node
// @return The allocated B-tree node
//
DldSparseFile::BTreeNode*
DldSparseFile::getBTreeNodeReference( __in off_t offset, __in bool acquireLock )
{
    BTreeNode* newNode;
    
    assert( preemption_enabled() );
    
    //
    // say the reaper thread that the sparse file is active
    //
    this->accessed = true;
    
    if( DldSparseFile::InvalidOffset != offset ){
        
        //
        // search in the cache
        //
        newNode = this->referenceCachedBTreeNodeByOffset( offset, acquireLock );
        if( newNode )
            return newNode;
    }
    
    //
    // Allocate memory for the node
    //
    newNode = (BTreeNode*)IOMalloc( sizeof(*newNode) );
    assert( newNode );
    if( !newNode ){
        
        DBG_PRINT_ERROR(("IOMalloc( sizeof(*newNode) ) failed\n"));
        return NULL;
    }
    
    bzero( newNode, sizeof(*newNode) );
    
    if( DldSparseFile::InvalidOffset == offset ){
    
        //
        // reserve the space for the root node in the data file
        //
        newNode->h.nodeOffset = this->allocateChunkFromDataFile( acquireLock );
        assert( DldSparseFile::InvalidOffset != newNode->h.nodeOffset );
        if( DldSparseFile::InvalidOffset == newNode->h.nodeOffset ){
            
            DBG_PRINT_ERROR(("this->allocateChunkFromDataFile( true ) failed\n"));
            
            IOFree( newNode, sizeof(*newNode) );
            return NULL;
        }
        
#if defined(DBG)
        newNode->h.signature = DLD_BTNODE_SIGNATURE;
#endif // DBG
        
        
        //
        // Initialize the number of active nodes
        //
        newNode->h.nrActive = 0;
        
        //
        // Used to determine whether it is a leaf
        //
        newNode->h.leaf = true;
        
        //
        // Use to determine the level in the tree
        //
        newNode->h.level = 0;
        
        for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( newNode->children ) ; ++i ){
            
            newNode->children[ i ] = DldSparseFile::InvalidOffset;
            
            newNode->keyEntries[ i ].tagListEntry.flink = DldSparseFile::InvalidOffset;
            newNode->keyEntries[ i ].tagListEntry.blink = DldSparseFile::InvalidOffset;
        }
        
    } else {
        
        errno_t error;
        
        //
        // read in the existing node
        //
        error = this->readChunkFromDataFile( newNode, offset );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("readChunkFromDataFile() faile with error(%u)\n", error));
            
            IOFree( newNode, sizeof(*newNode) );
            newNode = NULL;
            return NULL;
            
        } // end if( error )
        
        assert( newNode->h.nodeOffset == offset );
        assert( DLD_BTNODE_SIGNATURE == newNode->h.signature );
    }
    
    if( newNode ){
        
        BTreeNode*  cachedNode;
        
        if( acquireLock )
            this->LockExclusive();
        { // start of the lock
            
            cachedNode = this->referenceCachedBTreeNodeByOffset( offset, false );
            if( cachedNode ){
                
                //
                // a concurrent thread sneaked in and fetched or created the node before this thread
                //
                IOFree( newNode, sizeof(*newNode) );
                newNode = cachedNode;
                
            } else {
                
                newNode->h.referenceCount = 0x1;
                
                //
                // insert in the list of cached entries, insert in the list as with
                // a good probability the entry will be searched for very soon
                //
                InsertHeadList( &this->cachedBTreeNodesList, &newNode->h.listEntry );
                
                //
                // reference the entry, all entries in the list are referenced
                //
                this->referenceBTreeNode( newNode );
                
                OSIncrementAtomic( &this->numberOfCachedNodes );
                
#if defined(DBG)
                OSIncrementAtomic( &gBTreeNodeAllocationCounter );
#endif // DBG
            }
            
        } // end of the lock
        if( acquireLock )
            this->UnLockExclusive();

    } // end if( newNode ){
    
    assert( !( newNode && 0x0 == newNode->h.referenceCount ) );
    assert( !( newNode && DldSparseFile::InvalidOffset != offset && offset != newNode->h.nodeOffset ) );
    
    return newNode;
}

//--------------------------------------------------------------------

//
// free the memory occupied by a node, do not remove from the backing store
//
void
DldSparseFile::freeBTreeNodeMemory( __in BTreeNode* node )
{
    assert( node );
    assert( 0x0 == node->h.referenceCount );
    assert( !node->h.dirty );
    assert( DLD_BTNODE_SIGNATURE == node->h.signature );
    
#if defined(DBG)
    assert( gBTreeNodeAllocationCounter > 0 );
    OSDecrementAtomic( &gBTreeNodeAllocationCounter );
#endif // DBG
    
    IOFree( node, sizeof(*node) );
}

//--------------------------------------------------------------------

void
DldSparseFile::dereferenceBTreeNode( __in BTreeNode* node, __in bool acquireLock )
{
    assert( node->h.referenceCount > 0x0 );
    
    if( 0x1 != OSDecrementAtomic( &node->h.referenceCount ) )
        return;
    
    //
    // remove the node from the cached list
    //
    if( !IsListEmpty( &node->h.listEntry ) ){
        
        if( acquireLock )
            this->LockExclusive();
        { // start of the lock
            
            if( !IsListEmpty( &node->h.listEntry ) ){
                
                RemoveEntryList( &node->h.listEntry );
                InitializeListHead( &node->h.listEntry );
                
                assert( this->numberOfCachedNodes > 0x0 );
                OSDecrementAtomic( &this->numberOfCachedNodes );
            }
        } // end of the lock
        if( acquireLock )
            this->UnLockExclusive();
        
    } // end if( !IsListEmpty( &node->h.listEntry ) )
    
    this->freeBTreeNodeMemory( node );
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::removeBTreeNodeFromBackingStore( __in BTreeNode* node, __in bool acquireLock )
{
    errno_t error = KERN_SUCCESS;
    
    assert( preemption_enabled() );
    
    //
    // remove the dirty flag as it is not relevant any more,
    // the node is being removed and there is no need to flush it
    //
    node->h.dirty = false;
    
    //
    // remove the node from the cached list
    //
    if( !IsListEmpty( &node->h.listEntry ) ){
        
        if( acquireLock )
            this->LockExclusive();
        { // start of the lock
            
            //
            // cancel the node contribution to the cache hit counter
            //
            if( node->h.accessed && this->accessedCachedNodesCount > 0x0 )
                this->accessedCachedNodesCount -= 0x1;
            
            if( !IsListEmpty( &node->h.listEntry ) ){
                
                RemoveEntryList( &node->h.listEntry );
                InitializeListHead( &node->h.listEntry );
                
                assert( this->numberOfCachedNodes > 0x0 );
                OSDecrementAtomic( &this->numberOfCachedNodes );
                
                //
                // release a reference from the cached list
                //
                this->dereferenceBTreeNode( node, false );
            }
        } // end of the lock
        if( acquireLock )
            this->UnLockExclusive();
        
    } // end if( !IsListEmpty( &node->h.listEntry ) )
    
    
    //
    // return the chunk back
    //
    if( DldSparseFile::InvalidOffset != node->h.nodeOffset ){
        
        if( acquireLock )
            this->LockExclusive();
        { // start of the lock
            
            if( DldSparseFile::InvalidOffset != node->h.nodeOffset ){
                
                error = this->returnChunkToFreeList( node->h.nodeOffset, false );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("returnChunkToFreeList() failed with error(%u)\n", error));
                }
                
                node->h.nodeOffset = DldSparseFile::InvalidOffset;
                
            } // end if( DldSparseFile::InvalidOffset != node->h.nodeOffset )
            
        } // end of the lock
        if( acquireLock )
            this->UnLockExclusive();
        
    } // end if( DldSparseFile::InvalidOffset != node->h.nodeOffset )
    
    return error;
}

//--------------------------------------------------------------------

//
// Used to split the child node and adjust the parent so that
// it has two children
// @param parent Parent Node
// @param index Index of the child node
// @param child  Full child node
//
// for reference see "Introduction to algorithms" / Thomas H. Cormen . . . [et al.].—2nd ed.
// pp 443-445
// 
errno_t
DldSparseFile::BTreeSplitChild(
    __in    BTree*         btree,
    __inout BTreeNode*     parent, 
    __in    unsigned int   index,
    __inout BTreeNode*     child,
    __in    bool           acquireLock
    )
//
// the function splits a full node y (having 2t − 1 keys) around its median key keyt [y]
// into two nodes having t − 1 keys each. The median key moves up into y’s parent to identify
// the dividing point between the two new trees
//
{
    errno_t         error;
	unsigned int    order = DldSparseFile::BTreeOrder;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( order >= 1 );
    
    assert( child->h.nrActive == ( (2 * order) - 0x1) );
    
	BTreeNode * newChild = this->getBTreeNodeReference( DldSparseFile::InvalidOffset, acquireLock );
    assert( newChild );
    if( !newChild ){
        
        DBG_PRINT_ERROR(("getBTreeNodeReference( DldSparseFile::InvalidOffset, acquireLock ) failed\n"));
        btree->damaged = true;
        return ENOMEM;
    }
    DldIncrementReferenceTaken();
    
	newChild->h.leaf = child->h.leaf;
	newChild->h.level = child->h.level;
	newChild->h.nrActive = order - 1;
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        //
        // Copy the higher order keys to the new child
        //
        for( int i=0; i < (order - 1); i++ ){
            
            newChild->keyEntries[ i ] = child->keyEntries[ i + order ];
            
            if( !child->h.leaf )
                newChild->children[ i ] = child->children[ i + order ];
            
        } // end for( i=0; i<order - 1; i++ )
        
        //
        // Copy the last child offset for the above "for" cycle
        //
        if( !child->h.leaf ){
            
            //
            // this is a continuation of the above for cycle
            // for i == order - 1
            //
            newChild->children[ order - 1 ] = child->children[ 2*order - 1 ];
            
        } // end if( !child->leaf )
        
        //
        // set the new number of keys as a half of the previous full number
        // as the higher order keys were copied to the newChild node
        //
        child->h.nrActive = order - 1;
        
        //
        // move the parent's children to one position to the right to free
        // an element for the new child
        //
        for( int i = (parent->h.nrActive + 1); i > (index + 1); i-- ){
            
            parent->children[ i ] = parent->children[ i - 1 ];
        }
        
        //
        // insert the new child in the freed position
        //
        parent->children[ index + 1 ] = newChild->h.nodeOffset;
        
        for( int i = parent->h.nrActive; i > index; i-- ){
            
            parent->keyEntries[i] = parent->keyEntries[i - 1];
        }
        
        parent->keyEntries[index] = child->keyEntries[order - 1];
        parent->h.nrActive++;
        
        //
        // write down the modified parent and both children
        //
        error = this->BTreeFlushNode( btree, parent );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, parent ) failed with error(%u)\n", error));
        } // end if( error )
        
        if( !error ){
            
            error = this->BTreeFlushNode( btree, child );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, child ) failed with error(%u)\n", error));
            }
        } // end if( !error )
        
        if( !error ){
            
            error = this->BTreeFlushNode( btree, newChild );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, newChild ) failed with error(%u)\n", error));
            }
        } // end if( !error )
        
        if( error )
            btree->damaged = true;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    //
    // free the newChild memory as the node has been flushed to a disk
    //
    this->dereferenceBTreeNode( newChild, acquireLock );
    DldDecrementReferenceTaken();
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

//
// The function is used to insert a key in the non-full node
// @param btree The btree
// @param node The node to which the key will be added
// @param the key value pair
// @return void
//

errno_t
DldSparseFile::BTreeInsertNonfull(
    __in    BTree*      btree,
    __inout BTreeNode*  parentNode,
    __in    BTreeKey*   keyVal,
    __in    bool        acquireLock
    )
//
// the function inserts key k into node x, which is assumed to be nonfull when the procedure is called.
//
{
	//unsigned int    key = btree->value(keyVal->key);
    errno_t         error;
	int             i;
	BTreeNode*      node = parentNode;
    CompareKeysFtr  cmp  = btree->compareKeys;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( cmp );
    
    node = parentNode;
    this->referenceBTreeNode( node );
    DldIncrementReferenceTaken();
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
    insert:
        i = node->h.nrActive - 1;
        
        if( node->h.leaf ){
            
            //
            //  handle the case in which x is a leaf node by inserting key k into x.
            //
            
            while( i >= 0 && kKeyOneSmaller == cmp( keyVal, &node->keyEntries[i].keyValue ) ){ // key < node->key_vals[i]
                
                node->keyEntries[i + 1] = node->keyEntries[i];
                i--;
            }
            
            //
            // insert the key value in the node
            //
            node->keyEntries[ i + 1 ].keyValue = *keyVal;
            
            //
            // the entry is not in the tags list yet
            //
            node->keyEntries[i + 1].tagListEntry.flink = DldSparseFile::InvalidOffset;
            node->keyEntries[i + 1].tagListEntry.blink = DldSparseFile::InvalidOffset;
            
            node->h.nrActive++;
            error = KERN_SUCCESS; // just yo provide a correct behaviour if the next call will be removed
            
            //
            // insert in the tags list
            //
            error = this->BTreeUpdateTagsListPosition( btree, node, (i+1), false );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeUpdateTagsListPosition() failed with an error(%u)\n", error));
            }
            
        } else {
            
            BTreeNode*      child;
            unsigned int    order = DldSparseFile::BTreeOrder;
            
            //
            // x is not a leaf node, so we must insert k into the appropriate leaf node
            // in the subtree rooted at internal node x.
            //
            
            while( i >= 0 && kKeyOneSmaller == cmp( keyVal, &node->keyEntries[i].keyValue ) ){ // key < btree->value(node->key_vals[i]->key)
                i--;
            }
            
            i++;
            
            assert( DldSparseFile::InvalidOffset != node->children[i] );
            
            child = this->getBTreeNodeReference( node->children[i], false ); 
            assert( child );
            if( !child ){
                
                DBG_PRINT_ERROR(("getBTreeNodeReference( node->children[%u], false ) failed\n", i));
                error = ENOMEM;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            //
            // check whether the recursion would descend to a full child
            //
            if( child->h.nrActive == (2*order - 1) ){
                
                //
                // a full child, split it
                //
                error = this->BTreeSplitChild( btree, node, i, child, false );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("BTreeSplitChild( btree, node, %i, child, false ) failed with an error(%u)\n", i, error));
                    
                    this->dereferenceBTreeNode( child, false );
                    DldDecrementReferenceTaken();
                    goto __exit;
                } //end if( error )
                
                
                if( kKeyOneBigger == cmp( keyVal, &node->keyEntries[i].keyValue )){ // key_val > node->key_vals[i]
                    
                    //
                    // change the child
                    //
                    i++;
                    this->dereferenceBTreeNode( child, false );
                    child = NULL;
                    DldDecrementReferenceTaken();
                    
                    assert( DldSparseFile::InvalidOffset != node->children[i] );
                    
                    child = this->getBTreeNodeReference( node->children[i], false ); 
                    assert( child );
                    if( !child ){
                        
                        DBG_PRINT_ERROR(("getBTreeNodeReference( node->children[%u], false ) failed\n", i));
                        error = ENOMEM;
                        goto __exit;
                    } // end if( !child )
                    DldIncrementReferenceTaken();
                    
                } // end if( kKeyOneBigger = cmp( keyVal, &node->key_vals[i] ))
                
            } // end if( child->h.nrActive == (2*order - 1) )
            
            
            //
            // tail recursion, relese the node from the previous iteration before changing it
            //
            this->dereferenceBTreeNode( node, false );
            DldDecrementReferenceTaken();
            
            //
            // replace the node for the next iteration, the child is referenced
            //
            node = child; //node->children[i];
            goto insert;  // repeat again
            
        } // end for else
        
    __exit:
        
        if( !error ){
            
            //
            // flush the modified node
            //
            error = this->BTreeFlushNode( btree, node );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, node ) failed with error(%u)\n", error));
            } // end if( error )
            
        }
        
        if( error ){
            
            btree->damaged = true;
        }
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    if( node ){
        
        //
        // release the node from the previous iteration
        //
        this->dereferenceBTreeNode( node, acquireLock );
        DldDecrementReferenceTaken();
    }
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

//
//  The function is used to insert node into a B-Tree
//  @param root Root of the B-Tree
//  @param node The node to be inserted
//  @param compare Function used to compare the two nodes of the tree
//  @return success or failure
//
errno_t
DldSparseFile::BTreeInsertKey(
    __inout BTree*      btree,
    __in    BTreeKey*   keyVal,
    __in    bool        acquireLock
    )
{
    errno_t         error;
	BTreeNode*      rnode;
    unsigned int    order = DldSparseFile::BTreeOrder;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( 0x0 == ( keyVal->mapEntry.sparseFileOffset & ( DldSparseFile::BlockSize - 0x1 ) ) );
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        rnode = btree->root;
        
        if( rnode->h.nrActive == ((2 * order) - 1) ){
            
            //
            // the root is full
            //
            
            BTreeNode* newRoot;
            
            newRoot = this->getBTreeNodeReference( DldSparseFile::InvalidOffset, false );
            assert( newRoot );
            if( !newRoot ){
                
                DBG_PRINT_ERROR(("getBTreeNodeReference( DldSparseFile::InvalidOffset, false ) failed\n"));
                error = ENOMEM;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            newRoot->h.level = rnode->h.level + 1;
            newRoot->h.leaf = false;
            newRoot->h.nrActive = 0;
            newRoot->children[0] = rnode->h.nodeOffset;
            
            //
            // flush the changed newRoot
            //
            error = this->BTreeFlushNode( btree, newRoot );
            assert( !error );
            if( error ){
                
                //
                // dereference the former root as the refernce was held by BTree
                //
                this->dereferenceBTreeNode( rnode, false );
                DldDecrementReferenceTaken();
                
                DBG_PRINT_ERROR(("BTreeFlushNode( btree, newRoot ) failed with error(%u)\n", error));
                goto __exit;
            } // end if( error )
            
            //
            // set the new root, the old root is saved in the rnode so it gets the reference from the BTree
            //
            btree->root = newRoot;
            
            //
            // a full child( former root ), split it
            //
            error = BTreeSplitChild( btree, newRoot, 0, rnode, false );
            this->dereferenceBTreeNode( rnode, false );// dereference the former root as the reference was held by BTree
            DldDecrementReferenceTaken();
            rnode = NULL;
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeSplitChild( btree, newRoot, 0, rnode) failed with an error(%u)\n", error));
                goto __exit;
            } //end if( error )
            
            error = this->BTreeInsertNonfull( btree, newRoot, keyVal, false );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeInsertNonfull( btree, newRoot, keyVal, false ) failed with an error %u\n", error));
            }
            
        } else {
            
            error = this->BTreeInsertNonfull( btree, rnode, keyVal, false );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeInsertNonfull( btree, rnode, keyVal, false ) failed with an error %u\n", error));
            }
        }
        
    __exit:
        if( error )
            btree->damaged = true;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

//
// The function is used to get the position of the MAX or MIN key within the subtree
// @param btree The btree
// @param subtree The subtree to be searched
// @return The node containing the key and position of the key,
//  if there was an error then nodePos.nodeOffset == DldSparseFile::InvalidOffset
//  the node pointer is returned and referenced!
//  nodePos.node == NULL is returned for an empty tree
//
DldSparseFile::BtNodePos
DldSparseFile::BTreeGetMaxOrMinKeyPos(
    __in BTree* btree,
    __in BTreeNode* subtree,
    __in bool getMax, // if true returns MAX, else returns MIN
    __in bool acquireLock
    )
{
	BtNodePos   nodePos;
	BTreeNode*  node;
    DldDeclareReeferenceTaken;
    
    assert( subtree );
    assert( preemption_enabled() );
    
    node = subtree;
    this->referenceBTreeNode( node );
    DldIncrementReferenceTaken();
    
    nodePos.node = NULL;
    nodePos.nodeOffset = DldSparseFile::InvalidOffset;
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        while( true ){
            
            if( node == NULL || 0x0 == node->h.nrActive ) {
                
                //
                // node->h.nrActive == 0x0 means that this is an empty root,
                // you can return NULL or the empty root's node, the meaning is
                // arguarable - what is min or max for an empty set?
                //
                assert( NULL == node || node->h.leaf );
                break;
            }
            
            if( node->h.leaf ){
                
                nodePos.nodeOffset = node->h.nodeOffset;
                nodePos.index 	   = getMax ? (node->h.nrActive - 1) : 0x0;
                nodePos.node       = node;
                node = NULL; // the ownership has been transferred to nodePos
                
                break;
                
            } else {
                
                nodePos.nodeOffset = node->h.nodeOffset;
                nodePos.index 	   = getMax ? (node->h.nrActive - 1) : 0;
                
                off_t  childOffset = node->children[ getMax ? node->h.nrActive : 0 ];
                
                assert( DldSparseFile::InvalidOffset != childOffset ); 
                
                //
                // dereference the node before changing the pointer
                //
                this->dereferenceBTreeNode( node, false );
                DldDecrementReferenceTaken();
                node = this->getBTreeNodeReference( childOffset, false );
                assert( node );
                if( !node ){
                    
                    DBG_PRINT_ERROR(("getBTreeNodeReference( node->children[node->h.nrActive], false ) failed"));
                    nodePos.nodeOffset = DldSparseFile::InvalidOffset;
                    btree->damaged = true;
                    break; 
                } // end if( !node )
                DldIncrementReferenceTaken();
            } // end else
        } // end while( true )
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    if( node ){
        
        this->dereferenceBTreeNode( node, acquireLock );
        DldDecrementReferenceTaken();
    }
    
    assert( !( NULL != nodePos.node && DldSparseFile::InvalidOffset == nodePos.nodeOffset ) );
    
    if( nodePos.node ){
        DldCheckReferenceTaken( 0x1 );
    } else {
        DldCheckReferenceTaken( 0x0 );
    }
    
	return nodePos;
}

//--------------------------------------------------------------------

DldSparseFile::BtNodePos
DldSparseFile::BTreeGetMaxKeyPos(
    __in BTree* btree,
    __in BTreeNode* subtree,
    __in bool acquireLock
    )
{
    return this->BTreeGetMaxOrMinKeyPos( btree, subtree, true, acquireLock );
}

//--------------------------------------------------------------------

DldSparseFile::BtNodePos
DldSparseFile::BTreeGetMinKeyPos(
    __in BTree* btree,
    __in BTreeNode* subtree,
    __in bool acquireLock
    )
{
    return this->BTreeGetMaxOrMinKeyPos( btree, subtree, false, acquireLock );
}

//--------------------------------------------------------------------

//
// Merge nodes n1 and n2 (case 3b from Cormen)
// @param btree The btree 
// @param node The parent node 
// @param index of the child
// @param pos left or right
// @returns a referenced node 
//
DldSparseFile::BTreeNode*
DldSparseFile::BTreeMergeSiblings(
    __inout  BTree* btree,
    __in     BTreeNode* parent,
    __in     unsigned int index, 
    __in     BtPosition pos,
    __in     bool acquireLock
    )
{
    unsigned int    order = DldSparseFile::BTreeOrder;
	unsigned int    i,j;
	BTreeNode*      newNode = NULL;
	BTreeNode*      n1, * n2;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( parent->h.nrActive > 0 );
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        if( index == (parent->h.nrActive) ){
            
            index--;
            
            assert( DldSparseFile::InvalidOffset != parent->children[ parent->h.nrActive - 1 ] );
            assert( DldSparseFile::InvalidOffset != parent->children[ parent->h.nrActive ] );
            
            n1 = this->getBTreeNodeReference( parent->children[ parent->h.nrActive - 1 ], false );
            n2 = this->getBTreeNodeReference( parent->children[ parent->h.nrActive ], false );
            
            assert( n1 && n2 );
            
        } else {
            
            assert( DldSparseFile::InvalidOffset != parent->children[ index ] );
            assert( DldSparseFile::InvalidOffset != parent->children[ index + 1 ] );
            
            n1 = this->getBTreeNodeReference( parent->children[ index ], false );
            n2 = this->getBTreeNodeReference( parent->children[ index + 1 ], false );
            
            assert( n1 && n2 );
        }
        
        if( NULL == n1 || NULL == n2 ){
            
            DBG_PRINT_ERROR(("n1 = 0x%p , n2 = 0x%p \n", n1, n2));
            assert( NULL == newNode );
            
            btree->damaged = true;
            goto __exit;
        }
        DldIncrementReferenceTaken(); // n1
        DldIncrementReferenceTaken(); // n2
        
        //
        // Merge the current node with the left node
        //
        newNode = this->getBTreeNodeReference( DldSparseFile::InvalidOffset, false );
        assert( newNode );
        if( !newNode ){
            
            DBG_PRINT_ERROR(("getBTreeNodeReference( DldSparseFile::InvalidOffset, false ) failed\n"));
            goto __exit;
        }
        DldIncrementReferenceTaken();
        
        newNode->h.level = n1->h.level;
        newNode->h.leaf  = n1->h.leaf;
        
        for( j=0; j < order - 1; j++ ){
            
            newNode->keyEntries[ j ] = n1->keyEntries[ j ];
            newNode->children[ j ]   = n1->children[ j ];
        }
        
        newNode->keyEntries[ order - 1 ] = parent->keyEntries[ index ];
        newNode->children[ order - 1 ]   = n1->children[ order - 1 ];
        
        for( j=0; j < order - 1; j++ ){
            
            newNode->keyEntries[ j + order ] = n2->keyEntries[j];
            newNode->children[ j + order ]   = n2->children[j];
        }
        newNode->children[ 2*order - 1 ] = n2->children[ order - 1 ];
        
        parent->children[ index ] = newNode->h.nodeOffset;
        
        for( j = index; j < parent->h.nrActive; j++ ){
            
            parent->keyEntries[ j ]   = parent->keyEntries[ j + 1 ];
            parent->children[ j + 1 ] = parent->children[ j + 2 ];
        }
        
        newNode->h.nrActive = n1->h.nrActive + n2->h.nrActive + 1;
        parent->h.nrActive--;
        
        assert( newNode->h.nrActive <= (2*order - 1 ) );
        
        for( i = parent->h.nrActive + 1;i < 2*order; i++ ){
            
            // ???????
            parent->children[i] = DldSparseFile::InvalidOffset; 
        }
        
        this->removeBTreeNodeFromBackingStore( n1, false );
        this->removeBTreeNodeFromBackingStore( n2, false );
        
        if( parent->h.nrActive == 0 && btree->root == parent ) {
            
            this->removeBTreeNodeFromBackingStore( btree->root, false );
            this->dereferenceBTreeNode( btree->root, false ); // the reference was held by BTree
            DldDecrementReferenceTaken();
            parent = NULL; // parent is not referenced, so just zero the pointer
            btree->root = NULL;
            
            btree->root = newNode;
            this->referenceBTreeNode( newNode ); // the root is referenced to be retained
            DldIncrementReferenceTaken();
            if( 0x0 != newNode->h.level )
                newNode->h.leaf = false;
            else
                newNode->h.leaf = true; 
        }
        
    __exit:
        
        if( n1 ){
            this->dereferenceBTreeNode( n1, false );
            DldDecrementReferenceTaken();
        }
        
        if( n2 ){
            this->dereferenceBTreeNode( n2, false );
            DldDecrementReferenceTaken();
        }
        
        //
        // flush the modified nodes
        //
        if( parent )
            this->BTreeFlushNode( btree, parent );
        
        if( newNode )
            this->BTreeFlushNode( btree, newNode );
        
        if( !newNode )
            btree->damaged = true;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    if( newNode ){
        DldCheckReferenceTaken( 0x1 );
    } else {
        DldCheckReferenceTaken( 0x0 );
    }
    
    //
    // the returned node is referenced
    //
	return newNode;
}

//--------------------------------------------------------------------

//
// Move the key from node to another	
// @param btree The B-Tree
// @param node The parent node
// @param index of the key to be moved done 
// @param pos the position of the child to receive the key 
// @return none
//
errno_t
DldSparseFile::BTreeMoveKey(
    __inout BTree*       btree,
    __in    BTreeNode*   node,
    __in    unsigned int index,
    __in    BtPosition   pos,
    __in    bool         acquireLock
    )
{
    errno_t         error;
    unsigned int    order = DldSparseFile::BTreeOrder;
	BTreeNode*      lchild;
	BTreeNode*      rchild;
    DldDeclareReeferenceTaken;
    
	if( pos == right ) {
        
        assert( index > 0 );
		index--;
	}
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        assert( index <= 2*order - 1 );
        assert( DldSparseFile::InvalidOffset != node->children[ index ] );
        assert( DldSparseFile::InvalidOffset != node->children[ index + 1 ] );
        
        lchild = this->getBTreeNodeReference( node->children[ index ], false );
        rchild = this->getBTreeNodeReference( node->children[ index + 1 ], false );
        
        assert( lchild && rchild );
        
        if( NULL == lchild || NULL == rchild ){
            
            DBG_PRINT_ERROR(("lchild = 0x%p , rchild = 0x%p \n", lchild, rchild));
            
            if( lchild )
                this->dereferenceBTreeNode( lchild, false );
            
            if( rchild )
                this->dereferenceBTreeNode( rchild, false );
            
            error = ENOMEM;
            goto __exit;
        } // end if( NULL == lchild || NULL == rchild )
        
        DldIncrementReferenceTaken(); // rchild
        DldIncrementReferenceTaken(); // lchild
        
        assert( order >= 2 );
        assert( rchild->h.nrActive >= (order-1) );
        assert( lchild->h.nrActive >= (order-1) );
        
        if( pos == left ) {
            
            //
            // Move the key from the parent to the left child
            //
            assert( lchild->h.nrActive < (2*order - 1) );
            
            lchild->keyEntries[ lchild->h.nrActive ]   = node->keyEntries[ index ];
            lchild->children[ lchild->h.nrActive + 1 ] = rchild->children[ 0 ];
            
            rchild->children[ 0 ] = DldSparseFile::InvalidOffset; // will be rewritten by the following for() cycle
            lchild->h.nrActive++;
            
            node->keyEntries[index] = rchild->keyEntries[0];
            
            //
            // left node borrowed a right's child from the 0 position, move right node's children array
            //
            for( int i=0; i < rchild->h.nrActive - 1; i++ ){
                
                rchild->keyEntries[ i ] = rchild->keyEntries[ i + 1 ];
                rchild->children[ i ]   = rchild->children[ i + 1 ];
            }
            
            rchild->children[ rchild->h.nrActive - 1 ] = rchild->children[ rchild->h.nrActive ];
            rchild->children[ rchild->h.nrActive ] = DldSparseFile::InvalidOffset;
            rchild->h.nrActive--;
            
        } else {
            
            //
            // Move the key from the parent to the right child
            //
            assert( rchild->h.nrActive < (2*order - 1) );
            assert( pos == right );
            
            for( int i = rchild->h.nrActive; i > 0 ; i-- ){
                
                rchild->keyEntries[ i ]   = rchild->keyEntries[ i - 1 ];
                rchild->children[ i + 1 ] = rchild->children[ i ];		
            }
            
            rchild->children[ 1 ] = rchild->children[ 0 ];		
            rchild->children[ 0 ] = DldSparseFile::InvalidOffset;
            
            rchild->keyEntries[ 0 ] = node->keyEntries[ index ];
            
            rchild->children[ 0 ] = lchild->children[ lchild->h.nrActive ];	
            lchild->children[ lchild->h.nrActive ] = DldSparseFile::InvalidOffset;
            
            node->keyEntries[ index ] = lchild->keyEntries[ lchild->h.nrActive - 1 ];
            
            lchild->h.nrActive--;
            rchild->h.nrActive++;		
        }
        
        //
        // flush the modified nodes
        //
        error = this->BTreeFlushNode( btree, node );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("BTreeFlushNode() failed with an error %u\n", error));
            goto __exit;
        }
        
        error = this->BTreeFlushNode( btree, lchild );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("BTreeFlushNode() failed with an error %u\n", error));
            goto __exit;
        }
        
        error = this->BTreeFlushNode( btree, rchild );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("BTreeFlushNode() failed with an error %u\n", error));
            goto __exit;
        }
        
        this->dereferenceBTreeNode( lchild, false );
        DldDecrementReferenceTaken();
        this->dereferenceBTreeNode( rchild, false );
        DldDecrementReferenceTaken();
        
    __exit:
        if( error )
            btree->damaged = true;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

//
// Merge nodes n1 and n2
// @param n1 First node
// @param n2 Second node
// @return a referenced combined node and removes n1 and n2 from the backing dtore
//
DldSparseFile::BTreeNode*
DldSparseFile::BTreeMergeNodes(
    __inout BTree*          btree,
    __in    BTreeNode*      n1,
    __in    BTreeKeyEntry*  kv,
    __in    BTreeNode*      n2,
    __in    bool            acquireLock
    )
{
    unsigned int    order = DldSparseFile::BTreeOrder;
	BTreeNode* newNode;
    DldDeclareReeferenceTaken;
    
	newNode = this->getBTreeNodeReference( DldSparseFile::InvalidOffset, acquireLock );
    assert( newNode );
    if( !newNode ){
        
        DBG_PRINT_ERROR(("getBTreeNodeReference failed\n"));
        btree->damaged = true;
        return NULL;
    }
    DldIncrementReferenceTaken();
    
	newNode->h.leaf = true;
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        for( int i=0; i < n1->h.nrActive; ++i ){
            
            newNode->keyEntries[i] = n1->keyEntries[i];
            newNode->children[i]   = n1->children[i];
        }
        
        newNode->children[n1->h.nrActive]   = n1->children[n1->h.nrActive];
        newNode->keyEntries[n1->h.nrActive] = *kv;
        
        for( int i=0; i < n2->h.nrActive; ++i ){
            
            assert( (i + n1->h.nrActive + 1) < (2*order - 1) );
            
            newNode->keyEntries[i + n1->h.nrActive + 1] = n2->keyEntries[i];
            newNode->children[i + n1->h.nrActive + 1]   = n2->children[i];
        }
        
        newNode->children[2*order - 1] = n2->children[n2->h.nrActive];
        
        assert( (n1->h.nrActive + n2->h.nrActive + 1) <= (2*order - 1) );
        
        newNode->h.nrActive = n1->h.nrActive + n2->h.nrActive + 1;
        newNode->h.leaf     = n1->h.leaf;
        newNode->h.level    = n1->h.level;
        
        //
        // flush the modified nodes
        //
        errno_t error;
        error = this->BTreeFlushNode( btree, newNode );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("BTreeFlushNode() failed with an error %u\n", error));
            
            this->removeBTreeNodeFromBackingStore( newNode, false );
            this->dereferenceBTreeNode( newNode, false );
            DldDecrementReferenceTaken();
            newNode = NULL;
        }
        
        //
        // remove the old nodes if they were replaced by a new one
        //
        if( newNode ){
            
            this->removeBTreeNodeFromBackingStore( n1, false );
            this->removeBTreeNodeFromBackingStore( n2, false );
            
        } else {
            
            //
            // there was an error
            //
            btree->damaged = true;
        }
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    if( newNode ){
        DldCheckReferenceTaken( 0x1 );
    } else {
        DldCheckReferenceTaken( 0x0 );
    }
    
	return newNode;
}

//--------------------------------------------------------------------

//
// Used to delete a key from the B-tree leaf(!) node
// @param btree The btree
// @param node The node from which the key is to be deleted 	
// @param key The key to be deleted
// if the node containes only the key being deleted then
// the entire node is being deleted
//

errno_t
DldSparseFile::BTreeDeleteKeyFromNode(
    __inout BTree*      btree,
    __in    BtNodePos*  nodePos,
    __in    bool        acquireLock
    )
{
    errno_t         error = KERN_SUCCESS;
    unsigned int    order = DldSparseFile::BTreeOrder;
	//BTreeKey*       keyVal;
	BTreeNode*      node;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( nodePos->node || DldSparseFile::InvalidOffset != nodePos->nodeOffset );
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
        node = nodePos->node;
        if( node )
            this->referenceBTreeNode( node );
        else
            node = this->getBTreeNodeReference( nodePos->nodeOffset, false );
        
        assert( node );
        if( !node ){
            
            DBG_PRINT_ERROR(("getBTreeNodeReference failed\n"));
            error = ENOMEM;
            goto __exit;
        }
        DldIncrementReferenceTaken();
        
        assert( node->h.leaf );
        
        if( node->h.leaf == false ){
            
            error = -1;
            goto __exit;
        }
        
        assert( node->h.nrActive > 0 );
        assert( nodePos->index < node->h.nrActive );
        
        error = this->BTreeDeleteFromTagsListPosition( btree, node, nodePos->index, false );
        assert( !error );
        if( error )
            goto __exit;
        
        //keyVal = &node->keyVals[ nodePos->index ];
        
        // TO DO i < ((2*order - 1) - 1) might be replaced to i < (node->h.nrActive - 0x1)
        for( int i = nodePos->index; i < (node->h.nrActive - 0x1); i++ ){
            
            node->keyEntries[ i ] = node->keyEntries[ i + 1 ];	
        }
        
        node->h.nrActive--;
        
#if defined(DBG)
        //
        // all children must be invalid as this is a leaf
        //
        for( int i = 0x0; i < (2*order); ++i ){
            
            assert( DldSparseFile::InvalidOffset == node->children[ i ] );
        }
#endif // DBG
        
        if( 0 == node->h.nrActive ){
            
            //
            // delete the node if this is not the root,
            // the caller must be prepared to this for nodes
            // with the nrActive count equal to 1
            //
            if( btree->root != node ){
                
                this->removeBTreeNodeFromBackingStore( node, false );
                this->dereferenceBTreeNode( node, false );
                DldDecrementReferenceTaken();
                node = NULL;
                
            } else {
                
                //
                // flush the modified root
                //
                
                assert( node == btree->root );
                
                error = this->BTreeFlushNode( btree, node );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("BTreeFlushNode() failed with an error %u\n", error));
                }
            }
            
        } else {
            
            //
            // flush the modified node
            //
            error = this->BTreeFlushNode( btree, node );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeFlushNode() failed with an error %u\n", error));
            }
            
        }
        
    __exit:
        
        if( node ){
            this->dereferenceBTreeNode( node, false );
            DldDecrementReferenceTaken();
        }
        
        if( error )
            btree->damaged = true;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    DldCheckReferenceTaken( 0x0 );
    
	return error;
}

//--------------------------------------------------------------------

#if defined(DBG)
#define LogLinesDeclaration   int   linesLog[50]; int linesLogIndex = 0x0; bzero( linesLog, sizeof(linesLog) );
#define LogLine()  do{ linesLog[ linesLogIndex++ ] = __LINE__ ; }while(0);
#else
#define LogLinesDeclaration 
#define LogLine()
#endif

//
// Function is used to delete a node from a  B-Tree
// @param btree The B-Tree
// @param key Key of the node to be deleted
// @param value function to map the key to an unique integer value        
// @param compare Function used to compare the two nodes of the tree
// @return success or failure
//
errno_t
DldSparseFile::BTreeDeleteKey(
    __inout BTree*      btree,
    __inout BTreeNode*  subtree,
    __in    BTreeKey*   key,
    __in    bool        acquireLock
    )
{
/*
 "Introduction to algorithms" / Thomas H. Cormen . . . [et al.].—2nd ed. pp 449 - 452
   Assume that procedure B-TREE-DELETE is asked to delete the key k from the subtree rooted at x.
 This procedure is structured to guarantee that whenever B- TREE-DELETE is called recursively on a node x,
 the number of keys in x is at least the minimum degree t. Note that this condition requires one more key
 than the minimum required by the usual B-tree conditions, so that sometimes a key may have to be moved into
 a child node before recursion descends to that child. This strengthened condition allows us to delete a key
 from the tree in one downward pass without having to “back up” (with one exception, which we’ll explain).
 The following specification for deletion from a B-tree should be interpreted with the understanding that
 if it ever happens that the root node x becomes an internal node having no keys (this situation can occur
 in cases 2c and 3b, below), then x is deleted and x’s only child c1[x] becomes the new root of the tree,
 decreasing the height of the tree by one and preserving the property that the root of the tree contains
 at least one key (unless the tree is empty).
 
   We sketch how deletion works instead of presenting the pseudocode. 
 1. If the key k is in node x and x is a leaf,delete the key k from x.
 2. If the key k is in node x and x is an internal node,do the following.
   a. If the child y that precedes k in node x has at least t keys, then find the predecessor k′ of k
      in the subtree rooted at y. Recursively delete k′, and replace k by k′ in x. (Finding k′ and
      deleting it can be performed in a single downward pass.)
   b. Symmetrically, if the child z that follows k in node x has at least t keys, then find the successor
      k′ of k in the subtree rooted at z. Recursively delete k′, and replace k by k′ in x. (Finding k′
      and deleting it can be performed in a single downward pass.)
   c. Otherwise,if both y and z have only t−1 keys,merge k and all of z into y, so that x loses both k and
      the pointer to z, and y now contains 2t − 1 keys. Then, free z and recursively delete k from y.
 3. If the key k is not present in internal node x, determine the root ci[x] of the appropriate subtree
    that must contain k, if k is in the tree at all. If ci [x] has only t − 1 keys, execute step 3a or 3b
    as necessary to guarantee that we descend to a node containing at least t keys. Then, finish by recursing
    on the appropriate child of x.
   a. If ci [x ] has only t − 1 keys but has an immediate sibling with at least t keys, give ci[x] an extra key
      by moving a key from x down into ci[x], moving a key from ci[x]’s immediate left or right sibling up into x,
      and moving the appropriate child pointer from the sibling into ci [x ].
   b. If ci [x] and both of ci [x]’s immediate siblings have t − 1 keys, merge ci [x] with one sibling, which involves
      moving a key from x down into the new merged node to become the median key for that node.
 
   Since most of the keys in a B-tree are in the leaves, we may expect that in practice, deletion operations
 are most often used to delete keys from leaves. The B- TREE-DELETE procedure then acts in one downward pass
 through the tree, without having to back up. When deleting a key in an internal node, however, the procedure
 makes a downward pass through the tree but may have to return to the node from which the key was deleted to
 replace the key with its predecessor or successor (cases 2a and 2b).
   Although this procedure seems complicated, it involves only O(h) disk oper- ations for a B-tree of height h,
 since only O(1) calls to DISK-READ and DISK- WRITE are made between recursive invocations of the procedure.
 The CPU time required is O(th) = O(t logt n).
 */
    errno_t         error = KERN_SUCCESS;
    unsigned int    order = DldSparseFile::BTreeOrder;
	unsigned int    index;
	BTreeNode*      node;
    BTreeNode*      parent;
	BTreeKey        keyVal;
    CompareKeysFtr  cmp;
    LogLinesDeclaration;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( 0x0 == ( key->mapEntry.sparseFileOffset & ( DldSparseFile::BlockSize - 0x1 ) ) );
    
    cmp      = btree->compareKeys;
	parent   = NULL;
    
    node = subtree;
    this->referenceBTreeNode( node );
    DldIncrementReferenceTaken();
    
    if( acquireLock )
        this->LockExclusive();
    { // start fo the locked region
        
    del_loop:
        
        for( int i = 0; ; i = 0 ){
            
            LogLine();
            
            //
            // If there are no keys simply return
            //
            if( 0x0 == node->h.nrActive ){
                
                error = -1;
                goto __exit;
            }
            
            //
            // Fix the index of the key greater than or equal
            // to the key that we would like to search
            //
            i = this->BTreeGetKeyIndex( btree, node, key );
            /*
            while( i < node->h.nrActive && kKeyOneBigger == cmp( key, &node->keyEntries[ i ].keyValue ) ){ // *key > &node->keyVals[i]
                i++;
            }
             */
            
            index = i;
            
            //
            // If we find such a key then break, this is a case 1
            //
            if(i < node->h.nrActive && kKeysEquals == cmp( key, &node->keyEntries[ i ].keyValue ) ){ // *key == node->keyVals[i]
                
                LogLine();
                break;
            }
            
            if( node->h.leaf ){
                
                LogLine();
                error = -1;
                goto __exit;
            }
            
            //
            // Store the parent node, remove the old before replacing
            //
            if( parent ){
                this->dereferenceBTreeNode( parent, false);
                DldDecrementReferenceTaken();
            }
            parent = node; // node is referenced
            node = NULL;
            
            LogLine();
            
            //
            // get a child node, parent and the node are the same at this point ( paren == node )
            //
            if( DldSparseFile::InvalidOffset == parent->children[ i ] ){
                
                //
                // no child
                //
                LogLine();
                error = -1;
                goto __exit;
            }
            
            //
            // go to child, the above replacing of the parent and
            // taking a child here provides a progressive downward movement
            //
            node = this->getBTreeNodeReference( parent->children[ i ], false );
            assert( node );
            if( !node ){
                
                LogLine();
                DBG_PRINT_ERROR(("getBTreeNodeReference failed\n"));
                error = ENOMEM;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            assert( parent->h.nrActive > 0 );
            
            if( node->h.nrActive == (order - 1) && parent ){
                
                //
                // this is the case 3
                //
                LogLine();
                
                BTreeNode* rsibling;
                BTreeNode* lsibling;
                
                rsibling = NULL;
                lsibling = NULL;
                
                if( index == parent->h.nrActive ){   
                    
                    LogLine();
                    assert( DldSparseFile::InvalidOffset != parent->children[parent->h.nrActive - 1] );
                    
                    lsibling = this->getBTreeNodeReference( parent->children[parent->h.nrActive - 1], false );
                    assert( lsibling );
                    if( !lsibling ){
                        
                        error = ENOMEM;
                        goto __exit;
                    }
                    DldIncrementReferenceTaken();
                    
                    rsibling = NULL;
                    
                } else if( index == 0 ){
                    
                    LogLine();
                    lsibling = NULL;
                    
                    if( 0x1 == parent->h.nrActive && DldSparseFile::InvalidOffset == parent->children[ 0x1 ] ){
                        
                        LogLine();
                        //
                        // this is a root with a single and more than a half full child
                        //
                        rsibling = NULL;
                        assert( parent == btree->root );
                        assert( node->h.nrActive >= (order - 1) );
                        
                    } else {
                        
                        LogLine();
                        assert( DldSparseFile::InvalidOffset != parent->children[ 0x1 ] );
                        
                        rsibling = this->getBTreeNodeReference( parent->children[ 0x1 ], false );
                        assert( rsibling );
                        if( !rsibling ){
                            
                            error = ENOMEM;
                            goto __exit;
                        }
                        DldIncrementReferenceTaken();
                    }
                    
                } else {
                    
                    LogLine();
                    assert( DldSparseFile::InvalidOffset != parent->children[ i - 1 ] );
                    assert( DldSparseFile::InvalidOffset != parent->children[ i + 1 ] );
                    
                    lsibling = this->getBTreeNodeReference( parent->children[ i - 1 ], false );
                    assert( lsibling );
                    if( !lsibling ){
                        
                        error = ENOMEM;
                        goto __exit;
                    }
                    DldIncrementReferenceTaken();
                    
                    rsibling = this->getBTreeNodeReference( parent->children[ i + 1 ], false );
                    assert( rsibling );
                    if( !rsibling ){
                        
                        this->dereferenceBTreeNode( lsibling, false );
                        DldDecrementReferenceTaken();
                        lsibling = NULL;
                        
                        error = ENOMEM;
                        goto __exit;
                    }
                    DldIncrementReferenceTaken();
                    
                }
                
                
                if (rsibling && ( rsibling->h.nrActive > (order - 1) ) ){
                    
                    LogLine();
                    //
                    // The current node has (t - 1) keys but the right sibling has > (t - 1) keys,
                    // so borrow the key from the right sibling
                    //
                    
                    error = BTreeMoveKey( btree, parent, i, left, false );
                    assert( !error );
                    if( error ){
                        
                        DBG_PRINT_ERROR(("BTreeMoveKey() failed with an error(%u)\n", error));
                        // do not goto __exit as siblings must be freed
                    }
                    
                } else
                    // The current node has (t - 1) keys but the left sibling has > (t - 1)
                    // keys, borrow from the left sibling
                    if( lsibling && (lsibling->h.nrActive > (order - 1) ) ){
                        
                        LogLine();
                        
                        error = BTreeMoveKey( btree, parent, i, right, false);
                        assert( !error );
                        if( error ){
                            
                            DBG_PRINT_ERROR(("BTreeMoveKey() failed with an error(%u)\n", error));
                            // do not goto __exit as siblings must be freed
                        }
                        
                    } else
                        // Left sibling has (t - 1) keys
                        if( lsibling && (lsibling->h.nrActive == (order - 1) ) ){
                            
                            //
                            // remove the old node before replacing
                            //
                            
                            LogLine();
                            
                            if( node ){
                                this->dereferenceBTreeNode( node, false );
                                DldDecrementReferenceTaken();
                            }
                            
                            node = BTreeMergeSiblings( btree, parent, i, left, false );
                            assert( node );
                            if( !node ){
                                
                                DBG_PRINT_ERROR(("BTreeMergeSiblings() failed\n"));
                                error = ENOMEM;
                                // do not goto __exit as siblings must be freed
                            }
                            DldIncrementReferenceTaken();
                            
                        } else
                            // Right sibling has (t - 1) keys
                            if(rsibling && (rsibling->h.nrActive == (order - 1) ) ){
                                
                                //
                                // remove the old node before replacing
                                //
                                
                                LogLine();
                                
                                if( node ){
                                    this->dereferenceBTreeNode( node, false );
                                    DldDecrementReferenceTaken();
                                }
                                
                                node = BTreeMergeSiblings( btree, parent, i, right, false );
                                assert( node );
                                if( !node ){
                                    
                                    DBG_PRINT_ERROR(("BTreeMergeSiblings() failed\n"));
                                    error = ENOMEM;
                                    // do not goto __exit as siblings must be freed
                                }
                                DldIncrementReferenceTaken();
                            }
                
                LogLine();
                
                if( lsibling ){
                    this->dereferenceBTreeNode( lsibling, false );
                    DldDecrementReferenceTaken();
                }
                lsibling = NULL;
                
                if( rsibling ){
                    this->dereferenceBTreeNode( rsibling, false );
                    DldDecrementReferenceTaken();
                }
                rsibling = NULL;
                
            } // if (node->h.nrActive == (order - 1) && parent)
            
        } // end for
        
        if( error )
            goto __exit;
        
        //
        // Case 1 : The node containing the key is found and is the leaf node.
        // Also the leaf node has keys greater than the minimum required.
        // Simply remove the key
        //
        if( node->h.leaf && ( node->h.nrActive > (order - 1) ) ){
            
            BtNodePos           nodePos;
            
            LogLine();
            
            nodePos.node        = node;
            nodePos.nodeOffset  = node->h.nodeOffset;
            nodePos.index       = index;
            
            error = this->BTreeDeleteKeyFromNode( btree, &nodePos, false );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeDeleteKey failed with an error(%u)\n", error));
            }
            
            goto __exit;
            
        } // end if( node->leaf && ( node->nr_active > (order - 1) ) )
        
        //
        // If the leaf node is the root permit deletion even if the number of keys is
        // less than (t - 1)
        //
        if( node->h.leaf && (node == btree->root) ){
            
            BtNodePos           nodePos;
            
            LogLine();
            
            nodePos.node        = node;
            nodePos.nodeOffset  = node->h.nodeOffset;
            nodePos.index       = index;
            
            error = this->BTreeDeleteKeyFromNode( btree, &nodePos, false );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("BTreeDeleteKey failed with an error(%u)\n", error));
            }
            
            goto __exit;
        }
        
        
        //
        // Case 2: The node containing the key is found and is an internal node
        //
        if( node->h.leaf == false ){
            
            LogLine();
            
            //
            // check for the root with more than a half full single child, in that case
            // the child becomes the new root
            //
            if( 0x1 == node->h.nrActive && DldSparseFile::InvalidOffset == node->children[ 0x1 ] ){
                
                //
                // this is a root with a single more than a half full child
                //
                assert( node == btree->root );
                assert( 0x0  == index );
                assert( DldSparseFile::InvalidOffset != node->children[ 0x0 ] );
                
                LogLine();
                
                BTreeNode* rootChild = this->getBTreeNodeReference( node->children[ 0x0 ], false );
                assert( rootChild );
                if( !rootChild ){
                    
                    DBG_PRINT_ERROR(("getBTreeNodeReference( node->children[ index ], false ) failed\n"));
                    
                    error = ENOMEM;
                    goto __exit;
                }
                DldIncrementReferenceTaken();
                
                assert( rootChild->h.nrActive >= (order - 1) );
                
                //
                // prepare the root for removal, node == btree->root
                //
                assert( node == btree->root );
                this->BTreeDeleteFromTagsListPosition( btree, btree->root, 0x0, false );
                this->removeBTreeNodeFromBackingStore( btree->root, false );
                //
                // the node is referenced by the BTree and by this function, so dereference it twice
                //
                this->dereferenceBTreeNode( btree->root, false ); // the BTree reference has gone
                DldDecrementReferenceTaken();
                this->dereferenceBTreeNode( node, false ); // this function reference has gone
                DldDecrementReferenceTaken();
                node        = NULL;
                btree->root = NULL;
                
                //
                // set the new root, the rootChild has been referenced by getBTreeNodeReference 
                //
                btree->root = rootChild;
                assert( KERN_SUCCESS == error );
                goto __exit;
            }
            
            LogLine();
            assert( index < (2*order -1 ) );
            assert( DldSparseFile::InvalidOffset != node->children[ index ] );
            assert( DldSparseFile::InvalidOffset != node->children[ index + 1 ] );
            
            BTreeNode* child1 = this->getBTreeNodeReference( node->children[ index ], false );
            assert( child1 );
            if( !child1 ){
                
                DBG_PRINT_ERROR(("getBTreeNodeReference( node->children[ index ], false ) failed\n"));
                
                error = ENOMEM;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            BTreeNode* child2 = this->getBTreeNodeReference( node->children[ index + 1 ], false );
            assert( child2 );
            if( !child2 ){
                
                this->dereferenceBTreeNode( child1, false );
                DldDecrementReferenceTaken();
                DBG_PRINT_ERROR(("getBTreeNodeReference( node->children[ index ], false ) failed\n"));
                
                error = ENOMEM;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            if( child1->h.nrActive > (order - 1) ){
                
                BtNodePos  subNodePos;
                
                LogLine();
                
                subNodePos = this->BTreeGetMaxKeyPos( btree, child1, false );
                assert( DldSparseFile::InvalidOffset != subNodePos.nodeOffset );
                if( DldSparseFile::InvalidOffset == subNodePos.nodeOffset ){
                    
                    DBG_PRINT_ERROR(("DldSparseFile::InvalidOffset == subNodePos.node\n"));
                    this->dereferenceBTreeNode( child1, false );
                    this->dereferenceBTreeNode( child2, false );
                    DldDecrementReferenceTaken();
                    DldDecrementReferenceTaken();
                    
                    error = ENOMEM;
                    goto __exit;
                }
                DldIncrementReferenceTaken();
                
                assert( subNodePos.node );
                
                BTreeNode* subNode = subNodePos.node;// this->getBTreeNodeReference( subNodePos.nodeOffset, false );
                assert( subNode );
                if( !subNode ){
                    
                    DBG_PRINT_ERROR(("subNodePos.node is NULL\n"));
                    this->dereferenceBTreeNode( child1, false );
                    this->dereferenceBTreeNode( child2, false );
                    DldDecrementReferenceTaken();
                    DldDecrementReferenceTaken();
                    
                    error = ENOMEM;
                    goto __exit;
                }
                
                keyVal = subNode->keyEntries[subNodePos.index].keyValue;
                node->keyEntries[index] = subNode->keyEntries[subNodePos.index];	
                
                //
                // recursion, TO DO remove this tail recursion
                //
                error = this->BTreeDeleteKey( btree, child1, &keyVal, false );
                assert( !error );
                this->dereferenceBTreeNode( subNode, false );
                this->dereferenceBTreeNode( child1, false );
                this->dereferenceBTreeNode( child2, false );
                DldDecrementReferenceTaken();
                DldDecrementReferenceTaken();
                DldDecrementReferenceTaken();
                
            } else if ( child2->h.nrActive > (order - 1) ){
                
                BtNodePos  subNodePos;
                
                LogLine();
                
                subNodePos = this->BTreeGetMinKeyPos( btree, child2, false );
                assert( DldSparseFile::InvalidOffset != subNodePos.nodeOffset );
                if( DldSparseFile::InvalidOffset == subNodePos.nodeOffset ){
                    
                    DBG_PRINT_ERROR(("DldSparseFile::InvalidOffset == subNodePos.nodeOffset\n"));
                    this->dereferenceBTreeNode( child1, false );
                    this->dereferenceBTreeNode( child2, false );
                    DldDecrementReferenceTaken();
                    DldDecrementReferenceTaken();
                    
                    error = ENOMEM;
                    goto __exit;
                }
                DldIncrementReferenceTaken();
                
                BTreeNode* subNode = subNodePos.node; // this->getBTreeNodeReference( subNodePos.nodeOffset, false );
                assert( subNode );
                if( !subNode ){
                    
                    DBG_PRINT_ERROR(("subNodePos.node is NULL\n"));
                    this->dereferenceBTreeNode( child1, false );
                    this->dereferenceBTreeNode( child2, false );
                    DldDecrementReferenceTaken();
                    DldDecrementReferenceTaken();
                    
                    error = ENOMEM;
                    goto __exit;
                }            
                
                keyVal = subNode->keyEntries[subNodePos.index].keyValue;
                node->keyEntries[index] = subNode->keyEntries[subNodePos.index];	
                
                //
                // recursion, TO DO remove this tail recursion
                //
                error = this->BTreeDeleteKey( btree, child2, &keyVal, false );
                assert( !error );
                this->dereferenceBTreeNode( subNode, false );
                this->dereferenceBTreeNode( child1, false );
                this->dereferenceBTreeNode( child2, false );
                DldDecrementReferenceTaken();
                DldDecrementReferenceTaken();
                DldDecrementReferenceTaken();
                
            } else if ( child1->h.nrActive == (order - 1) && child2->h.nrActive == (order - 1) ){
                
                BTreeNode*  combNode;
                
                LogLine();
                
                combNode = BTreeMergeNodes( btree,
                                           child1,
                                           &node->keyEntries[index],
                                           child2,
                                           false );
                assert( combNode );
                if( !combNode ){
                    
                    DBG_PRINT_ERROR(("BTreeMergeNodes() failed\n"));
                    this->dereferenceBTreeNode( child1, false );
                    this->dereferenceBTreeNode( child2, false );
                    DldDecrementReferenceTaken();
                    DldDecrementReferenceTaken();
                    
                    error = ENOMEM;
                    goto __exit;
                }
                DldIncrementReferenceTaken();
                
                //
                // the children have been combined in a one node and were
                // removed from the backing store by the merge routine
                //
                this->dereferenceBTreeNode( child1, false );
                this->dereferenceBTreeNode( child2, false );
                DldDecrementReferenceTaken();
                DldDecrementReferenceTaken();
                
                child1 = NULL;
                child2 = NULL;
                
                assert( DldSparseFile::InvalidOffset != combNode->h.nodeOffset );
                
                //
                // save a pointer to the new node
                //
                node->children[index] = combNode->h.nodeOffset;
                
                //
                // move all children to the left at one position as (index+1) children
                // has been deleted
                //
                for( int i=index + 1; i < node->h.nrActive; i++ ){
                    
                    node->children[i]       = node->children[i + 1];
                    node->keyEntries[i - 1] = node->keyEntries[i];
                }
                
                node->h.nrActive--;
                
                if( node->h.nrActive == 0 && node == btree->root ){
                    
                    //
                    // remove the empty root ...
                    //
                    
                    LogLine();
                    
                    assert( node == btree->root );
                    this->removeBTreeNodeFromBackingStore( btree->root, false );
                    this->dereferenceBTreeNode( btree->root, false); // the BTree reference has gone
                    DldDecrementReferenceTaken();
                    btree->root = NULL;
                    
                    //
                    // ... and replace with the new one
                    //
                    btree->root = combNode;
                    this->referenceBTreeNode( combNode ); // hold the node
                    DldIncrementReferenceTaken();
                }
                
                //
                // free the node's memory if this is not a root or a parameter
                // and then replace with the combined node pointer
                //
                if( node ){
                    this->dereferenceBTreeNode( node, false );
                    DldDecrementReferenceTaken();
                }
                
                node = combNode;
                goto del_loop;
            }
            
        } // end if( node->leaf == false )
        
        //
        // Case 3:
        // In this case start from the top of the tree and continue
        // moving to the leaf node making sure that each node that
        // we encounter on the way has atleast 't' (order of the tree)
        // keys
        //
        if( node->h.leaf && (node->h.nrActive <= (order - 1) ) ){
            
            //
            // this can only happens when we tried to "cut corners" with already known node
            //
            assert( node->h.nrActive == (order - 1) );
            assert( btree->root != subtree );
            
            LogLine();
            
            //
            // after returning from BTreeDeleteKey the node content is invalid
            //
            error = this->BTreeDeleteKey( btree, btree->root, key, false );
            assert( !error );
        }
        
    __exit:
        
        LogLine();
        
        if( parent ){
            this->dereferenceBTreeNode( parent, false );
            DldDecrementReferenceTaken();
        }
        
        if( node ){
            this->dereferenceBTreeNode( node, false );
            DldDecrementReferenceTaken();
        }
        
        if( error )
            btree->damaged = true;
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

/*
 the function semantic is 
 {
    while( i < node->h.nrActive && kKeyOneBigger == cmp( key, &node->keyEntries[i].keyValue ) ) {
        i++;
    }
 
    return i;
 }
 */
int
DldSparseFile::BTreeGetKeyIndex(
    __in BTree*     btree,
    __in BTreeNode* node,
    __in BTreeKey*  key
    )
{
    CompareKeysFtr  cmp  = btree->compareKeys;
    int             i = 0x0;
    
    //
    // a caller must hold the lock
    //
    
    if( 0x0 == node->h.nrActive )
        return 0x0;
    
    //
    // Fix the index of the key greater than or equal
    // to the key that we would like to search
    //
    int l = 0;
    int r = node->h.nrActive-0x1;
    
    while( l < r ){
        
        BTreeKeyCmp  res;
        
        i = l + (r - l)/2;
        
        res = cmp( key, &node->keyEntries[ i ].keyValue );
        
        if( kKeyOneBigger == res )
            l = i + 0x1;
        else if( kKeyOneSmaller == res )
            r = i - 0x1;
        else
            break;
        
    } // end while( l < r )
    
    i = l + (r - l)/2;
    
    assert( i < node->h.nrActive );
    
    if( kKeyOneBigger == cmp( key, &node->keyEntries[i].keyValue ) ){
        
        //
        // we one element behind the required 
        //  OR
        // we hit left or right border, go other the border if this is the right
        // border or move to the next element if this is the left one which is
        // the bigger element ( if there is one elemet in the array these operations
        // are identical, or you can divide the arry on two subarrys and see that the
        // operations are complimentary )
        //
        i += 0x1;
    }
    
    assert( i <= node->h.nrActive );
    assert( i == node->h.nrActive ||
            kKeyOneSmaller == cmp( key, &node->keyEntries[ i ].keyValue ) || 
            kKeysEquals == cmp( key, &node->keyEntries[ i ].keyValue ) );
    assert( !( 0x0 != i && kKeyOneSmaller == cmp( key, &node->keyEntries[ i - 1 ].keyValue ) ) );
    assert( !( i < (node->h.nrActive-0x1) &&  kKeyOneBigger == cmp( key, &node->keyEntries[ i + 1 ].keyValue ) ) );
    
    return i;
}

//--------------------------------------------------------------------

//
// Function used to get the node containing the given key
// @param btree The btree to be searched
// @param key The the key to be searched
// @return The node and position of the key within the node
// the non NULL node is returned and must be freed by a caller
// the caller must be cautios in changing the node as the node
// might be a root one so any changes must be reflected in the
// BTree root
//
DldSparseFile::BtNodePos
DldSparseFile::BTreeGetNodeByKey(
    __in BTree*    btree,
    __in BTreeKey* key,
    __in bool      acquireLock
    )
{
	BtNodePos       kp;
	BTreeNode*      node;
    CompareKeysFtr  cmp  = btree->compareKeys;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( 0x0 == ( key->mapEntry.sparseFileOffset & ( DldSparseFile::BlockSize - 0x1 ) ) );
    
    kp.node       = NULL;
    kp.nodeOffset = DldSparseFile::InvalidOffset;
    
	node = btree->root;
    this->referenceBTreeNode( node );
    DldIncrementReferenceTaken();
    
	for( int i = 0;; i = 0 ){	
        
        //
        // Find the index of the key greater than or equal
        // to the key that we would like to search
        //
        i = this->BTreeGetKeyIndex( btree, node, key );
		
        /*
	    while( i < node->h.nrActive && kKeyOneBigger == cmp( key, &node->keyEntries[i].keyValue ) ) {
            // key > node->keyVals[i]
		    i++;
	    }
         */
        
        //
	    // If we find such key return the key-value pair
        //
	    if(i < node->h.nrActive && kKeysEquals == cmp( key, &node->keyEntries[i].keyValue ) ) {
            
            // key == node->keyVals[i]
            
            kp.node       = node;
            kp.nodeOffset = node->h.nodeOffset;
		    kp.index      = i;
            
		    break;
	    }
        
        //
	    // If the node is leaf and if we did not find the key 
	    // return NULL
        //
	    if( node->h.leaf ){
            
            assert( NULL == kp.node );
            this->dereferenceBTreeNode( node, acquireLock );
            DldDecrementReferenceTaken();
            node = NULL;
		    break;
	    }
        
        off_t childOffset = node->children[i];
        assert( DldSparseFile::InvalidOffset != childOffset );
        
        //
        // free the cached copy before replacing the pointer
        //
        if( node ){
            
            this->dereferenceBTreeNode( node, acquireLock );
            DldDecrementReferenceTaken();
            node = NULL;
        }
        
        //
	    // got a child node
        //
        node = this->getBTreeNodeReference( childOffset, acquireLock );
        assert( node );
        if( !node ){
            
            DBG_PRINT_ERROR(("getBTreeNodeReference( childOffset, acquireLock ) failed\n"));
            assert( NULL == kp.node );
            break;
        }
        DldIncrementReferenceTaken();
        
	} // end for
    
    if( kp.node ){
        DldCheckReferenceTaken( 0x1 );
    } else {
        DldCheckReferenceTaken( 0x0 );
    }
    
    //
    // this also returns a pointer to a node!
    // a caller must free it
    //
    return kp;
}

//--------------------------------------------------------------------

//
// Function used to get the node either
//  - containing the given key
//  - containing the next bigger key if the equal key value doesn't exist
//
// @param btree The btree to be searched
// @param key The the key to be searched
// @return The node and position of the key within the node
// the non NULL node is returned and must be freed by a caller
// the caller must be cautios in changing the node as the node
// might be a root one so any changes must be reflected in the
// BTree root
//
DldSparseFile::BtNodePos
DldSparseFile::BTreeGetEqualOrBiggerNodeByKey(
    __in BTree*    btree,
    __in BTreeKey* key,
    __in bool      acquireLock
    )
{
	BtNodePos       kp;
	BTreeNode*      node;
    CompareKeysFtr  cmp  = btree->compareKeys;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( 0x0 == ( key->mapEntry.sparseFileOffset & ( DldSparseFile::BlockSize - 0x1 ) ) );
    
    kp.node       = NULL;
    kp.nodeOffset = DldSparseFile::InvalidOffset;
    
	node = btree->root;
    this->referenceBTreeNode( node );
    DldIncrementReferenceTaken();
    
	for( int i = 0;; i = 0 ){	
        
        //
        // Fix the index of the key greater than or equal
        // to the key that we would like to search
        //
        i = this->BTreeGetKeyIndex( btree, node, key );
        /*
	    while( i < node->h.nrActive && kKeyOneBigger == cmp( key, &node->keyEntries[i].keyValue ) ) {
            // key > node->keyVals[i]
		    i++;
	    }
         */
        
        //
	    // If we find an equal key return the key-value pair
        //
	    if( i < node->h.nrActive && kKeysEquals == cmp( key, &node->keyEntries[i].keyValue ) ) {
            
            // key == node->keyVals[i]
            
            kp.node       = node;
            kp.nodeOffset = node->h.nodeOffset;
		    kp.index      = i;
            
		    break;
	    }
        
        //
	    // If the node is leaf and if we did not find the bigger key 
	    // return NULL else return a found bigger key's entry
        //
	    if( node->h.leaf ){
            
            if( i >= node->h.nrActive ){
                
                assert( i == node->h.nrActive );
                assert( NULL == kp.node );
                this->dereferenceBTreeNode( node, acquireLock );
                DldDecrementReferenceTaken();
                node = NULL;
                
            } else {
                
                assert( kKeyOneSmaller == cmp( key, &node->keyEntries[i].keyValue ) );
                assert( !( 0x0 != i && kKeyOneSmaller == cmp( key, &node->keyEntries[i - 0x1].keyValue ) ) );
                
                kp.node       = node;
                kp.nodeOffset = node->h.nodeOffset;
                kp.index      = i;
            }
            
		    break;
	    }
        
        off_t childOffset = node->children[i];
        assert( DldSparseFile::InvalidOffset != childOffset );
        
        //
        // free the cached copy before replacing the pointer
        //
        if( node ){
            
            this->dereferenceBTreeNode( node, acquireLock );
            DldDecrementReferenceTaken();
            node = NULL;
        }
        
        //
	    // got a child node
        //
        node = this->getBTreeNodeReference( childOffset, acquireLock );
        assert( node );
        if( !node ){
            
            DBG_PRINT_ERROR(("getBTreeNodeReference( childOffset, acquireLock ) failed\n"));
            assert( NULL == kp.node );
            break;
        }
        DldIncrementReferenceTaken();
        
	} // end for
    
    if( kp.node ){
        DldCheckReferenceTaken( 0x1 );
    } else {
        DldCheckReferenceTaken( 0x0 );
    }
    
    //
    // this also returns a pointer to a node!
    // a caller must free it
    //
    return kp;
}

//--------------------------------------------------------------------

DldSparseFile::BTreeKeyCmp
DldSparseFile::compareOffsets( __in BTreeKey* key1, __in BTreeKey* key2)
{
    if( key1->mapEntry.sparseFileOffset > key2->mapEntry.sparseFileOffset )
        return kKeyOneBigger;
    else if( key1->mapEntry.sparseFileOffset < key2->mapEntry.sparseFileOffset )
        return kKeyOneSmaller;
    else
        return kKeysEquals;
}

//--------------------------------------------------------------------

DldSparseFile::BTreeKeyCmp
DldSparseFile::compareTags( __in BTreeKey* key1, __in BTreeKey* key2)
{
    if( key1->mapEntry.tag.timeStamp > key2->mapEntry.tag.timeStamp )
        return kKeyOneBigger;
    else if( key1->mapEntry.tag.timeStamp < key2->mapEntry.tag.timeStamp )
        return kKeyOneSmaller;
    else
        return kKeysEquals;
}

//--------------------------------------------------------------------

void DldSparseFile::test()
{
    DldSparseFile*  sfile;
    int             nrEntries = 100000;
    SInt32          nodesAllocated = gBTreeNodeAllocationCounter;
    
    //__asm__ volatile( "int $0x3" );
    
    sfile = DldSparseFile::withPath( "/work/sparse_file_test", NULL, NULL, "test" );
    assert( sfile );
    if( !sfile )
        return;
    
    if( sfile->sparseFileOffsetBTree )
        nodesAllocated += 0x1; // the root has been allocated
    
    BTree* btree = sfile->createBTree( DldSparseFile::compareOffsets, DldSparseFile::InvalidOffset, true );
    assert( btree );
    if( !btree )
        goto __exit;
    
    //
    // the root has been added
    //
    assert( (nodesAllocated + 0x1) == gBTreeNodeAllocationCounter );
    
    // put
    for( int i = 0; i < nrEntries; ++i ){
        
        errno_t   error;
        BTreeKey  key;
        
        key.mapEntry.sparseFileOffset = i * 4096;
        key.mapEntry.tag.timeStamp = i;
        
        error = sfile->BTreeInsertKey( btree, &key, true );
        assert( !error );
        
        //
        // we don't put nodes in the cache yet
        //
        assert( (nodesAllocated + 0x1) == gBTreeNodeAllocationCounter );
        
    } // end for
    
     //__asm__ volatile( "int $0x3" );
    
    // find
    for( int i = 0; i < nrEntries; ++i ){
        
        BTreeKey  key;
        BtNodePos pos;
        
        key.mapEntry.sparseFileOffset = i * 4096;
        
        pos = sfile->BTreeGetNodeByKey( btree, &key, true );
        assert( DldSparseFile::InvalidOffset != pos.nodeOffset );
        assert( pos.node && (i * 4096) == pos.node->keyEntries[pos.index].keyValue.mapEntry.sparseFileOffset );
        assert( i == pos.node->keyEntries[pos.index].keyValue.mapEntry.tag.timeStamp );
        
        if( pos.node )
            sfile->dereferenceBTreeNode( pos.node, true );
        
        //
        // we don't put nodes in the cache yet
        //
        assert( (nodesAllocated + 0x1) == gBTreeNodeAllocationCounter );
        
    } // end for
    
     //__asm__ volatile( "int $0x3" );
    
    // remove
    for( int i = 0; i < nrEntries; ++i ){
        
        errno_t   error;
        BTreeKey  key;
        
        key.mapEntry.sparseFileOffset = i * 4096;
        
        error = sfile->BTreeDeleteKey( btree, btree->root, &key, true );
        assert( !error );
        
        //
        // we don't put nodes in the cache yet
        //
        assert( (nodesAllocated + 0x1) == gBTreeNodeAllocationCounter );
        
    } // end for
    
    assert( btree->root && 0x0 == btree->root->h.nrActive && 0x1 == btree->root->h.referenceCount );
    assert( (nodesAllocated + 0x1) == gBTreeNodeAllocationCounter );
    
__exit:
    
    if( btree )
        sfile->deleteBTree( btree, true );
    
    if( sfile->sparseFileOffsetBTree )
        nodesAllocated -= 0x1; // the root will be removed

    sfile->release();
    
    //
    // the tree and sfile were both removed
    //
    assert( nodesAllocated == gBTreeNodeAllocationCounter );
}

//--------------------------------------------------------------------

void
DldSparseFile::performIO(
    __inout IoBlockDescriptor*  descriptors,
    __in unsigned int dscrCount
    )
{
    assert( preemption_enabled() );
    DldDeclareReeferenceTaken;
    
    this->LockExclusive();
    { // start of the locked block
        
        assert( preemption_enabled() );
        
        bool acquireLock = false;
        
        for( unsigned int i = 0x0; i < dscrCount; ++i ){
            
            IoBlockDescriptor*  dscr = &descriptors[ i ];
            void*               buffer = NULL;
            BTreeKey            key;
            BtNodePos           pos;
            off_t               alignedOffset;
            errno_t             error = KERN_SUCCESS;
            bool                newKey = false;
            
            if( InvalidOffset == dscr->fileOffset || 0x0 == dscr->size ){
                
                //
                // skipped or terminating entry, not an error
                //
                dscr->error = KERN_SUCCESS;
                continue;
            }
            
            assert( dscr->size <= DldSparseFile::BlockSize );
            assert( 0x0 == dscr->flags.blockIsNotPresent );
            
            //
            // check for a block boundary crossing
            //
            assert( DldSparseFile::alignToBlock( dscr->fileOffset + dscr->size - 0x1 ) == DldSparseFile::alignToBlock( dscr->fileOffset ) );
            
            //
            // get the data map for the aligned offset
            //
            alignedOffset = DldSparseFile::alignToBlock( dscr->fileOffset );
            //assert( trunc_page_64( dscr->fileOffset ) == alignedOffset );// just to check the compiler sign extension
            
            bzero( &key, sizeof(key) );
            key.mapEntry.sparseFileOffset = alignedOffset;
            key.mapEntry.tag = dscr->tag;
            
            pos = this->BTreeGetNodeByKey( this->sparseFileOffsetBTree,
                                           &key,
                                           acquireLock );
            assert( KERN_SUCCESS == error );
            if( !pos.node ){
                
                //
                // there is no node for this map, if this is a write operation
                // for aligned offset with a block size then allocate new node
                // and continue the operation ( actually it is enough to check only
                // the buffer size as the buffer can't cross the block boundary )
                //
                if( 0x1 == dscr->flags.write &&
                    alignedOffset == dscr->fileOffset &&
                    DldSparseFile::BlockSize == dscr->size )
                {
                    
                    //
                    // reserve the spase for the data
                    //
                    key.mapEntry.dataFileOffset = allocateChunkFromDataFile( acquireLock );
                    assert( DldSparseFile::InvalidOffset != key.mapEntry.dataFileOffset );
                    if( DldSparseFile::InvalidOffset != key.mapEntry.dataFileOffset ){
                        
                        //
                        // add offset B-Tree entry
                        //
                        error = this->BTreeInsertKey( this->sparseFileOffsetBTree,
                                                      &key,
                                                      acquireLock );
                        assert( !error );
                        if( KERN_SUCCESS == error ){
                            
                            //
                            // retrieve the just created entry
                            //
                            pos = this->BTreeGetNodeByKey( this->sparseFileOffsetBTree,
                                                           &key,
                                                           acquireLock );
                            assert( pos.node );
                            if( !pos.node ){
                                
                                DBG_PRINT_ERROR(("BTreeGetNodeByKey() failed\n"));
                                error = ENOMEM;
                            }
                            
                            newKey = true;
                            
                            if( KERN_SUCCESS == error ){
                                
                                //
                                // if the first entry has been created the CAWLed
                                // file becomes "dirty", the first and single entry
                                // is detected by the absence of children, else this
                                // is a rebalanced B-Tree
                                //
                                assert( this->sparseFileOffsetBTree->root );
                                if( 0x1 == this->sparseFileOffsetBTree->root->h.nrActive && 
                                    DldSparseFile::InvalidOffset == this->sparseFileOffsetBTree->root->children[ 0 ] &&
                                    DldSparseFile::InvalidOffset == this->sparseFileOffsetBTree->root->children[ 1 ] ){
                                    
                                    DldVfsMntHook*  mnt = DldVfsMntHook::withVfsMnt( this->mnt );
                                    assert( mnt && mnt->isAsControlledByCAWL() );
                                    if( mnt ){
                                        
                                        mnt->incrementDirtyCawlFilesCounter();
                                        mnt->release();
                                        
                                    } else {
                                        
                                        DBG_PRINT_ERROR(( "a hook mount object was not found for mnt = %p\n", this->mnt ));
                                    }
                                } // end if( 0x1 == this->sparseFileOffsetBTree->root->h.nrActive )
                            } // if( KERN_SUCCESS == error )
                            
                        } else {
                            
                            DBG_PRINT_ERROR(("BTreeInsertKey() failed with an error(%u)\n", error));
                        }
                        
                    } else {
                        
                        DBG_PRINT_ERROR(("allocateChunkFromDataFile() failed\n"));
                        error = ENOMEM;
                    }

                } // end if( 0x1 == dscr->flags.write &&
                
            } // end if( !pos.node )
            
            if( !pos.node ){
                
                //
                // the block is completely missing
                //
                dscr->flags.blockIsNotPresent = ( KERN_SUCCESS == error) ? 0x1 : 0x0;
                error = ENOENT;
                goto __continue;
            }
            DldIncrementReferenceTaken(); // pos.node
            
            assert( alignedOffset == pos.node->keyEntries[ pos.index ].keyValue.mapEntry.sparseFileOffset );
            assert( 0x0 != pos.node->keyEntries[ pos.index ].keyValue.mapEntry.dataFileOffset ); // the zero offset is for the header
            
            //
            // we have a valid data in the sparse file
            //
            if( dscr->size < DldSparseFile::BlockSize ){
                
                //
                // allocate an intermediate buffer as the IO is not for the entire block,
                // the IO is not allowed to cross the block boundry so checking the block
                // size is enough
                //
                buffer = IOMalloc( DldSparseFile::BlockSize );
                assert( buffer );
                
            } else {
                
                buffer = dscr->buffer;
                assert( buffer );
            }
            
            if( !buffer ){
                
                DBG_PRINT_ERROR(("buffer is NULL\n"));
                
                error = ENOMEM;
                goto __continue;
            }
            
            assert( InvalidOffset != pos.node->keyEntries[ pos.index ].keyValue.mapEntry.dataFileOffset );
            
            //
            // there are two cases for read
            // - read of the entire block itself
            // - unaligned read or write of the size that is smaller than the block size
            //
            if( 0x0 == dscr->flags.write || buffer != dscr->buffer ){
                
                error = this->readChunkFromDataFile( buffer, pos.node->keyEntries[ pos.index ].keyValue.mapEntry.dataFileOffset );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("readChunkFromDataFile() failed with an error(%u)\n", error));
                    goto __continue;
                }
            }
            
            
            if( buffer != dscr->buffer ){
                
                //
                // unaligned case
                //
                
                off_t   delta = dscr->fileOffset - alignedOffset;
                
                assert( dscr->fileOffset >= alignedOffset );
                
                //
                // for write copy data in the intermediate buffer and then issue the IO,
                // for read copy data from the intermediate buffer to output buffer
                //
                if( 0x1 == dscr->flags.write ){
                    
                    //
                    // write IO
                    //
                    memcpy( (char*)buffer+delta, dscr->buffer, dscr->size );
                    
                } else {
                    
                    //
                    // read IO
                    //
                    memcpy( dscr->buffer, (char*)buffer+delta, dscr->size );
                }
                
            } // end if( buffer != dscr->buffer )
            
            //
            // write the block if required
            //
            if( 0x1 == dscr->flags.write ){
                
                //
                // metadata is flushed before the data
                //
                if( !newKey ){
                    
                    //
                    // update the key timestamp
                    //

                    //
                    // actually the assert might fail in the case of concurrent writing threads
                    // TO DO - we need to solve this situation as it might be important for CAWL when
                    // a new data is rewritten with an old and the timeStamp goes in the decrising order
                    //
                    assert( pos.node->keyEntries[ pos.index ].keyValue.mapEntry.tag.timeStamp <= key.mapEntry.tag.timeStamp );
                    
                    pos.node->keyEntries[ pos.index ].keyValue.mapEntry.tag = key.mapEntry.tag;
                    
                    error = this->BTreeUpdateTagsListPosition( this->sparseFileOffsetBTree,
                                                               pos.node,
                                                               pos.index,
                                                               false );
                    assert( !error );
                    if( error ){
                        
                        DBG_PRINT_ERROR(("this->writeChunkToDataFile() failed with an error(%u)\n", error));
                        goto __continue;
                    }
                } // end if( !newKey )
                
                error = this->writeChunkToDataFile( buffer, pos.node->keyEntries[ pos.index ].keyValue.mapEntry.dataFileOffset );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("this->writeChunkToDataFile() failed with an error(%u)\n", error));
                    goto __continue;
                }
                
            } // end if( 0x1 == dscr->flags.write )
            
            //
            // the IO has been completed successfully
            //
            assert( KERN_SUCCESS == error );
            
    __continue:
            
            dscr->error = error;
            
            if( buffer != dscr->buffer )
                IOFree( buffer, DldSparseFile::BlockSize );
            
            if( pos.node ){
                this->dereferenceBTreeNode( pos.node, acquireLock );
                DldDecrementReferenceTaken();
            }
            
        } // end for
        
    } // end of the locked block
    this->UnLockExclusive();
    
    DldCheckReferenceTaken( 0x0 );
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::BTreeUpdateTagsListPosition(
    __inout BTree*      btree,
    __inout BTreeNode*  node,
    __in    int         index,
    __in    bool        acquireLock
    )
{
    errno_t    error = KERN_SUCCESS;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( index < (2 * DldSparseFile::BTreeOrder) - 0x1 );
    
    if( acquireLock )
        this->LockExclusive();
    { // start of the locked region
        
        BTreeKey*       key;
        TagListEntry*   listEntry;
        BtNodePos       posHead;
        BtNodePos       posTail;
        
        posHead.node = NULL;
        posTail.node = NULL;
        
        //
        // at first remove from the list
        //
        error = this->BTreeDeleteFromTagsListPosition( btree, node, index, false );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("BTreeDeleteFromTagsListPosition() failed with an error(%u)\n", error));
            goto __exit;
        }
        
        //
        // and now update the positon in the list
        //
        key       = &node->keyEntries[ index ].keyValue;
        listEntry = &node->keyEntries[ index ].tagListEntry;
        
        if( DldSparseFile::InvalidOffset != this->oldestWrittenBlock ){
            
            BTreeKey   keyList;
            
            //
            // get a list head
            //
            keyList.mapEntry.sparseFileOffset = this->oldestWrittenBlock;
            
            posHead = this->BTreeGetNodeByKey( btree, &keyList, false );
            
            assert( DldSparseFile::InvalidOffset != posHead.nodeOffset );
            assert( posHead.node && keyList.mapEntry.sparseFileOffset == posHead.node->keyEntries[posHead.index].keyValue.mapEntry.sparseFileOffset );
            if( NULL == posHead.node ){
                
                DBG_PRINT_ERROR(("this->oldestWrittenBlock contains an invalid value\n"));
                error = EINVAL;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            //
            // get a list tail
            //
            keyList.mapEntry.sparseFileOffset = posHead.node->keyEntries[posHead.index].tagListEntry.blink;
            assert( DldSparseFile::InvalidOffset != keyList.mapEntry.sparseFileOffset );
            
            posTail = this->BTreeGetNodeByKey( btree, &keyList, false );
            
            assert( DldSparseFile::InvalidOffset != posTail.nodeOffset );
            assert( posTail.node && keyList.mapEntry.sparseFileOffset == posTail.node->keyEntries[posTail.index].keyValue.mapEntry.sparseFileOffset );
            if( NULL == posTail.node ){
                
                DBG_PRINT_ERROR(("posTail contains an invalid value\n"));
                error = EINVAL;
                goto __exit;
            }
            DldIncrementReferenceTaken();
            
            BtNodePos  posLeft;
            
            posLeft.node = NULL;
            

            if( kKeyOneBigger == compareTags( &posHead.node->keyEntries[posHead.index].keyValue, key ) ){
                
                //
                // replace the head with the current key, so the tail becomes the left entry for this procedure to perform
                //
                posLeft = posTail;
                this->referenceBTreeNode( posLeft.node );
                DldIncrementReferenceTaken();
                
                //
                // we have the new head
                //
                this->oldestWrittenBlock = key->mapEntry.sparseFileOffset;
                
            } else {
                
                posLeft = posTail;
                this->referenceBTreeNode( posLeft.node );
                DldIncrementReferenceTaken();
                
                //
                // strat form the tail until we find the node which is smaller or hit the list head
                //
                while( kKeyOneSmaller == compareTags( key, &posLeft.node->keyEntries[posLeft.index].keyValue ) && 
                       posLeft.node != posHead.node )
                {
                    BTreeKey   keyLeft;
                    
                    //
                    // go back to the head
                    //
                    keyLeft.mapEntry.sparseFileOffset = posLeft.node->keyEntries[posLeft.index].tagListEntry.blink;
                    
                    //
                    // dereference before replacing
                    //
                    this->dereferenceBTreeNode( posLeft.node, false );
                    DldDecrementReferenceTaken();
                    
                    posLeft = this->BTreeGetNodeByKey( btree, &keyLeft, false );
                    assert( posLeft.node );
                    if( !posLeft.node ){
                        
                        DBG_PRINT_ERROR(("posLeft.node is NULL\n"));
                        error = EINVAL;
                        break;
                    }
                    DldIncrementReferenceTaken();
                    
                    assert( kKeyOneBigger != compareTags( &posHead.node->keyEntries[posHead.index].keyValue,
                                                          &posLeft.node->keyEntries[posLeft.index].keyValue ) );
                    
                } // end while
                
            }
            
            if( posLeft.node ){
                
                //
                // so the configuration must be
                //
                //  [head]-...-[left]-[key]-[right]-[tail]
                //
                BtNodePos   posRight;
                BTreeKey    keyRight;
                
                keyRight.mapEntry.sparseFileOffset = posLeft.node->keyEntries[posLeft.index].tagListEntry.flink;
                
                posRight = this->BTreeGetNodeByKey( btree, &keyRight, false );
                assert( posRight.node );
                if( posRight.node ){
                    
                    DldIncrementReferenceTaken();
                    
                    assert( !( posLeft.node != posRight.node &&
                               posLeft.node->keyEntries[posLeft.index].keyValue.mapEntry.sparseFileOffset == posRight.node->keyEntries[posRight.index].keyValue.mapEntry.sparseFileOffset ));
                    
                    //
                    // insert between the left and right, if the left and the right are the same this also works
                    //
                    posRight.node->keyEntries[posRight.index].tagListEntry.blink = key->mapEntry.sparseFileOffset;
                    listEntry->flink = posRight.node->keyEntries[posRight.index].keyValue.mapEntry.sparseFileOffset;
                    
                    posLeft.node->keyEntries[posLeft.index].tagListEntry.flink = key->mapEntry.sparseFileOffset;
                    listEntry->blink = posLeft.node->keyEntries[posLeft.index].keyValue.mapEntry.sparseFileOffset;
                    
                    //
                    // write down the modified left and right nodes and the node
                    //
                    error = this->BTreeFlushNode( btree, posLeft.node );
                    assert( !error );
                    if( error ){
                        
                        DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, posLeft.node ) failed with error(%u)\n", error));
                    } // end if( error )
                    
                    if( posRight.node != posLeft.node && !error){
                        
                        error = this->BTreeFlushNode( btree, posRight.node );
                        assert( !error );
                        if( error ){
                            
                            DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, posRight.node ) failed with error(%u)\n", error));
                        } // end if( error )
                    }
                    
                    if( posRight.node != node && posLeft.node != node && !error ){
                        
                        error = this->BTreeFlushNode( btree, node );
                        assert( !error );
                        if( error ){
                            
                            DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, node ) failed with error(%u)\n", error));
                        } // end if( error )
                    }
                    
                    this->dereferenceBTreeNode( posRight.node, false );
                    DldDecrementReferenceTaken();
                    
                } else {
                    
                    DBG_PRINT_ERROR(("posRight.node is NULL\n"));
                    error = EINVAL;
                }
                
                this->dereferenceBTreeNode( posLeft.node, false );
                DldDecrementReferenceTaken();
            } // end if( posLeft.node )
            
        } else {
            
            //
            // the list is empty
            //
            
            //
            // the first comparision in each assert for a case of a new antry and the second for an entry removed from the list
            //
            assert( DldSparseFile::InvalidOffset == listEntry->flink || key->mapEntry.sparseFileOffset == listEntry->flink );
            assert( DldSparseFile::InvalidOffset == listEntry->blink || key->mapEntry.sparseFileOffset == listEntry->blink );
            
            //
            // set a new head
            //
            this->oldestWrittenBlock = key->mapEntry.sparseFileOffset;
            
            //
            // init the head
            //
            if( listEntry->flink != key->mapEntry.sparseFileOffset ){
                
                listEntry->flink = key->mapEntry.sparseFileOffset;
                listEntry->blink = key->mapEntry.sparseFileOffset;
                
                //
                // flush the modified node
                //
                error = this->BTreeFlushNode( btree, node );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, node ) failed with error(%u)\n", error));
                } // end if( error )
            }
            
            assert( listEntry->flink == key->mapEntry.sparseFileOffset && listEntry->blink == key->mapEntry.sparseFileOffset );
            assert( !error );
        }

        
    __exit:
        if( posHead.node ){
            this->dereferenceBTreeNode( posHead.node, false );
            DldDecrementReferenceTaken();
        }
        
        if( posTail.node ){
            
            this->dereferenceBTreeNode( posTail.node, false );
            DldDecrementReferenceTaken();
        }
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    if( error )
        btree->damaged = true;
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::BTreeDeleteFromTagsListPosition(
    __inout BTree*      btree,
    __inout BTreeNode*  node,
    __in    int         index,
    __in    bool        acquireLock
    )
{
    errno_t    error = KERN_SUCCESS;
    DldDeclareReeferenceTaken;
    
    assert( preemption_enabled() );
    assert( index < (2 * DldSparseFile::BTreeOrder) - 0x1 );
    
    
    if( acquireLock )
        this->LockExclusive();
    { // start of the locked region
        
        BtNodePos   posRight;
        BTreeKey    keyRight;
        BtNodePos   posLeft;
        BTreeKey    keyLeft;
        
        posRight.node = NULL;
        posLeft.node  = NULL;
        
        if( DldSparseFile::InvalidOffset == node->keyEntries[index].tagListEntry.flink ){
            
            //
            // the entry is not in the list
            //
            assert( DldSparseFile::InvalidOffset == node->keyEntries[index].tagListEntry.blink );
            goto __exit;
        }
        
        assert( DldSparseFile::InvalidOffset != node->keyEntries[index].tagListEntry.flink &&
                DldSparseFile::InvalidOffset != node->keyEntries[index].tagListEntry.blink );
        
        keyRight.mapEntry.sparseFileOffset = node->keyEntries[index].tagListEntry.flink;
        keyLeft.mapEntry.sparseFileOffset  = node->keyEntries[index].tagListEntry.blink;
        
        posRight = this->BTreeGetNodeByKey( btree, &keyRight, false );
        assert( posRight.node );
        if( !posRight.node ){
            
            DBG_PRINT_ERROR(("posRight.node is NULL\n"));
            error = EINVAL;
            goto __exit;
        }
        DldIncrementReferenceTaken();
        
        posLeft = this->BTreeGetNodeByKey( btree, &keyLeft, false );
        assert( posLeft.node );
        if( !posLeft.node ){
            
            DBG_PRINT_ERROR(("posLeft.node is NULL\n"));
            error = EINVAL;
            goto __exit;
        }
        DldIncrementReferenceTaken();
        
        assert( posRight.node->keyEntries[posRight.index].tagListEntry.blink == node->keyEntries[index].keyValue.mapEntry.sparseFileOffset );
        assert( posLeft.node->keyEntries[posLeft.index].tagListEntry.flink == node->keyEntries[index].keyValue.mapEntry.sparseFileOffset );
        
        //
        // remove the node from the list, if the node is the single element in the list this does nothing
        //
        posRight.node->keyEntries[posRight.index].tagListEntry.blink = node->keyEntries[index].tagListEntry.blink;
        posLeft.node->keyEntries[posLeft.index].tagListEntry.flink   = node->keyEntries[index].tagListEntry.flink;
        
        if( node->keyEntries[index].keyValue.mapEntry.sparseFileOffset == this->oldestWrittenBlock ){
            
            //
            // the head has been removed, set a new one
            //
            if( posLeft.node->keyEntries[posLeft.index].keyValue.mapEntry.sparseFileOffset != node->keyEntries[index].keyValue.mapEntry.sparseFileOffset )
                this->oldestWrittenBlock = posLeft.node->keyEntries[posLeft.index].keyValue.mapEntry.sparseFileOffset;
            else
                this->oldestWrittenBlock = DldSparseFile::InvalidOffset;
        }
        
        //
        // set the removed entry flink and blink pointers to point the entry itself
        //
        node->keyEntries[index].tagListEntry.blink = node->keyEntries[index].keyValue.mapEntry.sparseFileOffset;
        node->keyEntries[index].tagListEntry.flink = node->keyEntries[index].keyValue.mapEntry.sparseFileOffset;
        
        //
        // write down the modified left and right nodes and the node
        //
        error = this->BTreeFlushNode( btree, posLeft.node );
        assert( !error );
        if( error ){
            
            DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, posLeft.node ) failed with error(%u)\n", error));
        } // end if( error )
        
        if( posRight.node != posLeft.node && !error){
            
            error = this->BTreeFlushNode( btree, posRight.node );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, posRight.node ) failed with error(%u)\n", error));
            } // end if( error )
        }
        
        if( posRight.node != node && posLeft.node != node && !error ){
            
            error = this->BTreeFlushNode( btree, node );
            assert( !error );
            if( error ){
                
                DBG_PRINT_ERROR(("this->BTreeFlushNode( btree, node ) failed with error(%u)\n", error));
            } // end if( error )
        }
        
        //
        // check that there are no entries left which are not accounted for in the tags list,
        // btree->root->h.nrActive is checked for 0x1 as the ast entry is removed first from
        // the tags list and then from the offset list,
        // the assert can't be placed after __exit as during the insertion the remove is called
        // to reposition an entry and this can be done for an empty list, when the ehtry has never
        // been in a list ( a first created entry ), this will break an assertion, the corresponding
        // call stack is below
        /*
         #3  0x4725ccf0 in DldSparseFile::BTreeDeleteFromTagsListPosition (this=0xae59490, btree=0xaa37590, node=0x5ef7000, index=0, acquireLock=false) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:4191
         #4  0x4725ce07 in DldSparseFile::BTreeUpdateTagsListPosition (this=0xae59490, btree=0xaa37590, node=0x5ef7000, index=0, acquireLock=false) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:3803
         #5  0x47265dcb in DldSparseFile::BTreeInsertNonfull (this=0xae59490, btree=0xaa37590, parentNode=0x5ef7000, keyVal=0x31b537a8, acquireLock=false) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:1578
         #6  0x47266fdb in DldSparseFile::BTreeInsertKey (this=0xae59490, btree=0xaa37590, keyVal=0x31b537a8, acquireLock=false) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:1796
         #7  0x4726a629 in DldSparseFile::performIO (this=0xae59490, descriptors=0x31b53990, dscrCount=1) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:3577
         #8  0x470d09b2 in DldCoveringFsd::rwData (this=0x56c26b0, coveredVnode=0x31b53cb8, args=0x31b53bc4) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldCoveringVnode.cpp:1500
         #9  0x470d3e0d in DldCoveringFsd::processWrite (this=0x56c26b0, coverdVnode=0x31b53cb8, ap=0x31b53d9c, result=0x31b53d18) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldCoveringVnode.cpp:2022
         #10 0x471ba4be in DldFsdWriteHook (ap=0x31b53d9c) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldVNodeHook.cpp:956
         #11 0x0032eaf8 in VNOP_WRITE (vp=0xad9b48c, uio=0x31b53e74, ioflag=1, ctx=0x31b53f04) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/kpi_vfs.c:3520
         #12 0x003239eb in vn_write (fp=0x71d0610, uio=0x31b53e74, flags=<value temporarily unavailable, due to optimizations>, ctx=0x31b53f04) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_vnops.c:845
         #13 0x004f017b in dofilewrite (ctx=0x31b53f04, fp=0x71d0610, bufp=8591663104, nbyte=3132, offset=-1, flags=0, retval=0x6abe254) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/sys_generic.c:568
         #14 0x004f02d9 in write_nocancel (p=0x6b552c0, uap=0x65028a8, retval=0x6abe254) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/sys_generic.c:458
         #15 0x0054e9fd in unix_syscall64 (state=0x65028a4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365
         (gdb) frame 3
         #3  0x4725ccf0 in DldSparseFile::BTreeDeleteFromTagsListPosition (this=0xae59490, btree=0xaa37590, node=0x5ef7000, index=0, acquireLock=false) at /work/DL_MacSvn/mac/dl-0.x/DLDriver/DldSparseFile.cpp:4191         
         */
        //
        assert( !( 0x1 != btree->root->h.nrActive && DldSparseFile::InvalidOffset == this->oldestWrittenBlock ) );
        
    __exit:
        
        if( posRight.node ){
            this->dereferenceBTreeNode( posRight.node, false );
            DldDecrementReferenceTaken();
        }
        
        if( posLeft.node ){
            this->dereferenceBTreeNode( posLeft.node, false );
            DldDecrementReferenceTaken();
        }
        
    } // end of the locked region
    if( acquireLock )
        this->UnLockExclusive();
    
    if( error )
        btree->damaged = true;
    
    DldCheckReferenceTaken( 0x0 );
    
    return error;
}

//--------------------------------------------------------------------

errno_t DldSparseFile::BTreeFlushNode( __in BTree* btree, __in BTreeNode* node )
{

    //
    // if the node is cached then do not flush it to not generate the IO traffic,
    // just mark it as dirty
    //
    if( !IsListEmpty( &node->h.listEntry ) ){
        
        node->h.dirty = true;
        return KERN_SUCCESS;
    } // end if( !IsListEmpty( &node->h.listEntry ) )
   
    errno_t  error;
    
    //
    // flip the dirty flag before flushing
    //
    node->h.dirty = false;
    
    error = this->writeChunkToDataFile( node, node->h.nodeOffset );
    assert( !error );
    if( error ){
        
        btree->damaged = true;
        DBG_PRINT_ERROR(("this->writeChunkToDataFile( node, node->h.nodeOffset ) failed with error(%u)\n", error));
    } // end if( error )
    
    return error;
}

//--------------------------------------------------------------------

vnode_t
DldSparseFile::exchangeCawlRelatedVnode( __in vnode_t newVnode )
{
    vnode_t     oldVnode;
    
    //
    // covering vnode doesn't have attached data
    //
    assert( !( newVnode && NULL != vnode_fsnode( newVnode ) ) );
    
    oldVnode = this->identificationInfo.cawlCoveringVnode;
    
    while( !OSCompareAndSwapPtr( oldVnode, newVnode, &this->identificationInfo.cawlCoveringVnode ) ){
        
        oldVnode = this->identificationInfo.cawlCoveringVnode;
    }
    
    return oldVnode;
}

//--------------------------------------------------------------------

vnode_t
DldSparseFile::getCoveringVnodeRefBySparseFileID(
    __in const unsigned char* sparseFileID/*char[16]*/ 
    )
{
    DldSparseFile*   sparseFile;
    
    DldSparseFilesHashTable::sSparseFilesHashTable->LockShared();
    { // strat of the lock
        
        sparseFile = DldSparseFilesHashTable::sSparseFilesHashTable->RetrieveEntry( sparseFileID, true );
        
    } // end of the lock
    DldSparseFilesHashTable::sSparseFilesHashTable->UnLockShared();
    
    if( !sparseFile )
        return NULL;
    
    DldIOVnode*  dldCoveringVnode;
    vnode_t      coveringVnode = NULL;
    
    dldCoveringVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( sparseFile->identificationInfo.cawlCoveringVnode );
    assert( dldCoveringVnode );
    if( dldCoveringVnode ){
        
        coveringVnode = dldCoveringVnode->getReferencedVnode();
        dldCoveringVnode->release();
    }
    
    sparseFile->release();
    
    //
    // covering vnode doesn't have attached data
    //
    assert( !( coveringVnode && NULL != vnode_fsnode( coveringVnode ) ) );
    
    return coveringVnode;
}

//--------------------------------------------------------------------

errno_t
DldSparseFile::flushUpToTimeStamp(
    __in SInt64   timeStamp,
    __in vnode_t  coveredVnode,
    __in off_t    fileDataSize,
    __in vfs_context_t vfsContext
    )
{
    
    errno_t    error = KERN_SUCCESS;
    
    this->LockExclusive();
    { // strat of the locked region
        
        bool   allDone = false;
        bool   lastEntryRemoved = false;
        
        while( !error && !allDone && DldSparseFile::InvalidOffset != this->oldestWrittenBlock ){
            
            BTreeKey   key;
            BtNodePos  posHead;
            
            //
            // get a list head, the entry will be removed so the head will change
            //
            key.mapEntry.sparseFileOffset = this->oldestWrittenBlock;
            
            posHead = this->BTreeGetNodeByKey( this->sparseFileOffsetBTree, &key, false );
            assert( NULL != posHead.node );
            if( NULL == posHead.node ){
                
                DBG_PRINT_ERROR(( "posHead.node is NULL\n" ));
                break;
            }
            
            assert( DldSparseFile::InvalidOffset != posHead.node->keyEntries[ posHead.index ].keyValue.mapEntry.dataFileOffset );
            
            allDone = (timeStamp < posHead.node->keyEntries[ posHead.index ].keyValue.mapEntry.tag.timeStamp);
            
            if( !allDone ){
                
                //
                // write down the block
                //
                void*   block;
                
                block = IOMalloc( DldSparseFile::BlockSize );
                // NO breaks after this point else the block won't be freed
                assert( block );
                if( block ){
                    
                    off_t  dataOffsetRead = posHead.node->keyEntries[ posHead.index ].keyValue.mapEntry.dataFileOffset;
                    off_t  dataOffsetWrite = posHead.node->keyEntries[ posHead.index ].keyValue.mapEntry.sparseFileOffset;
                    
                    //
                    // read data from the sparse file
                    //
                    error = this->readChunkFromDataFile( block, dataOffsetRead );
                    if( !error ){
                        
                        //
                        // remove the key for the block, from here the posHead.node might
                        // contain invalid values, this also changes the head of the tags list
                        //
                        error = this->BTreeDeleteKey( this->sparseFileOffsetBTree,
                                                      this->sparseFileOffsetBTree->root,
                                                      &key,
                                                      false );
                        lastEntryRemoved = ( 0x0 == this->sparseFileOffsetBTree->root->h.nrActive );
                        assert( !error );
                        if( !error ){
                            
                            //
                            // return the memory occupied by the block to the free list
                            //
                            error = this->returnChunkToFreeList( dataOffsetRead, false );
                            assert( !error );
                            if( !error ){
                                
                                int sizeToWrite = DldSparseFile::BlockSize;
                                
                                if( (dataOffsetWrite + sizeToWrite) > fileDataSize )
                                    sizeToWrite = (int)( fileDataSize - dataOffsetWrite );
                                
                                    if( sizeToWrite > 0x0 ){
                                        
                                        //
                                        // do the write w/o lock being held
                                        //
                                        this->UnLockExclusive();
                                        { // start of the unlocked region
                                            
                                            //
                                            // write to the covered vnode
                                            //
                                            error = vn_rdwr( UIO_WRITE,
                                                             coveredVnode,
                                                             (char*)block,
                                                             sizeToWrite,
                                                             dataOffsetWrite,
                                                             UIO_SYSSPACE,
                                                             IO_NOAUTH | IO_SYNC,
                                                             vfs_context_ucred(vfsContext),
                                                             NULL,
                                                             vfs_context_proc(vfsContext) );
                                            assert( !error );
                                            if( error ){
                                                
                                                DBG_PRINT_ERROR(("vn_rdwr() failed with an error(%u)\n", error));
                                            }
                                            
                                        } // end of the unlocked region
                                        this->LockExclusive();
                                        
                                    } // end if( sizeToWrite > 0x0 )
                                
                            } else {
                                
                                DBG_PRINT_ERROR(("returnChunkToFreeList() failed with an error(%u)\n", error));
                            }
                            
                        } else {
                            
                            DBG_PRINT_ERROR(("BTreeDeleteKey() failed with an error(%u)\n", error));
                        }
                        
                    } else {
                        
                        DBG_PRINT_ERROR(("readChunkFromDataFile() failed with an error(%u)\n", error));
                    }
                    
                    IOFree( block, DldSparseFile::BlockSize );
                    
                } else {
                    
                    DBG_PRINT_ERROR(("IOMalloc() failed\n"));
                    error = ENOMEM;
                }
                
            } // end if( timeStamp >= posHead.node->keyEntries[index].keyValue.mapEntry.tag.timeStamp )
            
            this->dereferenceBTreeNode( posHead.node, false );
            
        } // end while( DldSparseFile::InvalidOffset != this->oldestWrittenBlock )
        
        //
        // if all data has been flushed then decrement the dirty files count,
        // the second check is required as the lock was released after lastEntryRemoved
        // was set, so a concurrent thread could add a new entry, for the same reason of
        // concurrency the second check is not enough as might result in underflow
        //
        assert( this->sparseFileOffsetBTree->root );
        if( lastEntryRemoved && 0x0 == this->sparseFileOffsetBTree->root->h.nrActive ){
            
            DldVfsMntHook*  mnt = DldVfsMntHook::withVfsMnt( this->mnt );
            assert( mnt && mnt->isAsControlledByCAWL() );
            if( mnt ){
                
                mnt->decrementDirtyCawlFilesCounter();
                mnt->release();
                
            } else {
                
                DBG_PRINT_ERROR(( "a hook mount object was not found for mnt = %p\n", this->mnt ));
            }
        } // if( 0x0 == this->sparseFileOffsetBTree->root->h.nrActive )
        
    } // end of the locked region
    this->UnLockExclusive();
    
    return error;
}

//--------------------------------------------------------------------

//
// purges data fromn the sparse file starting from the purgeOffset
//
errno_t
DldSparseFile::purgeFromOffset(
    __in off_t   purgeOffset
    )
{
    
    errno_t    error = KERN_SUCCESS;
    
    assert( preemption_enabled() );
    
    //
    // round up to BlockSize size as the data granularity is one BlockSize
    //
    purgeOffset = ( purgeOffset + BlockSize - 0x1 ) & ~(BlockSize - 0x1);
    
    //
    // two cases
    //   - entire purge from 0x0 offset ( a very often case )
    //   - purge from non zero offset
    //
    if( false && 0x0 == purgeOffset ){
        
        //
        // replace the 
        this->LockExclusive();
        { // strat of the locked region
            
            // TO DO            
        } // end of the locked region
        this->UnLockExclusive();

    } else {
        
        this->LockExclusive();
        { // strat of the locked region

            bool        lastEntryRemoved = false;
            
            //
            // find the key and purge its entry, do this untill all keys that are bigger have been purged
            //
            do{
                
                BtNodePos  maxPos;
                
                maxPos = this->BTreeGetMaxKeyPos( this->sparseFileOffsetBTree,
                                                  this->sparseFileOffsetBTree->root,
                                                  false );
                
                if( NULL == maxPos.node || maxPos.node->keyEntries[ maxPos.index ].keyValue.mapEntry.sparseFileOffset < purgeOffset ){
                    
                    if( maxPos.node )
                        this->dereferenceBTreeNode( maxPos.node, false );
                    
                    break;
                }
                
                //pos = this->BTreeGetEqualOrBiggerNodeByKey( this->sparseFileOffsetBTree, &key, false );
                
                BTreeKey    keyToDelete;
                off_t       dataOffsetToFree;
                
                bzero( &keyToDelete, sizeof( keyToDelete ) );
                
                keyToDelete.mapEntry.sparseFileOffset = maxPos.node->keyEntries[ maxPos.index ].keyValue.mapEntry.sparseFileOffset;
                dataOffsetToFree = maxPos.node->keyEntries[ maxPos.index ].keyValue.mapEntry.dataFileOffset;
                
                //
                // remove the key for the block, from here the pos.node might
                // contain invalid values, this also changes the head of the tags list
                //
                error = this->BTreeDeleteKey( this->sparseFileOffsetBTree,
                                              maxPos.node,
                                              &keyToDelete,
                                              false );
                assert( !error );
                if( !error ){
                    
                    //
                    // return the memory occupied by the block to the free list
                    //
                    error = this->returnChunkToFreeList( dataOffsetToFree, false );
                    assert( !error );
                    if( error ){
                        
                        DBG_PRINT_ERROR(("returnChunkToFreeList() failed with an error(%u)\n", error));
                        // do not break here as the node must be dereferenced!
                    }
                    
                } else {
                    
                    DBG_PRINT_ERROR(("BTreeDeleteKey() failed with an error(%u)\n", error));
                    // do not break here as the node must be dereferenced!
                }
                
                lastEntryRemoved = ( 0x0 == this->sparseFileOffsetBTree->root->h.nrActive );
                
                this->dereferenceBTreeNode( maxPos.node, false );
                
            } while( !error );
            
            //
            // if all data has been removed then decrement the dirty files count
            //
            assert( this->sparseFileOffsetBTree->root );
            if( lastEntryRemoved ){
                
                DldVfsMntHook*  mnt = DldVfsMntHook::withVfsMnt( this->mnt );
                assert( mnt && mnt->isAsControlledByCAWL() );
                if( mnt ){
                    
                    mnt->decrementDirtyCawlFilesCounter();
                    mnt->release();
                    
                } else {
                    
                    DBG_PRINT_ERROR(( "a hook mount object was not found for mnt = %p\n", this->mnt ));
                }
            } // if( 0x0 == this->sparseFileOffsetBTree->root->h.nrActive )
            
        } // end of the locked region
        this->UnLockExclusive();
    }
    
    return error;
}

//--------------------------------------------------------------------

DldSparseFilesHashTable* DldSparseFilesHashTable::sSparseFilesHashTable = NULL;

//--------------------------------------------------------------------

bool
DldSparseFilesHashTable::CreateStaticTableWithSize( int size, bool non_block )
{
    assert( !DldSparseFilesHashTable::sSparseFilesHashTable );
    
    DldSparseFilesHashTable::sSparseFilesHashTable = DldSparseFilesHashTable::withSize( size, non_block );
    assert( DldSparseFilesHashTable::sSparseFilesHashTable );
    
    gDldDbgData.sSparseFilesHashTable = (void*)sSparseFilesHashTable;
    
    return ( NULL != DldSparseFilesHashTable::sSparseFilesHashTable );
}

void
DldSparseFilesHashTable::DeleteStaticTable()
{
    if( NULL != DldSparseFilesHashTable::sSparseFilesHashTable ){
        
        DldSparseFilesHashTable::sSparseFilesHashTable->free();
        
        delete DldSparseFilesHashTable::sSparseFilesHashTable;
        
        DldSparseFilesHashTable::sSparseFilesHashTable = NULL;
    }// end if
}

//--------------------------------------------------------------------

DldSparseFilesHashTable*
DldSparseFilesHashTable::withSize( int size, bool non_block )
{
    DldSparseFilesHashTable* sparseFilesHashTable;
    
    assert( preemption_enabled() );
    
    sparseFilesHashTable = new DldSparseFilesHashTable();
    assert( sparseFilesHashTable );
    if( !sparseFilesHashTable )
        return NULL;
    
    sparseFilesHashTable->RWLock = IORWLockAlloc();
    assert( sparseFilesHashTable->RWLock );
    if( !sparseFilesHashTable->RWLock ){
        
        delete sparseFilesHashTable;
        return NULL;
    }
    
    sparseFilesHashTable->HashTable = ght_create( size, non_block );
    assert( sparseFilesHashTable->HashTable );
    if( !sparseFilesHashTable->HashTable ){
        
        IORWLockFree( sparseFilesHashTable->RWLock );
        sparseFilesHashTable->RWLock = NULL;
        
        delete sparseFilesHashTable;
        return NULL;
    }
    
    return sparseFilesHashTable;
}

//--------------------------------------------------------------------

void
DldSparseFilesHashTable::free()
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
        DBG_PRINT_ERROR( ("DldSparseFilesHashTable::free() found an entry for an object(0x%p)\n", *(void**)p_key ) );
        
        DldSparseFile* entry = (DldSparseFile*)p_e->p_data;
        assert( entry );
        entry->release();
        
        p_table->fn_free( p_e, p_e->size );
    }
    
    ght_finalize( p_table );
    
    IORWLockFree( this->RWLock );
    this->RWLock = NULL;
    
}

//--------------------------------------------------------------------

bool
DldSparseFilesHashTable::AddEntry(
    __in unsigned char* coveredVnodeID/*char[16]*/,
    __in DldSparseFile* entry
    )
/*
 the caller must allocate space for the entry and
 free it only after removing the entry from the hash,
 the entry is referenced, so a caller can release it
 */
{
    GHT_STATUS_CODE RC;
    
    assert( 16 == sizeof( entry->identificationInfo.cawlCoveredVnodeID ) );
    assert( 0x0 == memcmp( coveredVnodeID, entry->identificationInfo.cawlCoveredVnodeID, sizeof( entry->identificationInfo.cawlCoveredVnodeID ) ) );
    
    RC = ght_insert( this->HashTable, entry, 16, coveredVnodeID );
    assert( GHT_OK == RC );
    if( GHT_OK != RC ){
        
        DBG_PRINT_ERROR( ( "DldSparseFilesHashTable::AddEntry->ght_insert( 0x%p, 0x%p ) failed RC = 0x%X\n",
                          (void*)coveredVnodeID, (void*)entry, RC ) );
    } else {
        
        entry->retain();
        entry->inHashTable = true;
    }
    
    return ( GHT_OK == RC );
}

//--------------------------------------------------------------------

DldSparseFile*
DldSparseFilesHashTable::RemoveEntry(
    __in const unsigned char* coveredVnodeID/*char[16]*/
    )
/*
 the returned entry is referenced!
 */
{
    DldSparseFile* entry;
    
    //
    // the entry was refernced when was added to the hash table
    //
    entry = (DldSparseFile*)ght_remove( this->HashTable, 16, coveredVnodeID );
    if( entry ){
        
        assert( true == entry->inHashTable );
        entry->inHashTable = false;
    }
    
    return entry;
}

//--------------------------------------------------------------------

void
DldSparseFilesHashTable::RemoveEntryByObject( __in DldSparseFile* sparseFile )
{
    DldSparseFile*   removedSparseFile = NULL;
    
    assert( preemption_enabled() );
    
    DldSparseFilesHashTable::sSparseFilesHashTable->LockExclusive();
    { // strat of the lock
        
        //
        // if the entry is not in the hash do not remove, this is not
        // only optimization - this is a protection from removing a new just
        // created entry with the same ID
        //
        if( sparseFile->inHashTable ){
            
            removedSparseFile = DldSparseFilesHashTable::sSparseFilesHashTable->RemoveEntry( sparseFile->identificationInfo.cawlCoveredVnodeID );
            assert( sparseFile == removedSparseFile );
            assert( !sparseFile->inHashTable );
        }
        
    } // end of the lock
    DldSparseFilesHashTable::sSparseFilesHashTable->UnLockExclusive();
    
    if( removedSparseFile )
        removedSparseFile->release();
}

//--------------------------------------------------------------------

DldSparseFile*
DldSparseFilesHashTable::RetrieveEntry(
    __in const unsigned char* coveredVnodeID/*char[16]*/,
    __in bool reference
    )
/*
 the returned entry is referenced if the refernce's value is "true"
 */
{
    DldSparseFile* entry;
    
    entry = (DldSparseFile*)ght_get( this->HashTable, 16, coveredVnodeID );
    
    if( entry && reference )
        entry->retain();
    
    assert( !( entry && !entry->inHashTable ) );
    
    return entry;
}

//--------------------------------------------------------------------

