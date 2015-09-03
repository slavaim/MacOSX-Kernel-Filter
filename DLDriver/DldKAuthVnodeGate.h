/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDKAUTHVNODE_H
#define _DLDKAUTHVNODE_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldIOVnode.h"
#include "DldKernAuthorization.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

#define NOT_USED /**/

class DldIOKitKAuthVnodeGate : public OSObject
{
    OSDeclareDefaultStructors( DldIOKitKAuthVnodeGate )
    
private:
    
    //
    // the callback is called when a vnode is being created or have been created depending on the type of a file open,
    // also the callback s called when the vnode is being accessed
    //
    static int DldVnodeAuthorizeCallback( kauth_cred_t    credential, // reference to the actor's credentials
                                          void           *idata,      // cookie supplied when listener is registered
                                          kauth_action_t  action,     // requested action
                                          uintptr_t       arg0,       // the VFS context
                                          uintptr_t       arg1,       // the vnode in question
                                          uintptr_t       arg2,       // parent vnode, or NULL
                                          uintptr_t       arg3);      // pointer to an errno value
    
    //
    // a notification callback for the open, close, rename, link and execute operations, the returned value is ignored
    //
    static int DldFileopCallback( kauth_cred_t    credential, // reference to the actor's credentials
                                  void           *idata,      // cookie supplied when listener is registered
                                  kauth_action_t  action,     // requested action
                                  uintptr_t       arg0,       // the VFS context
                                  uintptr_t       arg1,       // the vnode in question
                                  uintptr_t       arg2,       // parent vnode, or NULL
                                  uintptr_t       arg3);      // pointer to an errno value
    
    
    bool initVnodeLogData(
                          __in     UInt32               scopeID,    // DLD_KAUTH_SCOPE_VNODE_ID or DLD_KAUTH_SCOPE_FILEOP_ID
                          __in     DldIOVnode*          dldVnode,
                          __in     vnode_t              vnode,
                          __in     DldFileOperation     operation,
                          __in     DldRequestedAccess*  action,
                          __in_opt int32_t              pid,        // BSD process ID, (-1) if not provided
                          __in_opt kauth_cred_t         credential, // NULL if not provided
                          __in     bool                 accessDisabled,
                          __in_opt char*                name,       // optional, dldVnode is a primary source for the name
                          __inout  DldDriverDataLogInt* intData     // at least sizeof( *intData->logData )
                          );
    
    void convertKauthToWin( __in DldAccessCheckParam* param,
                            __in DldIOVnode*  dldVnode,
                            __in vnode_t      vnode );
    
    //
    // KAUTH_SCOPE_VNODE listener, used for the acess permissions check
    //
    kauth_listener_t                 VnodeListener;
    
    //
    // KAUTH_SCOPE_FILEOP listener, used for open-close logging,
    // not used anymore, see comments in RegisterFileopCallback and
    // DldFsdCloseHook
    //
    kauth_listener_t                 FileopListener NOT_USED;
    
    //
    // a supporting class, use carefully as there is no connection with hooked object
    //
    DldHookerCommonClass             commonHooker;
    
protected:

    virtual   void free();
    
public:
    
    virtual IOReturn  RegisterVnodeScopeCallback(void);
    virtual void      UnregisterVnodeScopeCallback(void);
    
    virtual IOReturn  RegisterFileopCallback(void);
    virtual void      UnregisterFileopCallback(void);
    
    int DldFileopCallbackWrap( __in kauth_cred_t    credential, // reference to the actor's credentials
                               __in kauth_action_t  action,     // requested action
                               __in vnode_t         vn       // the vnode
                             );
    
    static DldIOKitKAuthVnodeGate*  withCallbackRegistration();
    
    static void DldLogVnodeIoOperation( __in vnode_t vnode,
                                        __in DldFileOperation  ioOperation,
                                        __in dld_classic_rights_t  requestedAccess );
    
};

//--------------------------------------------------------------------

extern DldIOKitKAuthVnodeGate*     gVnodeGate;

//--------------------------------------------------------------------

dld_classic_rights_t
ConvertWinEncryptedRightsToWinRights(
    __in dld_classic_rights_t    rights
    );

//--------------------------------------------------------------------

#endif//_DLDKAUTHVNODE_H
