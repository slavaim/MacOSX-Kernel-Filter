/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "DldIOKitHookDictionaryEntry.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIOKitHookDictionaryEntry, OSObject )

//--------------------------------------------------------------------

DldIOKitHookDictionaryEntry*
DldIOKitHookDictionaryEntry::withIOKitClassName(
    __in OSString*                    IOKitClassName,
    __in DldHookDictionaryEntryData*  HookData
    )
{
    assert( NULL != IOKitClassName );
    assert( NULL != HookData );
    assert( NULL != HookData->HookFunction );
    assert( NULL != HookData->UnHookFunction );
    
    DldIOKitHookDictionaryEntry*   pEntry = new DldIOKitHookDictionaryEntry();
    assert( NULL != pEntry );
    if( NULL == pEntry )
        return NULL;
    
    //
    // init the IOKit base classes
    //
    if( !pEntry->init() ){
        
        assert( !"DldIOKitHookDictionaryEntry: class init failed" );
        DBG_PRINT_ERROR( ( "DldIOKitHookDictionaryEntry::withIOKitClassName( %s ) pEntry->init() failed\n",
                            IOKitClassName->getCStringNoCopy() ) );
        
        pEntry->release();
        return NULL;
    }
    
    IOKitClassName->retain();
    pEntry->IOKitClassName = IOKitClassName;
    
    pEntry->HookData = *HookData;
    
    return pEntry;
    
}

//--------------------------------------------------------------------

bool
DldIOKitHookDictionaryEntry::setNotifier(
    __in const OSSymbol * type,
    __in IONotifier*    ObjectNotifier
    )
{
    assert( ObjectNotifier );
    assert( !this->NotifiersDictionary->getObject( type ) );
    
    if( !this->NotifiersDictionary->setObject( type, (OSMetaClassBase*)ObjectNotifier ) ){
        
        assert( !"DldIOKitHookDictionaryEntry::setNotifier failed" );
        DBG_PRINT_ERROR( ( "DldIOKitHookDictionaryEntry(%s)::setPublisNotifier( %s ) failed\n",
                            this->IOKitClassName->getCStringNoCopy(), type->getCStringNoCopy() ) );
        
        return false;
    }

    return true;
}

//--------------------------------------------------------------------

bool
DldIOKitHookDictionaryEntry::init()
{
    if( !super::init() )
        return false;
    
    this->NotifiersDictionary = OSDictionary::withCapacity(2);
    assert( this->NotifiersDictionary );
    if( !this->NotifiersDictionary ) {
        
        DBG_PRINT_ERROR( ( "DldIOKitHookDictionaryEntry(%s)::init() OSDictionary::withCapacity(2) failed\n", this->IOKitClassName->getCStringNoCopy() ) );
        return false;
    }
    
    this->NotifiersDictionaryIterator = OSCollectionIterator::withCollection( this->NotifiersDictionary );
    assert( this->NotifiersDictionaryIterator );
    if( !this->NotifiersDictionaryIterator ){
        
        DBG_PRINT_ERROR( ( "DldIOKitHookDictionaryEntry(%s)::init() OSDictionary::withCapacity(2) failed\n", this->IOKitClassName->getCStringNoCopy() ) );
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

void
DldIOKitHookDictionaryEntry::free(void)
{
    //
    // check for null in case of failed initialization or a dummy new operator call
    //
    assert( this->IOKitClassName );
    if( this->IOKitClassName )
        this->IOKitClassName->release();
    
    
    if( this->NotifiersDictionary ){
        
        //
        // remove all registered notifications
        //
        if( this->NotifiersDictionaryIterator ){
            
            OSObject* key;
            
            this->NotifiersDictionaryIterator->reset();
            while( NULL != (key = this->NotifiersDictionaryIterator->getNextObject()) ){
                
                OSSymbol*   type;
                OSObject*   obj;
                IONotifier* objectNotifier;
                
                type = OSDynamicCast( OSSymbol, key );
                assert( type );
                
                obj = this->NotifiersDictionary->getObject( type );
                assert( obj );
                
                objectNotifier = OSDynamicCast( IONotifier, obj );
                assert( objectNotifier );
                
                //
                // remove the registration
                //
                objectNotifier->remove();
                
            }// end while
            
            this->NotifiersDictionaryIterator->release();
            
        }
        
        //
        // remove the dictionary, the dictionary's objects will be released
        //
        this->NotifiersDictionary->release();
    }
    
    super::free();
    return;
}

//--------------------------------------------------------------------

