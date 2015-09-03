/*
 *  DldMacPolicy.cpp
 *  DeviceLock
 *
 *  Created by Slava on 3/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

/*
 the file contains an implementation for DeviceLock MAC plugin module ( Mandatory Access Control (MAC) framework ),
 MAC is a Mac OS X clone clone for TrustedBSD. For more information see
 http://www.trustedbsd.org
 */

#include "DldMacPolicy.h"

//--------------------------------------------------------------------

#define super OSObject
OSDefineMetaClassAndStructors( DldMacPolicy, OSObject)

//--------------------------------------------------------------------

DldMacPolicy*  DldMacPolicy::createPolicyObject( __in struct mac_policy_conf *mpc, __in void* xd )
{
    assert( preemption_enabled() );
    
    DldMacPolicy* newObject = new DldMacPolicy();
    assert( newObject );
    if( !newObject ){
        DBG_PRINT_ERROR(("new DldMacPolicy() failed\n"));
        return NULL;
    }
    
    if( !newObject->initWithPolicyConf( mpc, xd ) ){
        
        DBG_PRINT_ERROR(("newObject->initWithPolicyConf( mpc, xd ) failed\n"));
        newObject->release();
        return NULL;
    }
    
    return newObject;
}

//--------------------------------------------------------------------

bool DldMacPolicy::initWithPolicyConf( __in struct mac_policy_conf *mpc, __in void* xd )
{
    if( !this->init() ){
        
        DBG_PRINT_ERROR(("this->init() failed\n"));
        return false;
    }
    
    this->mpc = mpc;
    this->xd  = xd;
    
    return true;
}

//--------------------------------------------------------------------


void DldMacPolicy::free()
{
    assert( ! this->registered );
    super::free();
}

//--------------------------------------------------------------------

kern_return_t DldMacPolicy::registerMacPolicy()
{
    kern_return_t  err;
    
    assert( preemption_enabled() );
    assert( !this->registered );
    
    if( this->registered )
        return KERN_SUCCESS;
    
    err = mac_policy_register( this->mpc, &this->handlep, this->xd );
    this->registered = ( KERN_SUCCESS == err );
    
    assert( KERN_SUCCESS == err );
    if( KERN_SUCCESS != err ){
        
        DBG_PRINT_ERROR(("registerMacPolicy failed with err = %d\n", err));
    }
    
    return err;
}

//--------------------------------------------------------------------

kern_return_t DldMacPolicy::unRegisterMacPolicy()
{
    kern_return_t  err = ENOENT;
    
    assert( preemption_enabled() );
    
    if( this->registered ){
        
        err = mac_policy_unregister( this->handlep );
        this->registered = !( KERN_SUCCESS == err );
        
        assert( KERN_SUCCESS == err );
        if( KERN_SUCCESS != err ){
            
            DBG_PRINT_ERROR(("unRegisterMacPolicy failed with err = %d\n", err));
        }
    }
    
    return err;
}

//--------------------------------------------------------------------
