/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include <sys/proc.h> // for proc_name
#include "DldIOVnode.h"
#include "IOBSDSystem.h"
#include "DldKernAuthorization.h"
#include "DldKAuthVnodeGate.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIOVnode, OSObject )

#if defined( DBG )
SInt32     DldIOVnode::DldIOVnodesCount = 0x0;
#endif//DBG

OSString*        DldIOVnode::EmptyName = NULL;
const OSSymbol*  DldIOVnode::UnknownProcessName = NULL;

//--------------------------------------------------------------------

DldIOVnode*
DldIOVnode::withBSDVnode( __in vnode_t vnode )
/*
 the function must not assume that it won't be called 
 multiple times for the same vnode, and each time it must
 return a new IOVnode, this is a caller's responsibility
 to preserve only one and destroy all others
 
 an example of a stack for this function calling
 #3  0x46569dc4 in DldIOVnode::withBSDVnode (vnode=0x8357314) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOVnode.cpp:94
 #4  0x4656b220 in DldVnodeHashTable::CreateAndAddIOVnodByBSDVnode (this=0x8375800, vnode=0x8357314) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldVnodeHashTable.cpp:304
 #5  0x4652602d in DldIOKitKAuthVnodeGate::DldVnodeAuthorizeCallback (credential=0x5656ea4, idata=0x6acc520, action=4, arg0=115219732, arg1=137720596, arg2=0, arg3=834583448) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKAuthVnodeGate.cpp:120
 #6  0x00508a71 in kauth_authorize_action (scope=0x56a2804, credential=0x5656ea4, action=4, arg0=115219732, arg1=137720596, arg2=0, arg3=834583448) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_authorization.c:420
 #7  0x003375e7 in vnode_authorize (vp=0x8357314, dvp=0x0, action=4, ctx=0x6de1d14) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:4863
 #8  0x0034e092 in vn_open_auth (ndp=0x31bebcf0, fmodep=0x31bebc74, vap=0x31bebe44) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_vnops.c:407
 #9  0x0033f4a3 in open1 (ctx=0x6de1d14, ndp=0x31bebcf0, uflags=1537, vap=0x31bebe44, retval=0x6de1c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:2717
 #10 0x0033fd31 in open_nocancel (p=0x63a97e0, uap=0x6c669a8, retval=0x6de1c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:2921
 #11 0x0033fbf8 in open (p=0x63a97e0, uap=0x6c669a8, retval=0x6de1c54) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:2903
 #12 0x005b19b5 in unix_syscall64 (state=0x6c669a4) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365
 */
{
    DldIOVnode* newIOVnode;
    
    newIOVnode = new DldIOVnode();
    assert( newIOVnode );
    if( !newIOVnode )
        return NULL;
    
    if( !newIOVnode->init() ){
        
        assert( !"newIOVnode->init() failed" );
        
        newIOVnode->release();
        return NULL;
        
    }
    
    //
    // set the data for a new object
    //
    
    newIOVnode->vnode   = vnode;
    newIOVnode->v_type  = vnode_vtype( vnode );
    newIOVnode->vnodeID = (UInt64)newIOVnode;
    
    //
    // check for a direct disk open, there are two cases - the first is when
    // the vnode's type is VBLK or VCHR - this is a case when /dev/(r)diskXX is opened,
    // the second case is not a direct disk open - when a directory where a disk is mounted is opened -
    // i.e. /Volume/SanDisk ( i.e. w/o the last / ) - in that case the vnode->v_name is NULL,
    // but it is not acessibe through KPI, so we make use of a comparision for
    // a directory name where a disk is mounted and a full vnode's path, if they
    // are equal then this is a FS root open, for the second case the type for
    // a vnode is VDIR and requests are processed as requests to a directory ( FS root )
    //
    assert( vnode_mount( vnode ) );
    assert( VNON != newIOVnode->v_type && NULL != newIOVnode->nameStr );
    
    if( VBLK == newIOVnode->v_type || VCHR == newIOVnode->v_type ){
        
        //
        // a direct open through /dev/diskXXX
        //
        newIOVnode->flags.directDiskOpen = 0x1;
        
    }
    
    if( vnode_issystem( vnode ) )
        newIOVnode->flags.system = 0x1;
        
    if( vnode_islnk( vnode ) )
        newIOVnode->flags.link = 0x1;
        
#ifndef DLD_MACOSX_10_5// vnode_isswap() declared but not exported for 10.5
    if( vnode_isswap( vnode ) )
        newIOVnode->flags.swap = 0x1;
#endif//DLD_MACOSX_10_5
    
    if( vnode_isdir( vnode ) )
        newIOVnode->flags.dir = 0x1;
    
#if defined( DBG )
    OSIncrementAtomic( &DldIOVnode::DldIOVnodesCount );
#endif//DBG
    
    return newIOVnode;
}

//--------------------------------------------------------------------

void
DldIOVnode::logVnodeOperation( __in DldIOVnode::VnodeOperation op )
{
#if DBG
    
    assert( kVnodeOp_Unknown != op );
    
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE(this->vnodeOperationsLog); ++i ){
        
        if( kVnodeOp_Unknown != this->vnodeOperationsLog[ i ] )
            continue;
        
        if( OSCompareAndSwap( kVnodeOp_Unknown, op, &this->vnodeOperationsLog[ i ] ) )
           break;
           
    }// end for

#endif//DBG

}

//--------------------------------------------------------------------

OSString*
DldIOVnode::getNameRef()
{
    OSString* nameRef;
    
    assert( preemption_enabled() );
    
    this->LockShared();
    {// start of the lock
        
        assert( this->nameStr );
        
        nameRef = this->nameStr;
        nameRef->retain();
        
    }// end of the lock
    this->UnLockShared();
    
    return nameRef;
}

//--------------------------------------------------------------------

bool
DldIOVnode::isNameVaid()
{
    return ( this->nameStr != DldIOVnode::EmptyName );
}

//--------------------------------------------------------------------

void
DldIOVnode::setName( __in OSString* name )
{
    OSString* oldName;
    
    assert( name );
    assert( preemption_enabled() );
    
    this->LockExclusive();
    {// start of the lock
        
        assert( this->nameStr );
        
        oldName = this->nameStr;
        this->nameStr = name;
        
    }// end of the lock
    this->UnLockExclusive();
    
    assert( oldName );
    oldName->release();
    
    return;
}

//--------------------------------------------------------------------

void
DldIOVnode::updateNameToValid()
{
    
    assert( preemption_enabled() );
    assert( this->nameStr );
    
    if( this->nameStr != DldIOVnode::EmptyName ){
        
        //
        // already has been updated, this provides an idempotent behaviour!
        //
        return;
        
    }// end if( this->nameStr != DldIOVnode::EmptyName )
    
    //
    // get a vnode's path, if called before the KAUTH callback
    // the vnode might not have a valid name and parent,
    // the name amust be reinitialized when called from the KAUTH
    // subsystem
    //
    errno_t     error;
    OSString*   newName;
    
    assert( this->vnode );
    
    int    nameLength = MAXPATHLEN;
    char*  nameBuffer = (char*)IOMalloc( MAXPATHLEN );
    assert( nameBuffer );
    if( ! nameBuffer )
        return;
    
    error = vn_getpath( this->vnode, nameBuffer, &nameLength );
    if( 0x0 != error  ){
        
        DBG_PRINT_ERROR( ("vn_getpath() failed with the 0x%X error\n", error ) );
        
        /*
         
         in some cases the system provides us with a vnode w/o parent, so FQN can not be assembled by vn_getpath
         
         #0  DldIOVnode::updateNameToValid (this=0xffffff80254a7500) at DldIOVnode.cpp:228
         #1  0xffffff7f81fcf0e5 in DldIOKitKAuthVnodeGate::DldVnodeAuthorizeCallback (credential=0xffffff8011d307f0, idata=0xffffff8024fe0c00, action=-2147483548, arg0=18446743524329052896, arg1=18446743524429184152, arg2=0, arg3=18446743527994768164) at DldKAuthVnodeGate.cpp:348
         #2  0xffffff800053fdb1 in kauth_authorize_action (scope=0xffffff8011f73008, credential=0xffffff8011d307f0, action=-2147483548, arg0=18446743524329052896, arg1=18446743524429184152, arg2=0, arg3=18446743527994768164) at /SourceCache/xnu/xnu-2050.22.13/bsd/kern/kern_authorization.c:422
         #3  0xffffff80002f44cb in vnode_authorize (vp=0xffffff801c56bc98, dvp=0x0, action=-2147483548, ctx=0xffffff80165edae0) at /SourceCache/xnu/xnu-2050.22.13/bsd/vfs/vfs_subr.c:5730
         #4  0xffffff80004e626c in hfs_real_user_access [inlined] () at :1031
         #5  0xffffff80004e626c in packcommonattr [inlined] () at :744
         #6  0xffffff80004e626c in hfs_packattrblk (abp=0xffffff80f0dd3da0, hfsmp=0xffffff801539c808, vp=<value temporarily unavailable, due to optimizations>, descp=0xffffff801c570df0, attrp=0xffffff801c570e08, datafork=0xffffff80f0dd3d38, rsrcfork=0xffffff8011d307f0, ctx=<value temporarily unavailable, due to optimizations>) at /SourceCache/xnu/xnu-2050.22.13/bsd/hfs/hfs_attrlist.c:435
         #7  0xffffff80004e582e in hfs_vnop_readdirattr (ap=0xffffff80f0dd3e18) at /SourceCache/xnu/xnu-2050.22.13/bsd/hfs/hfs_attrlist.c:308
         #8  0xffffff80003123d6 in VNOP_READDIRATTR (vp=<value temporarily unavailable, due to optimizations>, alist=<value temporarily unavailable, due to optimizations>, uio=<value temporarily unavailable, due to optimizations>, maxcount=<value temporarily unavailable, due to optimizations>, options=<value temporarily unavailable, due to optimizations>, newstate=<value temporarily unavailable, due to optimizations>, eofflag=0xffffff80f0dd3eb4, actualcount=0xffffff80f0dd3ebc, ctx=0xffffff80165edae0) at /SourceCache/xnu/xnu-2050.22.13/bsd/vfs/kpi_vfs.c:5416
         #9  0xffffff800030122a in getdirentriesattr (p=<value temporarily unavailable, due to optimizations>, uap=0xffffff8015d4ce54, retval=0xffffff80165ed9c8) at /SourceCache/xnu/xnu-2050.22.13/bsd/vfs/vfs_syscalls.c:7269
         #10 0xffffff80005e063a in unix_syscall64 (state=0xffffff8015d4ce50) at /SourceCache/xnu/xnu-2050.22.13/bsd/dev/i386/systemcalls.c:384
         
         (gdb) p *(vnode_t)0xffffff801c56bc98
         $2 = {
         v_lock = {
         opaque = {0, 18446744069414584320}
         }, 
         v_freelist = {
         tqe_next = 0x0, 
         tqe_prev = 0xdeadb
         }, 
         v_mntvnodes = {
         tqe_next = 0xffffff801c56bf80, 
         tqe_prev = 0xffffff801c56bbc0
         }, 
         v_nclinks = {
         lh_first = 0x0
         }, 
         v_ncchildren = {
         lh_first = 0x0
         }, 
         v_defer_reclaimlist = 0x0, 
         v_listflag = 0, 
         v_flag = 542720, 
         v_lflag = 49152, 
         v_iterblkflags = 0 '\0', 
         v_references = 0 '\0', 
         v_kusecount = 0, 
         v_usecount = 0, 
         v_iocount = 1, 
         v_owner = 0x0, 
         v_type = 2, 
         v_tag = 16, 
         v_id = 564813592, 
         v_un = {
         vu_mountedhere = 0x0, 
         vu_socket = 0x0, 
         vu_specinfo = 0x0, 
         vu_fifoinfo = 0x0, 
         vu_ubcinfo = 0x0
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
         v_cred = 0xffffff8011d307f0, 
         v_authorized_actions = -2147483548, 
         v_cred_timestamp = 0, 
         v_nc_generation = 2, 
         v_numoutput = 0, 
         v_writecount = 0, 
         v_name = 0xffffff80124713a8 "Library",  <-- not a FQN path
         v_parent = 0x0,                         <-- a NULL paren
         v_lockf = 0x0, 
         v_reserved1 = 0, 
         v_reserved2 = 0, 
         v_op = 0xffffff8011f70a08, 
         v_mount = 0xffffff8011f78000, 
         v_data = 0xffffff801c570d80, 
         v_label = 0x0, 
         v_resolve = 0x0
         }
         
         */
        
        const char* shortName = vnode_getname( this->vnode );
        if( shortName ){
            
            nameLength = min( strlen( shortName ), /*sizeof( stackBuffer )*/MAXPATHLEN - 1 );
            strncpy( nameBuffer, shortName, nameLength );
            nameBuffer[ nameLength ] = '\0';
            
            vnode_putname( shortName );
            shortName = NULL;
            
        } else {
            
            nameBuffer[ 0 ] = '\0';
            nameLength = 0x1;
        }
        
    }// end if( 0x0 != error  )
    
    newName = OSString::withCString( nameBuffer );
    assert( newName );
    if( !newName ){
        
        DBG_PRINT_ERROR(("OSString::withString( nameBuffer ) failed"));
        goto __exit;
        
    }// end if( !newName )
    
    //
    // check for the roor, as we now have a valid name
    //
    if( VDIR == this->v_type &&
        strlen( newName->getCStringNoCopy() ) == strlen( vfs_statfs( vnode_mount( this->vnode ) )->f_mntonname ) &&
        0x0 == strcasecmp ( newName->getCStringNoCopy(), vfs_statfs( vnode_mount( this->vnode ) )->f_mntonname  ) ){
        
        //
        // so this is a FS root opening through a mounting DIR
        //
        this->flags.fsRoot = 0x1;
    }
    
    DLD_COMM_LOG( COMMON, ("adding a vnode(%s)\n", newName->getCStringNoCopy()));
    
    this->setName( newName );
    
__exit:
    IOFree( nameBuffer, MAXPATHLEN );
}

//--------------------------------------------------------------------

void
DldIOVnode::updateProtectedFile()
{
    if( this->nameStr == DldIOVnode::EmptyName || NULL == gServiceProtection )
        return;
    
    if( this->serviceProtectionSettingsSequenceCounter == gServiceProtection->getSettingsSequenceCounter() ){
        
        //
        // the settings are up to date
        //
        return;
    }
    
    //
    // update the security sequence
    //
    this->serviceProtectionSettingsSequenceCounter = gServiceProtection->getSettingsSequenceCounter();
    
    //
    // get a new protected file object
    //
    DldProtectedFile* newProtFile = NULL;
    {
        //
        // get a reference to the string object with a path to avoid its changing
        // or removing when the raw C buffer is being used
        //
        OSString* filePathRef = this->getNameRef();
        assert( filePathRef );
        if( filePathRef ){
            
            newProtFile = gServiceProtection->getProtectedFileRef( filePathRef->getCStringNoCopy() );
            filePathRef->release();
            DLD_DBG_MAKE_POINTER_INVALID(filePathRef);
        }
    }
    
    //
    // set a new object
    //
    DldProtectedFile* oldProtFile;
    this->LockExclusive();
    { // start of the lock
        
        oldProtFile = this->protectedFile;
        this->protectedFile = newProtFile;
        
    } // end of the lock
    this->UnLockExclusive();
    DLD_DBG_MAKE_POINTER_INVALID( newProtFile ); // a refernce was consumed by this DldVnode
    
    if( oldProtFile )
        oldProtFile->release();
}

//--------------------------------------------------------------------

DldProtectedFile* DldIOVnode::getProtectedFileRef()
{
    DldProtectedFile* protectedFile;
    
    this->LockShared();
    { // start of the lock
        
        protectedFile = this->protectedFile;
        if( protectedFile )
            protectedFile->retain();
        
    } // end of the lock
    this->UnLockShared();
    
    return protectedFile;
}

//--------------------------------------------------------------------

bool
DldIOVnode::init()
{
#if DBG
    this->listEntry.cqe_next = this->listEntry.cqe_prev = NULL;
#endif//DBG
    
    if( !super::init() )
        return false;
    
    this->RecursiveLock = IORecursiveLockAlloc();
    assert( this->RecursiveLock );
    if( !this->RecursiveLock ){
        
        DBG_PRINT_ERROR(("IORecursiveLockAlloc() failed"));
        return false;
    }// end if( !this->RecursiveLock )
    
    this->spinLock = IOSimpleLockAlloc();
    assert( this->spinLock );
    if( !this->spinLock ){
        
        DBG_PRINT_ERROR(("IOLockAlloc() failed\n"));
        return false;
    }
    
    //
    // set a default invalid name, the correct name will be set later
    // when the first KAUTH callback is invoked - at that time
    // the BSD subsystem initialized the name and the parent vnode
    //
    assert( DldIOVnode::EmptyName );
    this->nameStr = DldIOVnode::EmptyName;
    this->nameStr->retain();
    
    assert( DldIOVnode::UnknownProcessName );
    
    this->auditData.process.processName = DldIOVnode::UnknownProcessName;
    this->auditData.process.processName->retain(); // bump a reference to retain the placeholder
    
    this->auditData.process.pid = (-1);
    this->auditData.userID.uid  = (-1);
    
#ifdef _DLD_MACOSX_CAWL
    InitializeListHead( &this->cawlWaitListHead );
#endif // _DLD_MACOSX_CAWL
    
    return true;
}

//--------------------------------------------------------------------

void
DldIOVnode::free()
{
    assert( (NULL == this->listEntry.cqe_next) && (NULL == this->listEntry.cqe_prev) );
    
    if( this->nameStr )
        this->nameStr->release();
    
    if( this->spinLock )
        IOSimpleLockFree( this->spinLock );
    
    if( this->RecursiveLock )
        IORecursiveLockFree( this->RecursiveLock );
    
    if( this->auditData.process.processName )
        this->auditData.process.processName->release();
    
#ifdef _DLD_MACOSX_CAWL
    if( this->coveredVnode ){
        
        //
        // remove the underlying vnode
        //
        if( kVnodeType_CoveringFSD == this->dldVnodeType ){
            
            //
            // allow the node to be reuses immediately by calling vnode_recycle
            // which triggers 
            // vnode_reclaim_internal()->vgone()->vclean()->VNOP_RECLAIM()
            // when iocounts and userio count drop to zero,
            // the reclaim hook removes vnode from the hash table so there
            // is no need to do this here
            //
            assert( !vnode_isinuse( this->coveredVnode, 0x1 ) );
            vnode_recycle( this->coveredVnode );
            vnode_rele( this->coveredVnode );
            
            this->coveredVnode = NULL;
        }
        
    }// end if( this->coveredVnode )
    
    assert( IsListEmpty( &this->cawlWaitListHead ) );
    
    if( this->sparseFile ){
        
        assert( DldSparseFilesHashTable::sSparseFilesHashTable );
        //
        // emulate the reclaim
        //
        if( 0x0 == this->flags.reclaimed )
            this->sparseFile->decrementUsersCount();
        //DldSparseFilesHashTable::sSparseFilesHashTable->RemoveEntryByObject( this->sparseFile ); done by decrementUsersCount()
        
        this->sparseFile->exchangeCawlRelatedVnode( NULL );
        this->sparseFile->release();
    }
#endif//#ifdef _DLD_MACOSX_CAWL
    
    if( this->protectedFile )
        this->protectedFile->release();
    
    super::free();
    
#if defined( DBG )
    assert( 0x0 != DldIOVnode::DldIOVnodesCount );
    OSDecrementAtomic( &DldIOVnode::DldIOVnodesCount );
#endif//DBG
    
}

//--------------------------------------------------------------------

const char*
DldIOVnode::vnodeTypeCStrNoCopy()
{

    switch( this->v_type){
        case VNON:
            return "VNON";
        case VREG:
            return "VREG";
        case VDIR:
            return "VDIR";
        case VBLK:
            return "VBLK";
        case VCHR:
            return "VCHR";
        case VLNK:
            return "VLNK";
        case VSOCK:
            return "VFIFO";
        case VFIFO:
            return "VFIFO";
        case VBAD:
            return "VBAD";
        case VSTR:
            return "VSTR";
        case VCPLX:
            return "VCPLX";
        default:
            return "U";
    }
}

//--------------------------------------------------------------------

void
DldIOVnode::LockShared()
{   assert( this->RecursiveLock );
    assert( preemption_enabled() );
    
    IORecursiveLockLock( this->RecursiveLock );
};


void
DldIOVnode::UnLockShared()
{   assert( this->RecursiveLock );
    assert( preemption_enabled() );
    
    IORecursiveLockUnlock( this->RecursiveLock );
};


void
DldIOVnode::LockExclusive()
{
    assert( this->RecursiveLock );
    assert( preemption_enabled() );
    
    IORecursiveLockLock( this->RecursiveLock );
    
#if defined(DBG)
    this->ExclusiveThread = current_thread();
    this->exclusiveCounter += 0x1;
#endif//DBG
    
};

//--------------------------------------------------------------------

void
DldIOVnode::UnLockExclusive()
{
    assert( this->RecursiveLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() == this->ExclusiveThread );
    this->exclusiveCounter -= 0x1;
    if( 0x0 == this->exclusiveCounter )
        this->ExclusiveThread = NULL;
#endif//DBG
    
    IORecursiveLockUnlock( this->RecursiveLock );
};

//--------------------------------------------------------------------

//
// called once at the driver initialization
//
bool DldIOVnode::InitVnodeSubsystem()
{
    assert( NULL == DldIOVnode::EmptyName );
    
    DldIOVnode::EmptyName = OSString::withCString("empty name");
    assert( DldIOVnode::EmptyName );
    if( !DldIOVnode::EmptyName ){
        
        DBG_PRINT_ERROR(("OSString::withString() failed"));
        return false;
    }
    
    DldIOVnode::UnknownProcessName = OSSymbol::withCString( "Unknown" );
    assert( DldIOVnode::UnknownProcessName );
    if( !DldIOVnode::UnknownProcessName ){
        
        DBG_PRINT_ERROR(("OSSymbol::withString() failed"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

//
// called at driver unload ( which is never happened )
//
void DldIOVnode::UninitVnodeSubsystem()
{
    if( DldIOVnode::EmptyName )
        DldIOVnode::EmptyName->release();
    
    if( DldIOVnode::UnknownProcessName )
        DldIOVnode::UnknownProcessName->release();
}

//--------------------------------------------------------------------

DldIOService* DldIOVnode::getReferencedService()
{
    assert( preemption_enabled() );
    assert( 0x0 == this->flags.reclaimed );
    
    //
    // a caller must be cautios to not call this routine for reclaimed vnodes
    // or when the vnode can be reclaimed concurrently
    //
    if( 0x1 == this->flags.reclaimed )
        return NULL;
    
    assert( this->vnode );
    
    DldIOService* dldIOService = DldGetReferencedDldIOServiceFoMediaByVnode( this->vnode );
    if( !dldIOService ){
        
        //
        // check for IOSerialBSDClient device
        //
        if( VCHR == this->v_type )
            dldIOService = DldGetReferencedDldIOServiceForSerialDeviceVnode( this );
            
            if( !dldIOService )
                goto __exit;
    }
    
    if( dldIOService->getObjectProperty() ){
        
        //
        // get the lowest physical media which defines the actual type of the device -
        // this is applicable only for virtual drives
        //
        DldIOService*    physicalIOMedia;
        
        physicalIOMedia = dldIOService->getObjectProperty()->getLowestBackingObjectReferenceForVirtualObject();
        
        //
        // set the found physical media as the IOMedia object which defines the security
        //
        if( physicalIOMedia ){
            
            dldIOService->release();
            dldIOService = physicalIOMedia;
        }
    }
    
__exit:
    
    //
    // return a referenced object
    //
    return dldIOService;
}

//--------------------------------------------------------------------

void DldIOVnode::defineStatusForCAWL( __in vfs_context_t vfsContext )
{
    assert( preemption_enabled() );
    assert( vfsContext );
    
    DldAccessCheckParam param;
    bool cawlOn;
    
    //
    // the flag is set once and never removed
    //
    if( 0x1 == this->flags.controlledByCAWL )
        return;
    
    DldIOService* dldIOService = this->getReferencedService();
    if( !dldIOService )
        goto __exit;
    
    if( !dldIOService->getObjectProperty() ){
        
        DBG_PRINT_ERROR(( "dldIOService->getObjectProperty() is NULL\n" ));
        assert( !"no object property" );
        
        goto __exit;
    }
    
    if( DEVICE_TYPE_REMOVABLE != dldIOService->getObjectProperty()->dataU.property->deviceType.type.major ){
        
        //
        // apply the disk CAWL ony for removable volumes, overrule any CAWL settings for internal disks
        //
        goto __exit;
    }
    
    if( 0x1 == dldIOService->getObjectProperty()->dataU.property->flags.cawlControlled ){
        
        //
        // everything goes under CAWL controll to provide consisten view on a storage
        //
        this->flags.controlledByCAWL = 0x1;
        goto __exit;
    }
    
#if defined(DBG)
    this->flags.controlledByCAWL = 0x1;
#endif // DBG
    
    bzero( &param, sizeof( param ) );
    
    param.userSelectionFlavor = kDefaultUserSelectionFlavor;
    param.aclType             = kDldAclTypeDiskCAWL;
    param.checkParentType     = true;
    param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_WRITE_DATA; // actually the value is ignored
    param.dldRequestedAccess.winRequestedAccess = DEVICE_WRITE; // actually the value is ignored
    param.credential          = vfs_context_ucred( vfsContext );
    param.service             = NULL;
    param.dldIOService        = dldIOService;
    
    ::DldAcquireResources( &param );
    ::DldIsDiskCawledUser( &param );
    ::DldReleaseResources( &param );
    
    cawlOn = ( param.output.diskCAWL.result[ DldFullTypeFlavor ].checkByCAWL || 
              param.output.diskCAWL.result[ DldMajorTypeFlavor ].checkByCAWL || 
              param.output.diskCAWL.result[ DldParentTypeFlavor ].checkByCAWL );
    
    if( cawlOn ){
        
        this->flags.controlledByCAWL = 0x1;
        
        //
        // all data streams are under CAWL controll so the view will be consistent
        //
        dldIOService->getObjectProperty()->dataU.property->flags.cawlControlled = 0x1;
    }
    
__exit:
    
    if( dldIOService )
        dldIOService->release();
}

//--------------------------------------------------------------------

vnode_t
DldIOVnode::getReferencedVnode()
{
    vnode_t    vnode = NULL;
    bool       reclaimed;
    
    assert( preemption_enabled() );
    
    if( 0x1 == this->flags.reclaimed )
        return NULL;
    
    //
    // vnode_getwithref() can block so must not be called with
    // any lock being held
    //
    
    OSIncrementAtomic( &this->delayReclaimCount );
    
    IOSimpleLockLock( this->spinLock );
    { // start of the lock
        
        reclaimed = ( 0x1 == this->flags.reclaimed );
        
    } // end of the lock
    IOSimpleLockUnlock( this->spinLock );
    
    if( !reclaimed && KERN_SUCCESS == vnode_getwithref( this->vnode ) )
        vnode = this->vnode;
    
    OSDecrementAtomic( &this->delayReclaimCount );
    
    //
    // allow the reclaim to continue
    //
    thread_wakeup( &this->delayReclaimCount );
    
    return vnode;
}

//--------------------------------------------------------------------

void
DldIOVnode::prepareForReclaiming()
{
    bool wait = false;
    
    assert( preemption_enabled() );
    
    IOSimpleLockLock( this->spinLock );
    { // start of the lock
        
        assert( 0x0 == this->flags.reclaimed );
        this->flags.reclaimed = 0x1;
        
        if( 0x0 != this->delayReclaimCount )
            wait = ( THREAD_WAITING == assert_wait( &this->delayReclaimCount, THREAD_UNINT ) );
        
    } // end of the lock
    IOSimpleLockUnlock( this->spinLock );
    
    //
    // wait until all callers of getReferencedVnode() returns,
    // this is required as DldIOVnode might outlive the underlying
    // BSD vnode so we need to provide a guarantee that the users
    // of DldIOVnode will not hit a reclaimed or reused vnode
    //
    while( wait ){
        
        thread_block( THREAD_CONTINUE_NULL );
        
        IOSimpleLockLock( this->spinLock );
        { // start of the lock
            
            if( 0x0 != this->delayReclaimCount )
                wait = ( THREAD_WAITING == assert_wait( &this->delayReclaimCount, THREAD_UNINT ) );
            else
                wait = false;
            
        } // end of the lock
        IOSimpleLockUnlock( this->spinLock );
        
    } // end while
}

//--------------------------------------------------------------------

dld_classic_rights_t
DldIOVnode::getAccessMask()
{
    
    //
    // a node type specific mapping
    //
    if( this->flags.directDiskOpen ){
        
        //
        // a vnode for a disk, see withBSDVnode() for a direct disk open detection
        //
        assert( VBLK == this->v_type || VCHR == this->v_type );
        
        return DEVICE_DIRECT_READ |
               DEVICE_DIRECT_WRITE |
               DEVICE_DISK_FORMAT |
               DEVICE_VOLUME_DEFRAGMENT ;
        
    } else if( this->flags.dir ){
        
        //
        // a directory
        //
        assert( VDIR == this->v_type );
        
        return  DEVICE_DIR_LIST |
                DEVICE_DIR_CREATE |
                DEVICE_WRITE |
                DEVICE_DELETE |
                DEVICE_RENAME;
        
    } else {
        
        //
        // a regular file
        //
        assert( VREG == this->v_type || VLNK == this->v_type );
        
        return DEVICE_READ |
               DEVICE_WRITE |
               DEVICE_EXECUTE |
               DEVICE_DELETE |
               DEVICE_RENAME;
    }
    
    return 0x0;
    
}

//--------------------------------------------------------------------

void
DldIOVnode::setUserID( __in kauth_cred_t credential )
{
    if( ! OSCompareAndSwap( (-1), kauth_cred_getuid( credential ), &this->auditData.userID.uid ) ) {
        
        return;
    }
    
    //
    // a user's GUID,
    // kauth_cred_getguid can return an error, disregard it
    //
    kauth_cred_getguid( credential, &this->auditData.userID.guid );
    this->auditData.userID.gid = kauth_cred_getgid( credential );
}

//--------------------------------------------------------------------

void
DldIOVnode::setProcess( __in pid_t _pid )
{    
    assert( (-1) != _pid );
    
    if( ! OSCompareAndSwap( (-1), _pid, &this->auditData.process.pid ) ){
        
        //
        // we were not the first
        //
        return;
    }
       
    char p_comm[MAXCOMLEN + 1];
    
    bzero( p_comm, sizeof(p_comm) );
    proc_name( _pid, p_comm, sizeof( p_comm ) );
    
    const OSSymbol*  newName = OSSymbol::withCString( p_comm );
    assert( newName );
    if( ! newName )
        return;
    
    const OSSymbol* oldName = NULL;
    
    IOSimpleLockLock( this->spinLock );
    { // start of the lock
        
        //
        // set the new name, but do not forget to release the old one
        //
        oldName = this->auditData.process.processName;
        this->auditData.process.processName = newName;
        
    } // end of the lock
    IOSimpleLockUnlock( this->spinLock );
    
    if( oldName )
        oldName->release();
    
    assert( this->auditData.process.processName && (-1) != this->auditData.process.pid );
}

//--------------------------------------------------------------------

const OSSymbol*
DldIOVnode::getProcessAuditNameRef()
{
    const OSSymbol* processName;
    
    IOSimpleLockLock( this->spinLock );
    { // start of the lock
        
        //
        // process.processName must always contain a pointer to an object
        //
        processName = this->auditData.process.processName;
        assert( processName );
        if( NULL == processName )
            processName = DldIOVnode::UnknownProcessName;
        
        processName->retain();
        
    } // end of the lock
    IOSimpleLockUnlock( this->spinLock );
    
    return processName;
}

//--------------------------------------------------------------------

void
DldIOVnode::addAuditRights( __in dld_classic_rights_t  _rightsToAudit /*this should be non encrypted rights*/)
{
    //
    // the idea here is as following - in the KAUTH we do not receive the comprehensive
    // information about future operations on the file, for example file rename and file write
    // are the same for KAUTH callback, but in the VFS hooks we know the operation, so to not
    // miss an operation the mask is extended
    //
    
    dld_classic_rights_t   rightsToAudit = _rightsToAudit;
    
    if( rightsToAudit & ALL_READ )   rightsToAudit |= ALL_READ_NOENCRYPT;
    if( rightsToAudit & ALL_WRITE )  rightsToAudit |= ALL_WRITE_NOENCRYPT;
    if( rightsToAudit & ALL_FORMAT ) rightsToAudit |= ALL_FORMAT_NOENCRYPT;
    
    //
    // convert any encrypted rights, the normal rigts are converted back to encrypted while
    // sending audit data to the service, internally driver use normal rights fot its chore
    //
    rightsToAudit = ConvertWinEncryptedRightsToWinRights( rightsToAudit );
    
    assert( 0x0 == (rightsToAudit & (DEVICE_ENCRYPTED_READ | DEVICE_ENCRYPTED_WRITE | DEVICE_ENCRYPTED_DIRECT_WRITE | DEVICE_ENCRYPTED_DISK_FORMAT)) );
    
    this->auditData.rightsToAudit |=  (rightsToAudit & this->getAccessMask());
}

//--------------------------------------------------------------------

#ifdef _DLD_MACOSX_CAWL

DldSparseFile*
DldIOVnode::getSparseFileRef()
{
    DldSparseFile*    sparseFile;
    
    IOSimpleLockLock( this->spinLock );
    { // start of the lock
        
        sparseFile = this->sparseFile;
        if( sparseFile )
            sparseFile->retain();
            
    } // end of the lock
    IOSimpleLockUnlock( this->spinLock );
    
    return sparseFile;
}

#endif // _DLD_MACOSX_CAWL

//--------------------------------------------------------------------

errno_t
DldVnodeSetsize(vnode_t vp, off_t size, int ioflag, vfs_context_t ctx)
{
    errno_t error;
	struct vnode_attr	va;
    
    //
    // get io count reference
    //
    error = vnode_getwithref( vp );
    if( error )
        return error;
    
	VATTR_INIT(&va);
	VATTR_SET(&va, va_data_size, size);
	va.va_vaflags = ioflag & 0xffff;
	error = vnode_setattr(vp, &va, ctx);
    
    vnode_put( vp );
    return error;
}

//--------------------------------------------------------------------
