/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "DldIOServiceTree.h"

#define super OSObject

//--------------------------------------------------------------------

OSDefineMetaClassAndStructors( DldIOServiceTreeEntry, OSObject )

//--------------------------------------------------------------------

DldIOServiceTreeEntry*
DldIOServiceTreeEntry::withIOService(
    __in IOService* serviceObject
    )
{
    DldIOServiceTreeEntry*   Entry;
    
    Entry = new DldIOServiceTreeEntry();
    assert( Entry );
    if( NULL == Entry )
        return NULL;
    
    if( !Entry->init() ){
        
        assert( !"DldIOServiceTreeEntry::withIOService() Entry->init() failed" );
        DBG_PRINT_ERROR( ("DldIOServiceTreeEntry::withIOService(0x%X) Entry->init() failed\n", serviceObject ) );
        Entry->release();
        Entry = NULL;
    }
    
    Entry->serviceObject = serviceObject;
    
    return Entry;
}

//--------------------------------------------------------------------

bool DldIOServiceTreeEntry::init()
{
    
    assert( NULL == this->serviceObject );
    assert( NULL == this->Parents );
    assert( NULL == this->Chidren );
    
    if( !super::init() )
        return false;
    
    return true;
}

//--------------------------------------------------------------------

void DldIOServiceTreeEntry::free()
{
    
    if( this->Parents )
        this->Parents->release();
    
    if( this->Chidren )
        this->Chidren->release();
    
}

//--------------------------------------------------------------------
