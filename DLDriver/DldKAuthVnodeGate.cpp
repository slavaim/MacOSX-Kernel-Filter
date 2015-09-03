/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include <sys/proc.h>
#include <sys/vm.h>
#include "DldKAuthVnodeGate.h"
#include "DldVNodeHook.h"
#include "IOBSDSystem.h"
#include "DldVnodeHashTable.h"
#include "DldIOLog.h"
#include "DldKernAuthorization.h"
#include "DldIOShadow.h"
#include "DldSupportingCode.h"
#include "DldDVDWhiteList.h"
#include "DldDiskCAWL.h"

//--------------------------------------------------------------------

#define super OSObject
OSDefineMetaClassAndStructors( DldIOKitKAuthVnodeGate, OSObject)

//--------------------------------------------------------------------

IOReturn
DldIOKitKAuthVnodeGate::RegisterVnodeScopeCallback(void)
{
    
    //
    // register our listener
    //
    this->VnodeListener = kauth_listen_scope( KAUTH_SCOPE_VNODE,                                 // for the vnode scope
                                              DldIOKitKAuthVnodeGate::DldVnodeAuthorizeCallback, // using this callback
                                              this );                                            // give a cookie to callback
    
    if( NULL == this->VnodeListener ){
        
        DBG_PRINT_ERROR( ( "kauth_listen_scope failed\n" ) );
        return kIOReturnInternalError;
        
    }
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn
DldIOKitKAuthVnodeGate::RegisterFileopCallback(void)
{
    //
    // we can't rely on kauth callbacks for close notification as they are skipped
    // when the process is being terminated and proc_exit() calls fdfree() that calls
    // closef_locked() that does not calls kauth callbacks, so we need to notify about
    // open from the hook to keep the balance between open and close notifications
    //
    return kIOReturnSuccess;
    /*
    
    //
    // register our listener
    //
    this->FileopListener = kauth_listen_scope( KAUTH_SCOPE_FILEOP,                        // for the file scope
                                               DldIOKitKAuthVnodeGate::DldFileopCallback, // using this callback
                                               this );                                    // give a cookie to callback
    
    if( NULL == this->FileopListener ){
        
        DBG_PRINT_ERROR( ( "kauth_listen_scope failed\n" ) );
        return kIOReturnInternalError;
        
    }
    
    return kIOReturnSuccess;
     */
}


//--------------------------------------------------------------------

void
DldIOKitKAuthVnodeGate::UnregisterVnodeScopeCallback(void)
{
    if( this->VnodeListener ){
        
        //
        // unregister the listener
        //
        kauth_unlisten_scope( this->VnodeListener );
        this->VnodeListener = NULL;
    }
}

//--------------------------------------------------------------------

void
DldIOKitKAuthVnodeGate::UnregisterFileopCallback(void)
{
    if( this->FileopListener ){
        
        //
        // unregister the listener
        //
        kauth_unlisten_scope( this->FileopListener );
        this->FileopListener = NULL;
    }
}

//--------------------------------------------------------------------

void
DldIOKitKAuthVnodeGate::convertKauthToWin(
    __in DldAccessCheckParam* param,
    __in DldIOVnode*  dldVnode,
    __in vnode_t      vnode
    )
{
    assert( 0x0 == param->dldRequestedAccess.winRequestedAccess );
    assert( 0x0 != param->dldRequestedAccess.kauthRequestedAccess );
    
    //
    // the high order bits represent flags and must be converted to rights if possible
    //
    if(  param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_LINKTARGET |
                                                            KAUTH_VNODE_CHECKIMMUTABLE |
                                                            KAUTH_VNODE_ACCESS |
                                                            KAUTH_VNODE_NOIMMUTABLE |
                                                            KAUTH_VNODE_SEARCHBYANYONE ) )
    {
        if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_CHECKIMMUTABLE ){
            
            //
            // the check for an immutable object should be converted to write access
            //
            param->dldRequestedAccess.kauthRequestedAccess |= KAUTH_VNODE_WRITE_DATA;
        }
        
        //
        // remove the flags as they are not present in ACLs as rights so
        // the result of the check against ACLs will always be KAUTH_RESULT_DENY
        //
        param->dldRequestedAccess.kauthRequestedAccess &= ~( KAUTH_VNODE_LINKTARGET |
                                                             KAUTH_VNODE_CHECKIMMUTABLE |
                                                             KAUTH_VNODE_ACCESS |
                                                             KAUTH_VNODE_NOIMMUTABLE |
                                                             KAUTH_VNODE_SEARCHBYANYONE );
        
    }// end if( eval->ae_requested & (
    
    if( 0x0 == param->dldRequestedAccess.kauthRequestedAccess )
        return;
    
    //
    // a node type specific mapping
    //
    if( dldVnode->flags.directDiskOpen ){
        
        //
        // a vnode for a disk, see withBSDVnode() for a direct disk open detection
        //
        assert( VBLK == vnode_vtype( vnode ) || VCHR == vnode_vtype( vnode ) );
        
        if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_READ_DATA )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIRECT_READ;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_WRITE_DATA |
                                                               KAUTH_VNODE_APPEND_DATA ) )
            param->dldRequestedAccess.winRequestedAccess |= (DEVICE_DIRECT_WRITE | DEVICE_DISK_FORMAT);
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_WRITE_ATTRIBUTES |
                                                               KAUTH_VNODE_WRITE_EXTATTRIBUTES |
                                                               KAUTH_VNODE_WRITE_SECURITY |
                                                               KAUTH_VNODE_TAKE_OWNERSHIP ))
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIRECT_WRITE;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_READ_SECURITY |
                                                               KAUTH_VNODE_READ_EXTATTRIBUTES |
                                                               KAUTH_VNODE_READ_ATTRIBUTES ) )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIRECT_READ;
        
    } else if( dldVnode->flags.dir ){
        
        //
        // a directory
        //
        assert( VDIR == vnode_vtype( vnode ) );
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_LIST_DIRECTORY |
                                                               KAUTH_VNODE_READ_DATA |
                                                               KAUTH_VNODE_SEARCH ) )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIR_LIST;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_WRITE_DATA |
                                                               KAUTH_VNODE_APPEND_DATA |
                                                               KAUTH_VNODE_ADD_SUBDIRECTORY |
                                                               KAUTH_VNODE_ADD_FILE ) )
            param->dldRequestedAccess.winRequestedAccess |= (DEVICE_DIR_CREATE | DEVICE_WRITE);
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_DELETE_CHILD |
                                                               KAUTH_VNODE_DELETE ) )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DELETE;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_WRITE_ATTRIBUTES |
                                                               KAUTH_VNODE_WRITE_EXTATTRIBUTES |
                                                               KAUTH_VNODE_WRITE_SECURITY |
                                                               KAUTH_VNODE_TAKE_OWNERSHIP ))
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_WRITE;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_READ_SECURITY |
                                                               KAUTH_VNODE_READ_EXTATTRIBUTES |
                                                               KAUTH_VNODE_READ_ATTRIBUTES ) )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIR_LIST;
        
    } else {
        
        //
        // a regular file
        //
        assert( VREG == vnode_vtype( vnode ) || VLNK == vnode_vtype( vnode ) );
        
        if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_READ_DATA )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_READ;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_WRITE_DATA | 
                                                               KAUTH_VNODE_APPEND_DATA ) )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_WRITE;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_EXECUTE )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_EXECUTE;
        
        //
        // rename operation requires KAUTH_VNODE_DELETE
        //
        if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_DELETE )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_DELETE;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_WRITE_ATTRIBUTES |
                                                               KAUTH_VNODE_WRITE_EXTATTRIBUTES |
                                                               KAUTH_VNODE_WRITE_SECURITY |
                                                               KAUTH_VNODE_TAKE_OWNERSHIP ))
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_WRITE;
        
        if( param->dldRequestedAccess.kauthRequestedAccess & ( KAUTH_VNODE_READ_SECURITY |
                                                               KAUTH_VNODE_READ_EXTATTRIBUTES |
                                                               KAUTH_VNODE_READ_ATTRIBUTES ) )
            param->dldRequestedAccess.winRequestedAccess |= DEVICE_READ;
    }
    
    assert( 0x0 != param->dldRequestedAccess.winRequestedAccess );
    
}

//--------------------------------------------------------------------

int
DldIOKitKAuthVnodeGate::DldVnodeAuthorizeCallback(
    kauth_cred_t    credential, // reference to the actor's credentials
    void           *idata,      // cookie supplied when listener is registered
    kauth_action_t  action,     // requested action
    uintptr_t       arg0,       // the VFS context
    uintptr_t       arg1,       // the vnode in question
    uintptr_t       arg2,       // parent vnode, or NULL
    uintptr_t       arg3)       // pointer to an errno value
{
    DldIOKitKAuthVnodeGate*    _this;
    bool                       disable   = false;
    bool                       log       = false;
    IOReturn                   cawlError = KERN_SUCCESS;
    vnode_t                    vnode = (vnode_t)arg1;
    DldAccessCheckParam        param;
    bool                       paramWasAcquired = false;
    
    assert( preemption_enabled() );
    
#if defined(DBG)
    //
    // for the debug purposses, normally this is not required
    //
    param.resourcesReleased = true;
#endif // DBG
    
    //
    // sometimes the kernel calls us with a zero action parameter,
    // like in a case of following call stack
    /*
     #4  0x46833b40 in DldIOKitKAuthVnodeGate::DldVnodeAuthorizeCallback (credential=0x596bd4c, idata=0x6e07880, action=0, arg0=114809396, arg1=197683668, arg2=0, arg3=837074008) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKAuthVnodeGate.cpp:328
     #5  0x00508a71 in kauth_authorize_action (scope=0x59aa104, credential=0x596bd4c, action=0, arg0=114809396, arg1=197683668, arg2=0, arg3=837074008) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/kern/kern_authorization.c:420
     #6  0x003375e7 in vnode_authorize (vp=0xbc869d4, dvp=0x0, action=0, ctx=0x6d7da34) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_subr.c:4863
     #7  0x0034354f in chmod2 (ctx=0x6d7da34, vp=0xbc869d4, vap=0x31e4be6c) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:4759
     #8  0x00343652 in chmod1 (ctx=0x6d7da34, path=4635961024, vap=0x31e4be6c) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:4789
     #9  0x0034398a in chmod (p=0x66b2d20, uap=0x6f0ba88, retval=0x6d7d974) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/vfs/vfs_syscalls.c:4869
     #10 0x005b19b5 in unix_syscall64 (state=0x6f0ba84) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/bsd/dev/i386/systemcalls.c:365
     */
    if( 0x0 == action )
        return KAUTH_RESULT_DEFER;
    
    if( gGlobalSettings.doNotHookVFS )
        return KAUTH_RESULT_DEFER;
    
    //
    // if this is a dead vnode then skip it
    //
    if( vnode_isrecycled( vnode ) )
        return KAUTH_RESULT_DEFER;
    
    _this = (DldIOKitKAuthVnodeGate*)idata;
    
    //
    // VNON vnode is created by devfs_devfd_lookup() for /dev/fd/X vnodes that
    // are not of any interest for us
    // VSOCK is created for UNIX sockets
    // etc.
    //
    enum vtype   vnodeType = vnode_vtype( vnode );    
    if( VREG != vnodeType &&
        VDIR != vnodeType &&
        VBLK != vnodeType &&
        VCHR != vnodeType &&
        VLNK != vnodeType )
        return KAUTH_RESULT_DEFER;
    
    //
    // a case when DirectoryService calls FS, DirectoryService/opendirectoryd is a process that
    // processes callbacks from the kernel for kauth_cred_getguid,  DirectoryService calls
    // identitysvc() that blocks until a callback from kauth_cred_getguid is placed in a queue,
    // fetches the callback structure and returns from the system call,
    // the DirectoryService process should run with superuser credentials,
    // pay attention that the requested access is set to (-1) to select only
    // the most critical processes that should not be examinied because of an
    // impending deadlock
    // TO DO - research teh posibility of optimization by remembering the process ID
    //
    if( DldIsProcessinWhiteList((vfs_context_t)arg0, (-1)) || DldIsDLServiceOrItsChild(vfs_context_pid( (vfs_context_t)arg0) ) )
        return KAUTH_RESULT_DEFER;
    
    DldIOVnode*  dldVnode;
    dldVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode( vnode );
    assert( dldVnode );
    if( !dldVnode ){
        
        int    nameLength = MAXPATHLEN;
        char*  nameBuffer = (char*)IOMalloc( MAXPATHLEN );
        if( nameBuffer ){
            int    error;
            
            error = vn_getpath( vnode, nameBuffer, &nameLength );
            
            DBG_PRINT_ERROR(( "unable to create a dldVnode for %s\n", (error?"UnableToGetName":nameBuffer) ));
            IOFree( nameBuffer, MAXPATHLEN );
        }
        
        return KAUTH_RESULT_DEFER;
    }
    
    //
    // update the name, the function is idempotent in its behaviour so it is okay to call it multiple times
    //
    dldVnode->updateNameToValid();
    
    /*
    //   test, a filter by volume
    if( dldVnode->isNameVaid() ){
        
        OSString* name = dldVnode->getNameRef();
        if( name ){
            
            char prefix[20] = {0};
            
            for( int i = 0; i < (sizeof("/Volumes/H")-1); ++i ){
                
                prefix[i] = name->getChar(i);
                prefix[i+1] = '\0';
            }
            
            if( 0x0 != strcmp( "/Volumes/H", prefix ) ){
                
                dldVnode->release();
                name->release();
                return KAUTH_RESULT_DEFER;
            }
            
            name->release();
        }
    }
     */
    
    //
    // update the protected file, do this after the name has been updated
    //
    dldVnode->updateProtectedFile();
    
    if( gServiceProtection->protectAgentFiles() ){
        
        DldProtectedFile* protectedFile = dldVnode->getProtectedFileRef();
        if( protectedFile ){
            
            //
            // this is a protected file
            //
            
            bool isAccessAllowedForProcess;
            
            //
            // check protected file access permissions for a calling process
            //
            
            isAccessAllowedForProcess = protectedFile->isAccessAllowed( (vfs_context_t)arg0,
                                                                       credential,
                                                                       action );
            
            protectedFile->release();
            DLD_DBG_MAKE_POINTER_INVALID( protectedFile );
            
            if( !isAccessAllowedForProcess ){
                
                //
                // the access was not granted for the process to access a protected file
                //
                return KAUTH_RESULT_DENY;
            } // end if( !isAccessAllowedForProcess )
        } // end if( protectedFile )
        DLD_DBG_MAKE_POINTER_INVALID( protectedFile );
    } // end if( gServiceProtection->protectAgentFiles() )
    
    
    //
    // do not use here dldVnode->getReferencedService() as we need to process
    // serial device objects
    // TO DO - it seems that  _this->commonHooker.checkAndLogUserClientAccess()
    // for a serial device can be called after dldVnode->getReferencedService()
    // after checking for device type, so replace for getReferencedService()
    // including the following transformation to physicalIOMedia
    //
    DldIOService* dldIOService = DldGetReferencedDldIOServiceFoMediaByVnode( vnode );
    if( !dldIOService ){
        
        //
        // check for IOSerialBSDClient device, DldGetReferencedDldIOServiceForSerialDeviceVnode is
        // unable to resolv a vnode opened through the hard link, in that case the request will be cancelled
        // at the bus level ( usually USB )
        //
        if( VCHR == vnodeType ){
            
            dldIOService = DldGetReferencedDldIOServiceForSerialDeviceVnode( dldVnode );
            if( dldIOService ){
                
                //
                // this IOService is definitely having a client as only a client can issue requests
                // that results in KAUTH callbacks invokation
                //
                dldIOService->userClientAttached();
                
                IOService*          service;
                DldRequestedAccess  requestedAccess = {0x0};
                
                //
                // TO DO - set win access
                //
                requestedAccess.kauthRequestedAccess = action;
                
                service = dldIOService->getSystemServiceRef();
                assert( service );
                
                if( service ){
                    
                    //
                    // imagine that a user client is attached to a IOSerialBSDClient's device
                    //
                    disable = ( kIOReturnSuccess != _this->commonHooker.checkAndLogUserClientAccess( service,
                                                                                                     vfs_context_pid( (vfs_context_t)arg0 ),
                                                                                                     credential,
                                                                                                     &requestedAccess ) );
                    
                    dldIOService->putSystemServiceRef( service );
                }// end if( service )
            }
        } // end if( VCHR == vnodeType )
        
        goto __exit;
    }
    
    //
    // this IOService is definitely having a client as only a client can issue requests
    // that results in KAUTH callbacks invokation
    //
    dldIOService->userClientAttached();
    
    if( dldIOService->getObjectProperty() ){
        
        if( 0x0 == dldVnode->auditData.deviceType.type.major )
            dldVnode->auditData.deviceType = dldIOService->getObjectProperty()->dataU.property->deviceType;
        
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

    //
    // check the security
    //
    bzero( &param, sizeof( param ) );
    
    param.userSelectionFlavor = kDefaultUserSelectionFlavor;
    param.aclType             = kDldAclTypeSecurity;
    param.checkParentType     = true;
    param.dldRequestedAccess.kauthRequestedAccess = action;
    param.credential          = credential;
    param.service             = NULL;
    param.dldIOService        = dldIOService;
#if defined(LOG_ACCESS)
    param.sourceFunction      = __PRETTY_FUNCTION__;
    param.sourceFile          = __FILE__;
    param.sourceLine          = __LINE__;
    param.dldVnode            = dldVnode;
#endif//#if defined(LOG_ACCESS)
    
    _this->convertKauthToWin( &param, dldVnode, vnode );
    //
    // here is a second call to DldIsProcessinWhiteList with a real requested access to allow some process
    // to continue if the access is deemed to be benign
    //
    if( 0x0 == param.dldRequestedAccess.winRequestedAccess || DldIsProcessinWhiteList((vfs_context_t)arg0, param.dldRequestedAccess.winRequestedAccess)){
        
        //
        // nothinh to check, this usually happens when the higher order bits are set for action
        // which means the request is not for a security check
        //
        goto __exit;
    }
    
    /*if( dldIOService->getObjectProperty() &&
       DLD_DEVICE_TYPE_REMOVABLE == dldIOService->getObjectProperty()->dataU.property->deviceType.type.major ){}*/
        
#if DBG
    /*
    if( dldIOService->getObjectProperty() &&
       DLD_DEVICE_TYPE_CD_DVD == (dldIOService->getObjectProperty())->dataU.property->deviceType.type.major ){
        
        assert( gCDDVDWhiteList );
        
        IOService* service;
        
        service = dldIOService->getSystemServiceRef();
        assert( service );
        if( service ){
            
            DldDVDWhiteList::AuthorizationState  mediaAuthState;
            
            gCDDVDWhiteList->authorizeMedia( service, &mediaAuthState );
            
            dldIOService->putSystemServiceRef( service );
        }
    }
     */
#endif
    
    ::DldAcquireResources( &param );
    paramWasAcquired = true;
#ifdef _DLD_MACOSX_CAWL
    if( dldVnode->isControlledByServiceCAWL() ){
        
        DldDiskCAWL::NotificationData  notificationData;
        
        notificationData.accessRequest.accessParam = &param;
        
        //
        // for CAWLed files the security is checked by the service
        //
        assert( gDiskCAWL );
        cawlError = gDiskCAWL->diskCawlNotification( dldVnode,
                                                     kDldCawlOpAccessRequest, // KAUTH callbacks can be considered as a user request to create a handle
                                                     (vfs_context_t)arg0,
                                                     &notificationData );
        assert( !cawlError );
        if( KERN_SUCCESS != cawlError ){
            
            DBG_PRINT_ERROR(( "diskCawlNotification() failed with an error(%u)\n", cawlError ));
            
            if( gSecuritySettings.securitySettings.disableAccessOnCawlError ){
                
                //
                // fail the request
                //
                param.output.access.result[ DldFullTypeFlavor ].disable = true;
                
                //
                // log it in any case as this is a serious error
                //
                param.output.access.result[ DldFullTypeFlavor ].log = true;
                
            } else {
                
                param.output.access.result[ DldFullTypeFlavor ].disable   = false;
                param.output.access.result[ DldMajorTypeFlavor ].disable  = false;
                param.output.access.result[ DldParentTypeFlavor ].disable = false;
                
                cawlError = KERN_SUCCESS;
            }
            
        } // end if( KERN_SUCCESS != cawlError )
        
    } else
#endif // #ifdef _DLD_MACOSX_CAWL
    {
        ::isAccessAllowed( &param );
    }
    
    disable = ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                param.output.access.result[ DldMajorTypeFlavor ].disable || 
                param.output.access.result[ DldParentTypeFlavor ].disable );
    
    log = ( param.output.access.result[ DldFullTypeFlavor ].log || 
            param.output.access.result[ DldMajorTypeFlavor ].log || 
            param.output.access.result[ DldParentTypeFlavor ].log );
    
    //
    // skip the log if the request is an advisory check, i.e. getattrlist_internal does this 
    // to infer the access rights set for a vnode
    //
    if( log && 0x0 != (action & KAUTH_VNODE_ACCESS) )
        log = false;
    

    if( log &&
        ( param.output.access.result[ DldFullTypeFlavor ].log ||
          param.output.access.result[ DldMajorTypeFlavor ].log ) ){
        
        kauth_cred_t        logCredential;
        
        //
        // the following logic is not easy to grasp but the sequence
        // follows the sequence of checks by isAccessAllowed for 
        // types flavor
        //
        if( param.loggingKauthCred[ DldFullTypeFlavor ] )
            logCredential = param.loggingKauthCred[ DldFullTypeFlavor ];
        else if( param.loggingKauthCred[ DldMajorTypeFlavor ] )
            logCredential = param.loggingKauthCred[ DldMajorTypeFlavor ];
        else
            logCredential = credential;
        
        //
        // the callback might be called for operations other than create/open,
        // so we accumulate all audit rights
        //
        dldVnode->addAuditRights( param.output.access.result[ DldFullTypeFlavor ].rightsToAudit | 
                                  param.output.access.result[ DldMajorTypeFlavor ].rightsToAudit );
        
        //
        // set user ID and process name that will be used for auditing IO operations on the file
        // during IO operations
        //      
        dldVnode->setAuditData( logCredential, vfs_context_pid( (vfs_context_t)arg0 ), param.wasProcessedAsEncrypted );
    }
    
    //
    // log the vnode access
    //
    if( log && gLog->isUserClientPresent() ){
        
        //
        // FSD level log
        //
        if( param.output.access.result[ DldFullTypeFlavor ].log ||
            param.output.access.result[ DldMajorTypeFlavor ].log ){
            
            DldDriverDataLogInt intData;
            DldDriverDataLog    data;
            bool                logDataValid;
            
            /*
            has been done and saved in DldVnode
             
            kauth_cred_t        logCredential;
            
            assert( paramWasAcquired );
            
            //
            // the following logic is not easy to grasp but the sequence
            // follows the sequence of checks by isAccessAllowed for 
            // types flavor
            //
            if( param.loggingKauthCred[ DldFullTypeFlavor ] )
                logCredential = param.loggingKauthCred[ DldFullTypeFlavor ];
            else if( param.loggingKauthCred[ DldMajorTypeFlavor ] )
                logCredential = param.loggingKauthCred[ DldMajorTypeFlavor ];
            else
                logCredential = credential;
             */
            
            //
            // the data structure will be initialized by a call to initVnodeLogData,
            // do not set any fields here - the data will be overwritten
            //
            
            intData.logDataSize = sizeof( data );
            intData.logData = &data;
            
            //
            // log at the FSD level
            //            
            logDataValid = _this->initVnodeLogData( DLD_KAUTH_SCOPE_VNODE_ID,
                                                    dldVnode,
                                                    vnode,
                                                    DldFileOperationOpen,
                                                    &param.dldRequestedAccess,
                                                    (-1), // use dldVnode's pid
                                                    NULL, // use dldVnode's credentials
                                                    ( param.output.access.result[ DldFullTypeFlavor ].disable || 
                                                      param.output.access.result[ DldMajorTypeFlavor ].disable ),
                                                    NULL,
                                                    &intData );
            
            assert( logDataValid );
            if( logDataValid ){
                
                //
                // set some data skipped by initVnodeLogData, not an excelent solution but a fast and easy one
                //
                assert( 0x0 != data.Header.deviceType.type.major );
                assert( 0x0 == data.Header.cawlError );
                
                data.Header.cawlError = cawlError;
                gLog->logData( &intData );
                
            } else {
                
                OSString* fileNameRef = dldVnode->getNameRef();
                assert( fileNameRef );
                
                DBG_PRINT_ERROR(("FSD log data creation has failed for %s\n", fileNameRef->getCStringNoCopy() ));
                
                fileNameRef->release();
                DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
            }
            
        }// end if( param.output.access.result[ DldFullTypeFlavor ].log ||

        
        //
        // log at the device level
        //
        if( param.output.access.result[ DldParentTypeFlavor ].log ){
            
            DldDriverDataLogInt intData;
            DldDriverDataLog    data;
            bool                logDataValid;
            kauth_cred_t        logCredential;
            
            assert( paramWasAcquired );
            
            logCredential = (param.loggingKauthCred[ DldParentTypeFlavor ]) ? param.loggingKauthCred[ DldParentTypeFlavor ] : credential;
            
            intData.logDataSize = sizeof( data );
            intData.logData = &data;
            
            logDataValid = dldIOService->initParentDeviceLogData( &param.dldRequestedAccess,
                                                                  DldFileOperationOpen,
                                                                  vfs_context_pid( (vfs_context_t)arg0 ),
                                                                  logCredential,
                                                                  param.output.access.result[ DldParentTypeFlavor ].disable,
                                                                  &intData );
            
            assert( logDataValid );
            if( logDataValid ){
                
                gLog->logData( &intData );
                
            } else {
                
                OSString* fileNameRef = dldVnode->getNameRef();
                assert( fileNameRef );
                
                DBG_PRINT_ERROR(("FSD log data creation has failed for %s\n", fileNameRef->getCStringNoCopy() ));
                
                fileNameRef->release();
                DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
            }
            
        }// end if( param.output.access.result[ DldParentTypeFlavor ].log )
        
    }// end if( log && gLog->isUserClientPresent() )
    
__exit:
    
    if( paramWasAcquired )
        ::DldReleaseResources( &param );
    
#if defined(DBG)
    assert( !param.resourcesAcquired || param.resourcesReleased );
#endif//DBG
    
    if( dldVnode )
        dldVnode->release();
    
    if( dldIOService )
        dldIOService->release();
    
    if( disable )
        return KAUTH_RESULT_DENY;
    else
        return KAUTH_RESULT_DEFER; // defer decision to other listeners
}

//--------------------------------------------------------------------

int
DldIOKitKAuthVnodeGate::DldFileopCallback(
    kauth_cred_t    credential, // reference to the actor's credentials
    void           *idata,      // cookie supplied when listener is registered
    kauth_action_t  action,     // requested action
    uintptr_t       arg0,       // the vnode in question or an old name for rename op
    uintptr_t       arg1,       // the vnode's name or a new name
    uintptr_t       arg2,       // NULL
    uintptr_t       arg3)       // NULL
{
    DldIOKitKAuthVnodeGate*    _this;
    vnode_t                    vnode = NULL;
    char*                      name = NULL;
    vfs_context_t              vfsContext = NULL;
    DldIOService*              dldIOService = NULL;
    DldRequestedAccess         dldRequestedAccess = { 0x0 };

#if DBG
    volatile int                stackSignature = 0xABCDEF;
#endif
    
    if( gGlobalSettings.doNotHookVFS )
        return KAUTH_RESULT_DEFER;
    
    //
    // this callback is used only to support OPEN-CLOSE notification
    // for the user part of the shadowing module
    //
    
    if( !gShadow->isUserClientPresent() )
        return KAUTH_RESULT_DEFER;
        
    if( action == KAUTH_FILEOP_OPEN || action == KAUTH_FILEOP_CLOSE ){
     
        vnode = (vnode_t)arg0;
        name = (char*)arg1;
        
    } else {
        
        //
        // the returned value is ignored
        //
        return KAUTH_RESULT_DEFER;
    }
    
    assert( preemption_enabled() );
    assert( vnode );
    
    enum vtype   vnodeType = vnode_vtype( vnode );
    if( VREG != vnodeType &&
        VDIR != vnodeType &&
        VBLK != vnodeType &&
        VCHR != vnodeType &&
        VLNK != vnodeType )
        return KAUTH_RESULT_DEFER;
    
    _this = (DldIOKitKAuthVnodeGate*)idata;
    
    //
    // we don't have an idea what is the actual context, but for open and close this should work fine
    //
    vfsContext = vfs_context_create( NULL );
    assert(vfsContext);
    if( NULL == vfsContext )
        goto __exit;

    
    DldIOVnode*  dldVnode;
    dldVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode( vnode );
    assert( dldVnode || vnode_isrecycled( vnode ) );
    if( !dldVnode )
        goto __exit;
    
    //
    // update the name, the function is idempotent in its behaviour so it is okay to call it multiple times
    //
    dldVnode->updateNameToValid();
    
    //
    // update the protected file, do this after the name has been updated
    //
    dldVnode->updateProtectedFile();
    
    //
    // a case when DirectoryService calls FS, DirectoryService/opendirectoryd is a process that
    // processes callbacks from the kernel for kauth_cred_getguid,  DirectoryService calls
    // identitysvc() that blocks until a callback from kauth_cred_getguid is placed in a queue,
    // fetches the callback structure and returns from the system call,
    // the DirectoryService process should run with superuser credentials,
    // TO DO - research teh posibility of optimization by remebering the process ID
    //
    if( DldIsProcessinWhiteList(vfsContext, (-1)) || DldIsDLServiceOrItsChild(vfs_context_pid(vfsContext)) )
        goto __exit;
    
    dldIOService = dldVnode->getReferencedService();
    if( !dldIOService )
        goto __exit;
    
    if( dldIOService->getObjectProperty() && 0x0 == dldVnode->auditData.deviceType.type.major )
        dldVnode->auditData.deviceType = dldIOService->getObjectProperty()->dataU.property->deviceType;
    
    if( KAUTH_FILEOP_OPEN == action ){
        
        //
        // a user is opening a file, check for shadowing
        //
        if( 0x0 == dldVnode->flags.shadowWrites ){
            
            DldAccessCheckParam param;
            bool shadow;
            
            bzero( &param, sizeof( param ) );
            
            param.userSelectionFlavor = kDefaultUserSelectionFlavor;
            param.aclType             = kDldAclTypeShadow;
            param.checkParentType     = true;
            param.dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_READ_DATA;// actually the value is ignored
            param.credential          = credential;
            param.service             = NULL;
            param.dldIOService        = dldIOService;
            
            _this->convertKauthToWin( &param, dldVnode, vnode );
            dldRequestedAccess = param.dldRequestedAccess;
            
            ::DldAcquireResources( &param );
            assert( 0xABCDEF == stackSignature );
            ::DldIsShadowedUser( &param );
            assert( 0xABCDEF == stackSignature );
            ::DldReleaseResources( &param );
            
            shadow = ( param.output.shadow.result[ DldFullTypeFlavor ].shadow || 
                       param.output.shadow.result[ DldMajorTypeFlavor ].shadow || 
                       param.output.shadow.result[ DldParentTypeFlavor ].shadow );
            
            if( shadow )
                dldVnode->flags.shadowWrites = 0x1;
            
        }// end if( 0x0 == dldVnode->flags.shadowWrites )

        //
        // fetch all log flags for the vnode
        //
        {
            DldAccessCheckParam param;
            
            bzero( &param, sizeof( param ) );
            
            param.userSelectionFlavor = kDefaultUserSelectionFlavor;
            param.aclType             = kDldAclTypeSecurity;
            param.checkParentType     = false; // skip parents check, we are after the audit only
            param.checkForLogOnly     = true;  // do not check for access
            param.dldRequestedAccess.winRequestedAccess = dldVnode->getAccessMask(); // get all possible audit flags
            param.credential          = credential;
            param.service             = NULL;
            param.dldIOService        = dldIOService;
#if defined(LOG_ACCESS)
            param.sourceFunction      = __PRETTY_FUNCTION__;
            param.sourceFile          = __FILE__;
            param.sourceLine          = __LINE__;
            param.dldVnode            = dldVnode;
#endif//#if defined(LOG_ACCESS)
            
            ::DldAcquireResources( &param );
            assert( 0xABCDEF == stackSignature );
            {
                ::isAccessAllowed( &param );
                
                bool log;
                log = ( param.output.access.result[ DldFullTypeFlavor ].log || 
                        param.output.access.result[ DldMajorTypeFlavor ].log || 
                        param.output.access.result[ DldParentTypeFlavor ].log );
                
                
                if( log ){
                    
                    kauth_cred_t        logCredential;
                    
                    //
                    // the following logic is not easy to grasp but the sequence
                    // follows the sequence of checks by isAccessAllowed for 
                    // types flavor
                    //
                    if( param.loggingKauthCred[ DldFullTypeFlavor ] )
                        logCredential = param.loggingKauthCred[ DldFullTypeFlavor ];
                    else if( param.loggingKauthCred[ DldMajorTypeFlavor ] )
                        logCredential = param.loggingKauthCred[ DldMajorTypeFlavor ];
                    else
                        logCredential = credential;
                    
                    //
                    // the callback might be called for operations other than create/open,
                    // so we accumulate all audit rights
                    //
                    dldVnode->addAuditRights( param.output.access.result[ DldFullTypeFlavor ].rightsToAudit | 
                                              param.output.access.result[ DldMajorTypeFlavor ].rightsToAudit );
                    
                    //
                    // set user ID and process name that will be used for auditing IO operations on the file
                    // during IO operations
                    //      
                    dldVnode->setAuditData( logCredential, vfs_context_pid( (vfs_context_t)arg0 ), param.wasProcessedAsEncrypted );
                }
                
            }
            assert( 0xABCDEF == stackSignature );
            ::DldReleaseResources( &param );
            
        }
        
#ifdef _DLD_MACOSX_CAWL
        
        //
        // whether the file is CAWLed are checked in the hooked VFS lookup and create routines as
        // it is too late to check here, it is impossible to substitute a covering vnode
        // in the KAUTH callbacks, see DldIOVnode::defineStatusForCAWL for reference
        //
        
        if( dldVnode->isControlledByServiceCAWL() ){
            
            DldAccessCheckParam param;
            
            bzero( &param, sizeof( param ) );
            
            param.userSelectionFlavor = kDefaultUserSelectionFlavor;
            param.aclType             = kDldAclTypeDiskCAWL; // actually ignored
            param.checkParentType     = true;
            param.dldRequestedAccess.kauthRequestedAccess = (KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA); // actually should be ignorred by the service
            param.credential          = credential;
            param.service             = NULL;
            param.dldIOService        = dldIOService;
            
            _this->convertKauthToWin( &param, dldVnode, vnode );
            dldRequestedAccess = param.dldRequestedAccess;
            
            //
            // notify the service
            //
            assert( gDiskCAWL );
            ::DldAcquireResources( &param );
            {
                DldDiskCAWL::NotificationData  notificationData;
                
                notificationData.open.accessParam = &param;
                
                gDiskCAWL->diskCawlNotification( dldVnode,
                                                 kDldCawlOpFileOpen,
                                                 vfsContext,
                                                 &notificationData );
            }
            ::DldReleaseResources( &param );
        }
#endif // #ifdef _DLD_MACOSX_CAWL
        
    } else if( KAUTH_FILEOP_CLOSE == action ){
        
#ifdef _DLD_MACOSX_CAWL
        if( dldVnode->isControlledByServiceCAWL() ){
            
            //
            // notify the service
            //
            assert( gDiskCAWL );
            gDiskCAWL->diskCawlNotification( dldVnode,
                                             kDldCawlOpFileClose,
                                             vfsContext,
                                             NULL );
        }
#endif // #ifdef _DLD_MACOSX_CAWL
            
    }// end if( KAUTH_FILEOP_CLOSE == action )
    
    //
    // if the vnode is not shadowed then there is no reason
    // to continue processing here as the log is populated
    // by DldVnodeAuthorizeCallback
    //
    if( 0x0 == dldVnode->flags.shadowWrites )
        goto __exit;
    
    //
    // create a log structure to send it to the user part of the shadowing module
    //
    DldDriverDataLogInt intData;
    DldDriverDataLog    data;
    
    //
    // in release only the open operation requires
    // an initilized log structure
    //
    if( KAUTH_FILEOP_OPEN == action ){
        
        intData.logDataSize = sizeof( data );
        intData.logData = &data;
        
        _this->initVnodeLogData( DLD_KAUTH_SCOPE_FILEOP_ID,
                                dldVnode,
                                vnode,
                                DldFileOperationOpen,
                                &dldRequestedAccess,
                                proc_pid( current_proc() ),
                                credential,
                                false,// the value should be ignored for this scope
                                name,
                                &intData );
        
        assert( 0x0 != data.Header.deviceType.type.major );
        assert( 0x0 == data.Header.cawlError );
        
    }//end if( KAUTH_FILEOP_OPEN == action )
    
    //
    // shadow the operation
    //
    DldCommonShadowParams commonParams;
    UInt32                completionEvent;
    IOReturn              shadowCompletionRC;
    bool                  shadowed;
    
#if defined( DBG )
    shadowCompletionRC = kIOReturnInvalid;
#endif//DBG
    
    bzero( &commonParams, sizeof( commonParams ) );
    
    commonParams.operationID        = gShadow->generateUniqueID();
    commonParams.completionEvent    = &completionEvent;
    commonParams.shadowCompletionRC = &shadowCompletionRC;
    
    switch( action ){
            
        case KAUTH_FILEOP_OPEN:
            shadowed = gShadow->shadowFileOpen( dldVnode, &intData, &commonParams );
            break;
            
        case KAUTH_FILEOP_CLOSE:
            shadowed = gShadow->shadowFileClose( dldVnode, &commonParams );
            break;
            
        default:
            shadowed = false;
            panic("unknown action");
            break;
    }
    
    assert( shadowed );
    
#if defined( DBG )
    //
    // only for the test purposes, remove it when test is completed
    //
    //gLog->logData( &intData );
#endif//DBG
    
    //
    // actually it is profitable to wait for shadow completion
    // to not flood the shadowing subsystem with asynchronous 
    // OPEN-CLOSE processing, but potentially it might be made
    // asynchronous by coping DldDriverDataLog in the shadow 
    // kernel-to-kernel communication queue, if the asynchrnous
    // processing will be adopted the commonParams variable
    // must be removed as it is allocated on the stack
    //
    if( shadowed ){
        
        assert( dldVnode );
        assert( dldVnode->flags.shadowWrites );
        assert( &completionEvent == commonParams.completionEvent );
        
        //
        // wait for shadow completion
        //
        DldWaitForNotificationEvent( &completionEvent );
        
        //
        // at this point the *shadowCompletionRC value is valid
        //
        assert( kIOReturnInvalid != shadowCompletionRC );
        
        //
        // shadow the operation result, actually a fake as operation had been completed
        // successfully before the File Operation callback was called
        //
        DldShadowOperationCompletion  comp;
        
        bzero( &comp, sizeof( comp ) );
        comp.operationID = commonParams.operationID;
        comp.retval      = KERN_SUCCESS;
        
        gShadow->shadowOperationCompletion( dldVnode, &comp );
        
    }// end if( shadowed )
    
    
__exit:
    
    if( vfsContext )
        vfs_context_rele( vfsContext );
    
    if( dldVnode )
        dldVnode->release();
    
    if( dldIOService )
        dldIOService->release();
        
    //
    // the returned value is ignored
    //
    return KAUTH_RESULT_DEFER;
}

//--------------------------------------------------------------------

int DldIOKitKAuthVnodeGate::DldFileopCallbackWrap(
    __in kauth_cred_t    credential, // reference to the actor's credentials
    __in kauth_action_t  action,     // requested action
    __in vnode_t         vn          // the vnode
    )
{
    assert( preemption_enabled() );
    
    return DldIOKitKAuthVnodeGate::DldFileopCallback( credential,
                                                      this,
                                                      action,
                                                      (uintptr_t)vn,
                                                      0, 0, 0 );
}

//--------------------------------------------------------------------

//
// the caller must set intData->logDataSize to the size the allocated *intData->logData,
// the caller must allocate a room for *intData->logData of at least sizeof( *intData->logData ),
// on return the function initializes the fields for *intData->logData
// and sets intData->logDataSize according to the length of the header and the vnode name,
// so the caller must not consider this field as a real full size of the 
// intData->logData memory which he has allocated before calling this function
//

bool DldIOKitKAuthVnodeGate::initVnodeLogData(
    __in     UInt32                 scopeID,    // DLD_KAUTH_SCOPE_VNODE_ID or DLD_KAUTH_SCOPE_FILEOP_ID
    __in     DldIOVnode*            dldVnode,
    __in     vnode_t                vnode,
    __in     DldFileOperation       operation,
    __in     DldRequestedAccess*    action,
    __in_opt int32_t                pid,        // BSD process ID, (-1) if not provided
    __in_opt kauth_cred_t           credential, // NULL if not provided
    __in     bool                   accessDisabled,
    __in_opt char*                  name,       // optional, dldVnode is a primary source for the name
    __inout  DldDriverDataLogInt*   intData     // at least sizeof( *intData->logData )
    )
{
    vm_size_t            size;
    vm_size_t            nameBuffLength;
    DldDriverDataLog*    data = intData->logData;
    vm_size_t            name_len;
    
    assert( preemption_enabled() );
    assert( DLD_KAUTH_SCOPE_VNODE_ID == scopeID || DLD_KAUTH_SCOPE_FILEOP_ID == scopeID );
    assert( intData->logDataSize > (size_t)&(((DldDriverDataLog*)0)->Fsd.path[0]) );
    assert( dldVnode->vnode == vnode );
    
    size = (vm_size_t)&(((DldDriverDataLog*)0)->Fsd.path[0]);
    nameBuffLength = intData->logDataSize - size;
    name_len = nameBuffLength;
    
    bzero( data, size );
    
    data->Header.type = DLD_LOG_TYPE_FSD;
    data->Fsd.scopeID = scopeID;
    
    if( credential ){
        
        //
        // a user's GUID,
        // kauth_cred_getguid can return an error, disregard it
        //
        kauth_cred_getguid( credential, &data->Header.userGuid );
        data->Header.userUid = kauth_cred_getuid( credential );
        data->Header.userGid = kauth_cred_getgid( credential );
        
    } else {
        
        bcopy( &dldVnode->auditData.userID.guid, &data->Header.userGuid, sizeof( data->Header.userGuid ) );
        data->Header.userUid = dldVnode->auditData.userID.uid;
        data->Header.userGid = dldVnode->auditData.userID.gid;
    }

    data->Header.deviceType     = dldVnode->auditData.deviceType;
    
    data->Header.action         = *action;
    data->Header.isDisabledByDL = accessDisabled;
    
    data->Fsd.v_type            = dldVnode->v_type;
    data->Fsd.operation         = operation;
    
    if( dldVnode->auditData.auditAsEncrypted ){
        
        data->Header.isEncrypted = true;
        
        //
        // convert to encrypted rights and preserve some rights that do not map to encrypted one but provides
        // a valuable audit information
        //
        data->Header.action.winRequestedAccess = ConvertWinRightsToWinEncryptedRights( data->Header.action.winRequestedAccess ) |
                                                 ( data->Header.action.winRequestedAccess & ( DEVICE_RENAME |
                                                                                              DEVICE_DELETE |
                                                                                              DEVICE_DIR_CREATE |
                                                                                              DEVICE_DIR_LIST));
    }
    
    if( (-1) != pid ){
        
        //
        // a process' PID and a short name
        //
        data->Fsd.pid = pid;
        proc_name( pid, data->Fsd.p_comm, sizeof( data->Fsd.p_comm ) );
        
    } else {
        
        const OSSymbol* processName = dldVnode->getProcessAuditNameRef();
        assert( processName );
        if( processName );
        {
            data->Fsd.pid = dldVnode->auditData.process.pid;
            bcopy( processName->getCStringNoCopy(), data->Fsd.p_comm,
                   min( processName->getLength()*sizeof(char), sizeof(data->Fsd.p_comm) - sizeof('\0') ) );
            
            processName->release();
            processName = NULL;
        }
    }
    
    //
    // as the logging subsystem doesn't save the vnode state ( open-close-reclaim balance )
    // the name should be reported with each log entry
    //
    assert( name_len == nameBuffLength );
    if( dldVnode->isNameVaid() ){
        
        OSString*    fileNameRef = dldVnode->getNameRef();
        
        assert( fileNameRef );
        assert( name_len > sizeof( '\0' ) );
        
        name_len = min( fileNameRef->getLength(), name_len - sizeof( '\0' ) );
        memcpy( data->Fsd.path, fileNameRef->getCStringNoCopy(), name_len );
        
        //
        // set rhe terminating zero
        //
        data->Fsd.path[ name_len ] = '\0';
        
        //
        // account for the terminating zero
        //
        name_len += sizeof( '\0' );
        
        fileNameRef->release();
        DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
    
    } else if( name ){
        
        name_len = min( name_len, ((UInt32)strlen( name )+1) );
        memcpy( data->Fsd.path, name, name_len );
        
    } else if ( name_len >= MAXPATHLEN && vn_getpath( vnode, data->Fsd.path, (int*)&name_len) == 0 ){
        
        // nothing to do
        
    } else if( name_len >= sizeof( '\0' ) ){
        
        data->Fsd.path[0] = '\0';
        name_len = 1;
        
    } else {
        
        name_len = 0x0;
    }
    
    assert( name_len <= MAXPATHLEN );
    size += name_len;
    
    //
    // set the actual size of the valid data
    //
    assert( size <= intData->logDataSize );// check for overwrite
    intData->logDataSize = size;
    
    return true;
}

//--------------------------------------------------------------------

void DldIOKitKAuthVnodeGate::free()
{
    this->UnregisterVnodeScopeCallback();
    assert( NULL == this->VnodeListener );
    
    this->UnregisterFileopCallback();
    assert( NULL == this->FileopListener );
    
    super::free();
}

//--------------------------------------------------------------------

DldIOKitKAuthVnodeGate* DldIOKitKAuthVnodeGate::withCallbackRegistration()
/*
 the caller must call the release() function for the returned object when it is not longer needed
 */
{
    IOReturn                   RC;
    DldIOKitKAuthVnodeGate*    pKAuthVnodeGate;
    
    pKAuthVnodeGate = new DldIOKitKAuthVnodeGate();
    assert( pKAuthVnodeGate );
    if( !pKAuthVnodeGate ){
        
        DBG_PRINT_ERROR( ( "DldIOKitKAuthVnodeGate::withCallbackRegistration DldIOKitKAuthVnode allocation failed\n" ) );
        return NULL;
    }
    
    //
    // IOKit base classes initialization
    //
    if( !pKAuthVnodeGate->init() ){
        
        DBG_PRINT_ERROR( ( "DldIOKitKAuthVnodeGate::withCallbackRegistration init() failed\n" ) );
        pKAuthVnodeGate->release();
        return NULL;
    }
    
    //
    // register the callback, it will be active just after registration!
    //
    RC = pKAuthVnodeGate->RegisterFileopCallback();
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR( ( "pKAuthVnodeGate->RegisterFileopCallback() failed with the 0x%X error\n", RC ) );
        pKAuthVnodeGate->release();
        return NULL;
    }
    
    //
    // register the callback, it will be active just after registration!
    //
    RC = pKAuthVnodeGate->RegisterVnodeScopeCallback();
    assert( kIOReturnSuccess == RC );
    if( kIOReturnSuccess != RC ){
        
        DBG_PRINT_ERROR( ( "pKAuthVnodeGate->RegisterVnodeScopeCallback() failed with the 0x%X error\n", RC ) );
        pKAuthVnodeGate->release();
        return NULL;
    }
    
    return pKAuthVnodeGate;
}

//--------------------------------------------------------------------

void
DldIOKitKAuthVnodeGate::DldLogVnodeIoOperation(
    __in vnode_t vnode,
    __in DldFileOperation  ioOperation,
    __in dld_classic_rights_t  winRequestedAccess
    )
{
    //
    // the open operations are logged in the KAUTH callbacks
    //
    assert( DldFileOperationUnknown != ioOperation && DldFileOperationOpen != ioOperation );
    
    if( (! gLog->isUserClientPresent()) || 0x0 == winRequestedAccess )
        return;
    
    
    DldIOVnode*  dldVnode;
    dldVnode = DldVnodeHashTable::sVnodesHashTable->RetrieveReferencedIOVnodByBSDVnode( vnode );
    
    //
    // FSD level log
    //
    if( ! dldVnode )
        return;
    
    
    if( 0x0 != (dldVnode->auditData.rightsToAudit & winRequestedAccess) ){
        
        DldDriverDataLogInt intData;
        DldDriverDataLog    data;
        bool                logDataValid;
        DldRequestedAccess  requestedAccess;
        
        requestedAccess.winRequestedAccess = (dldVnode->auditData.rightsToAudit & winRequestedAccess);
        requestedAccess.kauthRequestedAccess = 0x0;
        
        //
        // the data structure will be initialized by a call to initVnodeLogData,
        // do not set any fields here - the data will be overwritten
        //
        
        intData.logDataSize = sizeof( data );
        intData.logData = &data;
        
        //
        // log at the FSD level
        //            
        logDataValid = gVnodeGate->initVnodeLogData( DLD_KAUTH_SCOPE_VNODE_ID,
                                                     dldVnode,
                                                     vnode,
                                                     ioOperation,
                                                     &requestedAccess,
                                                     (-1),  // use the pid from DldVnode
                                                     NULL,  // use the User IDs from DldVnode
                                                     false, // the request is allowedss
                                                     NULL,
                                                     &intData );
        
        assert( logDataValid );
        if( logDataValid ){
            
            //
            // set some data skipped by initVnodeLogData, not an excelent solution but a fast and easy one
            //
            assert( 0x0 != data.Header.deviceType.type.major );
            assert( 0x0 == data.Header.cawlError );
            assert( 0x0 != dldVnode->auditData.deviceType.type.major );
            
            //
            // send to the service ( actually place in the circular queue that is drained by the service )
            //
            gLog->logData( &intData );
            
        } else {
            
            OSString* fileNameRef = dldVnode->getNameRef();
            assert( fileNameRef );
            
            DBG_PRINT_ERROR(("FSD log data creation has failed for %s\n", fileNameRef->getCStringNoCopy() ));
            
            fileNameRef->release();
            DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
        }
    } // end if( 0x0 != (dldVnode->auditData.rightsToAudit & ...
    
    dldVnode->release();
}

//--------------------------------------------------------------------


