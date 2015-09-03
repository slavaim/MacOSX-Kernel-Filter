/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */


#include "IOBSDSystem.h"
#include <sys/vnode.h>
#include <IOKit/serial/IOSerialKeys.h>

//--------------------------------------------------------------------

#if defined(DBG)
    #define DLD_MNT_CACHE_INIT_CAPACITY 0x1
    #define DLD_MNT_CACHE_INCREMENT     0x2
#else
    #define DLD_MNT_CACHE_INIT_CAPACITY 0x4
    #define DLD_MNT_CACHE_INCREMENT     0x4
#endif

//--------------------------------------------------------------------

typedef struct _DldMntCacheEntry{
    
    IORegistryEntry*  ioMedia;// not referenced
    mount_t           mnt;
    
    //
    // the media name is needed as we can't control unmount requests so
    // there might be stalled entries in the cache if a filesystem
    // has been remounted
    //
    char              f_mntfromname[MAXPATHLEN];/* BSD device name as in the mount structure */
    
} DldMntCacheEntry;

typedef struct _DldMntCache{
    
    IORWLock*           RWlock;
    
    unsigned int        arrayValidEntries;
    unsigned int        arrayNumOfEntries;
    DldMntCacheEntry*   array;
    
} DldMntCache;

//--------------------------------------------------------------------

static DldMntCache    gMntCache;
static mount_t        gDevDirMount;// a mount_t for /dev
static IOService*     gBootMedia; // a referenced object

//--------------------------------------------------------------------

IOService* DldGetReferencedBootIOMedia();

//--------------------------------------------------------------------

bool
DldInitBSD()
{
    
    assert( preemption_enabled() );
    
    gDldDbgData.gMntCache = (void*)&gMntCache;
    
    gMntCache.RWlock = IORWLockAlloc();
    assert( gMntCache.RWlock );
    if( !gMntCache.RWlock ){
        
        DBG_PRINT_ERROR(( "DldInitBSD()->IORWLockAlloc() failed" ));
        return false;
    }
    
    gMntCache.arrayValidEntries = 0x0;
    gMntCache.arrayNumOfEntries = DLD_MNT_CACHE_INIT_CAPACITY;
    gMntCache.array = (DldMntCacheEntry*)IOMalloc( sizeof( gMntCache.array[0] ) * gMntCache.arrayNumOfEntries );
    assert( gMntCache.array );
    if( !gMntCache.array ){
        
        DBG_PRINT_ERROR(( "DldInitBSD()->IOMalloc() failed" ));
        
        IORWLockFree( gMntCache.RWlock );
        gMntCache.RWlock = NULL;
        
        return false;
    }
    
#if defined( DBG )
    //__asm__ volatile( "int $0x3" );
#endif
    
    bzero( gMntCache.array, sizeof( gMntCache.array[0] ) * gMntCache.arrayNumOfEntries );
    
#if defined( DBG )
    //__asm__ volatile( "int $0x3" );
#endif
    
    gBootMedia = DldGetReferencedBootIOMedia();
    assert( gBootMedia );
    
    return true;
}

void
DldUninitBSD()
{
    assert( preemption_enabled() );
    
    if( gBootMedia )
        gBootMedia->release();
    
    if( gMntCache.RWlock )
        IORWLockFree( gMntCache.RWlock );
    
    if( gMntCache.array )
        IOFree( gMntCache.array, sizeof( gMntCache.array[0] ) * gMntCache.arrayNumOfEntries );
}

//--------------------------------------------------------------------

DldMntCacheEntry*
DldRetrieveCacheEntryByMountWOLock(
    __in mount_t  mnt
    )
{
    int i;
    int validCount;
    
    assert( mnt );
    assert( gMntCache.array );
    assert( gMntCache.arrayValidEntries <= gMntCache.arrayNumOfEntries );
    
    for( i = 0x0, validCount = 0x0; validCount < gMntCache.arrayValidEntries; ++i ){
        
        assert( i < gMntCache.arrayNumOfEntries );
        assert( (NULL == gMntCache.array[ i ].ioMedia) == (NULL == gMntCache.array[ i ].mnt) );
        
        if( gMntCache.array[ i ].ioMedia )
            ++validCount;
        
        //
        // the name check is required as there might be remounts resulting in stalled entries
        //
        if( mnt == gMntCache.array[ i ].mnt &&
            0x0 == strcmp( gMntCache.array[ i ].f_mntfromname, vfs_statfs( mnt )->f_mntfromname ) ){
            
            return &gMntCache.array[ i ];
        }
        
    }// end for
    
    return NULL;
}

//--------------------------------------------------------------------

DldMntCacheEntry*
DldRetrieveCacheEntryByMediaWOLock(
    __in IORegistryEntry*  ioMedia
    )
/*
 the fuction returns the first found entry, so the returned entry
 might be a stalled one and should be verified
 */
{
    int i;
    int validCount;
    
    assert( ioMedia );
    assert( gMntCache.array );
    assert( gMntCache.arrayValidEntries <= gMntCache.arrayNumOfEntries );
    
    for( i = 0x0, validCount = 0x0; validCount < gMntCache.arrayValidEntries; ++i ){
        
        assert( i < gMntCache.arrayNumOfEntries );
        assert( (NULL == gMntCache.array[ i ].ioMedia) == (NULL == gMntCache.array[ i ].mnt) );
        
        if( gMntCache.array[ i ].ioMedia )
            ++validCount;
        
        if( ioMedia == gMntCache.array[ i ].ioMedia )
            return &gMntCache.array[ i ];
        
    }// end for
    
    return NULL;
}

//--------------------------------------------------------------------

DldMntCacheEntry*
DldRetrieveFreeCacheEntryWOLock()
{
    int i;
    
    assert( gMntCache.array );
    assert( gMntCache.arrayValidEntries <= gMntCache.arrayNumOfEntries );
    
    if( gMntCache.arrayValidEntries == gMntCache.arrayNumOfEntries )
        return NULL;
    
    for( i = 0x0; i < gMntCache.arrayNumOfEntries; ++i ){
        
        assert( (NULL == gMntCache.array[ i ].ioMedia) == (NULL == gMntCache.array[ i ].mnt) );
        
        if( !gMntCache.array[ i ].mnt )
            return &gMntCache.array[ i ];
        
    }// end for
    
    //
    // we should not be here as this means that gMntCache.arrayValidEntries != gMntCache.arrayNumOfEntries
    // and there is no free entries
    //
    assert( !"Check the correctness of the cache management as arrayNumOfEntries and arrayValidEntries have incorrect values" );
    
    return NULL;
}

//--------------------------------------------------------------------

void
DldExpandCacheWOLock(
    __in unsigned int increment
)
{
    assert( preemption_enabled() );
    assert( 0x0 != increment );
    
    DldMntCacheEntry* newArray;
    
    //
    // allocate a new array with an increased capacity,
    // TO DO:
    // actually we should not block waiting for physical page freeing to avoid
    // a deadlock if we are called on a hard disk path or somehow related path,
    // but with the high probability an entry for a hard disk was created
    // while there was a plenty of the wired memory just at the driver start
    //
    newArray = (DldMntCacheEntry*)IOMalloc( sizeof( gMntCache.array[0] ) * ( gMntCache.arrayNumOfEntries+increment ) );
    assert( newArray );
    if( !newArray )
        return;
    
    bzero( newArray, sizeof( gMntCache.array[0] ) * ( gMntCache.arrayNumOfEntries+increment ) );
    memcpy( newArray, gMntCache.array, sizeof( gMntCache.array[0] ) * gMntCache.arrayNumOfEntries );
    
    //
    // release the old array
    //
    IOFree( gMntCache.array, sizeof( gMntCache.array[0] ) * gMntCache.arrayNumOfEntries );
    
    //
    // save the new array
    //
    gMntCache.array = newArray;
    
    //
    // adjust to the new capacity
    //
    gMntCache.arrayNumOfEntries = gMntCache.arrayNumOfEntries+increment;
}

//--------------------------------------------------------------------

void
DldRemoveMediaEntriesFromMountCacheWOLock(
    __in IORegistryEntry*  ioMedia
    )
{
    assert( preemption_enabled() );
    assert( gMntCache.RWlock );
    assert( gMntCache.array );
    assert( ioMedia );
    
    DldMntCacheEntry* entry;
    
    //
    // remove the stalled entries for the previous mounts on the volume
    //
    while( NULL != (entry = DldRetrieveCacheEntryByMediaWOLock( ioMedia )) ){
        
        bzero( entry, sizeof( *entry ) );
        
        assert( gMntCache.arrayValidEntries > 0x0 );
        --gMntCache.arrayValidEntries;
        
    }//end while
    
}

//--------------------------------------------------------------------

void
DldRemoveMediaEntriesFromMountCache(
    __in IORegistryEntry*  ioMedia
    )
{
    assert( preemption_enabled() );
    assert( gMntCache.RWlock );
    assert( gMntCache.array );
    assert( ioMedia );
    
    bool inCache;
    
    //
    // avoid acquiring the lock for write to not
    // punish the disk clients acquiring it for read
    // when an object unrelated to the disk subsystem
    // is detached or removed
    //
    
    //
    // the following optimization with the first acquiring the
    // cache for read and checking for the object presence is used
    // instead of a check that the object is of the IOMedia class
    // or derived from it by using OSDynamicCast which requires
    // linking of the driver to the kmod containing the IOMedia's
    // meta class object
    //
    IORWLockRead( gMntCache.RWlock );
    {// start of the lock

        inCache = ( NULL != DldRetrieveCacheEntryByMediaWOLock( ioMedia ) );
        
    }// end of the lock
    IORWLockUnlock( gMntCache.RWlock );
    
    if( !inCache )
        return;
    
    IORWLockWrite( gMntCache.RWlock );
    {// start of the lock
        
        //
        // remove the stalled entries for the previous mounts on the volume
        //
        DldRemoveMediaEntriesFromMountCacheWOLock( ioMedia );
        assert( NULL == DldRetrieveCacheEntryByMediaWOLock( ioMedia ) );
        
    }// end of the lock
    IORWLockUnlock( gMntCache.RWlock );
    
}

//--------------------------------------------------------------------

void
DldAddMountInCache(
    __in IORegistryEntry*  ioMedia,
    __in mount_t  mnt
    )
{
    assert( preemption_enabled() );
    assert( gMntCache.RWlock );
    assert( gMntCache.array );
    assert( ioMedia );
    assert( mnt );
    
    IORWLockWrite( gMntCache.RWlock );
    {// start of the lock
        
        DldMntCacheEntry* entry = NULL;
        
        entry = DldRetrieveCacheEntryByMountWOLock( mnt );
        if( entry ){
         
            assert( gMntCache.arrayValidEntries > 0x0 );
            
            //
            // there are two possibilities 
            // - a concurrent thread managed to add the entry
            // - a stalled entry has been found ( i.e. a FS was unmounted and 
            //   a memory for a mount structure was reused )
            //
            if( ioMedia == entry->ioMedia ){
                
                //
                // the first case
                //
                goto __exit;
                
            } else {
                
                //
                // the second case, so reuse the found entry as this is a stalled entry
                //
                bzero( entry, sizeof( *entry ) );
                --gMntCache.arrayValidEntries; // account for a new free entry
            }

        }// end if( entry )
        
        //
        // remove the stalled entries for the previous mounts on the same volume
        //
        DldRemoveMediaEntriesFromMountCacheWOLock( ioMedia );
        
        if( !entry ){
            
            entry = DldRetrieveFreeCacheEntryWOLock();
            if( !entry ){
                
                DldExpandCacheWOLock( DLD_MNT_CACHE_INCREMENT );
                
                assert( gMntCache.arrayValidEntries < gMntCache.arrayNumOfEntries );
                
                entry = DldRetrieveFreeCacheEntryWOLock();
                
            }// end if
        }
        
        assert( entry );
        assert( gMntCache.arrayValidEntries < gMntCache.arrayNumOfEntries );
        
        if( !entry ){
         
            DBG_PRINT_ERROR(("an entry allocation failed!\n"));
            goto __exit;
        }
        
        entry->ioMedia = ioMedia;
        entry->mnt = mnt;
        memcpy( entry->f_mntfromname,
                vfs_statfs( mnt )->f_mntfromname,
                strlen( vfs_statfs( mnt )->f_mntfromname )+sizeof('\0') );
        
        ++gMntCache.arrayValidEntries;
        
    __exit:
        assert( gMntCache.arrayValidEntries <= gMntCache.arrayNumOfEntries );
    }// end of the lock
    IORWLockUnlock( gMntCache.RWlock );
    
}

//--------------------------------------------------------------------

IORegistryEntry*
DldRetrieveCachedMediaByMount(
    __in mount_t  mnt,
    __in bool reference
    )
{
    IORegistryEntry*   ioMedia = NULL;
    
    assert( preemption_enabled() );
    assert( gMntCache.RWlock );
    assert( mnt );
    
    IORWLockRead( gMntCache.RWlock );
    {// start of the lock
        
        DldMntCacheEntry*  entry;
        
        entry = DldRetrieveCacheEntryByMountWOLock( mnt );
        if( entry ){
            
            assert( entry->mnt == mnt );
            assert( entry->ioMedia );
            
            ioMedia = entry->ioMedia;
            
            //
            // it is safe to reference the object as
            // all entries for the media are removed
            // during its detaching from the provider,
            // so if the object pointer is found under
            // the lock protection it is valid
            //
            if( reference )
                ioMedia->retain();
            
        }// end if
        
    }// end of the lock
    IORWLockUnlock( gMntCache.RWlock );
    
    return ioMedia;
}

//--------------------------------------------------------------------

IORegistryEntry *
DldGetBsdMediaObjectFromName(
    __in const char * ioBSDNamePtr
    )
/*
 Uses the BSD name to get a reference to the corresponding IORegistryEntry object,
 the function is a derivative for the GetCDMediaObjectFromName ( cddafs kext ) function
 */
{
	
	OSIterator *		iteratorPtr			= NULL;
	IORegistryEntry *	registryEntryPtr	= NULL;
	OSDictionary *		matchingDictPtr		= NULL;
	
    assert( preemption_enabled() );
	assert( ioBSDNamePtr != NULL );
	
    //
	// Check to see if we need to strip off any leading stuff
    //
	if( 0x0 == strncmp ( ioBSDNamePtr, "/dev/r", 6 ) ){
        
        //
		// Strip off the /dev/r from /dev/rdiskX
        //
		ioBSDNamePtr = &ioBSDNamePtr[6];	
        
	} else if( 0x0 == strncmp ( ioBSDNamePtr, "/dev/", 5 ) ){
        
        //
		// Strip off the /dev/ from /dev/diskX
        //
		ioBSDNamePtr = &ioBSDNamePtr[5];	
        
	}
	
    
	if( strncmp ( ioBSDNamePtr, "disk", 4 ) ){
		
        //
		// Not in correct format, return NULL,
        // this is not an error as the /dev directory
        // is full of non-disk devices such as tty or random
        // number generator
        //
		//DBG_PRINT_ERROR ( ( "DldGetBsdObjectFromName: not in correct format, ioBSDNamePtr = %s.\n", ioBSDNamePtr ) );
		
		goto __exit;
		
	}
	
    //
	// Get a dictionary which describes the bsd device
    //
	matchingDictPtr = IOBSDNameMatching( ioBSDNamePtr );
	
    //
	// Get a referenced iterator of registry entries
    //
	iteratorPtr = IOService::getMatchingServices( matchingDictPtr );
	if( iteratorPtr == NULL ){
		
		DBG_PRINT_ERROR( ( "DldGetBsdObjectFromName: iteratorPtr is NULL.\n" ) );
		goto __exit;
		
	}
    
	//
	// Get the object out of the iterator ( NB: we're guaranteed only one object in the iterator
	// because there is a 1:1 correspondence between BSD Names for devices and IOKit objects )
    //
	registryEntryPtr = ( IORegistryEntry * )iteratorPtr->getNextObject();
	if( registryEntryPtr == NULL ){
        
		DBG_PRINT_ERROR( ( "GetCDMediaObjectFromName: registryEntryPtr is NULL.\n" ) );
		goto __exit;
	}
	
    //
	// Bump the refcount on the object ( actually of the IOMedia or IOCDMedia type )
    // so that when we release the iterator we still have a refcount on it
    //
    registryEntryPtr->retain();
	
__exit:
    
    //
	// Release the dictionary
    //
    if( NULL != matchingDictPtr )
        matchingDictPtr->release();
	
    //
	// Release the iterator
    //
    if( NULL != iteratorPtr )
        iteratorPtr->release();	
	
	return  registryEntryPtr;
}

//--------------------------------------------------------------------

IORegistryEntry*
DldGetBsdMediaObjectFromMount(
    __in mount_t mountPtr
    )
/*
 Uses the BSD mount point to get a reference to the corresponding
 media IORegistryEntry object
 */
{
	IORegistryEntry *       ioMediaPtr;
	char *					ioBSDNamePtr;
	
    assert( preemption_enabled() );
	assert( mountPtr );
	
    ioMediaPtr = DldRetrieveCachedMediaByMount( mountPtr, true );
    if( ioMediaPtr )
        return ioMediaPtr;
    
    //
    // the function can be called for a special file systems such as
    // devfs which don't have an underlying IOKit media, but this calls
    // are rare and I decide to not punish the real filesystems by checks
    // for these special ones, the underlying media won't be found
    // for special filesystems
    //
	ioBSDNamePtr = vfs_statfs( mountPtr )->f_mntfromname;
	assert( ioBSDNamePtr );
	
	ioMediaPtr = DldGetBsdMediaObjectFromName( ioBSDNamePtr );
#if defined( DBG )
    if( 0x0 != strncmp ( vfs_statfs( mountPtr )->f_fstypename, "devfs", 5 ) &&
        0x0 != strncmp ( vfs_statfs( mountPtr )->f_fstypename, "autofs", 6 ) ){
        
        //
        // for a real file system the underlying media should exist,
        // sometimes the assert fails on disk reinitialization
        //
        //assert ( ioMediaPtr );
    }
#endif//DBG
    
    if( ioMediaPtr ){
        
        DldAddMountInCache( ioMediaPtr, mountPtr );
        assert( ioMediaPtr == DldRetrieveCachedMediaByMount( mountPtr, false ) );
    }
	
	return ioMediaPtr;
	
}

//--------------------------------------------------------------------

/*
 the following is an example of a direct disk opening
 
 (gdb) bt
 #0  DldGetBsdMediaObjectFromName (ioBSDNamePtr=0x56be738 "devfs") at /work/DeviceLockProject/DeviceLockIOKitDriver/IOBSDSystem.cpp:439
 #1  0x45fccb85 in DldGetBsdMediaObjectFromMount (mountPtr=0x56be290) at /work/DeviceLockProject/DeviceLockIOKitDriver/IOBSDSystem.cpp:572
 #2  0x45fcccc1 in DldGetReferencedDldIOServiceFoMediaByVnode (vnode=0x89d6a04) at /work/DeviceLockProject/DeviceLockIOKitDriver/IOBSDSystem.cpp:610
 #3  0x45f7bf59 in DldIOKitKAuthVnodeGate::DldVnodeAuthorizeCallback (credential=0x5622a68, idata=0xc4fc090, action=2, arg0=98549764, arg1=144534020, arg2=0, arg3=1169570828) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKAuthVnodeGate.cpp:124
 #4  0x00463e50 in kauth_authorize_action (scope=0x56b9904, credential=0x5622a68, action=2, arg0=98549764, arg1=144534020, arg2=0, arg3=1169570828) at /SourceCache/xnu/xnu-1456.1.25/bsd/kern/kern_authorization.c:420
 #5  0x002d7e00 in vnode_authorize (vp=0x89d6a04, dvp=0x0, action=2, ctx=0x5dfc004) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_subr.c:4854
 #6  0x002ee863 in vn_open_auth (ndp=0x45b63d38, fmodep=0x45b63cec, vap=0x45b63e8c) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_vnops.c:407
 #7  0x002e7ffa in open1 (ctx=0x5dfc004, ndp=0x45b63d38, uflags=0, vap=0x45b63e8c, retval=0x5dfbf44) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_syscalls.c:2307
 #8  0x002e84a8 in open_nocancel (p=dwarf2_read_address: Corrupted DWARF expression.
 ) at /SourceCache/xnu/xnu-1456.1.25/bsd/vfs/vfs_syscalls.c:2511
 #9  0x004ed85f in unix_syscall64 (state=0x5c1dd84) at /SourceCache/xnu/xnu-1456.1.25/bsd/dev/i386/systemcalls.c:433
 
 
 (gdb) p *(vnode_t)0x89d6a04
 $29 = {
 v_lock = {
 opaque = {0, 0, 0}
 }, 
 v_freelist = {
 tqe_next = 0x7a397b4, 
 tqe_prev = 0x7b6225c
 }, 
 v_mntvnodes = {
 tqe_next = 0x6ebfcb8, 
 tqe_prev = 0x88d05dc
 }, 
 v_nclinks = {
 lh_first = 0x0
 }, 
 v_ncchildren = {
 lh_first = 0x0
 }, 
 v_defer_reclaimlist = 0x0, 
 v_listflag = 0, 
 v_flag = 526336, 
 v_lflag = 16384, 
 v_iterblkflags = 0 '\0', 
 v_references = 3 '\003', 
 v_kusecount = 0, 
 v_usecount = 0, 
 v_iocount = 1, 
 v_owner = 0x0, 
 v_type = 3, 
 v_tag = 18, 
 v_id = 232076100, 
 v_un = {
 vu_mountedhere = 0x59005e8, 
 vu_socket = 0x59005e8, 
 vu_specinfo = 0x59005e8, 
 vu_fifoinfo = 0x59005e8, 
 vu_ubcinfo = 0x59005e8
 }, 
 v_cleanblkhd = {
 lh_first = 0x0
 }, 
 v_dirtyblkhd = {
 lh_first = 0x0
 }, 
 v_knotes = {
 slh_first = 0x0
 }, 
 v_cred = 0x5622a68, 
 v_authorized_actions = 2, 
 v_cred_timestamp = 0, 
 v_nc_generation = 3, 
 v_numoutput = 0, 
 v_writecount = 0, 
 v_name = 0x68a1434 "disk1", 
 v_parent = 0x5779564, 
 v_lockf = 0x0, 
 v_unsafefs = 0x0, 
 v_op = 0x56b9c04, 
 v_mount = 0x56be290, 
 v_data = 0xbefaa84, 
 v_label = 0x0
 }
 
 
 (gdb) p *((vnode_t)0x89d6a04)->v_mount
 $30 = {
 mnt_list = {
 tqe_next = 0x56bd948, 
 tqe_prev = 0x56bebd8
 }, 
 mnt_count = 314, 
 mnt_mlock = {
 opaque = {0, 0, 0}
 }, 
 mnt_op = 0x824a80, 
 mnt_vtable = 0x822ab4, 
 mnt_vnodecovered = 0x5779bc0, 
 mnt_vnodelist = {
 tqh_first = 0x8513250, 
 tqh_last = 0x5779578
 }, 
 mnt_workerqueue = {
 tqh_first = 0x0, 
 tqh_last = 0x56be2bc
 }, 
 mnt_newvnodes = {
 tqh_first = 0x0, 
 tqh_last = 0x56be2c4
 }, 
 mnt_flag = 68161536, 
 mnt_kern_flag = 1048576, 
 mnt_lflag = 0, 
 mnt_maxsymlinklen = 0, 
 mnt_vfsstat = {
 f_bsize = 512, 
 f_iosize = 512, 
 f_blocks = 221, 
 f_bfree = 0, 
 f_bavail = 0, 
 f_bused = 221, 
 f_files = 630, 
 f_ffree = 0, 
 f_fsid = {
 val = {91839476, 19}
 }, 
 f_owner = 0, 
 f_flags = 0, 
 f_fstypename = "devfs\000\000\000\000\000\000\000\000\000\000", 
 f_mntonname = "/dev", '\0' <repeats 1019 times>, 
 f_mntfromname = "devfs", '\0' <repeats 1018 times>, 
 f_fssubtype = 0, 
 f_reserved = {0x0, 0x0}
 }, 
 mnt_data = 0x5795bf4, 
 mnt_maxreadcnt = 131072, 
 mnt_maxwritecnt = 131072, 
 mnt_segreadcnt = 32, 
 mnt_segwritecnt = 32, 
 mnt_maxsegreadsize = 0, 
 mnt_maxsegwritesize = 0, 
 mnt_alignmentmask = 0, 
 mnt_devblocksize = 0, 
 mnt_ioqueue_depth = 0, 
 mnt_ioscale = 0, 
 mnt_ioflags = 0, 
 mnt_pending_write_size = 0, 
 mnt_pending_read_size = 18446744073520506880, 
 mnt_rwlock = {
 opaque = {553648128, 0, 0}
 }, 
 mnt_renamelock = {
 opaque = {0, 0, 0}
 }, 
 mnt_devvp = 0x0, 
 mnt_devbsdunit = 0, 
 mnt_throttle_info = 0x0, 
 mnt_crossref = 0, 
 mnt_iterref = 0, 
 mnt_fsowner = 0, 
 mnt_fsgroup = 0, 
 mnt_mntlabel = 0x56e6a80, 
 mnt_fslabel = 0x0, 
 mnt_realrootvp = 0x5779564, 
 mnt_realrootvp_vid = 733542147, 
 mnt_generation = 13, 
 mnt_authcache_ttl = 2, 
 mnt_dependent_pid = 0, 
 mnt_dependent_process = 0x0
 }
 
 */

IOService*
DldGetReferencedIOServiceFoMediaByVnode(
    __in vnode_t vnode
    )
{
    IORegistryEntry*  ioMedia;
    IOService*        mediaService;
    
    assert( vnode );
    assert( preemption_enabled() );
    assert( vnode_mount( vnode ) );
    
    if( !vnode_mount( vnode ) )
        return NULL;
    
    //
    // check for a /dev/ directory access, the specific is that the mount structire
    // is the same for all devices and looks like the following excerpt, i.e.
    // it is mounted on a fake device "devfs"
    //
    // f_fstypename = "devfs\000\000\000\000\000\000\000\000\000\000", 
    // f_mntonname = "/dev" <repeats 1019 times>, 
    // f_mntfromname = "devfs", '\0' <repeats 1018 times>, 
    //
    // the cache can't be used here as there is no mount structure to cache
    //
    if( gDevDirMount == vnode_mount( vnode ) || 
       ( NULL == gDevDirMount &&
        0x0 == strncmp ( vfs_statfs( vnode_mount( vnode ) )->f_fstypename, "devfs", 5 ) &&
        0x0 == strncmp ( vfs_statfs( vnode_mount( vnode ) )->f_mntonname, "/dev", 4 ) &&
        0x0 == strncmp ( vfs_statfs( vnode_mount( vnode ) )->f_mntfromname, "devfs", 5 ) ) ){
        
        //
        // remember the value as it won't change
        //
        if( NULL == gDevDirMount )
            gDevDirMount = vnode_mount( vnode );
        
        //
        // get the device name from the vnode, as the full vnode path is the actual device name
        //
        char* name;
        int   nameLength = MAXPATHLEN;
        int   error;
        
        //
        // TO DO - change the function's input parameter to DldIOVnode
        // and get rid of name queryieng here
        //
        name = (char*)IOMalloc( MAXPATHLEN );
        assert( name );
        if( !name )
            return NULL;
        
        error = vn_getpath( vnode, name, &nameLength );
        if( 0x0 != error  ){
            
            const char* short_name = vnode_getname( vnode );
            DBG_PRINT_ERROR( ("vn_getpath failed with the 0x%X error for %s\n", error, (short_name ? short_name : "U")) );
            if( short_name )
                vnode_putname( short_name );
            
            IOFree( name, MAXPATHLEN );
            return NULL;
        }
        
        //
        // take a long path as there is no way to cache anything here and this
        // path won't be taken too often as the direct device open is not a very
        // frequent event ( usually an open of /dev/urandom and /dev/dtracehelper 
        // on a process start )
        //
        ioMedia = DldGetBsdMediaObjectFromName( name );
        IOFree( name, MAXPATHLEN );
        
    } else {
        
        //
        // this is a general vnode for a file on a volume
        //
        ioMedia = DldGetBsdMediaObjectFromMount( vnode_mount( vnode ) );
        
    }
    
    if( !ioMedia ){
        
        return NULL;
    }
    
    mediaService = OSDynamicCast( IOService, ioMedia );
    assert( mediaService );
    if( !mediaService ){
        
        ioMedia->release();
        ioMedia = NULL;
    }
    
    return mediaService;
}

//--------------------------------------------------------------------

DldIOService*
DldGetReferencedDldIOServiceFoMediaByVnode(
    __in vnode_t vnode
    )
{
    DldIOService*     dldIOService = NULL;
    IOService*        mediaService;
    
    mediaService = DldGetReferencedIOServiceFoMediaByVnode( vnode );
    if( mediaService ){

        dldIOService = DldIOService::RetrieveDldIOServiceForIOService( mediaService );
        assert( dldIOService );
        
        mediaService->release();
    }
    
    /*
    if( ! dldIOService ){
        
        const char* short_name = vnode_getname( vnode );
        //DBG_PRINT_ERROR( ("DldGetReferencedDldIOServiceFoMediaByVnode failed for %s\n", (short_name ? short_name : "U")) );
        if( short_name )
            vnode_putname( short_name );
    }
     */
    
    return dldIOService;
    
}

//--------------------------------------------------------------------

//
// the returned object is referenced
//
OSDictionary* DldCreateIOServiceMatchingDict( __in const char* className )
{
    OSDictionary*       dict;
    const OSSymbol*     classNameSym;
    
    classNameSym = OSSymbol::withCStringNoCopy( className );
    assert( classNameSym );
    if( !classNameSym )
        return NULL;
    
    dict = IOService::serviceMatching( classNameSym );
    assert( dict );
    
    classNameSym->release();
    
    return dict;
}

//--------------------------------------------------------------------

bool
DldAddToPropertyMatchingDict(
    __inout OSDictionary*  dict,
    __in const char *   key,
    __in const char *   name
    )
{
    bool  added = false;
    /*
    const OSSymbol*    strName = NULL;
    const OSSymbol*    strKey  = NULL;
    
    strName = OSSymbol::withCStringNoCopy( name );
    assert( strName );
    if( !strName )
        goto __exit;
    
    strKey = OSSymbol::withCStringNoCopy( key );
    assert( strKey );
    if( !strKey )
        goto __exit;
    
    added = dict->setObject( strKey, (OSObject*)strName );
    assert( added );
     */
    
    //
    // avoid using OSSymbol as the symbol's buffer might be
    // changed if the related object which buffer was used
    // to create a symbol object is being removed, this
    // is a bug which was encountered on Lion, using 
    // OSSymbol::withCString instead of OSSymbol::withCStringNoCopy
    // won't help for existing symbols and this is exactly the
    // case that crashed the system
    //
    const OSString*    strName = NULL;
    const OSString*    strKey  = NULL;
    
    strName = OSString::withCStringNoCopy( name );
    assert( strName );
    if( !strName )
        goto __exit;
    
    strKey = OSString::withCStringNoCopy( key );
    assert( strKey );
    if( !strKey )
        goto __exit;
    
    added = dict->setObject( strKey, (OSObject*)strName );
    assert( added );
    
__exit:
    
    if( strName )
        strName->release();
    
    if( strKey )
        strKey->release();
    
    return added;
}

//--------------------------------------------------------------------

//
// returns a DldIOService for IOSerialBSDClient related to vnode,
// a returned object is referenced, this function is unable to resolve
// the reference from the hard link
//
DldIOService*
DldGetReferencedDldIOServiceForSerialDeviceVnode(
    __in DldIOVnode* dldVnode
    )
{
    OSDictionary*       matchDict = NULL;
    OSDictionary*       propertyDict = NULL;
    OSString*           name = NULL;
    OSIterator*         iteratorPtr = NULL;
    IORegistryEntry*    registryEntryPtr = NULL;
    DldIOService*       dldIOService = NULL;
    
    assert( preemption_enabled() );
    assert( dldVnode );
    
    matchDict = DldCreateIOServiceMatchingDict( kIOSerialBSDServiceValue );
    assert( matchDict );
    if( !matchDict )
        goto __exit;
    
    propertyDict = OSDictionary::withCapacity(1);
    assert( propertyDict );
    if( !propertyDict )
        goto __exit;
    
    name = dldVnode->getNameRef();
    assert( name );
    if( !name )
        goto __exit;
    
    if( !matchDict->setObject( gIOPropertyMatchKey, propertyDict ) ){
        
        assert( !"matchDict->setObject( gIOPropertyMatchKey, propertyDict ) failed" );
        goto __exit;
    }
    
    /*
     | | |   |   | |     +-o IOSerialBSDClient  <class IORegistryEntry:DldIORegistryEntry:DldIOService, id 0x10000055a, retain 5>
     | | |   |   | |         {
     | | |   |   | |           "IOClass" = "IOSerialBSDClient"
     | | |   |   | |           "CFBundleIdentifier" = "com.apple.iokit.IOSerialFamily"
     | | |   |   | |           "IOProviderClass" = "IOSerialStreamSync"
     | | |   |   | |           "IOTTYBaseName" = "HUAWEIMobile-"
     | | |   |   | |           "Dld: DldIOService" = 134254592
     | | |   |   | |           "IOSerialBSDClientType" = "IOModemSerialStream"
     | | |   |   | |           "Dld: Class Name" = "IOSerialBSDClient"
     | | |   |   | |           "IOProbeScore" = 1000
     | | |   |   | |           "IOCalloutDevice" = "/dev/cu.HUAWEIMobile-Modem"
     | | |   |   | |           "IODialinDevice" = "/dev/tty.HUAWEIMobile-Modem"
     | | |   |   | |           "IOMatchCategory" = "IODefaultMatchCategory"
     | | |   |   | |           "IOTTYDevice" = "HUAWEIMobile-Modem"
     | | |   |   | |           "IOResourceMatch" = "IOBSD"
     | | |   |   | |           "IOTTYSuffix" = "Modem"
     | | |   |   | |           "Dld: PnPState" = "kDldPnPStateStarted"
     | | |   |   | |         }
     */
    if( !DldAddToPropertyMatchingDict( propertyDict, kIOCalloutDeviceKey, name->getCStringNoCopy() ) )
        goto __exit;
    
    //
	// Get a referenced iterator of registry entries
    //
	iteratorPtr = IOService::getMatchingServices( matchDict );
	if( !iteratorPtr ){
        
        //
        // try with another property
        //
        propertyDict->flushCollection();
        if( !DldAddToPropertyMatchingDict( propertyDict, kIODialinDeviceKey, name->getCStringNoCopy() ) )
            goto __exit;
        
        iteratorPtr = IOService::getMatchingServices( matchDict );
        if( !iteratorPtr )
            goto __exit;
    }
    
	//
	// Get the object out of the iterator ( NB: we're guaranteed only one object in the iterator
	// because there is a 1:1 correspondence between BSD Names for devices and IOKit objects ),
    // the returned object is not referenced as it is retained by the iterator's reference
    //
	registryEntryPtr = (IORegistryEntry*)iteratorPtr->getNextObject();
	if( !registryEntryPtr )
        goto __exit;
	
    assert( OSDynamicCast( IOService, registryEntryPtr ) );
    
    if( NULL == OSDynamicCast( IOService, registryEntryPtr ) )
        goto __exit;
    
    dldIOService = DldIOService::RetrieveDldIOServiceForIOService( OSDynamicCast( IOService, registryEntryPtr ) );
    assert( dldIOService );
    
__exit:
    
    //
	// Release the iterator, 
    // registryEntryPtr is not valid if the iterator has been released!
    //
    if( iteratorPtr )
        iteratorPtr->release();
    DLD_DBG_MAKE_POINTER_INVALID( iteratorPtr );
    
    if( matchDict )
        matchDict->release();
    DLD_DBG_MAKE_POINTER_INVALID( matchDict );
    
    if( propertyDict )
        propertyDict->release();
    DLD_DBG_MAKE_POINTER_INVALID( propertyDict );
    
    //
    // the name is released as the last object because its buffer is used for OSSymbol objects in propertyDict
    //
    if( name )
        name->release();
    DLD_DBG_MAKE_POINTER_INVALID( name );
    
    return dldIOService;
}

//--------------------------------------------------------------------

IOService*
DldGetReferencedBootIOMedia()
{
    vnode_t     rootVnode;
    IOService*  bootMedia;
    
    assert( preemption_enabled() );
    
    rootVnode = vfs_rootvnode();
    assert( rootVnode );
    if( !rootVnode )
        return NULL;
    
    bootMedia = DldGetReferencedIOServiceFoMediaByVnode( rootVnode );
    assert( bootMedia );
    
    vnode_put( rootVnode );
    
    return bootMedia;
}

//--------------------------------------------------------------------

Boolean
DldIsBootMedia( __in IOService*  service )
{
    assert( service && gBootMedia );
    return ( gBootMedia == service );
}

//--------------------------------------------------------------------



