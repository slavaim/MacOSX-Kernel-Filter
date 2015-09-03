/*
 *  DldProtectedFile.cpp
 *  DeviceLock
 *
 *  Created by Slava on 16/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

#include "DldProtectedFile.h"
#include "DldServiceProtection.h"

//--------------------------------------------------------------------

#define super OSObject
OSDefineMetaClassAndStructors( DldProtectedFile, OSObject)

//--------------------------------------------------------------------

DldProtectedFile* DldProtectedFile::withPathAndAcls( __in OSString* path,
                                                     __in_opt DldAclObject* usersAcl,
                                                     __in_opt DldAclWithProcsObject* procsAcl )
{
    DldProtectedFile*  protFile;
    
    assert( preemption_enabled() );
    
    protFile = new DldProtectedFile();
    assert( protFile );
    if( !protFile ){
        
        DBG_PRINT_ERROR(("new DldProtectedFile() failed\n"));
        return NULL;
    } // if( !protProcess )
    
    if( !protFile->init() ){
        
        DBG_PRINT_ERROR(("protFile->init() failed\n"));
        protFile->release();
        return NULL;
    }
    
    protFile->usersAcl = usersAcl;
    if( protFile->usersAcl )
        protFile->usersAcl->retain();
    
    protFile->procsAcl = procsAcl;
    if( protFile->procsAcl )
        protFile->procsAcl->retain();
        
    protFile->path = path;
    protFile->path->retain();
    
    return protFile;
}

//--------------------------------------------------------------------

void DldProtectedFile::free()
{
    if( this->path )
        this->path->release();
    
    if( this->usersAcl )
        this->usersAcl->release();
    
    if( this->procsAcl )
        this->procsAcl->release();
    
    super::free();
}

//--------------------------------------------------------------------

bool DldProtectedFile::isOnProtectedPath( __in char* pathToCheck, __in size_t pathLength /*w/o zero terminator*/ )
{
    assert( preemption_enabled() );
    
    if( pathLength < this->path->getLength() )
        return false;
    
    if( pathLength > this->path->getLength() ){
        
        if( '/' != pathToCheck[ pathLength - 0x1 ] )
            return false; // not a directory
    }
    
    assert( pathLength >= this->path->getLength() );
    
    return ( 0x0 == memcmp( pathToCheck, this->path->getCStringNoCopy(), this->path->getLength() ) );
}

//--------------------------------------------------------------------

//
// the input rights are native kauth rights, i.e. KAUTH_VNODE_*
//
bool DldProtectedFile::isAccessAllowed(__in vfs_context_t vfscontext,
                                       __in kauth_cred_t    credential,
                                       __in kauth_ace_rights_t requestedAccess )
{
    pid_t pid = vfs_context_pid( vfscontext );
    
    if( 0x0 == requestedAccess )
        return true; // nothing to check, grant access
    
    if( ( NULL == this->usersAcl || NULL == this->usersAcl->getAcl() ) &&
        ( NULL == this->procsAcl || NULL == this->procsAcl->getAcl() ) )
        return true; // nothing to check, access is granted to any process
    
    if( 0x0 == ( KAUTH_VNODE_WRITE_DATA |
                 KAUTH_VNODE_ADD_FILE |
                 KAUTH_VNODE_DELETE |
                 KAUTH_VNODE_APPEND_DATA |
                 KAUTH_VNODE_ADD_SUBDIRECTORY |
                 KAUTH_VNODE_DELETE_CHILD |
                 KAUTH_VNODE_WRITE_ATTRIBUTES |
                 KAUTH_VNODE_WRITE_EXTATTRIBUTES |
                 KAUTH_VNODE_WRITE_SECURITY |
                 KAUTH_VNODE_TAKE_OWNERSHIP |
                 KAUTH_VNODE_GENERIC_WRITE_BITS )
        & requestedAccess ){
    
        //
        // grant read access, it is unlikely that this will compromise the security
        // as the files are protected by the FSD's ACL so only the root user will be able
        // access files
        //
        return true;
    }
    
    //
    // allow the service access its files
    //
    pid_t    servicePid = gServiceUserClient.getUserClientPid();
    if( pid == servicePid ){
        return true;
    }
    
    bool accessGranted = true;
    
    if( this->usersAcl ){
        
        //
        // at present the process ACL is used to protect files,
        // so the requested access is converted to the process ones
        //
        accessGranted = gServiceProtection->isAccessAllowed( this->usersAcl,
                                                             DL_PROCESS_TERMINATE,
                                                             credential );
    }
    
    if( accessGranted && this->procsAcl && this->procsAcl->getAcl() ){
        
        kauth_ace_rights_t        residual = requestedAccess;
        const DldAclWithProcs*    procsAcl = this->procsAcl->getAcl();
        
        assert( procsAcl );
        
        for( UInt32 i = 0x0; i < procsAcl->count; ++i ){
            
            //
            // (-1) is matched with any process
            //
            if( pid != procsAcl->ace[ i ].pid && (-1) != procsAcl->ace[ i ].pid )
                continue;
            
            if( 0x0 != ( procsAcl->ace[ i ].flags & DldFileSecurityAceFlagsDisable ) &&
               0x0 != ( residual & procsAcl->ace[ i ].rights ) ){
                
                //
                // a denied entry was found
                //
                break;
            }
            
            if( 0x0 != ( procsAcl->ace[ i ].flags & DldFileSecurityAceFlagsEnable ) ){
                
                //
                // an allowed entry
                //
                residual = residual & ~( procsAcl->ace[ i ].rights );
            }
            
            if( 0x0 == residual )
                break;
            
        } // end for
        
        accessGranted = ( 0x0 == residual );
    }
    
    return accessGranted;
}

//--------------------------------------------------------------------
