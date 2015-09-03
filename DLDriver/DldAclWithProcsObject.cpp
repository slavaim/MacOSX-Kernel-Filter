/*
 *  DldAclWithProcsObject.cpp
 *  DeviceLock
 *
 *  Created by Slava on 17/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

#include "DldAclWithProcsObject.h"

//--------------------------------------------------------------------

#define super OSObject
OSDefineMetaClassAndStructors( DldAclWithProcsObject, OSObject)

//--------------------------------------------------------------------

DldAclWithProcsObject* DldAclWithProcsObject::withAcl( __in_opt DldAclWithProcs* procsAcl )
{
    
    DldAclWithProcsObject*  fileProcsAclObj = new DldAclWithProcsObject();
    assert( fileProcsAclObj );
    if( !fileProcsAclObj ){
        
        DBG_PRINT_ERROR(("new DldAclWithProcsObject() failed\n"));
        return NULL;
    }
    
    if( !fileProcsAclObj->init() ){
        
        DBG_PRINT_ERROR(("fileProcsAclObj->init() failed\n"));
        fileProcsAclObj->release();
        return NULL;
    }
    
    if( procsAcl ){
        
        //
        // allocate a memory and copy ACL, a caller must free an ACL provided as a parameter
        //
        
        fileProcsAclObj->acl = (DldAclWithProcs*)IOMalloc( DldAclWithProcsSize( procsAcl ) );
        assert( fileProcsAclObj->acl );
        if( !fileProcsAclObj->acl ){
            
            DBG_PRINT_ERROR(("IOMalloc( DldAclWithProcsSize( procsAcl ) ) failed\n"));
            fileProcsAclObj->release();
            return NULL;
        }
        
        bcopy( procsAcl, fileProcsAclObj->acl, DldAclWithProcsSize( procsAcl ) );
    } // end if( procsAcl )
    
    return fileProcsAclObj;
}

//--------------------------------------------------------------------

void DldAclWithProcsObject::free()
{
    if( this->acl )
        IOFree( this->acl, DldAclWithProcsSize( this->acl ) );
    
    super::free();
}

//--------------------------------------------------------------------
