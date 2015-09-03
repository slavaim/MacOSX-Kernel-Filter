/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include <IOKit/IOKitKeys.h>
#include <sys/proc.h>
#include "DldHookerCommonClass.h"
#include "DldSupportingCode.h"
#include "DldWhiteList.h"
#include "DldIOUserClientRef.h"

//--------------------------------------------------------------------

#define DLD_MAX_WL_PROCESS_NAME_LENGTH  (32)

//
// process is a task in Mach parlance
//
typedef struct _DldProcessWhiteListEntry{
    const char*     bsdName;             // an executable file name
    size_t          nameSize;            // a size reported by "sizeof" for the string, adjust DLD_MAX_WL_PROCESS_NAME_LENGTH accordihgly
    bool            isSuser;             // must be under super user credentials?
    pid_t           parentPID;           // parent ProcessID , (-1) if do not care
    dld_classic_rights_t  allowedAccess; // grant only this access 
} DldProcessWhiteListEntry;

static DldProcessWhiteListEntry   gProcessWhiteList[] = {
    //
    // 10.6 kauth resolver ( search for kauth_resolver_identity in the kernel source code ),
    // usually has PID 11
    //
    {"DirectoryService", sizeof("DirectoryService"), true, (pid_t)0x1, (dld_classic_rights_t)(-1) },
    
    //
    // the same as DirectoryService but for 10.7/10.8 ( Lion/Mountain Lion ), normally has PID 13/22
    //
    {"opendirectoryd", sizeof("opendirectoryd"), true, (pid_t)0x1, (dld_classic_rights_t)(-1) },
    
    //
    // diskarbitration and diskmanagementd daemon, required to correctly initialize volumes
    //
    {"diskarbitrationd", sizeof("diskarbitrationd"), true, (pid_t)0x1,(dld_classic_rights_t)( ~(ALL_WRITE | ALL_FORMAT)) },
    {"diskmanagementd", sizeof("diskmanagementd"), true, (pid_t)0x1, (dld_classic_rights_t)( ~(ALL_WRITE | ALL_FORMAT)) },
    
    //
    // allow DEVICE_DIRECT_READ for FS utils to allow FS mounting, pay attention that in the current code
    // the (-1) is passed as the requested access for DldIsProcessinWhiteList, so the allowedAccess
    // value is ignored, this is because of a chicken and egg problem, to convert kauth to win you need
    // a DldVnode, that requires calling kauth_cred_getguid which in turn calls kauth resolver
    // leading to a deadlock
    //
    
    {"hfs.util", sizeof("hfs.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"udf.util", sizeof("udf.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"cd9660.util", sizeof("cd9660.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"cddafs.util", sizeof("cddafs.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"exfat.util", sizeof("exfat.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"msdos.util", sizeof("msdos.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"ntfs.util", sizeof("ntfs.util"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    
    {"mount_hfs", sizeof("mount_hfs"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"mount_udf", sizeof("mount_udf"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"mount_cd9660", sizeof("mount_cd9660"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"mount_cddafs", sizeof("mount_cddafs"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"mount_exfat", sizeof("mount_exfat"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"mount_msdos", sizeof("mount_msdos"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"mount_ntfs", sizeof("mount_ntfs"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    
    {"fsck_hfs", sizeof("fsck_hfs"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"fsck_udf", sizeof("fsck_udf"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"fsck_exfat", sizeof("fsck_exfat"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"fsck_msdos", sizeof("fsck_msdos"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    {"fsck_cs", sizeof("fsck_cs"), true, (pid_t)(-1), DEVICE_DIRECT_READ | DEVICE_ENCRYPTED_READ },
    
    {"filecoordination", sizeof("filecoordination"), true, (pid_t)(-1), DEVICE_DIR_LIST }, // is used for copying files , run under root credentials
};

//--------------------------------------------------------------------

bool
DldIsProcessinWhiteList(
    __in vfs_context_t   vfsContext,
    __in dld_classic_rights_t winRequestedAccess
    )
{
    char   procName[ DLD_MAX_WL_PROCESS_NAME_LENGTH ];
    bool   isSuser;
    
    procName[ 0 ] = '\0';
    proc_name( vfs_context_pid( vfsContext ), procName, sizeof( procName ) );
    
    //
    // despite its description vfs_context_suser() returns an error
    //( i.e. 0x0 means - a user is a superuser, EPERM if not )
    //
    isSuser = ( 0x0 == vfs_context_suser( vfsContext ) );
    
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE(gProcessWhiteList); ++i ){
        
        DldProcessWhiteListEntry*   wlEntry = &gProcessWhiteList[ i ];
            
        if( wlEntry->isSuser != isSuser )
            continue;
        
        if( 0x0 != strncasecmp( procName, wlEntry->bsdName, wlEntry->nameSize ) )
            continue;
        
        if( (-1) != wlEntry->allowedAccess && 0x0 != ((~wlEntry->allowedAccess) & winRequestedAccess) )
            continue;
        
        if( (pid_t)(-1) != wlEntry->parentPID ){
            
            proc_t  proc = proc_find( vfs_context_pid( vfsContext ) ); // MUST be released by proc_rele() !
            if( proc ){
                
                pid_t parentPID = proc_ppid( proc );
                proc_rele( proc );
                proc = NULL;
                
                if( parentPID ==  wlEntry->parentPID )
                    return true;
            }
            
        } else {
            
            return true;
        }
        
    } // end for
    
    return false;
}

bool
DldIsDLServiceOrItsChild(
    __in pid_t  pid
    )
{    
    //
    // check that the process itself DL Service
    //
    if( gServiceUserClient.getUserClientPid() == pid )
        return true;
    
    //
    // check that the parent is DL Service
    //
    proc_t current = proc_find(pid);
    if( current ){
        pid_t ppid = proc_ppid( current );
        proc_rele( current );
        
        if( gServiceUserClient.getUserClientPid() == ppid )
            return true;
    }
    
    return false;
}

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldWhiteList, OSObject )

//--------------------------------------------------------------------

DldWhiteList* DldWhiteList::newWhiteList()
{
    DldWhiteList* wl = new DldWhiteList();
    assert( wl );
    if( !wl ){
        
        DBG_PRINT_ERROR(("new DldWhiteList() failed\n"));
        return NULL;
    }
    
    if( !wl->init() ){
        
        DBG_PRINT_ERROR(("wl->init() failed\n"));
        wl->release();
        return NULL;
    }
    
    return wl;
}

//--------------------------------------------------------------------

bool DldWhiteList::init()
{
    if( !super::init() ){
        
        DBG_PRINT_ERROR(("super::init() failed\n"));
        return false;
    }
    
    for( int i = 0x0; i < kDldWhiteListMax; ++i ){
        
        this->watermark[ i ] = 0x1;
        
        this->rwLock[i] = IORWLockAlloc();
        assert( this->rwLock[i] );
        if( !this->rwLock[i] ){
            
            DBG_PRINT_ERROR(("IORWLockAlloc() failed, i=%u\n", i));
            return false;
        }
    }// end for
    
    return true;
}

//--------------------------------------------------------------------

void DldWhiteList::free()
{
    for( int i = 0x0; i < kDldWhiteListMax; ++i ){
        
        if( this->rwLock[i] )
            IORWLockFree( this->rwLock[i] );
        
    }// end for
    
    for( int i = 0x0; i < kDldWhiteListMax; ++i ){
        
        if( this->wlDscr[i] )
            this->wlDscr[i]->release();
        
        if( this->wlACLs[i] )
            this->wlACLs[i]->release();
        
    }// end for
    
    super::free();
}

//--------------------------------------------------------------------

bool
DldWhiteList::setWhiteListWithCopy(
    __in     DldWhiteListType wlType,
    __in_opt const void   * whiteList,
    __in     unsigned int numBytes,
    __in_opt const kauth_acl_t  acl
    )
{
    assert( preemption_enabled() );
    assert( !( !whiteList && acl ) );
    
    OSData* newWL = NULL;
    OSData* oldWL = NULL;
    
    DldAclObject* newACL = NULL;
    DldAclObject* oldACL = NULL;
    
    if( whiteList && !DldWhiteList::isWhiteListValid( wlType, whiteList, numBytes ) ){
        
        DBG_PRINT_ERROR(( "A white list of 0x%X type and %u bytes size is invalid\n", wlType, numBytes ));
        return false;
    }
    
    if( whiteList ){
        
        newWL = OSData::withBytes( whiteList, numBytes );
        assert( newWL );
        if( !newWL ){
            
            DBG_PRINT_ERROR(("OSData::withBytes( whiteList, %i ) failed \n", numBytes ));
            return false;
        }
        
        //
        // despite returning (const void*) a description for getBytesNoCopy()
        // stipulates that it is allowed to change a content of a returned
        // buffer
        //
        ( (DldCommonWL*)newWL->getBytesNoCopy() )->kernelReserved = (UInt64)wlType;
        
        DldDeviceType devType;
        bzero( &devType, sizeof( devType ) );
        
        //
        // the type is irrelevant here
        //
        devType.type.major = kDldAclTypeUnknown;
        devType.type.minor = kDldAclTypeUnknown;
        
        if( acl ){
            
            newACL = DldAclObject::withAcl( acl, devType );
            assert( newACL );
            if( !newACL ){
                
                newWL->release();
                DBG_PRINT_ERROR(("DldAclObject::withAcl failed \n" ));
                return false;
            }// end if( !newACL )
            
        }
        
    } else {
        
        //
        // a request to remove the current WL
        //
        assert( !newWL && !newACL );
    }
    
    //
    // exchange the old and the new settings
    //
    this->LockExclusive( wlType );
    {// start of the lock
        
        oldWL = this->wlDscr[ wlType ];
        this->wlDscr[ wlType ] = newWL;
        
        oldACL = this->wlACLs[ wlType ];
        this->wlACLs[ wlType ] = newACL;
        
        //
        // the watermark must be updated with the lock held
        //
        this->updateWatermark( wlType );
        
    }// end of the lock
    this->UnLockExclusive( wlType );
    
    //
    // this removes the old object
    //
    if( oldWL )
        oldWL->release();
    DLD_DBG_MAKE_POINTER_INVALID( oldWL );
        
    if( oldACL )
        oldACL->release();
    DLD_DBG_MAKE_POINTER_INVALID( oldACL );
    
    //
    // log the new list
    //
    if( gGlobalSettings.logFlags.WHITE_LIST ){
        
        this->LockShared( wlType );
        {// start of the lock
            
            OSData* wlObj;
            
            wlObj = this->wlDscr[ wlType ];
            assert( wlObj );
            
            if( !wlObj )
                goto __exit_log;
            
            switch( wlType )
            {
                case kDldWhiteListUSBVidPid:
                case kDldTempWhiteListUSBVidPid:
                {
                    DldDeviceVidPidDscr* dscr;
                    
                    dscr = (DldDeviceVidPidDscr*)wlObj->getBytesNoCopy();
                    assert( dscr );
                    assert( wlType == dscr->common.kernelReserved );
                    
                    for( int i=0x0; i < dscr->count; ++i ){
                        
                        DLD_COMM_LOG( WHITE_LIST,
                                      ( "WhiteList.arrVidPid(%s)[%d]( VID=0x%x, PID=0x%x )\n",
                                       DldWhiteListTypeToName( wlType ),
                                       i, dscr->arrVidPid[i].VidPid.idVendor, dscr->arrVidPid[i].VidPid.idProduct ));
                        
                    }// end for
                    
                    break;
                }
                    
                case kDldWhiteListUSBUid:
                case kDldTempWhiteListUSBUid:
                case kDldWhiteListDVDUid:
                {
                    DldDeviceUIDDscr*    dscr;
                    
                    dscr = (DldDeviceUIDDscr*)wlObj->getBytesNoCopy();
                    assert( dscr );
                    assert( wlType == dscr->common.kernelReserved );
                    
                    for( int i=0x0; i < dscr->count; ++i ){
                        
                        DLD_COMM_LOG( WHITE_LIST,
                                      ( "WhiteList.arrUID(%s)[%d]( 0x%08x:0x%08x:0x%08x:0x%08x )\n",
                                       DldWhiteListTypeToName( wlType ),
                                       i, dscr->arrUID[i].uid.uid[0], dscr->arrUID[i].uid.uid[4],
                                       dscr->arrUID[i].uid.uid[8], dscr->arrUID[i].uid.uid[12] ));
                        
                    }// end for
                    break;
                }
                    
                default:
                    assert( !"A not implemented path" );
                    break;
            }// end switch( wlType )
            
        __exit_log:;
        }// end of the lock
        this->UnLockShared( wlType );
        
    }// end if( gGlobalSettings.logFlags.WHITE_LIST )
    
    //
    // rescan the device tree and apply the new white list settings
    //
    this->ApplyWhiteList( wlType );
    
    return true;
}

//--------------------------------------------------------------------

bool
DldWhiteList::isWhiteListValid( 
    __in DldWhiteListType type,
    __in const void   * whiteList,
    __in unsigned int   numBytes )
{
    
    if( NULL == whiteList )
        return false;
    
    switch( type ){
            
        case kDldWhiteListUSBVidPid:
        case kDldTempWhiteListUSBVidPid:
        {
            if( numBytes < FIELD_OFFSET(DldDeviceVidPidDscr,arrVidPid) )
                return false;
            
            DldDeviceVidPidDscr*  dscr = (DldDeviceVidPidDscr*)whiteList;
            
            if( (FIELD_OFFSET(DldDeviceVidPidDscr,arrVidPid) + dscr->count * sizeof(dscr->arrVidPid[0])) < numBytes )
                return false;
            
            return true;
        }
            
        case kDldWhiteListUSBUid:
        case kDldTempWhiteListUSBUid:
        case kDldWhiteListDVDUid:
        {
            if( numBytes < FIELD_OFFSET(DldDeviceUIDDscr,arrUID) )
                return false;
            
            DldDeviceUIDDscr*  dscr = (DldDeviceUIDDscr*)whiteList;
            
            if( (FIELD_OFFSET(DldDeviceUIDDscr,arrUID) + dscr->count * sizeof(dscr->arrUID[0])) < numBytes )
                return false;
            
            return true;
        }
            
        default:
            assert( !"An unknown white list type" );
            DBG_PRINT_ERROR(("An unknown white list type=0x%X\n", type));
            return false;
    }
}

//--------------------------------------------------------------------

void
DldWhiteList::LockShared( __in DldWhiteListType type )
{   
    assert( type < kDldWhiteListMax );
    assert( this->rwLock[ type ] );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock[ type ] );
};

//--------------------------------------------------------------------

void
DldWhiteList::UnLockShared( __in DldWhiteListType type )
{   
    assert( type < kDldWhiteListMax );
    assert( this->rwLock[ type ] );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock[ type ] );
};

//--------------------------------------------------------------------

void
DldWhiteList::LockExclusive( __in DldWhiteListType type )
{
    assert( type < kDldWhiteListMax );
    assert( this->rwLock[ type ] );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() != this->exclusiveThread[ type ] );
#endif//DBG
    
    IORWLockWrite( this->rwLock[ type ] );
    
#if defined(DBG)
    assert( NULL == this->exclusiveThread[ type ] );
    this->exclusiveThread[ type ] = current_thread();
#endif//DBG
    
};

//--------------------------------------------------------------------

void
DldWhiteList::UnLockExclusive(  __in DldWhiteListType type )
{
    assert( type < kDldWhiteListMax );
    assert( this->rwLock[ type ] );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() == this->exclusiveThread[ type ] );
    this->exclusiveThread[ type ] = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock[ type ] );
};

//--------------------------------------------------------------------

//
// returns the entry for the VidPid, if entry is not found the NULL value is returned,
// the entry is valid as long as the dscr is valid
//
DldUsbVidPidEntry*
DldWhiteList::getVidPidEnrty( __in DldUsbVidPid* VidPid, __in DldDeviceVidPidDscr* dscr )
{
    assert( sizeof( *VidPid ) == sizeof( dscr->arrVidPid[0].VidPid ) );
    
    for( int i=0x0; i < dscr->count; ++i ){
        
        if( 0x0 != memcmp( VidPid, &dscr->arrVidPid[i].VidPid, sizeof( *VidPid ) ) )
            continue;
        
        return &dscr->arrVidPid[i];
        
    }// end for
    
    return NULL;
}

//--------------------------------------------------------------------

//
// returns the entry for the UID, if entry is not found the NULL value is returned,
// the entry is valid as long as the dscr is valid,
// uid must be 16 bytes long
//
DldDeviceUIDEntry*
DldWhiteList::getDeviceUIDEnrty( __in DldDeviceUID* uid, __in DldDeviceUIDDscr* dscr )
{
    assert( 16 == sizeof( dscr->arrUID[0].uid ) );
    
    for( int i=0x0; i < dscr->count; ++i ){
        
        if( 0x0 != memcmp( uid, &dscr->arrUID[i].uid, 16 ) )
            continue;
        
        return &dscr->arrUID[i];
        
    }// end for
    
    return NULL;
}

//--------------------------------------------------------------------

//
// returns the entry copy for the VidPid, if entry is not found the false is returned,
// the caller must provide a space for the output entry,
// the returned *outAclObject is referenced
//
bool
DldWhiteList::getVidPidEnrtyEx( __in      DldWhiteListType type,
                                __in      DldUsbVidPid* VidPid,
                                __inout   DldUsbVidPidEntry* outEntry,
                                __out_opt DldAclObject** outAclObject )
{
    assert( preemption_enabled() );
    assert( kDldWhiteListUSBVidPid == type || kDldTempWhiteListUSBVidPid == type );
    
    if( ! ( kDldWhiteListUSBVidPid == type || kDldTempWhiteListUSBVidPid == type ) )
        return false;
    
    if( NULL == this->wlDscr[ type ] )
        return false;
    
    bool    found  = false;
    bool    repeat = false;
    
    do{// start of while( repeat );
        
        UInt32               currentWatermark;
        DldAclObject*        aclObject = NULL;
        OSData*              wlObj     = NULL;
        
        this->LockShared( type );
        {// start of the lock
            
            assert( !( !this->wlDscr[ type ] && this->wlACLs[ type ] ) );
            
            currentWatermark = this->getWatermark( type );
            
            wlObj = this->wlDscr[ type ];
            if( wlObj )
                wlObj->retain();
            
            aclObject = this->wlACLs[ type ];
            if( aclObject )
                aclObject->retain();
            
        }// end of the lock
        this->UnLockShared( type );
        
        assert( !( NULL == wlObj && NULL != aclObject ) );
        
        if( wlObj ){
            
            DldDeviceVidPidDscr* dscr;
            
            dscr = (DldDeviceVidPidDscr*)wlObj->getBytesNoCopy();
            assert( dscr );
            assert( type == dscr->common.kernelReserved );
            
            DldUsbVidPidEntry*   entry;
            
            //
            // search w/o the lock held as the size of the data might be substantially big
            //
            entry = DldWhiteList::getVidPidEnrty( VidPid, dscr );
            this->LockShared( type );
            {// start of the lock
                
                //
                // check that the setting are the same, the watermark is perfectly
                // suitable as it is updated with the lock held exclusive
                //
                if( currentWatermark == this->getWatermark( type )  ){
                    
                    if( entry ){
                        
                        *outEntry = *entry;
                        
                        if( outAclObject ){
                            
                            *outAclObject = aclObject;
                            
                            //
                            // the ownership has been transferred to a caller
                            //
                            aclObject = NULL;
                            
                        }// end if( outAclObject )
                        
                        found = true;
                        
                    }// end if( entry )
                    
                } else {
                    
                    //
                    // the settings has changed while we were searching through the old white list data
                    //
                    repeat = true;
                }
                
            }// end of the lock
            this->UnLockShared( type );
            
            wlObj->release();
            wlObj = NULL;
        }
        
        if( aclObject ){
            
            aclObject->release();
            aclObject = NULL;
        }
        
        assert( !( found && repeat ) );
        
    } while( repeat );
    
    return found;
}

//--------------------------------------------------------------------

//
// returns the entry copy for the VidPid, if entry is not found the false is returned,
// the caller must provide a space for the output entry,
// the returned *outAclObject is referenced
//
bool
DldWhiteList::getVidPidEnrty( __in      DldUsbVidPid* VidPid,
                              __inout   DldUsbVidPidEntry* outEntry,
                              __out_opt DldAclObject** outAclObject )
{
    return this->getVidPidEnrtyEx( kDldWhiteListUSBVidPid, VidPid, outEntry, outAclObject ) ||
           this->getVidPidEnrtyEx( kDldTempWhiteListUSBVidPid, VidPid, outEntry, outAclObject );
}

//--------------------------------------------------------------------

//
// returns the entry copy for the UID, if entry is not found the false is returned,
// the caller must provide a space for the outEntry entry
// uid must be 16 bytes long
//
bool
DldWhiteList::getUsbUIDEnrty( __in DldDeviceUID* uid,
                              __inout DldDeviceUIDEntry* outEntry,
                              __out_opt DldAclObject** outAclObject )
{    
    return this->getDeviceUIDEnrty( kDldWhiteListUSBUid, uid, outEntry, outAclObject ) ||
           this->getDeviceUIDEnrty( kDldTempWhiteListUSBUid, uid, outEntry, outAclObject );
}

bool
DldWhiteList::getDvdUIDEnrty( __in CDDVDDiskID* uid,
                              __inout DldDeviceUIDEntry* outEntry,
                              __out_opt DldAclObject** outAclObject )
{    
    return this->getDeviceUIDEnrty( kDldWhiteListDVDUid, uid, outEntry, outAclObject );
}

//--------------------------------------------------------------------

//
// returns the entry copy for the UID, if entry is not found the false is returned,
// the caller must provide a space for the outEntry entry
// uid must be 16 bytes long
//
bool
DldWhiteList::getDeviceUIDEnrty( __in DldWhiteListType type,
                                 __in DldDeviceUID* uid,
                                 __inout DldDeviceUIDEntry* outEntry,
                                 __out_opt DldAclObject** outAclObject )
{
    assert( preemption_enabled() );
    
    if( NULL == this->wlDscr[ type ] )
        return false;
    
    bool    found  = false;
    bool    repeat = false;
    
    do{// start of while( repeat );
        
        UInt32               currentWatermark;
        DldAclObject*        aclObject = NULL;
        OSData*              wlObj     = NULL;
        
        this->LockShared( type );
        {// start of the lock
            
            assert( !( !this->wlDscr[ type ] && this->wlACLs[ type ] ) );
            
            currentWatermark = this->getWatermark( type );
            
            wlObj = this->wlDscr[ type ];
            if( wlObj )
                wlObj->retain();
            
            aclObject = this->wlACLs[ type ];
            if( aclObject )
                aclObject->retain();
            
        }// end of the lock
        this->UnLockShared( type );
        
        assert( !( NULL == wlObj && NULL != aclObject ) );
        
        if( wlObj ){
            
            DldDeviceUIDDscr* dscr;
            
            dscr = (DldDeviceUIDDscr*)wlObj->getBytesNoCopy();
            assert( dscr );
            assert( type == dscr->common.kernelReserved );
            
            DldDeviceUIDEntry*   entry;
            
            //
            // search w/o the lock held as the size of the data might be substantially big
            //
            entry = DldWhiteList::getDeviceUIDEnrty( uid, dscr );
            this->LockShared( type );
            {// start of the lock
                
                //
                // check that the setting are the same, the watermark is perfectly
                // suitable as it is updated with the lock held exclusive
                //
                if( currentWatermark == this->getWatermark( type )  ){
                    
                    if( entry ){
                        
                        *outEntry = *entry;
                        
                        if( outAclObject ){
                            
                            *outAclObject = aclObject;
                            
                            //
                            // the ownership has been transferred to a caller
                            //
                            aclObject = NULL;
                            
                        }// end if( outAclObject )
                        
                        found = true;
                        
                    }// end if( entry )
                    
                } else {
                    
                    //
                    // the settings has changed while we were searching through the old white list data
                    //
                    repeat = true;
                }
                
            }// end of the lock
            this->UnLockShared( type );
            
            wlObj->release();
            wlObj = NULL;
        }
        
        if( aclObject ){
            
            aclObject->release();
            aclObject = NULL;
        }
        
        assert( !( found && repeat ) );
        
    } while( repeat );
    
    return found;
}

//--------------------------------------------------------------------

void DldWhiteList::ApplyWhiteList( __in DldWhiteListType type )
{
    assert( preemption_enabled() );
    
    //
    // add a code for a new type!
    //
    assert( type == kDldWhiteListUSBVidPid  ||
            type == kDldTempWhiteListUSBVidPid ||
            type == kDldWhiteListUSBUid     ||
            type == kDldTempWhiteListUSBUid ||
            type == kDldWhiteListDVDUid);
    
    switch( type ){
            
        case kDldWhiteListUSBVidPid:
        case kDldTempWhiteListUSBVidPid:
        case kDldWhiteListUSBUid:
        case kDldTempWhiteListUSBUid:
            this->ApplyWhiteListUSB( type );
            break;
            
        case kDldWhiteListDVDUid:
            this->ApplyWhiteListDVD( type );
            break;
            
        default:
            assert( !"a wrong type or unimplemented\n" );
            DBG_PRINT_ERROR(("a wrong type=%d\n", (int)type ));
            break;
    }
}

//--------------------------------------------------------------------

void DldWhiteList::ApplyWhiteListUSB( __in DldWhiteListType type )
{
    assert( preemption_enabled() );
    
    //
    // add a code for a new type!
    //
    assert( type == kDldWhiteListUSBVidPid || type == kDldTempWhiteListUSBVidPid ||
            type == kDldWhiteListUSBUid || type == kDldTempWhiteListUSBUid );
    
    if( type != kDldWhiteListUSBVidPid && type != kDldTempWhiteListUSBVidPid &&
        type != kDldWhiteListUSBUid && type != kDldTempWhiteListUSBUid )
        return;

    //
    // use the gIOUSBPlane as we need IOUSBDevice class objects, traversing gIOServicePlane
    // would be a waste of resources
    //
    const IORegistryPlane* IOUSBPlane = IORegistryEntry::getPlane( kIOUSBPlane );
    assert( IOUSBPlane );
    if( !IOUSBPlane ){
        
        DBG_PRINT_ERROR(("IORegistryEntry::getPlane( kIOUSBPlane ) returned NULL\n"))
        return;
    }
    
    IORegistryEntry *     next;
    IORegistryIterator *  iter;
    
    iter = IORegistryIterator::iterateOver( IOUSBPlane );
    assert( iter );
    if( !iter ){
        
        DBG_PRINT_ERROR(("IORegistryIterator::iterateOver( IOUSBPlane ) returned NULL\n"))
        return;
    }
    
    iter->reset();
    while( ( next = iter->getNextObjectRecursive() ) ){
        
        IOService*   service = NULL;
        
        service = OSDynamicCast( IOService, next );
        if( !service )
            continue;
        
        //
        // get the corresponding DldIOService object
        //
        DldIOService*  dldService;
        
        dldService = DldIOService::RetrieveDldIOServiceForIOService( service );
        assert( dldService );
        if( !dldService )
            continue;
        
        if( dldService->getObjectProperty() ){
            
            //
            // TO DO - find a way to say that only a white list of a particular type
            // must be checked instead a full update
            //
            dldService->getObjectProperty()->updateDescriptor( NULL, dldService, false );
        }
        
        dldService->release();
        
    }// end while( ( next = iter->getNextObjectRecursive() ) )
    iter->release();
    
    return;
}

//--------------------------------------------------------------------

void DldWhiteList::ApplyWhiteListDVD( __in DldWhiteListType type )
{
    assert( preemption_enabled() );
    
    //
    // add a code for a new type!
    //
    assert( type == kDldWhiteListDVDUid );
    
    if( type != kDldWhiteListDVDUid )
        return;
    
    //
    // we have to traverse the gIOServicePlane, a time and resouce consuming task,
    // in the future the optimization is possible if all required properties
    // can be retrieved without the tree traversing, for example by putting them in a list
    //
    const IORegistryPlane* IOServicePlane = IORegistryEntry::getPlane( kIOServicePlane );
    assert( IOServicePlane );
    if( !IOServicePlane ){
        
        DBG_PRINT_ERROR(("IORegistryEntry::getPlane( kIOServicePlane ) returned NULL\n"))
        return;
    }
    
    IORegistryEntry *     next;
    IORegistryIterator *  iter;
    
    iter = IORegistryIterator::iterateOver( IOServicePlane );
    assert( iter );
    if( !iter ){
        
        DBG_PRINT_ERROR(("IORegistryIterator::iterateOver( kIOServicePlane ) returned NULL\n"))
        return;
    }
    
    iter->reset();
    while( ( next = iter->getNextObjectRecursive() ) ){
        
        IOService*   service = NULL;
        
        service = OSDynamicCast( IOService, next );
        if( !service )
            continue;
        
        //
        // get the corresponding DldIOService object
        //
        DldIOService*  dldService;
        
        dldService = DldIOService::RetrieveDldIOServiceForIOService( service );
        assert( dldService );
        if( !dldService )
            continue;
        
        if( dldService->getObjectProperty() &&
            kDldWhiteListDVDUid == dldService->getObjectProperty()->dataU.property->whiteListState.type ){
            
            //
            // mark it as not whitelisted, the new setting will be applied if necessary
            // without rereading the media as the UID has been saved and will be preserved
            //
            
            DldAclObject*            wlACLToRemove;
            DldObjectPropertyEntry*  property;
            
            property = dldService->getObjectProperty();
            assert( property );
            
            property->dataU.property->LockExclusive();
            { // start of the lock
                
                wlACLToRemove = property->dataU.property->whiteListState.acl;
                property->dataU.property->whiteListState.acl              = NULL;
                property->dataU.property->whiteListState.inWhiteList      = false;
                property->dataU.property->whiteListState.currentWLApplied = false;
                
                //
                // meaningless for DVD/CD
                //
                assert( false == property->dataU.property->whiteListState.propagateUp );
                
            }// end of the lock
            property->dataU.property->UnLockExclusive();
            
            if( wlACLToRemove )
                wlACLToRemove->release();
            
        }
        
        dldService->release();
        
    }// end while( ( next = iter->getNextObjectRecursive() ) )
    iter->release();
    
    return;
}

//--------------------------------------------------------------------