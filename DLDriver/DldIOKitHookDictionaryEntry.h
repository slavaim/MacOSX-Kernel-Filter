/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOKITHOOKDICTIONARYENTRY_H
#define _DLDIOKITHOOKDICTIONARYENTRY_H

#include <IOKit/IOLib.h>
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/usb/IOUSBMassStorageClass.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"

//--------------------------------------------------------------------

typedef IOReturn ( *DldStaticClassHookFunction )( __inout OSObject* ObjectTooHook, __in DldHookType type );
typedef IOReturn ( *DldStaticClassUnHookFunction )( __inout OSObject* ObjectTooHook, __in DldHookType type, __in DldInheritanceDepth Depth );

typedef bool (*DldObjectFirstPublishCallback)( IOService * newService );
typedef bool (*DldObjectTerminatedCallback)( IOService * newService ); 


typedef struct _DldHookDictionaryEntryData{
    
    __in     const char*                         IOKitClassName;
    __in     DldStaticClassHookFunction          HookFunction;
    __in     DldStaticClassUnHookFunction        UnHookFunction;
    __in     DldHookType                         HookType;
    __in     DldInheritanceDepth                 Depth;
    
    //
    // the first publish callback is optional
    //
    __in_opt DldObjectFirstPublishCallback       ObjectFirstPublishCallback;
    
    //
    // the object termination callback is optional
    //
    __in_opt DldObjectTerminatedCallback         ObjectTerminatedCallback;
    
} DldHookDictionaryEntryData;


//--------------------------------------------------------------------


class DldIOKitHookDictionaryEntry : public OSObject {
    
    OSDeclareDefaultStructors( DldIOKitHookDictionaryEntry )
    
protected:
    
    OSString*                   IOKitClassName;
    //IONotifier*                 ObjectPublishNotifier;
    
    //
    // contains a dictionary of IONotifier objects, the object's remove() routines
    // will be called by the free() routine
    //
    OSDictionary*               NotifiersDictionary;
    OSCollectionIterator*       NotifiersDictionaryIterator;
    
    //
    // hook data as it has been passed to the withIOKitClassName "constructor"
    //
    DldHookDictionaryEntryData  HookData;
    
protected:
    
    virtual bool init();
    virtual void free();
    
public:
    
    //
    // the object pased as a parameter will be released by calling remove()
    // so the caller must not release the object received from addMatchingNotification
    // routine if no additional refernces were taken
    //
    bool setNotifier( __in const OSSymbol * type, __in IONotifier* ObjectNotifier );
    
    
    //
    // the caller must release the returned string object
    //
    OSString*   getIOKitClassNameCopy()
    { 
        assert( this->IOKitClassName );
        return OSString::withString( (const OSString*)this->IOKitClassName );
    }
        
    DldStaticClassHookFunction    getHookFunction(){ return this->HookData.HookFunction; };
    
    DldStaticClassUnHookFunction    getUnHookFunction(){ return this->HookData.UnHookFunction; };
    
    DldHookType    getHookType(){ return this->HookData.HookType; };
    
    DldObjectFirstPublishCallback getFirstPublishCallback(){ return this->HookData.ObjectFirstPublishCallback; };
    
    DldObjectTerminatedCallback getTerminationCallback(){ return this->HookData.ObjectTerminatedCallback; };
    
    //
    // IOKitClassName is a name for an IOKit class of objects to be hooked by HookFunction,
    // the IOKitClassName will be referenced
    //
    static DldIOKitHookDictionaryEntry* withIOKitClassName( __in OSString* IOKitClassName,
                                                            __in DldHookDictionaryEntryData*  HookData );
};   

//--------------------------------------------------------------------

#endif//_DLDIOKITHOOKDICTIONARYENTRY_H