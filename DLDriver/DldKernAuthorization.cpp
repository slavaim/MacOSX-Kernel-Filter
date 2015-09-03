/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */


#include "DldKernAuthorization.h"
#include "DldIOService.h"
#include <sys/vm.h>
#include <sys/proc.h>
#include <IOKit/IOBSD.h>
#include <AvailabilityMacros.h>
#include "DldEncryptionProviders.h"

//--------------------------------------------------------------------

static
void
convertKauthToWin( __in DldAccessCheckParam* param );

//--------------------------------------------------------------------

void
DldSetDefaultSecuritySettings()
{
    gSecuritySettings.securitySettings.controlUsbMassStorage = true;
    gSecuritySettings.securitySettings.controlUsbBluetooth = true;
    gSecuritySettings.securitySettings.controlUsbHid = true;
}

//--------------------------------------------------------------------

IOReturn
DldAllocateArrayOfAclArrays()
{
    for( unsigned int i = 0x0; i < sizeof( gArrayOfAclArrays )/sizeof( gArrayOfAclArrays[0] ); ++i ){
        
        assert( !gArrayOfAclArrays[ i ] );
        
        gArrayOfAclArrays[ i ] = DldTypePermissionsArray::newPermissionsArray();
        assert( gArrayOfAclArrays[ i ] );
        if( !gArrayOfAclArrays[ i ] )
            return kIOReturnNoMemory;
        
    }// end for
    
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

void
DldDeleteArrayOfAclArrays()
{
    for( unsigned int i = 0x0; i < sizeof( gArrayOfAclArrays )/sizeof( gArrayOfAclArrays[0] ); ++i ){
        
        if( !gArrayOfAclArrays[ i ] )
            continue;
        
        gArrayOfAclArrays[ i ]->release();
        gArrayOfAclArrays[ i ] = NULL;
        
    }// end for
}

//--------------------------------------------------------------------

#define DLD_KAUTH_DEBUG_ENABLE

static SInt32    gKauthLogIndx = 0x0;

#if defined( DLD_KAUTH_DEBUG_ENABLE )
    # define DLD_K_UUID_FMT "%08x:%08x:%08x:%08x"
    # define DLD_K_UUID_ARG(_u) *(int *)&_u.g_guid[0],*(int *)&_u.g_guid[4],*(int *)&_u.g_guid[8],*(int *)&_u.g_guid[12]

    # define DLD_KAUTH_DEBUG(fmt, args...)  do {\
                                                    DLD_COMM_LOG ( ACL_EVALUATUION, ("%s:%d: " fmt "\n", __PRETTY_FUNCTION__, __LINE__ , ##args));\
                                            } while (0)

    # define DLD_KAUTH_DEBUG_CTX(_c)		DLD_KAUTH_DEBUG("p = %p c = %p", _c->vc_proc, _c->vc_ucred)
    # define DLD_VFS_DEBUG(_ctx, _vp, fmt, args...)						\
        do {\
                DLD_COMM_LOG( ACL_EVALUATUION, ("%p '%s' %s:%d " fmt "\n",					\
                    _ctx,								\
                    (_vp != NULL && _vp->v_name != NULL) ? _vp->v_name : "????",	\
                    __PRETTY_FUNCTION__, __LINE__ ,					\
                    ##args) );\
        } while(0)
#else	/* !DLD_KAUTH_DEBUG_ENABLE */
    # define DLD_KAUTH_DEBUG(fmt, args...)		do { } while (0)
    # define DLD_VFS_DEBUG(ctx, vp, fmt, args...)	do { } while(0)
#endif	/* !DLD_KAUTH_DEBUG_ENABLE */

//--------------------------------------------------------------------

/*
 * DldKauthGuidEqual is a derivative of kauth_guid_equal
 *
 * Description:	Determine the equality of two GUIDs
 *
 * Parameters:	guid1				Pointer to first GUID
 *		guid2				Pointer to second GUID
 *
 * Returns:	0				If GUIDs are inequal
 *		!0				If GUIDs are equal
 */
int
DldKauthGuidEqual(guid_t *guid1, guid_t *guid2)
{
	return(bcmp(guid1, guid2, sizeof(*guid1)) == 0);
}

//--------------------------------------------------------------------

/*
 * kauth_wellknown_guid
 *
 * Description:	Determine if a GUID is a well-known GUID
 *
 * Parameters:	guid				Pointer to GUID to check
 *
 * Returns:	KAUTH_WKG_NOT			Not a wel known GUID
 *		KAUTH_WKG_EVERYBODY		"Everybody"
 *		KAUTH_WKG_NOBODY		"Nobody"
 *		KAUTH_WKG_OWNER			"Other"
 *		KAUTH_WKG_GROUP			"Group"
 *
 * the function is a derivative of the kauth_wellknown_guid function from the 1486 kernel branch
 */
int
DldKauthWellknownGuid(guid_t *guid)
{
	static unsigned char	fingerprint[] = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef};
	unsigned int		    code;
	/*
	 * All WKGs begin with the same 12 bytes.
	 */
	if (bcmp((void *)guid, fingerprint, 12) == 0) {
		/*
		 * The final 4 bytes are our code (in network byte order).
		 */
		code = OSSwapHostToBigInt32(*(u_int32_t *)&guid->g_guid[12]);
		switch(code) {
            case 0x0000000c:
                return(KAUTH_WKG_EVERYBODY);
            case 0xfffffffe:
                return(KAUTH_WKG_NOBODY);
            case 0x0000000a:
                return(KAUTH_WKG_OWNER);
            case 0x00000010:
                return(KAUTH_WKG_GROUP);
		}
	}
	return(KAUTH_WKG_NOT);
}

//--------------------------------------------------------------------

/*
 * ACL evaluator.
 *
 * Determines whether the credential has the requested rights for an object secured by the supplied
 * ACL.
 *
 * Evaluation proceeds from the top down, with access denied if any ACE denies any of the requested
 * rights, or granted if all of the requested rights are satisfied by the ACEs so far.
 * 
 * the function is a derivative of the kauth_acl_evaluate function from the 1486 kernel branch
 * 
 * the KAUTH_RESULT_DEFER evaluation value ( i.e. eval->ae_result ) means that neiver deny no
 * allowing entry was found for the requested rights and the request's execution context
 */
int
DldKauthAclEvaluate(
    __in    kauth_cred_t cred,
    __inout kauth_acl_eval_t eval
    )
{
	int           applies, error, i;
	kauth_ace_t   ace;
	guid_t        guid;
	uint32_t      rights;
	int           wkguid;
    
    assert( preemption_enabled() );
    
	/* always allowed to do nothing */
	if (eval->ae_requested == 0) {
		eval->ae_result = KAUTH_RESULT_DEFER;
		return(0);
	}
    
#if defined( DLD_KAUTH_DEBUG_ENABLE )
    SInt32    kauthLogIndex;
    if( gGlobalSettings.logFlags.ACL_EVALUATUION )
        kauthLogIndex = OSIncrementAtomic( &gKauthLogIndx );
#endif//defined( DLD_KAUTH_DEBUG_ENABLE )
    
	eval->ae_residual    = eval->ae_requested;
	eval->ae_found_deny  = FALSE;
    
	/*
	 * Get our guid for comparison purposes.
	 */
	if ((error = kauth_cred_getguid(cred, &guid)) != 0) {
		eval->ae_result = KAUTH_RESULT_DENY;
		DLD_KAUTH_DEBUG("[%-7d] ACL - can't get credential GUID (%d), ACL denied", (int)kauthLogIndex, error);
		return(error);
	}
    
	DLD_KAUTH_DEBUG("[%-7d] ACL - %d entries, initial residual 0x%x", (int)kauthLogIndex, eval->ae_count, eval->ae_residual);
	for (i = 0, ace = eval->ae_acl; i < eval->ae_count; i++, ace++) {
        
		/*
		 * Skip inherit-only entries.
		 */
		if (ace->ace_flags & KAUTH_ACE_ONLY_INHERIT)
			continue;
        
        rights = ace->ace_rights;
        
		/*
		 * Expand generic rights, if appropriate.
         not appropriate as we do not use the mac kauth masks and rights
		 */
        /*
		if (rights & KAUTH_ACE_GENERIC_ALL)
			rights |= eval->ae_exp_gall;
		if (rights & KAUTH_ACE_GENERIC_READ)
			rights |= eval->ae_exp_gread;
		if (rights & KAUTH_ACE_GENERIC_WRITE)
			rights |= eval->ae_exp_gwrite;
		if (rights & KAUTH_ACE_GENERIC_EXECUTE)
			rights |= eval->ae_exp_gexec;
         */
        
		/*
		 * Determine whether this entry applies to the current request.  This
		 * saves us checking the GUID if the entry has nothing to do with what
		 * we're currently doing.
		 */
		switch(ace->ace_flags & KAUTH_ACE_KINDMASK) {
            case KAUTH_ACE_PERMIT:
                if (!(eval->ae_residual & rights))
                    continue;
                break;
            case KAUTH_ACE_DENY:
                if (!(eval->ae_requested & rights))
                    continue;
                eval->ae_found_deny = TRUE;
                break;
            default:
                /* we don't recognise this ACE, skip it */
                continue;
		}
		
		/*
		 * Verify whether this entry applies to the credential.
		 */
		wkguid = DldKauthWellknownGuid(&ace->ace_applicable);
		switch(wkguid) {
            case KAUTH_WKG_OWNER:
                applies = eval->ae_options & KAUTH_AEVAL_IS_OWNER;
                break;
            case KAUTH_WKG_GROUP:
                applies = eval->ae_options & KAUTH_AEVAL_IN_GROUP;
                break;
                /* we short-circuit these here rather than wasting time calling the group membership code */
            case KAUTH_WKG_EVERYBODY:
                applies = 1;
                break;
            case KAUTH_WKG_NOBODY:
                applies = 0;
                break;
                
            default:
                /* check to see whether it's exactly us, or a group we are a member of */
                applies = DldKauthGuidEqual(&guid, &ace->ace_applicable);
                DLD_KAUTH_DEBUG("[%-7d] ACL[%d] - ACE applicable " DLD_K_UUID_FMT " caller " DLD_K_UUID_FMT " %smatched",
                            (int)kauthLogIndex,  (unsigned int)i, DLD_K_UUID_ARG(ace->ace_applicable), DLD_K_UUID_ARG(guid), applies ? "" : "not " );
                
                if (!applies) {
                    error = kauth_cred_ismember_guid(cred, &ace->ace_applicable, &applies);
                    /*
                     * If we can't resolve group membership, we have to limit misbehaviour.
                     * If the ACE is an 'allow' ACE, assume the cred is not a member (avoid
                     * granting excess access).  If the ACE is a 'deny' ACE, assume the cred
                     * is a member (avoid failing to deny).
                     */
                    if (error != 0) {
                        DLD_KAUTH_DEBUG("[%-7d] ACL[%d] - can't get membership, making pessimistic assumption",
                                        (int)kauthLogIndex, (unsigned int)i);
                        switch(ace->ace_flags & KAUTH_ACE_KINDMASK) {
                            case KAUTH_ACE_PERMIT:
                                applies = 0;
                                break;
                            case KAUTH_ACE_DENY:
                                applies = 1;
                                break;
                        }
                    } else {
                        DLD_KAUTH_DEBUG("[%-7d] ACL[%d] - %s group member", (int)kauthLogIndex, (unsigned int)i, applies ? "is" : "not");
                    }
                } else {
                    DLD_KAUTH_DEBUG("[%-7d ]ACL[%d] - entry matches caller", (int)kauthLogIndex, (unsigned int)i);
                }
		}
		if (!applies)
			continue;
        
		/*
		 * Apply ACE to outstanding rights.
		 */
		switch(ace->ace_flags & KAUTH_ACE_KINDMASK) {
            case KAUTH_ACE_PERMIT:
                /* satisfy any rights that this ACE grants */
                eval->ae_residual = eval->ae_residual & ~rights;
                DLD_KAUTH_DEBUG("[%-7d] ACL[%d] - rights 0x%x leave residual 0x%x", (int)kauthLogIndex, (unsigned int)i, rights, eval->ae_residual);
                /* all rights satisfied? */
                if (eval->ae_residual == 0) {
                    eval->ae_result = KAUTH_RESULT_ALLOW;
                    return(0);
                }
                break;
            case KAUTH_ACE_DENY:
                /* deny the request if any of the requested rights is denied */
                if (eval->ae_requested & rights) {
                    DLD_KAUTH_DEBUG("[%-7d] ACL[%d] - denying based on 0x%x", (int)kauthLogIndex, (unsigned int)i, rights);
                    eval->ae_result = KAUTH_RESULT_DENY;
                    return(0);
                }
                break;
            default:
                DLD_KAUTH_DEBUG("[%-7d] ACL[%d] - unknown entry kind 0x%x", (int)kauthLogIndex,  (unsigned int)i, ace->ace_flags & KAUTH_ACE_KINDMASK);
                break;
		}
	}
	/* if not permitted, defer to other modes of authorisation */
	eval->ae_result = KAUTH_RESULT_DEFER;
	return(0);
}

/*
{
    vfs_context_t a_context;
    kauth_cred_t cred = vfs_context_ucred(context);
    
    struct proc *p
    cr = kauth_cred_proc_ref(p);
}
 */
//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldAclObject, OSObject )

//--------------------------------------------------------------------

DldAclObject* DldAclObject::withAcl( __in kauth_acl_t acl )
{
    DldAclObject*  newAclObj;
    
    assert( preemption_enabled() );
    assert( NULL != acl && KAUTH_FILESEC_NOACL != acl->acl_entrycount );
    
    newAclObj = new DldAclObject();
    assert( newAclObj );
    if( !newAclObj )
        return NULL;
    
    if( !newAclObj->init() ){
        
        assert( !"newAclObj->init() failed" );
        
        newAclObj->release();
        return NULL;
    }
    
    newAclObj->acl = kauth_acl_alloc( acl->acl_entrycount );
    assert( newAclObj->acl );
    if( NULL == newAclObj->acl ){
        
        newAclObj->release();
        return NULL;
    }
    
    memcpy( newAclObj->acl, acl, KAUTH_ACL_SIZE( acl->acl_entrycount ) );
    
    return newAclObj;
}

//--------------------------------------------------------------------

DldAclObject* DldAclObject::withAcl(
    __in kauth_acl_t acl,
    __in DldDeviceType  type
    )
{
    DldAclObject*  newAclObj;
    
    assert( preemption_enabled() );
    assert( NULL != acl && KAUTH_FILESEC_NOACL != acl->acl_entrycount );
    
    newAclObj = DldAclObject::withAcl( acl );
    assert( newAclObj );
    if( !newAclObj ){
        
        DBG_PRINT_ERROR(("withAcl( acl ) failed\n"));
        return NULL;
    }
    
    newAclObj->type = type;
    
    return newAclObj;
}

//--------------------------------------------------------------------

void DldAclObject::free()
{
    if( NULL != this->acl )
        kauth_acl_free( this->acl );
}

//--------------------------------------------------------------------

#undef super
#define super OSObject

OSDefineMetaClassAndStructors( DldTypePermissionsEntry, OSObject )

//--------------------------------------------------------------------

DldTypePermissionsEntry* DldTypePermissionsEntry::newEntry()
{
    DldTypePermissionsEntry* newEntry;
    
    newEntry = new DldTypePermissionsEntry();
    assert( newEntry );
    if( !newEntry )
        return NULL;
    
    if( !newEntry->init() ){
        
        assert( !"newEntry->init() failed" );
        newEntry->release();
        
        return NULL;
    }
    
    return newEntry;
}

//--------------------------------------------------------------------

bool DldTypePermissionsEntry::init()
{
    if( !super::init() )
        return false;
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( !this->rwLock )
        return false;

    this->acls = OSArray::withCapacity( 0x3 );
    assert( this->acls );
    if( !this->acls )
        return false;
    
    return true;
}

//--------------------------------------------------------------------

void DldTypePermissionsEntry::free()
{
    if( this->acls ){
        
        this->acls->flushCollection();
        this->acls->release();
    }
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
    super::free();
}

//--------------------------------------------------------------------

DldAclObject*
DldTypePermissionsEntry::getReferencedAclByTypeWoLock(
    __in DldDeviceType  type,
    __out_opt unsigned int* indx
    )
{
    unsigned int   count;
    
    assert( preemption_enabled() );
    assert( this->rwLock );
    assert( this->acls );
    
    count = this->acls->getCount();
    
    for( unsigned int i = 0x0; i < count; ++i ){
        
        DldAclObject*   acl;
        
        acl = OSDynamicCast( DldAclObject, this->acls->getObject( i ) );
        assert( acl );
        assert( acl->getType().type.major == type.type.major );
        
        if( acl->getType().combined == type.combined ){
            
            //
            // found!
            //
            
            acl->retain();
            
            if( indx )
                *indx = i;
            
            return acl;
            
        }// end if
        
    }// end for
    
    return NULL;
}

//--------------------------------------------------------------------

bool DldTypePermissionsEntry::setAcl( __in DldAclObject* newAcl )
{
    bool          bAdded = true;
    DldAclObject* oldAcl = NULL;
    
    assert( preemption_enabled() );
    assert( this->rwLock );
    assert( this->acls );
    
    IORWLockWrite( this->rwLock );
    {// start of the lock
        
        unsigned int   indx = 0xFFFFF;
        
        oldAcl = this->getReferencedAclByTypeWoLock( newAcl->getType(), &indx );
        if( oldAcl ){
            
            //
            // replace the old object with the new
            //
            
            assert( indx < this->acls->getCount() );
            assert( oldAcl->getType().combined == newAcl->getType().combined );
            
            this->acls->replaceObject( indx, newAcl );
            
        } else {
            
            //
            // add the new object
            //
            this->acls->setObject( newAcl );
        }
        
    }// end of the lock
    IORWLockUnlock( this->rwLock );
    
    //
    // destroy the old acl after releasing the lock,
    // this reduces the time when the lock is held exclusive
    //
    if( oldAcl )
        oldAcl->release();
    DLD_DBG_MAKE_POINTER_INVALID( oldAcl );
    
    return bAdded;
}

//--------------------------------------------------------------------

void DldTypePermissionsEntry::deleteAllACLs()
{
    IORWLockWrite( this->rwLock );
    {// start of the lock
        
        if( this->acls ){
            
            this->acls->flushCollection();
        }
        
    }// end of the lock
    IORWLockUnlock( this->rwLock );
}

//--------------------------------------------------------------------

bool DldTypePermissionsEntry::setNullAcl( __in DldDeviceType deviceType )
{
    bool          bAdded = true;
    DldAclObject* oldAcl = NULL;
    
    assert( preemption_enabled() );
    assert( this->rwLock );
    assert( this->acls );
    
    IORWLockWrite( this->rwLock );
    {// start of the lock
        
        unsigned int   indx = 0xFFFFF;
        
        oldAcl = this->getReferencedAclByTypeWoLock( deviceType, &indx );
        if( oldAcl ){
            
            //
            // replace the old object with the NULL by simply removing the entry
            //
            
            assert( indx < this->acls->getCount() );
            assert( oldAcl->getType().combined == deviceType.combined );
            
            this->acls->removeObject( indx );
            
        } else {
            
            //
            // there is no ACL
            //
        }
        
    }// end of the lock
    IORWLockUnlock( this->rwLock );
    
    //
    // destroy the old acl after releasing the lock,
    // this reduces the time when the lock is held exclusive
    //
    if( oldAcl )
        oldAcl->release();
    DLD_DBG_MAKE_POINTER_INVALID( oldAcl );
    
    return bAdded;
}

//--------------------------------------------------------------------

DldAclObject*
DldTypePermissionsEntry::getReferencedAclByType( __in DldDeviceType  type )
{
    DldAclObject* acl;
    
    IORWLockRead( this->rwLock );
    {// start of the lock
        
        acl = this->getReferencedAclByTypeWoLock( type, NULL );
        
    }// end of the lock
    IORWLockUnlock( this->rwLock );
    
    return acl;
}

//--------------------------------------------------------------------

#undef super
#define super OSObject

OSDefineMetaClassAndStructors( DldTypePermissionsArray, OSObject )

//--------------------------------------------------------------------

bool DldTypePermissionsArray::init()
{
    if( !super::init() ){
        
        assert( !"super::init() failed" );
        return false;
    }
    
    for( unsigned int i = 0x0;
         i < sizeof( array )/ sizeof( array[0] );
         ++i )
    {
        array[ i ] = DldTypePermissionsEntry::newEntry();
        assert( array[ i ] );
        if( !array[ i ] )
            return false;
        
    }// end for
    
    return true;
}

//--------------------------------------------------------------------

void DldTypePermissionsArray::free()
{
    for( unsigned int i = 0x0;
        i < DLD_STATIC_ARRAY_SIZE( this->array );
        ++i )
    {
        if( this->array[ i ] )
            this->array[ i ]->release();
        
    }// end for
    
    super::free();
}

//--------------------------------------------------------------------

DldTypePermissionsArray* DldTypePermissionsArray::newPermissionsArray()
{
    DldTypePermissionsArray* newArray;
    
    newArray = new DldTypePermissionsArray();
    assert( newArray );
    if( !newArray )
        return NULL;
    
    if( !newArray->init() ){
        
        assert( !"newArray->init() failed" );
        
        newArray->release();
        return NULL;
    }
    
    return newArray;
}

//--------------------------------------------------------------------

DldAclObject* DldTypePermissionsArray::geReferencedAclByType( __in DldDeviceType  type )
{
    assert( preemption_enabled() );
    assert( type.type.major < sizeof( array )/ sizeof( array[0] ) && array[ type.type.major ] );
    
    if( type.type.major >= sizeof( array )/ sizeof( array[0] ) ||
        !array[ type.type.major ] )
        return NULL;
    
    return array[ type.type.major ]->getReferencedAclByType( type );
}

//--------------------------------------------------------------------

bool DldTypePermissionsArray::setAcl( __in DldAclObject* acl )
{
    assert( preemption_enabled() );
    assert( acl->getType().type.major < sizeof( array )/ sizeof( array[0] ) && array[ acl->getType().type.major ] );
    
    if( acl->getType().type.major >= sizeof( array )/ sizeof( array[0] ) ||
        !array[ acl->getType().type.major ] )
        return false;
    
    return array[ acl->getType().type.major ]->setAcl( acl );
}

//--------------------------------------------------------------------

bool DldTypePermissionsArray::setNullAcl( __in DldDeviceType deviceType )
{
    assert( preemption_enabled() );
    assert( deviceType.type.major < sizeof( array )/ sizeof( array[0] ) && array[ deviceType.type.major ] );
    
    if( deviceType.type.major >= sizeof( array )/ sizeof( array[0] ) ||
       !array[ deviceType.type.major ] )
        return false;
    
    return array[ deviceType.type.major ]->setNullAcl( deviceType );
}

//--------------------------------------------------------------------

void DldTypePermissionsArray::deleteAllACLs()
{
    for( unsigned int i = 0x0;
        i < DLD_STATIC_ARRAY_SIZE( this->array );
        ++i )
    {
        if( this->array[ i ] )
            this->array[ i ]->deleteAllACLs();
            
    }// end for
}

//--------------------------------------------------------------------

int
DldKauthAclEvaluate(
    __inout DldAccessEval* dldEval,
    __in DldAclObject* acl
    )
{
	struct kauth_acl_eval	    eval = { 0x0 };
	int                         error;
    bool                        deref_cred = false;
    kauth_cred_t                cred = NULL;
    
    assert( preemption_enabled() );
    
    cred = dldEval->cred;
    if( !cred ){
        
        //
        // get the current process credentials
        //
        cred = kauth_cred_proc_ref( current_proc() );
        assert( cred );
        
        deref_cred = true;
        
    }// end if( !cred )
    
    //
    // fill in the evaluation structure
    //
    eval.ae_requested  = dldEval->ae_requested;
    eval.ae_acl        = &(acl->getAcl()->acl_ace[0]);
    eval.ae_count      = acl->getAcl()->acl_entrycount;
    eval.ae_options    = 0;
    //eval.ae_options |= KAUTH_AEVAL_IS_OWNER;
    //eval.ae_options |= KAUTH_AEVAL_IN_GROUP;
    eval.ae_exp_gall   = KAUTH_VNODE_GENERIC_ALL_BITS;     // TO DO - must be DL generic types 
    eval.ae_exp_gread  = KAUTH_VNODE_GENERIC_READ_BITS;    // TO DO - must be DL generic types
    eval.ae_exp_gwrite = KAUTH_VNODE_GENERIC_WRITE_BITS;   // TO DO - must be DL generic types
    eval.ae_exp_gexec  = KAUTH_VNODE_GENERIC_EXECUTE_BITS; // TO DO - must be DL generic types
    
    //
    // evaluate the requested access rights
    //
    error = DldKauthAclEvaluate( cred, &eval );
    //assert( !error ); removed as fired on system shutdown
    if( error ){
        
        DBG_PRINT_ERROR(("DldKauthAclEvaluate returned an error %d\n", error));
        eval.ae_result = KAUTH_RESULT_DENY;
        
    }// end if( error )
    
    //
    // copy the result
    //
    dldEval->ae_result   = eval.ae_result;
    dldEval->ae_residual = eval.ae_residual;
    
    if( deref_cred ){
        
        assert( cred );
        kauth_cred_unref( &cred );
        
    }// end if( deref_cred )
    
    return error;
}

//--------------------------------------------------------------------

void
DldTypePermissionsArray::isAccessAllowed( __inout DldAccessEval* dldEval )
{
    
    assert( preemption_enabled() );
    
    DldAclObject*               acl;
    
    assert( dldEval->type.type.major < DLD_DEVICE_TYPE_MAX && dldEval->type.type.major < DLD_DEVICE_TYPE_MAX );
    
    acl = this->geReferencedAclByType( dldEval->type );
    if( !acl ){
        
        dldEval->null_acl_found = true;
        dldEval->ae_result = KAUTH_RESULT_DEFER;// NULL usually means ALLOW everyone
        dldEval->ae_residual = dldEval->ae_requested;
        return;
        
    }// end if( !acl )

    DldKauthAclEvaluate( dldEval, acl );

    //
    // release all acquired resources
    //
    
    acl->release();
    
}



//--------------------------------------------------------------------

/*
 #define KAUTH_VNODE_GENERIC_READ_BITS	(KAUTH_VNODE_READ_DATA |		\
 KAUTH_VNODE_READ_ATTRIBUTES |		\
 KAUTH_VNODE_READ_EXTATTRIBUTES |	\
 KAUTH_VNODE_READ_SECURITY)
 
 #define KAUTH_VNODE_GENERIC_WRITE_BITS	(KAUTH_VNODE_WRITE_DATA |		\
 KAUTH_VNODE_APPEND_DATA |		\
 KAUTH_VNODE_DELETE |			\
 KAUTH_VNODE_DELETE_CHILD |		\
 KAUTH_VNODE_WRITE_ATTRIBUTES |		\
 KAUTH_VNODE_WRITE_EXTATTRIBUTES |	\
 KAUTH_VNODE_WRITE_SECURITY)
 
 #define KAUTH_VNODE_GENERIC_EXECUTE_BITS (KAUTH_VNODE_EXECUTE)
 
 #define KAUTH_VNODE_GENERIC_ALL_BITS	(KAUTH_VNODE_GENERIC_READ_BITS |	\
 KAUTH_VNODE_GENERIC_WRITE_BITS |	\
 KAUTH_VNODE_GENERIC_EXECUTE_BITS)
 
 #define KAUTH_VNODE_WRITE_RIGHTS	(KAUTH_VNODE_ADD_FILE |				\
 KAUTH_VNODE_ADD_SUBDIRECTORY |			\
 KAUTH_VNODE_DELETE_CHILD |			\
 KAUTH_VNODE_WRITE_DATA |			\
 KAUTH_VNODE_APPEND_DATA |			\
 KAUTH_VNODE_DELETE |				\
 KAUTH_VNODE_WRITE_ATTRIBUTES |			\
 KAUTH_VNODE_WRITE_EXTATTRIBUTES |		\
 KAUTH_VNODE_WRITE_SECURITY |			\
 KAUTH_VNODE_TAKE_OWNERSHIP |			\
 KAUTH_VNODE_LINKTARGET |			\
 KAUTH_VNODE_CHECKIMMUTABLE)
 */
/*
kauth_ace_rights_t
DldConvertKauthRightsChildToParent(
    __in DldDeviceType child,
    __in DldDeviceType parent,
    __in kauth_ace_rights_t childRights
    )
{
 this one is not correct anymore, for reference see DldConvertWinRightsChildToParent
    kauth_ace_rights_t  parentRights = 0x0;
    
    if( ( KAUTH_VNODE_GENERIC_READ_BITS | KAUTH_VNODE_GENERIC_EXECUTE_BITS ) & childRights )
        parentRights |= KAUTH_VNODE_READ_DATA;
    
    if( KAUTH_VNODE_WRITE_RIGHTS & childRights )
        parentRights |= KAUTH_VNODE_WRITE_DATA;
    
    return parentRights;
}
*/
//--------------------------------------------------------------------

dld_classic_rights_t
DldConvertWinRightsChildToParent(
    __in UInt16 child,  // DLD_DEVICE_TYPE_...
    __in UInt16 parent, // DLD_DEVICE_TYPE_...
    __in dld_classic_rights_t childRights
    )
{
    dld_classic_rights_t  parentRights = 0x0;
    
    //
    // check that there is no encrypted rights here, they should be converted to a normal rights by a caller
    //
    assert( ( DEVICE_PLAY_AUDIO_CD | 0xFFFF ) & childRights );
    
    if( DEVICE_TYPE_USBHUB == parent || DEVICE_TYPE_1394 == parent ){
        
        //
        // USB and FireWire should have a full set of rights in ACL, except DEVICE_PLAY_AUDIO_CD and encrypted rights
        //
        parentRights = childRights & (~DEVICE_PLAY_AUDIO_CD);
        if( childRights & DEVICE_PLAY_AUDIO_CD )
            parentRights |= DEVICE_READ;
        
        return childRights;
    }
    
    if( DEVICE_FULL_WRITE & childRights )
        parentRights |= DEVICE_WRITE;
    
    if( (~DEVICE_FULL_WRITE) & childRights )
        parentRights |= DEVICE_READ;
    
    return parentRights;
}

//--------------------------------------------------------------------

void
DldIsShadowedUser( __inout DldAccessCheckParam* param )
{
    DldRequestedAccess   originalAccess;
    
    //
    // overule the requested access as the check for shadowing
    // doesn't depend on the requested access, so only
    // the write permision have a meaning for shadow ACLs
    //
    originalAccess = param->dldRequestedAccess;
    bzero( &param->dldRequestedAccess, sizeof( param->dldRequestedAccess ) );
    param->dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_WRITE_DATA;
    ::convertKauthToWin( param );// convert a new substituted rights
#if defined(LOG_ACCESS)
    param->sourceFunction      = __PRETTY_FUNCTION__;
    param->sourceFile          = __FILE__;
    param->sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
    
    ::isAccessAllowed( param );
    
    //
    // restore the original value as the field is not mutable
    // from the caller point of view
    //
    param->dldRequestedAccess = originalAccess;
}

//--------------------------------------------------------------------

void
DldIsDiskCawledUser( __inout DldAccessCheckParam* param )
{
    DldRequestedAccess   originalAccess;
    
    //
    // overule the requested access as the check for shadowing
    // doesn't depend on the requested access, so only
    // the write permision have a meaning for shadow ACLs
    //
    originalAccess = param->dldRequestedAccess;
    bzero( &param->dldRequestedAccess, sizeof( param->dldRequestedAccess ) );
    param->dldRequestedAccess.kauthRequestedAccess = KAUTH_VNODE_WRITE_DATA;
    ::convertKauthToWin( param );// convert a new substituted rights
#if defined(LOG_ACCESS)
    param->sourceFunction      = __PRETTY_FUNCTION__;
    param->sourceFile          = __FILE__;
    param->sourceLine          = __LINE__;
#endif//#if defined(LOG_ACCESS)
    
    ::isAccessAllowed( param );
    
    //
    // restore the original value as the field is not mutable
    // from the caller point of view
    //
    param->dldRequestedAccess = originalAccess;
}

//--------------------------------------------------------------------

bool
DldIsObjectPropertyWhiteListAppliedToUser(
    __in DldObjectPropertyEntry*      objProperty,
    __in kauth_cred_t                 credentials
)
{
    assert( credentials );
    assert( preemption_enabled() );
    
    if( !objProperty->dataU.property->whiteListState.acl ){
        
        //
        // a NULL ACL means - apply to any user
        //
        return true;
    }
    
    bool                applyToUser;
    DldAccessEval       wlAccesEval;
    DldAclObject*       acl;
    
    bzero( &wlAccesEval, sizeof( wlAccesEval ) );
    
    wlAccesEval.cred           = credentials;
    wlAccesEval.ae_requested   = DEVICE_READ | DEVICE_WRITE;
    wlAccesEval.ae_result      = KAUTH_RESULT_DEFER;
    wlAccesEval.null_acl_found = false;
    
    //
    // check the credentials againts the ACL
    //
    objProperty->dataU.property->LockShared();
    {// start of the lock
        
        acl = objProperty->dataU.property->whiteListState.acl;
        if( acl )
            acl->retain();
            
    }// end of the lock
    objProperty->dataU.property->UnLockShared();
    
    if( acl ){
        
        DldKauthAclEvaluate( &wlAccesEval, acl );
        acl->release();
        
    } else {
        
        //
        // no ACL means - apply to any user
        //
        wlAccesEval.ae_result = KAUTH_RESULT_ALLOW;
        
    }
    
    //
    // if not all requested permissions have been cleared then deny access,
    // the logic is in consisten with the Windows's SeAccessCheck()
    // ( for reference see Programming Windows Security by Keith Brown,
    // pp 198-200 )
    //
    if( KAUTH_RESULT_DEFER == wlAccesEval.ae_result &&
       0x0 != wlAccesEval.ae_residual &&
       !wlAccesEval.null_acl_found ){
        
        //
        // actually this should have not happened as the daemon must
        // provide the driver with a correct ACL where the mask is DEVICE_READ | DEVICE_WRITE
        //
        assert( !"the daemon provided an incorrect ACL for a WL\n" );
        wlAccesEval.ae_result = KAUTH_RESULT_DENY;
    }
    
    applyToUser = ( KAUTH_RESULT_ALLOW == wlAccesEval.ae_result );
    
    return applyToUser;
}

//--------------------------------------------------------------------

typedef struct _WhiteListStatus{
    //
    // inWhiteList means that a device has been whitelisted,
    // propagateUp means that the device's or its parent white
    // list settings must be propogated to all attached objects
    //
    bool    whiteListChecked;
    bool    inWhiteList; // valid if whiteListChecked == true
    bool    propagateUp; // valid if whiteListChecked == true
    
    bool    whiteListed[ DldMaxTypeFlavor ]; // valid if whiteListChecked == true
    
} WhiteListStatus;

void
DldFetchUsbWhiteListSettings(
    __in_opt DldIOService*        dldIOService,
    __in     DldDeviceType        deviceType,
    __in     kauth_cred_t         credentials,
    __out    WhiteListStatus*     whiteListStatus
    )
{
    assert( preemption_enabled() );
    
    if( NULL == dldIOService || DLD_DEVICE_TYPE_HARDDRIVE == deviceType.type.major ){
        
        whiteListStatus->whiteListChecked = true;
        whiteListStatus->inWhiteList = false;
        whiteListStatus->propagateUp = false;
        whiteListStatus->whiteListed[ DldFullTypeFlavor ] = false; 
        whiteListStatus->whiteListed[ DldMajorTypeFlavor ] = false;
        whiteListStatus->whiteListed[ DldParentTypeFlavor ] = false;
        return;
    }
    
    bool inWhiteList = false;
    bool propagateUp = false;
    DldObjectPropertyEntry* usbDeviceProp;// referenced
    
    usbDeviceProp = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_UsbDevice );
    assert( !( (DLD_DEVICE_TYPE_USB == deviceType.type.minor || DLD_DEVICE_TYPE_USB == deviceType.type.major ) && NULL == usbDeviceProp) );
    if( usbDeviceProp ){
        
        bool applyToUser;
        
        assert( DldObjectPopertyType_UsbDevice == usbDeviceProp->dataU.property->typeDsc.type );
        
        //
        // check whether the WL is applied to the user
        //
        applyToUser = DldIsObjectPropertyWhiteListAppliedToUser( usbDeviceProp, credentials );
        
        if( applyToUser ){
            
            inWhiteList = usbDeviceProp->dataU.property->whiteListState.inWhiteList;
            propagateUp = usbDeviceProp->dataU.property->whiteListState.propagateUp;
            
        }// end if( applyToUser )
        
        usbDeviceProp->release();
        DLD_DBG_MAKE_POINTER_INVALID( usbDeviceProp );
    }// end if( usbDeviceProp )
    
    whiteListStatus->whiteListChecked = true;
    whiteListStatus->inWhiteList = inWhiteList;
    whiteListStatus->propagateUp = propagateUp;
    
    //
    // define white list settings on different levels
    //
    whiteListStatus->whiteListed[ DldFullTypeFlavor ] = ( inWhiteList && propagateUp ) || // the whole device stack has been whitelisted
                                                        ( inWhiteList && (DLD_DEVICE_TYPE_USB == deviceType.type.major || DLD_DEVICE_TYPE_USB == deviceType.type.minor) );
    
    whiteListStatus->whiteListed[ DldMajorTypeFlavor ] = ( inWhiteList && propagateUp ) || // the whole device stack has been whitelisted
                                                         ( inWhiteList && DLD_DEVICE_TYPE_USB == deviceType.type.major ); // the device has been whitelisted only at the USB level
    
    whiteListStatus->whiteListed[ DldParentTypeFlavor ] = ( inWhiteList && propagateUp ) || // the whole device stack has been whitelisted
                                                          ( inWhiteList && DLD_DEVICE_TYPE_USB == deviceType.type.minor ); // the device has been whitelisted only at the USB level
    
    
}

//--------------------------------------------------------------------

#if DBG
    #define REMEMBER_QUIRK_LINE()  do{ accesEval->quirkLine = __LINE__; }while(false);
#else
    #define REMEMBER_QUIRK_LINE() do{ ; }while(false);
#endif 

void
DldApplySecurityQuirks(
    __in_opt DldIOService*        dldIOService,
    __inout  DldAccessCheckParam* param,
    __inout  DldAccessEval*       accesEval,
    __in     WhiteListStatus*     usbWhiteListStatus
    )
{
    
    assert( preemption_enabled() );
    assert( accesEval->type.type.major == param->deviceType.type.major || // a case of device type
            accesEval->type.type.major == param->deviceType.type.minor || // a case of device's parent type
            DLD_DEVICE_TYPE_UNKNOWN == accesEval->type.type.major );
    assert( accesEval->type.type.minor == param->deviceType.type.minor || DLD_DEVICE_TYPE_UNKNOWN == accesEval->type.type.minor );
    
    if( gClientWasAbnormallyTerminated && gSecuritySettings.securitySettings.disableAccessOnServiceCrash ){
        
        //
        // the service was abnormally terminated, may be becaus of attack,
        // disable the request but honer all special setting such
        // as access to the system volume for the admin
        //
        accesEval->ae_result = KAUTH_RESULT_DENY;
    }
    
    if( KAUTH_RESULT_DENY != accesEval->ae_result ){
        
        //
        // the request has not been disabled, nothing to do here
        //
        return;
    }
    
    if( kDldAclTypeSecurity != param->aclType ){
        
        //
        // currentrly applied only for the access permissions
        //
        return;
    }
    
    assert( usbWhiteListStatus->whiteListChecked );
    
    if( usbWhiteListStatus->inWhiteList && usbWhiteListStatus->propagateUp ){
        
        //
        // unconditionally allow access as the check is done from the bottom to the up,
        // so the parent is in a white list with the "propagare up" flag being set
        //
        accesEval->ae_result = KAUTH_RESULT_ALLOW;
        REMEMBER_QUIRK_LINE()
        return;
    }
    
    if( NULL == dldIOService )
        return;

    assert( dldIOService );
    //
    // check the exceptions for USB devices, use accesEval->type as it defines the current checked type
    // - for example, you don't want apply USB exceptions to a removable drive security as these
    // exceptions are applied exactly on the USB level
    //
    if( DLD_DEVICE_TYPE_USB == accesEval->type.type.major ||
        DLD_DEVICE_TYPE_USB == accesEval->type.type.minor ){
        
        int                       currentIndex = (-1);
        UInt32                    expectedNumberOfInterfaces = 0x0;
        DldObjectPropertyEntry*   usbDevicePropEntry = NULL;// referenced!
        
        usbDevicePropEntry = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_UsbDevice );
        assert( usbDevicePropEntry );
        if( usbDevicePropEntry )
            expectedNumberOfInterfaces = usbDevicePropEntry->dataU.usbDeviceProperty->numberOfInterfaces;
        
        while( true ){
            
            DldObjectPropertyEntry* usbInterfacePropEntry = NULL;// referenced!
            
            //
            // the returned interface might be an ancillary one
            //
            usbInterfacePropEntry = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_UsbInterface, currentIndex );
            assert( !( (-1) == currentIndex && !usbInterfacePropEntry ) || 0x0 == expectedNumberOfInterfaces ); // at least one property must exist
            if( !usbInterfacePropEntry ){
                
                //
                // an example of a device w/o any interface, in that case this is an iPad
                //
                /*
                 +-o iPad  <object 0xffffff8016fa2400, id 0x1000006d2, vtable 0xffffff7f80a404a0, registered, matched, active, busy 0, retain count 11>
                 | {
                 |   "USB Product Name" = "iPad"
                 |   "USB Vendor Name" = "Apple Inc."
                 |   "USB Serial Number" = "496f52691b56fc7e7d6624b28e4589c668f57646"
                 |   "bDeviceClass" = 0
                 |   "bDeviceSubClass" = 0
                 |   "bDeviceProtocol" = 0
                 |   "bMaxPacketSize0" = 64
                 |   "idVendor" = 1452
                 |   "idProduct" = 4767
                 |   "bcdDevice" = 528
                 |   "iManufacturer" = 1
                 |   "iProduct" = 2
                 |   "iSerialNumber" = 3
                 |   "bNumConfigurations" = 4
                 |   "Device Speed" = 2
                 |   "Bus Power Available" = 250
                 |   "USB Address" = 6
                 |   "sessionID" = 12846485593215
                 |   "PortNum" = 2
                 |   "locationID" = 337772544
                 |   "SupportsIPhoneOS" = Yes
                 |   "Preferred Configuration" = 3
                 |   "kCallInterfaceOpenWithGate" = Yes
                 |   "IOCFPlugInTypes" = {"9dc7b780-9ec0-11d4-a54f-000a27052861"="IOUSBFamily.kext/Contents/PlugIns/IOUSBLib.bundle"}
                 |   "IOUserClientClass" = "IOUSBDeviceUserClientV2"
                 |   "IOGeneralInterest" = `object 0xffffff8011947480, vt 0xffffff80008658b0 <vtable for IOCommand>`
                 | }
                 +-o IOService  <object 0xffffff801701f200, id 0x1000006d8, vtable 0xffffff8000860070 <vtable for IOService>, !registered, !matched, active, busy 0, retain count 1>
                 |   {
                 |     "IOProbeScore" = 98500
                 |     "CFBundleIdentifier" = "com.apple.kpi.iokit"
                 |     "IOProviderClass" = "IOUSBDevice"
                 |     "IOClass" = "IOService"
                 |     "idProductMask" = 65520
                 |     "idProduct" = 4752
                 |     "idVendor" = 1452
                 |     "bDeviceClass" = 0
                 |     "IOPersonalityPublisher" = "com.apple.driver.AppleMobileDevice"
                 |     "IOMatchCategory" = "IODefaultMatchCategory"
                 |   }
                 +-o ??  <object 0xffffff80132c3600, id 0x1000006e0, vtable 0xffffff7f80a473e0, !registered, !matched, active, busy 0, retain count 5>
                 {
                 "IOUserClientCreator" = "pid 163, SystemUIServer"
                 }                 
                 */
                break;
            }
            
            ++currentIndex;
            
            assert( DldObjectPopertyType_UsbInterface == usbInterfacePropEntry->dataU.property->typeDsc.type );
            
            //
            // skip the ancillary properties as we will get a real one later or already got it, an ancillary property adds
            // a complexity as it is hard to connect it with a related usbDeviceProperty, and normally this property does not
            // have a valid information
            //
            if( NULL == usbInterfacePropEntry->dataU.usbInterfaceProperty->usbDeviceProperty ){
                
                //
                // this is an ancillary ptoperty, skip it
                //
                usbInterfacePropEntry->release();
                continue;
            }
            
            //usbDeviceProp = usbInterfaceProp->dataU.usbInterfaceProperty->usbDeviceProperty;
            
            switch( usbInterfacePropEntry->dataU.usbInterfaceProperty->usbInterfaceClass ){
                    
                case kUSBHIDInterfaceClass:
                {
                    DldIoUsbDevicePropertyData*  usbDevProp = NULL; // not referenced
                    
                    //
                    // there are two cases
                    // - the interface object is for a real interface type entry
                    // - the interface object is for an ancillary type entry - a deprecated case
                    //
                    assert( usbInterfacePropEntry->dataU.usbInterfaceProperty->usbDeviceProperty ); // no ancillary properties here anymore
                    if( usbInterfacePropEntry->dataU.usbInterfaceProperty->usbDeviceProperty ){
                        
                        usbDevProp = usbInterfacePropEntry->dataU.usbInterfaceProperty->usbDeviceProperty->dataU.usbDeviceProperty;
                        
                    } else {
                        
                        //
                        // this is a bad idea as a found usbDevicePropEntry device might be not for this ancillary property in case of multiple parents
                        //
                        assert( usbDevicePropEntry );
                        if( usbDevicePropEntry )
                            usbDevProp = usbDevicePropEntry->dataU.usbDeviceProperty;
                    }
                    
                    assert( usbDevProp );
                    
                    
                    //
                    // a HID device ( mouse keyboard ), Apple USB HID devices are always allowed
                    // as internal keyboard and touch pad are USB devices, there is no PS/2 on Mac
                    //
                    if( !gSecuritySettings.securitySettings.controlUsbHid ||
                       ( usbDevProp && kDLD_APPLE_USB_VID == usbDevProp->VidPid.idVendor ) ){
                        
                        accesEval->ae_result = KAUTH_RESULT_ALLOW;
                        REMEMBER_QUIRK_LINE();
                    }
                    
                    break;
                }
                    
                case kUSBMassStorageInterfaceClass:
                    
                    //
                    // a mass storage device
                    //
                    if( !gSecuritySettings.securitySettings.controlUsbMassStorage ){
                        
                        accesEval->ae_result = KAUTH_RESULT_ALLOW;
                        REMEMBER_QUIRK_LINE()
                    }
                    
                    break;
                    
                case kUSBWirelessControllerInterfaceClass:
                    
                    //
                    // for reference see http://www.usb.org/developers/defined_class/#BaseClassE0h
                    //
                    // subclass 0x01
                    //    protocol 0x01 - bluetooth
                    //    protocol 0x04 - AMP bluetooth
                    //
                    if( kUSBRFControllerSubClass == usbInterfacePropEntry->dataU.usbInterfaceProperty->usbInterfaceSubClass &&
                       ( kUSBBluetoothProgrammingInterfaceProtocol == usbInterfacePropEntry->dataU.usbInterfaceProperty->usbInterfaceProtocol ||
                        0x04 == usbInterfacePropEntry->dataU.usbInterfaceProperty->usbInterfaceProtocol ) ){
                           
                           //
                           // a bluetooth controller
                           //
                           if( !gSecuritySettings.securitySettings.controlUsbBluetooth  ){
                               
                               accesEval->ae_result = KAUTH_RESULT_ALLOW;
                               REMEMBER_QUIRK_LINE();
                           }
                           
                           if( ! param->userClient ){
                               
                               accesEval->ae_result = KAUTH_RESULT_ALLOW;
                               REMEMBER_QUIRK_LINE();
                               
                           } /*else if( KAUTH_RESULT_ALLOW != accesEval->ae_result ){
                               
                               //
                               // the only devices that can't be controlled at the upper levels
                               // ( e.g. an IOUserClient, IOSerialStreamSync level ) are HID devices, so
                               // we do not check security at USB level if there are no
                               // connected bluetooth HID devices or bluetooth HID devices
                               // should not be controlled
                               //
                               
                               DldIOService* dldIOServiceToCheck = param->dldIOServiceRequestOwner ? param->dldIOServiceRequestOwner : dldIOService;
                               
                               //
                               // check for connected HID devices, including Apple Multitouch devices
                               //
                               DldObjectPropertyEntry* hidDevicePropEntry = dldIOServiceToCheck->retrievePropertyByTypeRef( DldObjectPropertyType_IOHIDSystem );
                               if( ! hidDevicePropEntry )
                                   hidDevicePropEntry = dldIOServiceToCheck->retrievePropertyByTypeRef( DldObjectPropertyType_AppleMultitouchDeviceUserClient );
                               
                               if( hidDevicePropEntry ){
                                   
                                   if( !gSecuritySettings.securitySettings.controlBluetoothHid ){
                                       
                                       accesEval->ae_result = KAUTH_RESULT_ALLOW;
                                       REMEMBER_QUIRK_LINE();
                                       
                                   }
                                   
                                   hidDevicePropEntry->release();
                                   DLD_DBG_MAKE_POINTER_INVALID( hidDevicePropEntry );
                                   
                               }
                               
                           } // else if( KAUTH_RESULT_ALLOW != accesEval->ae_result )
                              */
                           
                       }// end if( kUSBRFControllerSubClass == ...
                    break;
                    
                default:
                {
                    //
                    // check for the Network Interface
                    //
                    
                    bool      isNetworkInterface = false;
                    DldObjectPropertyEntry* networkInterfacePropEntry = NULL;// referenced!
                    
                    //
                    // there are two possibilities - Ethernet ( curently is represented by a naked network interface property )
                    // and WiFi
                    //
                    networkInterfacePropEntry = dldIOService->retrievePropertyByTypeRef( DldObjectPropertyType_IONetworkInterface );
                    if( NULL == networkInterfacePropEntry )
                        networkInterfacePropEntry = dldIOService->retrievePropertyByTypeRef( DldObjectPropertyType_Net80211Interface );
                    
                    if( networkInterfacePropEntry ){
                        
                        isNetworkInterface = true;
                        networkInterfacePropEntry->release();
                    } // end if( networkInterfacePropEntry )
                    
                    if( isNetworkInterface && !gSecuritySettings.securitySettings.controlUsbNetwork ){
                        
                        accesEval->ae_result = KAUTH_RESULT_ALLOW;
                        REMEMBER_QUIRK_LINE();
                    }
                    
                    break;
                }
                    
            }// end switch
            
            usbInterfacePropEntry->release();
            
            if( KAUTH_RESULT_ALLOW == accesEval->ae_result )
                break;
            
        } // end while( true )
        
        if( usbDevicePropEntry )
            usbDevicePropEntry->release();
        
    }// end if( DLD_DEVICE_TYPE_USB ...
    
    
    if( KAUTH_RESULT_ALLOW == accesEval->ae_result ){
        
        //
        // some quirks have been applied, stop checking
        //
        return;
    }
    
    
    if( usbWhiteListStatus->inWhiteList ){
        
        //
        // the device is in the white list, here is a subtle moment - without the "propagate up"
        // flag the accesEval->type.type.major type must be checked as this might be checking
        // for removable devices security
        //
        if( DLD_DEVICE_TYPE_USB == accesEval->type.type.major || usbWhiteListStatus->propagateUp ){
            
            accesEval->ae_result = KAUTH_RESULT_ALLOW;
            REMEMBER_QUIRK_LINE();
            return;
        }
    }
    
    //
    // check the list of exceptions for bluetooth devices
    //
    if( DLD_DEVICE_TYPE_BLUETOOTH == accesEval->type.type.major ||
        DLD_DEVICE_TYPE_BLUETOOTH == accesEval->type.type.minor ){
        
        if( ! param->userClient ){
            
            accesEval->ae_result = KAUTH_RESULT_ALLOW;
            REMEMBER_QUIRK_LINE();
            
        } else {
            
            DldObjectPropertyEntry* hidDevicePropEntry;// referenced
            
            hidDevicePropEntry = dldIOService->retrievePropertyByTypeRef( DldObjectPropertyType_IOHIDSystem );
            if( ! hidDevicePropEntry )
                hidDevicePropEntry = dldIOService->retrievePropertyByTypeRef( DldObjectPropertyType_AppleMultitouchDeviceUserClient );
            
            if( hidDevicePropEntry ){
                
                //
                // a HID device paired with the bluetooth controller
                //
                if( !gSecuritySettings.securitySettings.controlBluetoothHid ){
                    
                    accesEval->ae_result = KAUTH_RESULT_ALLOW;
                    REMEMBER_QUIRK_LINE();
                }
                
                hidDevicePropEntry->release();
            } // end if( hidDeviceProp )
        }
        
    } // end if( DLD_DEVICE_TYPE_BLUETOOTH == accesEval->type.type.major
    
    
    if( KAUTH_RESULT_ALLOW == accesEval->ae_result ){
        
        //
        // some quirks have been applied, stop checking
        //
        return;
    }
    
    //
    // check the exceptions for CD/DVD drives
    //
    if( DLD_DEVICE_TYPE_CD_DVD == accesEval->type.type.major ||
        DLD_DEVICE_TYPE_CD_DVD == accesEval->type.type.minor ){
        
        DldObjectPropertyEntry* scsi05Prop;// referenced!
        
        scsi05Prop = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_SCSIPeripheralDeviceType05 );
        assert( scsi05Prop );
        if( scsi05Prop ){
            
            //
            // check for recursion
            //
            if( current_thread() == scsi05Prop->dataU.ioSCSIPeripheralType05Property->currentUidRetrievalThread ){
                
                //
                // this is a recursive call, i.e. we are inside scsi05Prop->initDVDMediaWL()
                //
                accesEval->ae_result = KAUTH_RESULT_ALLOW;
                REMEMBER_QUIRK_LINE();
                
            } else {
                
                IOReturn RC;
                RC = scsi05Prop->initDVDMediaWL( dldIOService );
                assert( scsi05Prop->dataU.ioSCSIPeripheralType05Property->uidValid || kIOReturnNoMedia == RC ); // kIOReturnNoMedia is returned for a blank media or when access was denied
                
                scsi05Prop->setUIDProperty();
                
                if( scsi05Prop->dataU.property->whiteListState.inWhiteList ){
                    
                    //
                    // check whether the WL settings applied to a user
                    //
                    bool applyToUser;
                    
                    applyToUser = DldIsObjectPropertyWhiteListAppliedToUser( scsi05Prop, accesEval->cred );
                    if( applyToUser ){
                        
                        accesEval->ae_result = KAUTH_RESULT_ALLOW;
                        REMEMBER_QUIRK_LINE();
                        
                    }// end if( applyToUser )
                    
                }// if( scsi05Prop->dataU.property->whiteListState.inWhiteList )
                
            }
            
            scsi05Prop->release();
            DLD_DBG_MAKE_POINTER_INVALID( scsi05Prop );
            
        }// end if( scsi05Prop )

    }// end if( DLD_DEVICE_TYPE_CD_DVD == accesEval->type.type.major
    
    if( KAUTH_RESULT_ALLOW == accesEval->ae_result ){
        
        //
        // some quirks have been applied, stop checking
        //
        return;
    }
    
    //
    // check for IEEE1394 exceptions
    //
    if( DLD_DEVICE_TYPE_IEEE1394 == accesEval->type.type.major ||
        DLD_DEVICE_TYPE_IEEE1394 == accesEval->type.type.minor ){
        
        DldObjectPropertyEntry* sbp2Property;// referenced!
        
        sbp2Property = dldIOService->retrievePropertyByTypeRef( DldObjectPropertyType_IOFireWireSBP2 );
        if( sbp2Property ){
            
            assert( DldObjectPropertyType_IOFireWireSBP2 == sbp2Property->dataU.property->typeDsc.type );
            
            //
            // this is a FireWire storage device
            //
            if( !gSecuritySettings.securitySettings.controlIEE1394Storage ){
                
                accesEval->ae_result = KAUTH_RESULT_ALLOW;
                REMEMBER_QUIRK_LINE()
            } 
            
            sbp2Property->release();
            DLD_DBG_MAKE_POINTER_INVALID( sbp2Property );
        } // end if( sbp2Property )
        
    } // end if( DLD_DEVICE_TYPE_IEEE1394 == accesEval->type.type.major ||
    
    if( KAUTH_RESULT_ALLOW == accesEval->ae_result ){
        
        //
        // some quirks have been applied, stop checking
        //
        return;
    }
    
    //
    // check for superuser's exceptions
    //
    if( dldIOService->isOnBootDevicePath() && kauth_cred_issuser( param->credential ) )
    {
        //
        // a boot device is always accessible for the superuser
        //
        accesEval->ae_result = KAUTH_RESULT_ALLOW;
        REMEMBER_QUIRK_LINE();
        return;
    }
}

//--------------------------------------------------------------------

//
// a default function for rights conversons, applied if a caller didn't
// perform conversion before calling isAccessAllowed
//
void
convertKauthToWin( __in DldAccessCheckParam* param )
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
    // a one to one mapping
    //
    
    /*
     KAUTH_VNODE_LIST_DIRECTORY is the same as KAUTH_VNODE_READ_DATA, so do not extend
     rights as this right has a meaning only for FSD directory access which has its own
     convertion function
    if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_LIST_DIRECTORY )
        param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIR_LIST;
     */
    
    /*
     KAUTH_VNODE_ADD_FILE == KAUTH_VNODE_WRITE_DATA see above comment
    if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_ADD_FILE )
        param->dldRequestedAccess.winRequestedAccess |= DEVICE_DIR_CREATE;
     */
    
    if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_EXECUTE )
        param->dldRequestedAccess.winRequestedAccess |= DEVICE_EXECUTE;
    
    if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_DELETE_CHILD )
        param->dldRequestedAccess.winRequestedAccess |= DEVICE_DELETE;
    
    if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_READ_DATA )
        param->dldRequestedAccess.winRequestedAccess |= DEVICE_READ;
    
    if( param->dldRequestedAccess.kauthRequestedAccess & KAUTH_VNODE_WRITE_DATA )
        param->dldRequestedAccess.winRequestedAccess |= DEVICE_WRITE;
    
    assert( 0x0 != param->dldRequestedAccess.winRequestedAccess );
}

//--------------------------------------------------------------------

dld_classic_rights_t
ConvertWinRightsToWinEncryptedRights(
    __in dld_classic_rights_t    rights
    )
{
    dld_classic_rights_t    convertedRights;
    
    convertedRights = rights & ( DEVICE_ENCRYPTED_READ |
                                 DEVICE_ENCRYPTED_WRITE |
                                 DEVICE_ENCRYPTED_DIRECT_WRITE |
                                 DEVICE_ENCRYPTED_DISK_FORMAT );
    
    if( rights & ( DEVICE_DIRECT_READ | DEVICE_EJECT_MEDIA | DEVICE_DIR_LIST | DEVICE_READ | 
                   DEVICE_EXECUTE | DEVICE_PLAY_AUDIO_CD ) )
        convertedRights |= DEVICE_ENCRYPTED_READ;
    
    if( rights & ( DEVICE_VOLUME_DEFRAGMENT | DEVICE_DIR_CREATE | DEVICE_WRITE | DEVICE_RENAME | 
                   DEVICE_DELETE ) )
        convertedRights |= DEVICE_ENCRYPTED_WRITE;
    
    if( rights & DEVICE_DIRECT_WRITE )
        convertedRights |= DEVICE_ENCRYPTED_DIRECT_WRITE;
    
    if (rights & DEVICE_DISK_FORMAT)
        convertedRights |= DEVICE_ENCRYPTED_DISK_FORMAT;
    
    return convertedRights;
}

//--------------------------------------------------------------------

dld_classic_rights_t
ConvertWinEncryptedRightsToWinRights(
    __in dld_classic_rights_t    rights
    )
{
    dld_classic_rights_t    convertedRights;
    
    convertedRights = rights & ~( DEVICE_ENCRYPTED_READ |
                                  DEVICE_ENCRYPTED_WRITE |
                                  DEVICE_ENCRYPTED_DIRECT_WRITE |
                                  DEVICE_ENCRYPTED_DISK_FORMAT );
    
    if( rights & DEVICE_ENCRYPTED_READ )
        convertedRights |= DEVICE_READ;
    
    if( rights & DEVICE_ENCRYPTED_WRITE )
        convertedRights |= DEVICE_WRITE;
    
    if( rights & DEVICE_ENCRYPTED_DIRECT_WRITE )
        convertedRights |= DEVICE_DIRECT_WRITE;
    
    if (rights & DEVICE_ENCRYPTED_DISK_FORMAT)
        convertedRights |= DEVICE_DISK_FORMAT;
    
    return convertedRights;
}

//--------------------------------------------------------------------

char *
DldStrcat(char *dest, int destSize, const char *src)
{
    size_t i,j;
    
    if( 1 == destSize ){
        
        dest[0] = '\0';
        return dest;
    }
    
    for (i = 0; dest[i] != '\0'; i++)
        ;
    
    for (j = 0; src[j] != '\0' && (i+j) < (destSize - 1); j++)
        dest[i+j] = src[j];
    
    dest[i+j] = '\0';
    return dest;
}

//--------------------------------------------------------------------

kauth_cred_t
DldGetRealUserCredReferenceByUID( __in uid_t uid, __in gid_t gid )
{
    //
    // for a reference look on the kauth_cred_setresgid()'s source code
    // or 
    // kauth_cred_copy_real(kauth_cred_t cred)
    //
    
    kauth_cred_t    realUserCredentialRef;
    
    DldKauthCredEntry* credEntry = gCredsArray->getEntryByUidRef( uid );
    if( credEntry ){
        
        realUserCredentialRef = credEntry->getReferencedCred();
        assert( realUserCredentialRef );
        
        credEntry->release();
        DLD_DBG_MAKE_POINTER_INVALID( credEntry );
        
        return realUserCredentialRef;
    }
    
    //
    // A VERY IMPOTANT NOTICE - the size of the ucred structure for 10.5 and 10.6 differs,
    // that is why the 10.5 SDK can't be used to compile the kext for 10.6, the same is
    // true for 10.7 uncompatibility with 10.6 and 10.5
    //
    struct ucred    templateCred;
    
    bzero( &templateCred, sizeof( templateCred ) );
    
    //
    // set the effective UserID, RealUID, SavedUID and GroupID to real ones
    //
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
    templateCred.cr_posix.cr_uid   = uid;
    templateCred.cr_posix.cr_ruid  = uid;
    templateCred.cr_posix.cr_svuid = uid;
    templateCred.cr_posix.cr_gmuid = uid; // for group membership check via a userland
    templateCred.cr_posix.cr_groups[0] = gid;
    templateCred.cr_posix.cr_ngroups = 0x1;
#else
    templateCred.cr_uid   = uid;
    templateCred.cr_ruid  = uid;
    templateCred.cr_svuid = uid;
    templateCred.cr_gmuid = uid; // for group membership check via a userland
    templateCred.cr_groups[0] = gid;
    templateCred.cr_ngroups = 0x1;
#endif
    //
    // create a credential for a real user,
    // actually, kauth_cred_create can't fail, 
    // it sticks forever in case of low memory
    //
    realUserCredentialRef = kauth_cred_create( &templateCred );
    assert( realUserCredentialRef );
    
    //
    // the caller must call kauth_cred_unref()
    //
    return realUserCredentialRef;
}

//--------------------------------------------------------------------

kauth_cred_t
DldGetRealUserCredReferenceByEffectiveUser( __in kauth_cred_t effectiveUserCredential )
{
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
    return DldGetRealUserCredReferenceByUID( kauth_cred_getruid( effectiveUserCredential ), kauth_cred_getrgid( effectiveUserCredential ) );
#else
    return DldGetRealUserCredReferenceByUID( effectiveUserCredential->cr_ruid, effectiveUserCredential->cr_rgid );
#endif
}

//--------------------------------------------------------------------

kauth_cred_t
DldGetAuditUserCredReferenceByEffectiveUser( __in kauth_cred_t effectiveUserCredential )
{
    
    if( ! effectiveUserCredential->cr_audit.as_aia_p )
        return NULL;
    
    return DldGetRealUserCredReferenceByUID( effectiveUserCredential->cr_audit.as_aia_p->ai_auid, 0x0 );

}

//--------------------------------------------------------------------

//
// DldReleaseResources() must be called for every call to isAccessAllowed()
//
void
isAccessAllowed( __inout DldAccessCheckParam* param )
{
    DldDeviceType             deviceType;
    DldDeviceType             majorType;
    DldDeviceType             parentType;
    DldAccessEval             accesEval;
    DldIOService*             dldIOService;
    dld_classic_rights_t      requestedAccess;
    dld_classic_rights_t      requestedAccessForProviders;
    DldTypePermissionsArray*  permArray;
    int                       i = (-1);
    DldKauthCredEntry*        currentCredEntry = NULL;
    bool                      checkCompleted = false;
    bool                      checkForLogOnly = param->checkForLogOnly;
    kauth_cred_t              credArray[ 4 ] = { NULL };
    int                       credArrayIndx  = 0x0;
    kauth_cred_t              effectiveUserCredential     = NULL;
    kauth_cred_t              realUserCredentialRef       = NULL;
    kauth_cred_t              auditCredentialRef          = NULL;
    kauth_cred_t              currentProcessCredentialRef = NULL;
    int			              last_ae_result = KAUTH_RESULT_DEFER;
    uid_t	                  uidThatStoppedCheck = (-1); // used only for the debug audit
    
    assert( preemption_enabled() );
    assert( param->service || param->dldIOService || 0x0 != deviceType.combined );
    assert( NULL == param->activeKauthCredEntry );
    assert( NULL == param->decisionKauthCredEntry );
    
    if( 0x0 == param->dldRequestedAccess.winRequestedAccess ){
        
        //
        // the caller didn't converted KAUTH rights to the classic DL rights
        // apply the default conversion function
        //
        ::convertKauthToWin( param );
    }
    
#if defined( DBG )
    deviceType.combined = (-1);
    majorType.combined  = (-1);
    parentType.combined = (-1);
#endif// DBG
    
#if defined( DBG )
    //
    // check that DldAcquireResources() was called
    //
    assert( param->resourcesAcquired );
#endif//DBG
    
    //
    // TO DO - use param->dldRequestedAccess.winRequestedAccess when the daemon will be able
    // to send an actual settings
    //
    //requestedAccess = param->dldRequestedAccess.kauthRequestedAccess;
    requestedAccess = param->dldRequestedAccess.winRequestedAccess;
    requestedAccessForProviders = requestedAccess;
    effectiveUserCredential = param->credential;
    
    //
    // fill in with the default values
    //
    bzero( &param->output, sizeof( param->output ) );
    
    if( kDldAclTypeUnknown == param->aclType || param->aclType >= kDldAclTypeMax ){
        
        assert( !"an unknown type" );
        return;
        
    }// end switch( param->type )
    
    if( 0x0 == requestedAccess && 0x0 == requestedAccessForProviders )
        return;
    
    if( kDefaultUserSelectionFlavor != param->userSelectionFlavor ){
        
        //
        // the starting point is the active user
        //
        
        gCredsArray->LockShared();
        {// start of the lock
            
            param->activeKauthCredEntry = gCredsArray->getActiveUserEntry();
            if( NULL == param->activeKauthCredEntry && 0x0 != gCredsArray->getCount() ){
                
                //
                // a blind shot - take the first entry in the array
                //
                param->activeKauthCredEntry = gCredsArray->getEntry(0);
                
            }
            
            if( param->activeKauthCredEntry )
                param->activeKauthCredEntry->retain();

        }// end of the lock
        gCredsArray->UnlockShared();
        
        if( param->activeKauthCredEntry ){
            
            currentCredEntry = param->activeKauthCredEntry;
            currentCredEntry->retain();
            
        } else {
            
            //
            // if there is no active user the current thread credentials will
            // be used, this may be a kernel thread with a root credentials
            // or some internal user, so it is better to exit as this
            // not the user want - a random access denying when for example
            // the system is initializing and no any user's launchd agent has started
            //
            return;
        }
        
    } else {
        
        if( NULL == effectiveUserCredential ){
            
            //
            // use the current process credentials
            //
            currentProcessCredentialRef = kauth_cred_proc_ref( current_proc() );
            assert( currentProcessCredentialRef );
            if( !currentProcessCredentialRef ){
                
                DBG_PRINT_ERROR(( "kauth_cred_proc_ref( current_proc() ) failed\n" ));
                return;
            }// end if( !currentProcessCredentialRef )
            
            effectiveUserCredential = currentProcessCredentialRef;
        }
        
        //
        // if the effective user and the real user differs then use
        // the real user as setuid bit breaches the DL security
        // settings
        //
        assert( effectiveUserCredential );
        
        uid_t  uid;  // effective user ID
        uid_t  ruid; // real user ID
        uid_t  auid; // audit user ID
        
        uid  = kauth_cred_getuid( effectiveUserCredential );
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
        ruid = kauth_cred_getruid( effectiveUserCredential );
#else
        ruid = effectiveUserCredential->cr_ruid;
#endif
        
        if( NULL == effectiveUserCredential->cr_audit.as_aia_p || AU_DEFAUDITID == effectiveUserCredential->cr_audit.as_aia_p->ai_auid )
            auid = ruid;
        else
            auid = effectiveUserCredential->cr_audit.as_aia_p->ai_auid;

        //
        // Locum has ruid == 0 so it is unable to initiate copying FROM a drive if the access for root is not granted
        // to the drive while the user has read access, this is counterintuitive for customers, so do not add the root
        // in the array of users to be checked
        //
        if( uid != ruid && 0x0 != ruid ){
            
            //
            // create a credential for a real user,
            // actually, kauth_cred_create can't fail, 
            // it sticks forever in case of low memory
            //
            realUserCredentialRef = DldGetRealUserCredReferenceByEffectiveUser( effectiveUserCredential );
            assert( realUserCredentialRef );
            if( realUserCredentialRef ){
                
                //credArray[ 1 ] = realUserCredentialRef; // see some lines below, it is set there again
                
            } else {
                
                //
                // carry on with the existing credentials
                //
                DBG_PRINT_ERROR(("DldGetRealUserCredReferenceByEffectiveUser() failed\n"));
            }
            
        }// end if( ruid != euid )
        
        
        if( auid != ruid && ruid == uid && 0x0 == uid ){
            
            //
            // most propbably this is a case of elevated privileges ( aka sudo )
            // when the audit user ID is inhereted from a real user but both ruid and uid are changed to the root ID
            //
            auditCredentialRef = DldGetAuditUserCredReferenceByEffectiveUser( effectiveUserCredential );
            assert( auditCredentialRef );
            if( auditCredentialRef ){
                
                //credArray[ 2 ] = realUserCredentialRef; // see some lines below, it is set there again
                
            } else {
                
                //
                // carry on with the existing credentials
                //
                DBG_PRINT_ERROR(("DldGetAuditUserCredReferenceByEffectiveUser() failed\n"));
            }
            
        }

        if( NULL == realUserCredentialRef ){
            
            //
            // the last chance to catch a cheating user, if a process does not inherit its credentials from a parent
            // then it would have CRF_MAC_ENFORCE flag set, in that case we are setting the real user credentials
            // to the current logged user credentials to prevent a logged user with admin privileges from
            // elevating his privileges to superuser with uid == 0 and ruid == 0 and removed audit info
            //
            
#if (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)
            if( 0x0 != ( CRF_MAC_ENFORCE & effectiveUserCredential->cr_posix.cr_flags ) ){
#else
            if( 0x0 != ( CRF_MAC_ENFORCE & effectiveUserCredential->cr_flags ) ){
#endif
                
                realUserCredentialRef = gCredsArray->getActiveUserCredentialsRef();
            }
            
        } // end if( NULL == realUserCredentialRef )
        
        int i = 0x0;
        
        credArray[ i++ ] = effectiveUserCredential;    // an effective user credential
        assert( credArray[0] );
        
        if( realUserCredentialRef )
            credArray[ i++ ] = realUserCredentialRef;  // a real user credential
        
        if( auditCredentialRef )
            credArray[ i++ ] = auditCredentialRef;     // an audit user credentials
        
        credArray[ i++ ] = NULL;                       // always NULL, a terminating entry
        assert( i <= DLD_STATIC_ARRAY_SIZE( credArray ) );
        
    }
    
    permArray = DldGetPermissionsArrayByAclType( param->aclType );
    assert( permArray );
    
    if( param->dldIOService ){
        
        assert( param->dldIOService );
        
        dldIOService = param->dldIOService;
        dldIOService->retain();
        
    } else if( param->service ){
     
        assert( param->service );
        
        dldIOService = DldIOService::RetrieveDldIOServiceForIOService( param->service );
        
    } else {
        
        //
        // the param->deviceType value will be used as a device type
        //
        dldIOService = NULL;
        
    }// end !if( !dldIOService )

    //
    // if there is no service and no device type then there is no ground to stay on to make a decision,
    // we just do not know what acl to use
    //
    if( !dldIOService && 0x0 == param->deviceType.combined )
        goto __exit;
        
    //
    // don't apply the security settings to not started objects as this might preclude
    // a driver stack from intialization and moreover results in false negative
    // because of information lack for the full device description
    //
    if( dldIOService && kDldPnPStateStarted != dldIOService->getPnPState() )
        goto __exit;
    
    if( dldIOService && dldIOService->getObjectProperty() ){
        
        //
        // if the user client has not been attached then do not check security
        // as there no data will be sent or received from a user, for some
        // stacks there is no user clients at all so the userClientAttached
        // value is set to true after the initialization
        //
        if( !dldIOService->getObjectProperty()->dataU.property->userClientAttached )
            goto __exit;
    }
    
    //
    // encrypted devices use a special set of rights
    //
    assert( !( dldIOService->getObjectProperty() && dldIOService->getObjectProperty()->dataU.property->encryptionProvider > DLD_STATIC_ARRAY_SIZE( gEncryptionProvider ) ) );
    if( dldIOService->getObjectProperty() && gEncryptionProvider[ dldIOService->getObjectProperty()->dataU.property->encryptionProvider ].enabled ){
            
        //
        // the encryption settings are supported only for removable devices
        //
        if( DEVICE_TYPE_REMOVABLE == dldIOService->getObjectProperty()->dataU.property->deviceType.type.major ){
            
            param->wasProcessedAsEncrypted = true;
            requestedAccess = ConvertWinRightsToWinEncryptedRights( requestedAccess );
            if( 0x0 == requestedAccess && 0x0 == requestedAccessForProviders )
                goto __exit;
        }
    }
    
    //
    // the dldIOService property is a winner
    //
    if( dldIOService && dldIOService->getObjectProperty() ){
        
        assert( dldIOService->getObjectProperty() );
        
        deviceType = dldIOService->getObjectProperty()->dataU.property->deviceType;
        if( 0x0 == deviceType.combined )
            deviceType = param->deviceType;
        
        //
        // in that case save the device type as it will be required for logging
        //
        param->deviceType = deviceType;
        
    } else {
        
        assert( 0x0 != deviceType.combined );
        deviceType = param->deviceType;
    }
    
    assert( (-1) != deviceType.combined );
    if( 0x0 == deviceType.combined )
        goto __exit;
        
    /// DEBUG START
    /*if( effectiveUserCredential && effectiveUserCredential->cr_posix.cr_uid == 0x0 && DEVICE_TYPE_REMOVABLE == deviceType.type.major ){
        
        proc_t proc;
        
        proc = current_proc();
    }*/
    /// DEBUG END
        
    do{
        
        kauth_cred_t    credential;
        WhiteListStatus usbWhiteListStatus;
        
        assert( !( kDefaultUserSelectionFlavor != param->userSelectionFlavor && NULL == currentCredEntry ) );
        
        //
        // get the credentials for the current processing user
        //
        if( currentCredEntry ){
            
            credential = currentCredEntry->getCred();
            assert( !param->credential );// what does the caller want?
            
        } else {
            
            credential = credArray[ credArrayIndx ];
        }// end else for if( currentCredEntry )
        
        assert( credential );
        
        //
        // check by the full type ( major+minor ) e.g. REMOVABLE+USB
        //
        bzero( &accesEval, sizeof( accesEval ) );
        accesEval.type           = deviceType;
        accesEval.typeFlavor     = DldFullTypeFlavor;
        accesEval.ae_requested   = requestedAccess;
        accesEval.cred           = credential;
        accesEval.ae_result      = checkForLogOnly? last_ae_result : KAUTH_RESULT_DEFER;
        accesEval.null_acl_found = false;
        
        //
        // get the white list settings for USB devices, here we use param's type as the underlying device might
        // be whitelisted with propagate flag set so we need to check for the underlying USB settings even if a
        // removable settings is checked - a USB white list can render the removable settings if the "propagate up" flag
        // is set, the check for the hard drives as they are never considered as USB devices, note that this check
        // should be done for every request as white list state influences not only the security checking but the auditing
        // processing, so the param->inWhiteList value will be checked later while performing the audit
        //
        //
        DldFetchUsbWhiteListSettings( dldIOService, param->deviceType, credential, &usbWhiteListStatus );
        assert( usbWhiteListStatus.whiteListChecked );
                
        if( false == usbWhiteListStatus.whiteListed[ DldFullTypeFlavor ] ){
            
            if( !checkForLogOnly ){
                
                //
                // check by ACL
                //
                permArray->isAccessAllowed( &accesEval );
                //
                // if not all requested permissions have been cleared then deny access,
                // the logic is in consisten with the Windows's SeAccessCheck()
                // ( for reference see Programming Windows Security by Keith Brown,
                // pp 198-200 )
                //
                if( KAUTH_RESULT_DEFER == accesEval.ae_result &&
                   0x0 != accesEval.ae_residual &&
                   !accesEval.null_acl_found )
                    accesEval.ae_result = KAUTH_RESULT_DENY;
                
                //
                // check for special quirks which overwrite the ACL settings
                //
                DldApplySecurityQuirks( dldIOService, param, &accesEval, &usbWhiteListStatus );
                
                //
                // used when processing "log only"
                //
                last_ae_result = accesEval.ae_result;
                
                
                if( KAUTH_RESULT_DENY == accesEval.ae_result ){
                    
                    switch( param->aclType ){
                            
                        case kDldAclTypeSecurity:
                            
                            param->output.access.result[ DldFullTypeFlavor ].disable = true;
                            checkCompleted = true;
                            DBG_PRINT(("access denied on DldFullTypeFlavor: dldIOService=0x%p\n",dldIOService));
                            break;
                            
                        case kDldAclTypeShadow:
                            
                            param->output.shadow.result[ DldFullTypeFlavor ].shadow  = false;
                            break;
                            
                        case kDldAclTypeDiskCAWL:
                            
                            param->output.diskCAWL.result[ DldFullTypeFlavor ].checkByCAWL = false;
                            break;
                            
                        default:
                            
                            panic( "an unknown type" );
                            break;
                            
                    }// end switch( param->type )
                    
                } else if( KAUTH_RESULT_ALLOW == accesEval.ae_result ){
                    
                    assert( !accesEval.null_acl_found );
                    
                    switch( param->aclType ){
                            
                        case kDldAclTypeSecurity:
                            
                            param->output.access.result[ DldFullTypeFlavor ].disable = false;
                            break;
                            
                        case kDldAclTypeShadow:
                            
                            param->output.shadow.result[ DldFullTypeFlavor ].shadow  = true;
                            checkCompleted = true;
                            break;
                            
                        case kDldAclTypeDiskCAWL:
                            
                            param->output.diskCAWL.result[ DldFullTypeFlavor ].checkByCAWL = true;
                            checkCompleted = true;
                            break;
                            
                        default:
                            
                            panic( "an unknown type" );
                            break;
                            
                    }// end switch( param->type )
                    
                } else {
                    
                    assert( KAUTH_RESULT_DEFER == accesEval.ae_result );
                }
                
            } // end if( !checkForLogOnly )
            
            
            if( kDldAclTypeSecurity == param->aclType &&
                false == param->output.access.result[ DldFullTypeFlavor ].log ){
                
                //
                // check for logging, even if KAUTH_RESULT_DEFER was returned
                //
                DldTypePermissionsArray*  permLogArray;
                
                bzero( &accesEval, sizeof( accesEval ) );
                accesEval.type           = deviceType;
                accesEval.typeFlavor     = DldFullTypeFlavor;
                accesEval.ae_requested   = requestedAccess;
                accesEval.cred           = credential;
                accesEval.ae_result      = KAUTH_RESULT_DEFER;
                accesEval.null_acl_found = false;
                
                if( param->output.access.result[ DldFullTypeFlavor ].disable )
                    permLogArray = DldGetPermissionsArrayByAclType( kDldAclTypeLogDenied );
                else
                    permLogArray = DldGetPermissionsArrayByAclType( kDldAclTypeLogAllowed );
                
                assert( permLogArray );
                
                permLogArray->isAccessAllowed( &accesEval );
                //
                // if not all requested permissions have been cleared then allow logging,
                // as the allowed entries have been found for some permissions, this is
                // contrary to granting access
                //
                if( KAUTH_RESULT_DEFER == accesEval.ae_result && accesEval.ae_requested != accesEval.ae_residual )
                    accesEval.ae_result = KAUTH_RESULT_ALLOW;
                
                //
                // if NULL ACL is found then do not log as there is actually no settings,
                // yet again - this is contrary to granting access
                //
                if( KAUTH_RESULT_ALLOW == accesEval.ae_result && !accesEval.null_acl_found ){
                    
                    param->output.access.result[ DldFullTypeFlavor ].log = true;
                    param->output.access.result[ DldFullTypeFlavor ].rightsToAudit = (accesEval.ae_requested & ~(accesEval.ae_residual));
                    
                    //
                    // save the credentials that is being used for logging
                    //
                    
                    assert( NULL == param->loggingKauthCred[ DldFullTypeFlavor ] );
                    assert( DldFullTypeFlavor < DLD_STATIC_ARRAY_SIZE( param->loggingKauthCred ) );
                    
                    param->loggingKauthCred[ DldFullTypeFlavor ] = credential;
                    kauth_cred_ref( param->loggingKauthCred[ DldFullTypeFlavor ] );
                    
                } else {
                    param->output.access.result[ DldFullTypeFlavor ].log = false;
                }
                
            }// end if( kDldAclTypeSecurity == param->type )
            
        } // end if( false == whiteListed[ DldFullTypeFlavor ] )
        
        if( checkCompleted ){
            
            if( (-1) == uidThatStoppedCheck )
                uidThatStoppedCheck = kauth_cred_getuid( credential );
            
            if( kDldAclTypeSecurity == param->aclType )
                checkForLogOnly = true;
            else
                goto __exit_while;
            
        } // if( checkCompleted )
        
        //
        // check by the parent type(i.e. minor type), e.g. USB,
        // there is no decision here if
        // we reach this point, so checkCompleted
        // must be false
        //
        assert( !checkCompleted || checkForLogOnly );
        if( false == usbWhiteListStatus.whiteListed[ DldParentTypeFlavor ] && param->checkParentType && DLD_DEVICE_TYPE_UNKNOWN != deviceType.type.minor ){
            
            parentType = deviceType;
            parentType.type.major = parentType.type.minor;
            parentType.type.minor = DLD_DEVICE_TYPE_UNKNOWN;
            
            bzero( &accesEval, sizeof( accesEval ) );
            accesEval.type           = parentType;
            accesEval.typeFlavor     = DldParentTypeFlavor;
            accesEval.ae_requested   = DldConvertWinRightsChildToParent( deviceType.type.major, parentType.type.major, requestedAccessForProviders );
            accesEval.cred           = credential;
            accesEval.ae_result      = checkForLogOnly? last_ae_result : KAUTH_RESULT_DEFER;
            accesEval.null_acl_found = false;
            
            param->parentAccess = accesEval.ae_requested;
            
            if( !checkForLogOnly ){
                
                //
                // check by ACL
                //
                permArray->isAccessAllowed( &accesEval );
                //
                // if not all requested permissions have been cleared then deny access,
                // the logic is in consisten with the Windows's SeAccessCheck()
                // ( for reference see Programming Windows Security by Keith Brown,
                // pp 198-200 )
                //
                if( KAUTH_RESULT_DEFER == accesEval.ae_result &&
                    0x0 != accesEval.ae_residual &&
                    !accesEval.null_acl_found )
                        accesEval.ae_result = KAUTH_RESULT_DENY;
                
                //
                // check for special quirks which overwrite the ACL settings
                //
                DldApplySecurityQuirks( dldIOService, param, &accesEval, &usbWhiteListStatus );
                
                //
                // used for "log only" case
                //
                last_ae_result = accesEval.ae_result;
                
                if( KAUTH_RESULT_DENY == accesEval.ae_result ){
                    
                    switch( param->aclType ){
                            
                        case kDldAclTypeSecurity:
                            
                            param->output.access.result[ DldParentTypeFlavor ].disable = true;
                            checkCompleted = true;
                            DBG_PRINT(("access denied on DldParentTypeFlavor: dldIOService=0x%p\n",dldIOService));
                            break;
                            
                        case kDldAclTypeShadow:
                            
                            param->output.shadow.result[ DldParentTypeFlavor ].shadow  = false;
                            break;
                            
                        case kDldAclTypeDiskCAWL:
                            
                            param->output.diskCAWL.result[ DldParentTypeFlavor ].checkByCAWL = false;
                            break;
                            
                        default:
                            
                            panic( "an unknown type" );
                            break;
                            
                    }// end switch( param->type )
                    
                } else if( KAUTH_RESULT_ALLOW == accesEval.ae_result ){
                    
                    assert( !accesEval.null_acl_found );
                    
                    switch( param->aclType ){
                            
                        case kDldAclTypeSecurity:
                            
                            param->output.access.result[ DldParentTypeFlavor ].disable = false;
                            break;
                            
                        case kDldAclTypeShadow:
                            
                            param->output.shadow.result[ DldParentTypeFlavor ].shadow  = true;
                            checkCompleted = true;
                            break;
                            
                        case kDldAclTypeDiskCAWL:
                            
                            param->output.diskCAWL.result[ DldParentTypeFlavor ].checkByCAWL = true;
                            checkCompleted = true;
                            break;
                            
                        default:
                            
                            panic( "an unknown type" );
                            break;
                            
                    }// end switch( param->type )
                    
                } else {
                    
                    assert( KAUTH_RESULT_DEFER == accesEval.ae_result );
                }
                
                
            } // end if( !checkForLogOnly )
            
            
            if( kDldAclTypeSecurity == param->aclType && false == param->output.access.result[ DldParentTypeFlavor ].log ){
                
                //
                // check for logging, even if KAUTH_RESULT_DEFER was returned
                //
                DldTypePermissionsArray*  permLogArray;
                
                bzero( &accesEval, sizeof( accesEval ) );
                accesEval.type           = parentType;
                accesEval.typeFlavor     = DldParentTypeFlavor;
                accesEval.ae_requested   = param->parentAccess;
                accesEval.cred           = credential;
                accesEval.ae_result      = KAUTH_RESULT_DEFER;
                accesEval.null_acl_found = false;
                
                if( param->output.access.result[ DldParentTypeFlavor ].disable )
                    permLogArray = DldGetPermissionsArrayByAclType( kDldAclTypeLogDenied );
                else
                    permLogArray = DldGetPermissionsArrayByAclType( kDldAclTypeLogAllowed );
                
                assert( permLogArray );
                
                permLogArray->isAccessAllowed( &accesEval );
                //
                // if not all requested permissions have been cleared then allow logging,
                // as the allowed entries have been found for some permissions, this is
                // contrary to granting access
                //
                if( KAUTH_RESULT_DEFER == accesEval.ae_result && accesEval.ae_requested != accesEval.ae_residual )
                    accesEval.ae_result = KAUTH_RESULT_ALLOW;
                
                //
                // if NULL ACL is found then do not log as there is actually no setting,
                // yet again - this is contrary to granting access
                //
                if( KAUTH_RESULT_ALLOW == accesEval.ae_result && !accesEval.null_acl_found ){
                    
                    param->output.access.result[ DldParentTypeFlavor ].log = true;
                    param->output.access.result[ DldParentTypeFlavor ].rightsToAudit = (accesEval.ae_requested & ~(accesEval.ae_residual));
                    
                    //
                    // save the credentials that is being used for logging
                    //
                    
                    assert( NULL == param->loggingKauthCred[ DldParentTypeFlavor ] );
                    assert( DldParentTypeFlavor < DLD_STATIC_ARRAY_SIZE( param->loggingKauthCred ) );
                    
                    param->loggingKauthCred[ DldParentTypeFlavor ] = credential;
                    kauth_cred_ref( param->loggingKauthCred[ DldParentTypeFlavor ] );
                    
                } else {
                    
                    param->output.access.result[ DldParentTypeFlavor ].log = false;
                }
                
            }// end if( kDldAclTypeSecurity == param->type )
            
            if( checkCompleted ){
                
                if( (-1) == uidThatStoppedCheck )
                    uidThatStoppedCheck = kauth_cred_getuid( credential );
                
                if( kDldAclTypeSecurity == param->aclType )
                    checkForLogOnly = true;
                else
                    goto __exit_while;
                
            } // if( checkCompleted )
            
        } // end if( false == whiteListed[ DldParentTypeFlavor ] && param->checkParentType )
                
        //
        // check by the major type, e.g. REMOVABLE
        //
        
        majorType = deviceType;
        majorType.type.minor = DLD_DEVICE_TYPE_UNKNOWN;// remove the minor type
        
        if( false == usbWhiteListStatus.whiteListed[ DldMajorTypeFlavor ] && deviceType.combined != majorType.combined ){
            
            bzero( &accesEval, sizeof( accesEval ) );
            accesEval.type           = majorType;
            accesEval.typeFlavor     = DldMajorTypeFlavor;
            accesEval.ae_requested   = requestedAccess;
            accesEval.cred           = credential;
            accesEval.ae_result      = checkForLogOnly? last_ae_result : KAUTH_RESULT_DEFER;
            accesEval.null_acl_found = false;
            
            if( !checkForLogOnly ){
                
                //
                // check by ACL
                //
                permArray->isAccessAllowed( &accesEval );
                //
                // if not all requested permissions have been cleared then deny access,
                // the logic is in consisten with the Windows's SeAccessCheck()
                // ( for reference see Programming Windows Security by Keith Brown,
                // pp 198-200 )
                //
                if( KAUTH_RESULT_DEFER == accesEval.ae_result &&
                    0x0 != accesEval.ae_residual &&
                    !accesEval.null_acl_found )
                        accesEval.ae_result = KAUTH_RESULT_DENY;
                
                //
                // check for special quirks which overwrite the ACL settings
                //
                DldApplySecurityQuirks( dldIOService, param, &accesEval, &usbWhiteListStatus );
                assert( usbWhiteListStatus.whiteListChecked );
                
                
                if( KAUTH_RESULT_DENY == accesEval.ae_result ){
                    
                    switch( param->aclType ){
                            
                        case kDldAclTypeSecurity:
                            
                            param->output.access.result[ DldMajorTypeFlavor ].disable = true;
                            checkCompleted = true;
                            DBG_PRINT(("access denied on DldMajorTypeFlavor: dldIOService=0x%p\n",dldIOService));
                            break;
                            
                        case kDldAclTypeShadow:
                            
                            param->output.shadow.result[ DldMajorTypeFlavor ].shadow  = false;
                            break;
                            
                        case kDldAclTypeDiskCAWL:
                            
                            param->output.diskCAWL.result[ DldMajorTypeFlavor ].checkByCAWL = false;
                            break;
                            
                        default:
                            
                            panic( "an unknown type" );
                            break;
                            
                    }// end switch( param->type )
                    
                } else if( KAUTH_RESULT_ALLOW == accesEval.ae_result ){
                    
                    assert( !accesEval.null_acl_found );
                    
                    switch( param->aclType ){
                            
                        case kDldAclTypeSecurity:
                            
                            param->output.access.result[ DldMajorTypeFlavor ].disable = false;
                            break;
                            
                        case kDldAclTypeShadow:
                            
                            param->output.shadow.result[ DldMajorTypeFlavor ].shadow  = true;
                            checkCompleted = true;
                            break;
                            
                        case kDldAclTypeDiskCAWL:
                            
                            param->output.diskCAWL.result[ DldMajorTypeFlavor ].checkByCAWL = true;
                            checkCompleted = true;
                            break;
                            
                        default:
                            
                            panic( "an unknown type" );
                            break;
                            
                    }// end switch( param->type )
                    
                } else {
                    
                    assert( KAUTH_RESULT_DEFER == accesEval.ae_result );
                }
                
            } // end if( !checkForLogOnly )
            
            
            if( kDldAclTypeSecurity == param->aclType && false == param->output.access.result[ DldMajorTypeFlavor ].log ){
                
                //
                // check for logging, even if KAUTH_RESULT_DEFER was returned
                //
                DldTypePermissionsArray*  permLogArray;
                
                bzero( &accesEval, sizeof( accesEval ) );
                accesEval.type           = majorType;
                accesEval.typeFlavor     = DldMajorTypeFlavor;
                accesEval.ae_requested   = requestedAccess;
                accesEval.cred           = credential;
                accesEval.ae_result      = KAUTH_RESULT_DEFER;
                accesEval.null_acl_found = false;
                
                if( param->output.access.result[ DldMajorTypeFlavor ].disable )
                    permLogArray = DldGetPermissionsArrayByAclType( kDldAclTypeLogDenied );
                else
                    permLogArray = DldGetPermissionsArrayByAclType( kDldAclTypeLogAllowed );
                
                assert( permLogArray );
                
                //
                // check by ACL
                //
                permLogArray->isAccessAllowed( &accesEval );
                //
                // if not all requested permissions have been cleared then allow logging,
                // as the allowed entries have been found for some permissions, this is
                // contrary to granting access
                //
                if( KAUTH_RESULT_DEFER == accesEval.ae_result && accesEval.ae_requested != accesEval.ae_residual )
                    accesEval.ae_result = KAUTH_RESULT_ALLOW;
                
                //
                // if NULL ACL is found then do not log as there is actually no setting,
                // yet again - this is contrary to granting access
                //
                if( KAUTH_RESULT_ALLOW == accesEval.ae_result && !accesEval.null_acl_found ){
                    
                    param->output.access.result[ DldMajorTypeFlavor ].log = true;
                    param->output.access.result[ DldMajorTypeFlavor ].rightsToAudit = (accesEval.ae_requested & ~(accesEval.ae_residual));
                    
                    //
                    // save the credentials that is being used for logging
                    //
                    
                    assert( NULL == param->loggingKauthCred[ DldMajorTypeFlavor ] );
                    assert( DldMajorTypeFlavor < DLD_STATIC_ARRAY_SIZE( param->loggingKauthCred ) );
                    
                    param->loggingKauthCred[ DldMajorTypeFlavor ] = credential;
                    kauth_cred_ref( param->loggingKauthCred[ DldMajorTypeFlavor ] );
                    
                } else {
                    
                    param->output.access.result[ DldMajorTypeFlavor ].log = false;
                }
                
            }// end if( kDldAclTypeSecurity == param->type )
            
        }// end if( false == whiteListed[ DldMajorTypeFlavor ] && deviceType.combined != majorType.combined )
        
        if( checkCompleted ){
            
            if( (-1) == uidThatStoppedCheck )
                uidThatStoppedCheck = kauth_cred_getuid( credential );
            
            if( kDldAclTypeSecurity == param->aclType )
                checkForLogOnly = true;
            else
                goto __exit_while;
            
        } // end if( checkCompleted )
        
    __exit_while:
        
        //
        // if checkCompleted is TRUE then a decision has been made for some credentials,
        // overwise there is no any decision so it is hard to pick up a credentials for logging
        //
        if( checkCompleted ){
            
            if( (-1) == uidThatStoppedCheck )
                uidThatStoppedCheck = kauth_cred_getuid( credential );
            
            //
            // the checking has completed, save the REFERENCED entry
            // for the cred entry that was used to make a decision
            //
            if( currentCredEntry ){
                
                assert( NULL == param->decisionKauthCredEntry );
                
                //
                // save teh retained object and set the currentCredEntry
                // value to NULL to avoid releasing at the tail of this function
                // and to stop processing
                //                
                param->decisionKauthCredEntry = currentCredEntry;
                currentCredEntry = NULL;
                
            }// end if( decisionCredEntry )
            
            //
            // the processing is completed, the NULL value for the currentCredEntry
            // pointer marks this
            //
            assert( NULL == currentCredEntry );
            
        }// end if( checkCompleted )
        
        
        if( NULL != currentCredEntry ){
            
#if defined( DBG )
            DldKauthCredEntry*  prevEntry = currentCredEntry;
#endif//DBG
            
            //
            // move to the next logged user's credentials
            //
            i += 0x1;
            gCredsArray->LockShared();
            {// start of the lock

                //
                // do this under the lock to avoid ABA problem
                //
                if( currentCredEntry )
                    currentCredEntry->release();
                
                //
                // as the lock was not hold some entries might have been removed
                //
                if( i >= gCredsArray->getCount() ){
                    
                    currentCredEntry = NULL;
                    
                } else {
                    
                    int count = gCredsArray->getCount();
                    
                    while( i < count ){
                        
                        //
                        // skip the already checked entries
                        //
                        if( currentCredEntry == gCredsArray->getEntry( i ) ||
                            currentCredEntry == param->activeKauthCredEntry ){
                            
                            i += 0x1;
                            continue;
                        }
                        
                        //
                        // get the next entry
                        //
                        currentCredEntry = gCredsArray->getEntry( i );
                        break;
                        
                    }// end while( i < gCredsArray->getCount() )
                    
                    if( i == count )
                        currentCredEntry = NULL;
                    else
                        currentCredEntry->retain();
                    
                }// end for else
                
            }// end of the lock
            gCredsArray->UnlockShared();
            
#if defined( DBG )
            assert( prevEntry != currentCredEntry );
#endif//DBG
            
        }// end if( !checkCompleted &&

        
        //
        // only one iteration for the default behaviour, i.e. when the caller provides
        // the exact credentials
        //
    } while( NULL != currentCredEntry || NULL != credArray[ ++credArrayIndx ] );
    
__exit:
        
#if defined(LOG_ACCESS)
    if( gGlobalSettings.logFlags.ACCESS_DENIED ){
        
        //
        // log the output of the function calling
        //
        if( kDldAclTypeSecurity == param->aclType &&
            ( param->output.access.result[ DldFullTypeFlavor ].disable ||
              param->output.access.result[ DldMajorTypeFlavor ].disable ||
              param->output.access.result[ DldParentTypeFlavor ].disable ) ){
               
                //
                // "U" stands for "Unknown"
                //
                uid_t	       uid;			/* effective user id */
                char           procName[ 64 ];
                
                if( effectiveUserCredential )
                    uid  = kauth_cred_getuid( effectiveUserCredential );
                else{
                    
                    kauth_cred_t currentProcCred;
                    
                    currentProcCred = kauth_cred_proc_ref( current_proc() );
                    uid = kauth_cred_getuid( currentProcCred );
                    kauth_cred_unref( &currentProcCred );
                }
               
                const char*    callingFunctionName = "U";
                if( param->sourceFunction )
                    callingFunctionName = param->sourceFunction;
                
                unsigned int   callingLine = param->sourceLine;
                
                procName[ sizeof( procName ) - 0x1 ] = '\0';
                proc_selfname( procName, sizeof(procName) - 0x1 );// suppose that we are in the caller's context
                
                IOService*  service;
                
                if( param->service )
                    service = param->service;
                else
                    service = dldIOService->getSystemService();
                
                assert( service );
                
                //
                // "U" stands for "Unknown"
                //
                const char* className = service->getMetaClass()->getClassName();
                const char* bsdName   = "U";
                
                if( service->getProperty( kIOBSDNameKey ) ){
                    
                    OSObject*  property;
                    property = service->getProperty( kIOBSDNameKey );
                    assert( property );
                    
                    OSString* string;
                    string = OSDynamicCast( OSString, property );
                    assert( string );
                    
                    bsdName = string->getCStringNoCopy();
                    
                }// end if( service->getProperty( kIOBSDNameKey ) )
                
                const char*    vnodeName = "U";
                const char*    vnodeType = "U";
                OSString*      fileNameRef = NULL;
                
                if( param->dldVnode ){
                    
                    fileNameRef = param->dldVnode->getNameRef();
                    
                    vnodeName = fileNameRef->getCStringNoCopy();
                    
                    vnodeType = param->dldVnode->vnodeTypeCStrNoCopy();
                }
                
                char          accessStr[ 100 ];
                unsigned int  mask = 0x1;  
                
                accessStr[0] = '\0';
                
                while( mask ){
                    
                    if( requestedAccess & mask ){
                        
                        if( '\0' != accessStr[0] )
                            DldStrcat( accessStr, sizeof(accessStr), " | " );
                        
                        DldStrcat( accessStr, sizeof(accessStr), DldWinRightsToStr( mask ) );
                    }
                    
                    mask = mask<<1;
                    
                }// end while

                DLD_COMM_LOG( ACCESS_DENIED,
                              (  "!!!DLD: ACESS DENIED: requestedAccess(0x%x)(p:0x%x) is denied as (%u,%u,%u) for euid[%u][s:%u] to (%s:%s) IOService(%p,%p,%s,%s),"
                               " BSD proc[%u][%s], vnode(%s,%s), caller(%s):%u, [ %s ]\n",
                               (unsigned int)requestedAccess,
                               (unsigned int)param->parentAccess,
                               (unsigned int)param->output.access.result[ DldFullTypeFlavor ].disable,
                               (unsigned int)param->output.access.result[ DldMajorTypeFlavor ].disable,
                               (unsigned int)param->output.access.result[ DldParentTypeFlavor ].disable,
                               uid,
                               uidThatStoppedCheck,
                               DldDeviceTypeToString( deviceType.type.major ),
                               DldDeviceTypeToString( deviceType.type.minor ),
                               service,
                               dldIOService,
                               className,
                               bsdName,
                               proc_selfpid(),
                               procName,
                               vnodeName,
                               vnodeType,
                               callingFunctionName,
                               callingLine,
                               accessStr) );
                
                if( fileNameRef )
                    fileNameRef->release();
                DLD_DBG_MAKE_POINTER_INVALID( fileNameRef );
                
                /*
                 KAUTH_DEBUG("%p  AUTH - %s %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s on %s '%s' (0x%x:%p/%p)",
                 vp, vfs_context_proc(ctx)->p_comm,
                 (action & KAUTH_VNODE_ACCESS)		? "access" : "auth",
                 (action & KAUTH_VNODE_READ_DATA)		? vnode_isdir(vp) ? " LIST_DIRECTORY" : " READ_DATA" : "",
                 (action & KAUTH_VNODE_WRITE_DATA)		? vnode_isdir(vp) ? " ADD_FILE" : " WRITE_DATA" : "",
                 (action & KAUTH_VNODE_EXECUTE)		? vnode_isdir(vp) ? " SEARCH" : " EXECUTE" : "",
                 (action & KAUTH_VNODE_DELETE)		? " DELETE" : "",
                 (action & KAUTH_VNODE_APPEND_DATA)		? vnode_isdir(vp) ? " ADD_SUBDIRECTORY" : " APPEND_DATA" : "",
                 (action & KAUTH_VNODE_DELETE_CHILD)		? " DELETE_CHILD" : "",
                 (action & KAUTH_VNODE_READ_ATTRIBUTES)	? " READ_ATTRIBUTES" : "",
                 (action & KAUTH_VNODE_WRITE_ATTRIBUTES)	? " WRITE_ATTRIBUTES" : "",
                 (action & KAUTH_VNODE_READ_EXTATTRIBUTES)	? " READ_EXTATTRIBUTES" : "",
                 (action & KAUTH_VNODE_WRITE_EXTATTRIBUTES)	? " WRITE_EXTATTRIBUTES" : "",
                 (action & KAUTH_VNODE_READ_SECURITY)	? " READ_SECURITY" : "",
                 (action & KAUTH_VNODE_WRITE_SECURITY)	? " WRITE_SECURITY" : "",
                 (action & KAUTH_VNODE_CHANGE_OWNER)		? " CHANGE_OWNER" : "",
                 (action & KAUTH_VNODE_NOIMMUTABLE)		? " (noimmutable)" : "",
                 vnode_isdir(vp) ? "directory" : "file",
                 vp->v_name ? vp->v_name : "<NULL>", action, vp, dvp);
                 */
               
               //__asm__ volatile( "int $0x3" );
           }
        
    }// end if( gGlobalSettings.logAccessDenied )
#endif//#if defined(LOG_ACCESS)
    
    if( realUserCredentialRef )
        kauth_cred_unref( &realUserCredentialRef );
    DLD_DBG_MAKE_POINTER_INVALID( realUserCredentialRef );
    
    if( currentProcessCredentialRef )
        kauth_cred_unref( &currentProcessCredentialRef );
    DLD_DBG_MAKE_POINTER_INVALID( currentProcessCredentialRef );
    
    if( auditCredentialRef )
        kauth_cred_unref( &auditCredentialRef );
    DLD_DBG_MAKE_POINTER_INVALID( auditCredentialRef );
    
    //
    // release the current cred entry
    //
    if( currentCredEntry )
        currentCredEntry->release();
    
    if( dldIOService )
        dldIOService->release();
    
    //
    // the following is more a workaround, i.e. try to provide a caller with
    // some reasonable credentials for logging if credentials have not
    // been determined by the above code
    //
    if( kDefaultUserSelectionFlavor != param->userSelectionFlavor &&
        NULL == param->decisionKauthCredEntry ){
        
        //
        // usually this happens when a quirk has been applied ( e.g. to Apple USB HID ),
        // pick up the first active entry as the current user ( normally there is only 
        // one entry with the "session active" state )
        //
        
        param->decisionKauthCredEntry = param->activeKauthCredEntry;
        if( NULL != param->decisionKauthCredEntry ){
            
            //
            // reference the entry, the caller must call DldReleaseResources() to release
            //
            param->decisionKauthCredEntry->retain();
            
        } else {
            
            assert( NULL == param->decisionKauthCredEntry );
            
            //
            // try our best to find an entry
            //
            
            gCredsArray->LockShared();
            {// start of the lock
                                
                //
                // use the current active user
                //
                param->decisionKauthCredEntry = gCredsArray->getActiveUserEntry();
                
                if( NULL == param->decisionKauthCredEntry ){
                    
                    //
                    // a blind shot - take the first entry in the array
                    //
                    param->activeKauthCredEntry = gCredsArray->getEntry(0);
                }
                
                if( param->decisionKauthCredEntry ){
                    
                    //
                    // reference the entry, the caller must call DldReleaseResources() to release
                    //
                    param->decisionKauthCredEntry->retain();
                }
                
            }// end of the lock
            gCredsArray->UnlockShared();
        }
        
    }// end if( kDefaultUserSelectionFlavor != param->userSelectionFlavor && NULL == currentCredEntry )
    
}

//--------------------------------------------------------------------

void
DldAcquireResources( __inout DldAccessCheckParam* param )
{
#if defined( DBG )
    assert( !param->resourcesAcquired );
    assert( !param->resourcesReleased );
    param->resourcesAcquired = true;
#endif//DBG
}

void
DldReleaseResources( __inout DldAccessCheckParam* param )
{
    
#if defined(DBG)
    assert( !param->resourcesReleased );
    assert( param->resourcesAcquired );
#endif//DBG
    
    if( param->activeKauthCredEntry ){
        
        param->activeKauthCredEntry->release();
        param->activeKauthCredEntry = NULL;
    }
    
    if( param->decisionKauthCredEntry ){
        
        param->decisionKauthCredEntry->release();
        param->decisionKauthCredEntry = NULL;
    }
    
    for( int i = 0x0; i < DLD_STATIC_ARRAY_SIZE( param->loggingKauthCred ); ++i ){
        
        if( param->loggingKauthCred[ i ] ){
            
            kauth_cred_unref( &param->loggingKauthCred[ i ] );
            param->loggingKauthCred[ i ] = NULL;
        }
    }
   
#if defined(DBG)
    param->resourcesReleased = true;
#endif//DBG
    
}

//--------------------------------------------------------------------


