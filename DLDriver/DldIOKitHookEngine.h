/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOKITHOOKENGINE_H
#define _DLDIOKITHOOKENGINE_H

#include <IOKit/IOLib.h>
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/usb/IOUSBMassStorageClass.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>
#include "DldCommon.h"
#include "DldIOKitHookDictionaryEntry.h"
#include "DldHookerCommonClass.h"
#include "DldHookerCommonClass2.h"

//--------------------------------------------------------------------

class DldIOKitHookEngine : public OSObject {
    
    OSDeclareDefaultStructors( DldIOKitHookEngine )
    
protected:
    
    //
    // all leafe classes are per object hookers
    //
    OSDictionary*      DictionaryPerObjectHooks;
    
    //
    // all super classes are direct vtable hookers
    //
    OSDictionary*      DictionaryVtableClassHooks[ DldInheritanceDepth_Maximum ];
    
    
    OSDictionary* HookTypeToDictionary( __in DldHookType HookType, __in DldInheritanceDepth Depth );
    
private:
    
    
    bool AddNewIOKitClass( __in DldHookDictionaryEntryData* HookData );
    
    
#ifdef DLD_MACOSX_10_5
    static bool ObjectPublishCallback( __in    void * target,
                                       __in    void * refCon,
                                       __inout IOService * NewService );
#else
    static bool ObjectPublishCallback( __in    void * target,
                                       __in    void * refCon,
                                       __inout IOService * NewService,
                                       __in    IONotifier * notifier );
#endif


#ifdef DLD_MACOSX_10_5
    static bool ObjectTerminatedCallback( __in    void * target,
                                          __in    void * refCon,
                                          __inout IOService * NewService );
#else
    static bool ObjectTerminatedCallback( __in    void * target,
                                          __in    void * refCon,
                                          __inout IOService * NewService,
                                          __in    IONotifier * notifier );
#endif
    
    bool        CallObjectFirstPublishCallback( __in IOService* NewService );
    
    bool        CallObjectTerminatedCallback( __in IOService* NewService );
    
protected:
    
    virtual bool init();
    virtual void free();
    
public:
    
    static DldIOKitHookEngine* withNoHooks();
    
    bool startHookingWithPredefinedClasses();
    
    IOReturn    HookObject( __inout OSObject* object );
    
    //
    // CC stands for Containing Class
    // HC stands for Hooked Class
    //
    template<class CC, class HC> bool DldAddHookingClassInstance( __in DldHookType HookType );
    
    //IOReturn    UnHookObject( __inout OSObject* object );
    
};

//--------------------------------------------------------------------

//
// CC stands for Containing Class
// HC stands for Hooked Class
//
template<class CC, class HC>
bool
DldIOKitHookEngine::DldAddHookingClassInstance( __in DldHookType HookType )
{
    
    DldHookDictionaryEntryData    HookEntryData = { 0x0 };
    
    assert( DldHookTypeVtable == HookType || DldHookTypeObject == HookType );
    
    //
    // create a static instance
    //
    if( !DldHookerCommonClass2<CC,HC>::fStaticContainerClassInstance() ){
        
        assert( !"DldHookerCommonClass2<CC,HC>::DldAddPerObjectHookingClassInstance() failed" );
        DBG_PRINT_ERROR(("DldHookerCommonClass2<CC,HC>::DldAddPerObjectHookingClassInstance() failed for %s\n", DldHookerCommonClass2<CC,HC>::fGetHookedClassName()));
        return false;
    }
    
    HookEntryData.HookType = HookType;//DldHookTypeObject;
    HookEntryData.IOKitClassName = DldHookerCommonClass2<CC,HC>::fGetHookedClassName();
    HookEntryData.HookFunction = DldHookerCommonClass2<CC,HC>::fHookObject;
    HookEntryData.UnHookFunction = DldHookerCommonClass2<CC,HC>::fUnHookObject;
    
    HookEntryData.ObjectFirstPublishCallback = (DldHookTypeObject == HookType)?
    DldHookerCommonClass2<CC,HC>::fObjectFirstPublishCallback:
    NULL;
    
    HookEntryData.ObjectTerminatedCallback = (DldHookTypeObject == HookType)?
    DldHookerCommonClass2<CC,HC>::fObjectTerminatedCallback:
    NULL;
    
    HookEntryData.Depth = DldHookerCommonClass2<CC,HC>::fGetInheritanceDepth();
    
    assert( !( DldHookTypeObject == HookType &&
              DldInheritanceDepth_0 != (DldHookerCommonClass2<CC,HC>::fGetInheritanceDepth()) ) );
    
    return this->AddNewIOKitClass( &HookEntryData );
}


//--------------------------------------------------------------------

extern DldIOKitHookEngine*   gHookEngine;
extern bool                  gFirstScanCompleted;

//--------------------------------------------------------------------

#endif//_DLDIOKITHOOKENGINE_H

