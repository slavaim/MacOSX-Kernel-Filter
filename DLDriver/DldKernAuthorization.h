/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDKERNAUTHORIZATION_H
#define _DLDKERNAUTHORIZATION_H

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldIOService.h"
#include "DldCommon.h"
#include "DldKauthCredArray.h"
#include "DldIOVnode.h"
#include "DldAclObject.h"


//--------------------------------------------------------------------

extern int	DldKauthWellknownGuid(guid_t *_guid);
#define KAUTH_WKG_NOT		0	/* not a well-known GUID */
#define KAUTH_WKG_OWNER		1
#define KAUTH_WKG_GROUP		2
#define KAUTH_WKG_NOBODY	3
#define KAUTH_WKG_EVERYBODY	4

//--------------------------------------------------------------------

//
// the DldKauthAclEvaluate() function is overloaded
//
int
DldKauthAclEvaluate(
    __in    kauth_cred_t cred,
    __inout kauth_acl_eval_t eval
                    );

void
DldSetDefaultSecuritySettings();

//--------------------------------------------------------------------

typedef enum _DldTypeFlavor{
    DldFullTypeFlavor = 0x0,
    DldMajorTypeFlavor,
    DldParentTypeFlavor,
    
    //
    // always the last
    //
    DldMaxTypeFlavor
} DldTypeFlavor;

//--------------------------------------------------------------------

typedef struct _DldAccessCheckOut{
    bool disable;
    bool log;
    dld_classic_rights_t rightsToAudit;
} DldAccessCheckOut;

typedef struct _DldShadowCheckOut{
    bool shadow;
} DldShadowCheckOut;

typedef struct _DldDiskCAWLCheckOut{
    bool checkByCAWL;
} DldDiskCAWLCheckOut;

typedef enum{
    kDefaultUserSelectionFlavor = 0x0,
    kActiveUserSelectionFlavor,
    kAllLoggedUsersSelectionFlavor,
    
    //
    // always the last
    //
    kMaxUserSelectionFlavor
} DldUserSelectionFlavor;

typedef struct _DldAccessCheckParam{
    
    //
    // input parameters, initialized by a caller
    //
    
    __in DldAclType               aclType;
    
    //
    // might be set to DLD_DEVICE_TYPE_UNKNOWN if the device pointer
    // ( service or dldIOService ) is provided, be VERY CAUTIOUS
    // in providing directly the device type instead of providing
    // a service object as in this case the PnP state of the device
    // is unknown and this might result in interfering with the device's
    // initialization precluding the device stack from starting,
    //
    // if set to DLD_DEVICE_TYPE_UNKNOWN by a caller then set by a callee
    //
    __inout DldDeviceType         deviceType;
    
    //
    // it is advisible that either service or dldIOService must be non null or both,
    // Why it is impotant - without service object the property object can't be found
    // so the white list setting can't be applied
    //
    __in_opt IOService*           service;
    __in_opt DldIOService*        dldIOService;
    
    //
    // an optional pointer to a child that either issued or is processing request,
    // i.e. this is a device where the request is actually processing at the moment,
    // if NULL then dldIOService is a request owner
    //
    __in_opt DldIOService*        dldIOServiceRequestOwner;
    
    //__in kauth_ace_rights_t       requestedAccess;
    
    __in DldRequestedAccess       dldRequestedAccess;
    
    //
    // optional, if NULL the current process credential is used
    // if the userSelectionFlavor value is kDefaultUserSelectionFlavor
    //
    __in_opt kauth_cred_t         credential;
    
    //
    // checkParentType is valid only for kDldAclTypeSecurity type
    //
    __in  bool                    checkParentType;
    
    //
    // checkForLogOnly is valid only for kDldAclTypeSecurity type
    //
    __in  bool                    checkForLogOnly;
    
    //
    // if true then the call is made while traversing the parent properties
    //
    __in  bool                    calledOnTopDownTraversing;
    
    //
    // if true a call has been made to a user IOUserClient derived object
    //
    __in bool                     userClient;
    
    __in DldUserSelectionFlavor   userSelectionFlavor;
    
    //
    // if logged user is used for checks then a referenced
    // entry for an active user is returned, may be NULL
    //
    __out DldKauthCredEntry*      activeKauthCredEntry;
    
    //
    // if access has been disabled or shadowed an entry for a user
    // to whom access was denied is returned, may be NULL or
    // the same as activeKauthCredEntry,
    // if the access is diabled and this field is NULL use
    // the activeKauthCredEntry value instead,
    // if the access is denied the isAccessAllowed returns immediatelly
    // so there is only one credentials for denied access
    //
    __out DldKauthCredEntry*      decisionKauthCredEntry;
    
    //
    // the same as decisionKauthCredEntry but for the log,
    // but in the case of logging there is an entry for each 
    // type flavor as successful requests are also being logged
    // at different levels
    //
    __out kauth_cred_t            loggingKauthCred[ DldMaxTypeFlavor ];
    
    //
    // output parameters, set by a callee
    //
    
    __out kauth_ace_rights_t      parentAccess;
    
    //
    // set to true if the encrypted settings were applied
    //
    __out bool                    wasProcessedAsEncrypted;
    
    
    union{
        
        struct{
            
            __out DldAccessCheckOut   result[ DldMaxTypeFlavor ];
            
        } access;
        
        struct{
            
            __out DldShadowCheckOut   result[ DldMaxTypeFlavor ];
            
        } shadow;
        
        struct{
            
            __out DldDiskCAWLCheckOut   result[ DldMaxTypeFlavor ];
            
        } diskCAWL;
        
    } output;
    
#if defined(DBG)
    bool                         resourcesReleased;
    bool                         resourcesAcquired;
#endif//DBG
    
#if defined(LOG_ACCESS)
    //
    // the sourceFile and sourceLine fields are used to
    // log the subsystem calling the isAccessAllowed function
    //
    __in const char*             sourceFunction;
    __in const char*             sourceFile;
    __in unsigned int            sourceLine;
    __in_opt DldIOVnode*         dldVnode; 
#endif//#if defined(LOG_ACCESS)
    
} DldAccessCheckParam;

//--------------------------------------------------------------------

//
// this is a rule - 
// - DldAcquireResources( param ) must be called for every param structure provided
// to isAccessAllowed()
// - DldReleaseResources( param ) must be called after isAccessAllowed or isShadowedUser returned
// and the param structure is no longer needed or is being reused
//
void
isAccessAllowed( __inout DldAccessCheckParam* param );

void
DldIsShadowedUser( __inout DldAccessCheckParam* param );

void
DldIsDiskCawledUser( __inout DldAccessCheckParam* param );

void
DldAcquireResources( __inout DldAccessCheckParam* param );

void
DldReleaseResources( __inout DldAccessCheckParam* param );

//--------------------------------------------------------------------

class DldTypePermissionsEntry: public OSObject{
    
    OSDeclareDefaultStructors( DldTypePermissionsEntry )
    
private:
    
    //
    // the array contains DldAclObjects for the same major type but different minor types
    //
    OSArray*        acls;
    
    //
    // a lock to protect access to the array
    //
    IORWLock*       rwLock;
    
    //
    // returns a referenced object, the caller must release it
    //
    DldAclObject* getReferencedAclByTypeWoLock( __in DldDeviceType  type,
                                               __out_opt unsigned int* indx );
    
protected:
    
    virtual bool init();
    virtual void free();
    
public:
    
    static DldTypePermissionsEntry* newEntry();
    
    //
    // sets the new acl object for the type and release the old one,
    // the acl object is retained
    //
    bool       setAcl( __in DldAclObject* newAcl );
    
    //
    // sets the NULL ACL, i.e. an ACL that allows acces to everyone
    //
    bool       setNullAcl( __in DldDeviceType deviceType );
    
    //
    // removes all ACLs thus granting access to everyone
    //
    void       deleteAllACLs();
    
    //
    // returns a referenced object, the caller must release it
    //
    DldAclObject* getReferencedAclByType( __in DldDeviceType  type );
};

//--------------------------------------------------------------------

typedef struct _DldAccessEval{
    
    __in     DldDeviceType        type;
    __in     dld_classic_rights_t ae_requested;
    __in     DldTypeFlavor        typeFlavor;
    
    //
    // an optional parameter, if not provided the current process
    // credentials will be used
    //
    __in_opt kauth_cred_t         cred;
    
    //
    // the allowed values for the ae_result field is
    // KAUTH_RESULT_ALLOW	(1)
    // KAUTH_RESULT_DENY	(2)
    // KAUTH_RESULT_DEFER   (3)
    //
    __out    int			      ae_result;
    __out    dld_classic_rights_t ae_residual;
    
    __out    bool                 null_acl_found;
    
#if DBG
    unsigned int                  quirkLine;
#endif
    
} DldAccessEval;

//
// the DldKauthAclEvaluate() function is overloaded
//
int
DldKauthAclEvaluate( __inout DldAccessEval* dldEval,
                     __in DldAclObject* acl );

//--------------------------------------------------------------------

class DldTypePermissionsArray: public OSObject
{
    OSDeclareDefaultStructors( DldTypePermissionsArray )
    
protected:
    
    virtual bool init();
    virtual void free();
    
private:
    
    //
    // the array by major type
    //
    DldTypePermissionsEntry* array[ DLD_DEVICE_TYPE_MAX ];
    
public:
    
    static DldTypePermissionsArray* newPermissionsArray();
    
    //
    // returns a referenced Acl object or NULL if an object doesn't exist
    //
    DldAclObject* geReferencedAclByType( __in DldDeviceType  type );
    
    //
    // saves a new acl object and removed the old for the same type,
    // the acl object is retained
    //
    bool setAcl( __in DldAclObject* acl );
    
    //
    // sets the NULL ACL, i.e. an ACL that allows acces to everyone
    //
    bool setNullAcl( __in DldDeviceType deviceType );
    
    //
    // removes all ACLs thus granting access to everyone
    //
    void deleteAllACLs();
    
    void isAccessAllowed( __inout DldAccessEval* eval );
                          
    
};

//--------------------------------------------------------------------

typedef struct _DldKernelSecuritySettings{
    DldQuirksOfSecuritySettings        securitySettings;
} DldKernelSecuritySettings;

    
//--------------------------------------------------------------------

extern DldTypePermissionsArray*   gArrayOfAclArrays[ kDldAclTypeMax ];
extern DldKernelSecuritySettings  gSecuritySettings;
extern Boolean                    gClientWasAbnormallyTerminated;

//--------------------------------------------------------------------

//
// returns a non referenced pointer to DldTypePermissionsArray object,
// there is no need to reference the object as they never destoyed,
// except the driver unloading which is synchronized with the device access
//
__inline
DldTypePermissionsArray*
DldGetPermissionsArrayByAclType( __in DldAclType  aclType )
{
    assert( aclType < sizeof( gArrayOfAclArrays )/sizeof( gArrayOfAclArrays[0] ) );
    return gArrayOfAclArrays[ aclType ];
}

IOReturn
DldAllocateArrayOfAclArrays();

void
DldDeleteArrayOfAclArrays();

//
// the caller must call kauth_cred_unref()
//
kauth_cred_t
DldGetRealUserCredReferenceByEffectiveUser( __in kauth_cred_t effectiveUserCredential );

dld_classic_rights_t
ConvertWinRightsToWinEncryptedRights( __in dld_classic_rights_t    rights );

//--------------------------------------------------------------------

#endif//_DLDKERNAUTHORIZATION_H