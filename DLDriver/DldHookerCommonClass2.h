/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */
#ifndef _DLDHOOKERCOMMONCLASS2_H
#define _DLDHOOKERCOMMONCLASS2_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODataQueue.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include <kern/locks.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"


//--------------------------------------------------------------------

//
// IT IS IMPORTANT FOR ALL HOOKS AND ALL FUCNTIONS USED IN HOOKS TO BE
// IDEMPOTEN IN THEIR BEHAVIOUR, for example below id the call stack whe
// the terminate hook is called twice as there are two hooks which forms
// a hook chain ending by original function
//
/*
 #3  0x46c4c220 in DldIOService::terminate (this=0xd1c7bc0, options=1048583) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOService.cpp:368
 #4  0x46c3fe6f in DldHookerCommonClass::terminate (this=0x7901c78, serviceObject=0x5c2b010, options=1048583) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldHookerCommonClass.cpp:779
 #5  0x46c35046 in terminate_hook (this=0x5c2b010, options=1048583) at DldHookerCommonClass2.h:1083
 #6  0x46c12ac0 in terminate_hook (this=0x5c2b010, options=1048583) at DldHookerCommonClass2.h:1086
 #7  0x00593ad8 in IOService::terminateClient (this=0x5c21340, client=0x5c2b010, options=1048583) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:2235
 #8  0x46c36790 in terminateClient_hook (this=0x5c21340, client=0x5c2b010, options=1048583) at DldHookerCommonClass2.h:1117
 #9  0x00593a69 in IOService::requestTerminate (this=0x5c2b010, provider=0x5c21340, options=1048583) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1660
 #10 0x46c35268 in requestTerminate_hook (this=0x5c2b010, provider=0x5c21340, options=7) at DldHookerCommonClass2.h:986
 #11 0x46c12cc0 in requestTerminate_hook (this=0x5c2b010, provider=0x5c21340, options=7) at DldHookerCommonClass2.h:986
 #12 0x005995de in IOService::terminatePhase1 (this=0x5c21560, options=7) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:1727
 #13 0x0059977f in IOService::terminate (this=0x5c21560, options=3) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOService.cpp:2246
 #14 0x0141f1ce in ?? ()
 #15 0x46c3682c in terminate_hook (this=0x5c21560, options=3) at DldHookerCommonClass2.h:1086
 #16 0x01554ad9 in ?? ()
 #17 0x01557b9b in ?? ()
 #18 0x015579da in ?? ()
 #19 0x015548af in ?? ()
 #20 0x002462dc in thread_call_thread (group=0x59db010) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/thread_call.c:847
*/


//--------------------------------------------------------------------

//
// FYI - an instantiated template naming in GDB
// IOUserClientDldHook<(_DldInheritanceDepth)2>
//

//--------------------------------------------------------------------

//
// CC stands for ContainingClass - a class which contains the given hooker class
// HC stands for HookedClass - a class to hook, must be derived from IOService
// AF stands for ADDED_FUNCTIONS - how many new functions has been added,
// DP stands for Depth - a depth of a hooked class in the inheritance hierarchy
//

template <class CC, class HC>
class DldHookerCommonClass2{
    
    
private:
    
    //
    // the object of the class which contains code
    // for IOService hooks and all hooking logic
    //
    DldHookerCommonClass    mHookerCommon;
    
    //
    // the enums for the IOService hooks
    //
    /*
     the following defines the order of the overloaded setProperty and removeProperty declarations
     
     (1) virtual bool setProperty(const OSSymbol * aKey, OSObject * anObject);
     (2) virtual bool setProperty(const OSString * aKey, OSObject * anObject);
     (3) virtual bool setProperty(const char * aKey, OSObject * anObject);
     (4) virtual bool setProperty(const char * aKey, const char * aString);
     (5) virtual bool setProperty(const char * aKey, bool aBoolean);
     (6) virtual bool setProperty(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits);
     (7) virtual bool setProperty(const char * aKey, void * bytes, unsigned int length);
     
     (1) virtual void removeProperty( const OSSymbol * aKey);
     (2) virtual void removeProperty( const OSString * aKey);
     (3) virtual void removeProperty( const char * aKey);
     
     */
    enum DldBaseHooksEnum{
        kDld_free_hook = 0x0,
        kDld_terminate_hook,        
        kDld_finalize_hook,
        kDld_detach_hook,
        kDld_requestTerminate_hook,
        kDld_willTerminate_hook,
        kDld_didTerminate_hook,
        kDld_terminateClient_hook,
        kDld_start_hook,
        kDld_open_hook,
        kDld_attach_hook,
        kDld_attachToChild_hook,
        kDld_removeProperty1_hook,
        kDld_removeProperty2_hook,
        kDld_removeProperty3_hook,
        kDld_setPropertyTable_hook,
        kDld_setProperty1_hook,
        kDld_setProperty2_hook,
        kDld_setProperty3_hook,
        kDld_setProperty4_hook,
        kDld_setProperty5_hook,
        kDld_setProperty6_hook,
        kDld_setProperty7_hook,
        kDld_newUserClient1_hook,
        
        //
        // always the last
        //
        kDld_NumberOfBaseHooks
    };
    
    //
    // the terminating entry has (-1) as the VtableIndex's value
    //
    DldHookedFunctionInfo   mHookedVtableFunctionsInfo[ DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks + CC::kDld_NumberOfAddedHooks + 1 ];
    
    //
    // a static instance of the containing class, required as a hooking class
    // can't be passed to a hooked function and it is impossible
    // to count hooked class' objects for vtable direct hooks
    // so a persistent object is required to retrieve native functions
    // especially for unhooked objects or classe when the hooking function was
    // hit just before unhooking
    //
    static CC*    mStaticInstance;
    
    //
    // a containing object
    //
    CC*           mContainingObject;
    
    //
    // used to provide an idempotent behaviour for init() and free()
    //
    bool          mInit;
    
    //
    // a rw-lock to protect internal data
    //
    IORWLock*    rwLock;
    
#if defined( DBG )
    thread_t     exclusiveThread;
#endif//DBG
    
    //
    // returns true if the mStaticInstance's value has been saved,
    // the false value is returned if the mStaticInstance's value
    // has been already initialized
    //
    bool fSetStaticContainingClassObject( __in CC* obj );
    
    //
    // sets a containing object
    //
    void fSetContainingClassObject( __in CC* obj ){ this->mContainingObject = obj; };
    
    void  fAddHookingFunctionInternal( __in unsigned int hookIndex,// base is 0x0
                                       __in unsigned int vtableIndex,// base is 0x1
                                       __in DldVtableFunctionPtr HookingFunction );
    
    OSMetaClassBase::_ptf_t fGetOriginalFunction( __in OSObject* hookedObject, __in unsigned int indx );
    
    //
    // hooking functions declaration, these functions are placed instead
    // of original ones in hooked object's vtable,
    // all hooking functions must be declared as virtual as they hook virtual functions
    //
    virtual bool start_hook( IOService * provider );
    virtual bool open_hook(  IOService * forClient, IOOptionBits options, void * arg );
    virtual void free_hook();
    virtual bool requestTerminate_hook( IOService * provider, IOOptionBits options );
    virtual bool willTerminate_hook( IOService * provider, IOOptionBits options );
    virtual bool didTerminate_hook( IOService * provider, IOOptionBits options, bool * defer );
    virtual bool terminate_hook( IOOptionBits options );
    virtual bool terminateClient_hook( IOService * client, IOOptionBits options );
    virtual bool finalize_hook( IOOptionBits options );
    virtual bool attach_hook( IOService * provider );
    virtual bool attachToChild_hook( IORegistryEntry * child,const IORegistryPlane * plane );
    virtual void detach_hook( IOService * provider );
    virtual void setPropertyTable_hook( OSDictionary * dict );
    virtual bool setProperty1_hook(const OSSymbol * aKey, OSObject * anObject);
    virtual bool setProperty2_hook(const OSString * aKey, OSObject * anObject);
    virtual bool setProperty3_hook(const char * aKey, OSObject * anObject);
    virtual bool setProperty4_hook(const char * aKey, const char * aString);
    virtual bool setProperty5_hook(const char * aKey, bool aBoolean);
    virtual bool setProperty6_hook(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits);
    virtual bool setProperty7_hook(const char * aKey, void * bytes, unsigned int length);
    virtual void removeProperty1_hook(const OSSymbol * aKey);
    virtual void removeProperty2_hook(const OSString * aKey);
    virtual void removeProperty3_hook(const char * aKey);
    virtual IOReturn newUserClient1_hook( task_t owningTask, void * securityID,
                                          UInt32 type,  OSDictionary * properties,
                                          IOUserClient ** handler );
    
public:
    
    //
    // the following two functions returns either the static CC class or
    // the CC's DldHookerCommonClass2<> or DldHookerCommonClass2<>'s base
    // common hooker class ( mHookerCommon member ), if the static object
    // doesn't exists it is allocated and initialized
    //
    static CC*                           fStaticContainerClassInstance();
    static DldHookerCommonClass2<CC,HC>* fCommonHooker2();
    static DldHookerCommonClass*         fBaseCommonHooker();
    
    static IOReturn fHookObject( __inout OSObject* object, __in DldHookType type );
    static IOReturn fUnHookObject( __inout OSObject* object, __in DldHookType type, __in DldInheritanceDepth Depth );
    
    static bool fObjectFirstPublishCallback( __in IOService * newService );
    static bool fObjectTerminatedCallback( __in IOService * terminatedService );
    
    //
    // returns a number which is at least equal to the number of entries
    // in the hooked class's vtable, the returned entries number
    // might be bigger than the real number of entries
    //
    static int  fHookedClassVtaleSize();
    
    //
    // returns an inheritance index where a hooked class is placed
    // in the inheritance chain starting from the last derived
    // class having the DldInheritanceDepth_0 depth ( the returned 
    // value is ignored for per-object hooks )
    //
    static DldInheritanceDepth fGetInheritanceDepth();
    
    virtual void LockShared();
    virtual void UnLockShared();
    
    virtual void LockExclusive();
    virtual void UnLockExclusive();
    
    CC* fGetContainingClassObject(){ return this->mContainingObject; };
    
    //
    // an initialization function, must be called for each new instance of the class,
    // the counterpart is free(), the function has an idempotent behaviour
    //
    virtual bool init( __in CC* containingObject );
    
    //
    // free() must be called before deallocating a class instance for which init()
    // was called, the function has an idempotent behaviour
    //
    virtual void free();
    
    OSMetaClassBase::_ptf_t fGetOriginalFunctionExternal( __in OSObject* hookedObject, __in unsigned int indx );
    
    //
    // adds a new hooked function'd definition,
    // the index has a base 0x0 and should be smaller than ADDED_FUNCTIONS,
    // the vtableIndex has a base 0x1 as returned by DldConvertFunctionToVtableIndex()
    //
    virtual void  fAddHookingFunctionExternal( __in int hookIndex,// base is 0x0 relative to external caller
                                               __in int vtableIndex,// base is 0x1
                                               __in DldVtableFunctionPtr HookingFunction );
    
    static const char* fGetHookedClassName();
    
    //
    // _ptmf2ptf converts an internal compiler representation for a class function pointer to a 'C' functor,
    // This function is used instead OSMemberFunctionCast() as the DldHookerCommonClass2<CC,HC>
    // class is not OSMetaClassBase derived as required by OSMemberFunctionCast()
    //
    static DldVtableFunctionPtr
    _ptmf2ptf(const DldHookerCommonClass2<CC,HC> *self, void (DldHookerCommonClass2<CC,HC>::*func)(void));
    
    static DldVtableFunctionPtr
    _ptmf2ptf(const DldHookerBaseInterface *self, void (DldHookerBaseInterface::*func)(void));
    
    //virtual IOReturn HookObject( __inout HookedClass* object, __in DldHookType type );
    
    //virtual IOReturn UnHookObject( __inout HookedClass* object );
    
};

//--------------------------------------------------------------------

//
// declaration for the static member,
// gcc can do it wrong with the memory alocation for a template class's static,
// so be vigilant!
//
template <class CC, class HC>
CC* DldHookerCommonClass2<CC,HC>::mStaticInstance = NULL;

//--------------------------------------------------------------------

//
// the _ptmf2ptf code was borrowed from OSMetaClass.h
//
#if APPLE_KEXT_LEGACY_ABI

// Arcane evil code interprets a C++ pointer to function as specified in the
// -fapple-kext ABI, i.e. the gcc-2.95 generated code.  IT DOES NOT ALLOW
// the conversion of functions that are from MULTIPLY inherited classes.
// This function is used insted OSMemberFunctionCast() as the DldHookerCommonClass2<CC,HC>
// class is not OSMetaClassBase derived as required by OSMemberFunctionCast()
//

template <class CC, class HC>
DldVtableFunctionPtr
DldHookerCommonClass2<CC,HC>::_ptmf2ptf(const DldHookerCommonClass2<CC,HC> *self, void (DldHookerCommonClass2<CC,HC>::*func)(void))
{
    union {
        void (DldHookerCommonClass2<CC,HC>::*fIn)(void);
        struct {     // Pointer to member function 2.95
            unsigned short fToff;
            short  fVInd;
            union {
                DldVtableFunctionPtr fPFN;
                short  fVOff;
            } u;
        } fptmf2;
    } map;
    
    map.fIn = func;
    if (map.fptmf2.fToff) {
        panic("Multiple inheritance is not supported");
        return 0;
    } else if (map.fptmf2.fVInd < 0) {
        // Not virtual, i.e. plain member func
        return map.fptmf2.u.fPFN;
    } else {
        union {
            const DldHookerCommonClass2<CC,HC> *fObj;
            DldVtableFunctionPtr **vtablep;
        } u;
        u.fObj = self;
        
        // Virtual member function so dereference vtable
        return (*u.vtablep)[map.fptmf2.fVInd - 1];
    }
}

#else /* !APPLE_KEXT_LEGACY_ABI */

// Slightly less arcane and slightly less evil code to do
// the same for kexts compiled with the standard Itanium C++
// ABI

template <class CC, class HC>
DldVtableFunctionPtr
DldHookerCommonClass2<CC,HC>::_ptmf2ptf(const DldHookerCommonClass2<CC,HC> *self, void (DldHookerCommonClass2<CC,HC>::*func)(void))
{
    union {
        void (DldHookerCommonClass2<CC,HC>::*fIn)(void);
        uintptr_t fVTOffset;
        DldVtableFunctionPtr fPFN;
    } map;
    
    map.fIn = func;
    
    if (map.fVTOffset & 1) {
        // virtual
        union {
            const DldHookerCommonClass2<CC,HC> *fObj;
            DldVtableFunctionPtr **vtablep;
        } u;
        u.fObj = self;
        
        // Virtual member function so dereference vtable
        return *(DldVtableFunctionPtr *)(((uintptr_t)*u.vtablep) + map.fVTOffset - 1);
    } else {
        // Not virtual, i.e. plain member func
        return map.fPFN;
    }
}

#endif /* !APPLE_KEXT_LEGACY_ABI */

//--------------------------------------------------------------------

//
// the _ptmf2ptf code was borrowed from OSMetaClass.h
//
#if APPLE_KEXT_LEGACY_ABI

// Arcane evil code interprets a C++ pointer to function as specified in the
// -fapple-kext ABI, i.e. the gcc-2.95 generated code.  IT DOES NOT ALLOW
// the conversion of functions that are from MULTIPLY inherited classes.
// This function is used insted OSMemberFunctionCast() as the DldHookerCommonClass2<CC,HC>
// class is not OSMetaClassBase derived as required by OSMemberFunctionCast()
//

template <class CC, class HC>
DldVtableFunctionPtr
DldHookerCommonClass2<CC,HC>::_ptmf2ptf(const DldHookerBaseInterface *self, void (DldHookerBaseInterface::*func)(void))
{
    union {
        void (DldHookerBaseInterface::*fIn)(void);
        struct {     // Pointer to member function 2.95
            unsigned short fToff;
            short  fVInd;
            union {
                DldVtableFunctionPtr fPFN;
                short  fVOff;
            } u;
        } fptmf2;
    } map;
    
    map.fIn = func;
    if (map.fptmf2.fToff) {
        panic("Multiple inheritance is not supported");
        return 0;
    } else if (map.fptmf2.fVInd < 0) {
        // Not virtual, i.e. plain member func
        return map.fptmf2.u.fPFN;
    } else {
        union {
            const DldHookerBaseInterface *fObj;
            DldVtableFunctionPtr **vtablep;
        } u;
        u.fObj = self;
        
        // Virtual member function so dereference vtable
        return (*u.vtablep)[map.fptmf2.fVInd - 1];
    }
}

#else /* !APPLE_KEXT_LEGACY_ABI */

// Slightly less arcane and slightly less evil code to do
// the same for kexts compiled with the standard Itanium C++
// ABI

template <class CC, class HC>
DldVtableFunctionPtr
DldHookerCommonClass2<CC,HC>::_ptmf2ptf(const DldHookerBaseInterface *self, void (DldHookerBaseInterface::*func)(void))
{
    union {
        void (DldHookerBaseInterface::*fIn)(void);
        uintptr_t fVTOffset;
        DldVtableFunctionPtr fPFN;
    } map;
    
    map.fIn = func;
    
    if (map.fVTOffset & 1) {
        // virtual
        union {
            const DldHookerBaseInterface *fObj;
            DldVtableFunctionPtr **vtablep;
        } u;
        u.fObj = self;
        
        // Virtual member function so dereference vtable
        return *(DldVtableFunctionPtr *)(((uintptr_t)*u.vtablep) + map.fVTOffset - 1);
    } else {
        // Not virtual, i.e. plain member func
        return map.fPFN;
    }
}

#endif /* !APPLE_KEXT_LEGACY_ABI */

//--------------------------------------------------------------------

template <class CC, class HC>
bool
DldHookerCommonClass2<CC,HC>::init( __in CC* containingObject )
{
    if( this->mInit )
        return true;
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( !this->rwLock )
        return false;
     
    
    //
    // mark the terminating entry
    //
    this->mHookedVtableFunctionsInfo[ DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks + CC::kDld_NumberOfAddedHooks ].VtableIndex = (-1);
    
    //
    // initialize the base IOService hooks
    //
    //bool (IOService::*p0)(IOService*) = &IOService::start;
    //void (DldHookerCommonClass2<CC,HC>::*p)(IOService*) = &DldHookerCommonClass2<CC,HC>::start_hook;
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_start_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::start ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::start_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_open_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::open ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::open_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_free_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::free ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::free_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_requestTerminate_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::requestTerminate ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::requestTerminate_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_willTerminate_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::willTerminate ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::willTerminate_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_didTerminate_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::didTerminate ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::didTerminate_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_terminate_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::terminate ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::terminate_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_terminateClient_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::terminateClient ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::terminateClient_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_finalize_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::finalize ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::finalize_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_attach_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::attach ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::attach_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_attachToChild_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::attachToChild ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::attachToChild_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_detach_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::detach ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::detach_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setPropertyTable_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &IOService::setPropertyTable ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setPropertyTable_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty1_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const OSSymbol * aKey, OSObject * anObject) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty1_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty2_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const OSString * aKey, OSObject * anObject) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty2_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty3_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const char * aKey, OSObject * anObject) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty3_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty4_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const char * aKey, const char * aString) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty4_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty5_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const char * aKey, bool aBoolean) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty5_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty6_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty6_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_setProperty7_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( bool ( IOService::* ) (const char * aKey, void * bytes, unsigned int length) ) &IOService::setProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::setProperty7_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_removeProperty1_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) (void ( IOService::* )(const OSSymbol * aKey)) &IOService::removeProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::removeProperty1_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_removeProperty2_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) (void ( IOService::* )(const OSString * aKey)) &IOService::removeProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::removeProperty2_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_removeProperty3_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) (void ( IOService::* )(const char * aKey)) &IOService::removeProperty ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::removeProperty3_hook ) );
    
    this->fAddHookingFunctionInternal( DldHookerCommonClass2<CC,HC>::kDld_newUserClient1_hook,
                                       DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) (IOReturn ( IOService::* ) ( task_t owningTask, void * securityID,
                                                                                                                                       UInt32 type,  OSDictionary * properties,
                                                                                                                                       IOUserClient ** handler ) ) &IOService::newUserClient ),
                                       DldHookerCommonClass2<CC,HC>::_ptmf2ptf( this, (void (DldHookerCommonClass2<CC,HC>::*)()) &DldHookerCommonClass2<CC,HC>::newUserClient1_hook ) );

    //
    // provide the pointer to the IOService hooker class and additional information,
    // actually all this "additional infromation" is a remnant of the past, a legacy
    // as the mHookerCommon's code can be modified to retrieve all information directly
    // from the containingObject object
    //
    this->mHookerCommon.SetClassHookerObject( containingObject );
    this->mHookerCommon.SetHookedVtableSize( DldHookerCommonClass2<CC,HC>::fHookedClassVtaleSize() );
    this->mHookerCommon.SetInheritanceDepth( DldHookerCommonClass2<CC,HC>::fGetInheritanceDepth() );
    this->mHookerCommon.SetHookedVtableFunctionsInfo( this->mHookedVtableFunctionsInfo,
                                                      DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks + CC::kDld_NumberOfAddedHooks + 1 );

    
    this->fSetContainingClassObject( containingObject );
    
    this->mInit = true;
    
    //
    // save the container object as the static containing class instance,
    // this assignment must be done after full initialization to provide
    // a concurrent thread with a full-fledged object, after the static
    // container has been set the class can immediately participate
    // in hooking activity
    //
    if( !this->fSetStaticContainingClassObject( containingObject ) ){
        
        //
        // a concurrent thread managed to sneak in first and register another
        // container class as a static container
        //
        assert( containingObject == this->mStaticInstance );
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template <class CC, class HC>
DldInheritanceDepth
DldHookerCommonClass2<CC,HC>::fGetInheritanceDepth()
{
    return CC::fGetInheritanceDepth();
}

//--------------------------------------------------------------------

//
// returns the size in bytes of the hoooked clss's vtable
//
template <class CC, class HC>
int
DldHookerCommonClass2<CC,HC>::fHookedClassVtaleSize()
{
    //
    // declare a pure virtual class derived from the hooked class
    // and add one virtual function thus the parent class' vtable
    // numder of entries is equal to the added virtual function
    // index minus one
    // ( as the vtable entries are enumerated starting from 0x1 )
    //
    class PureVirtualClass: public HC{
        
    public:
        virtual void LastVtableFunction() = 0;
    };
    
    return (sizeof(DldVtableFunctionPtr))*( DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &PureVirtualClass::LastVtableFunction ) - 0x1 );
}

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::free()
{
    if( !this->mInit )
        return;
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
    this->mInit = false;
}

//--------------------------------------------------------------------

template <class CC, class HC>
bool
DldHookerCommonClass2<CC,HC>::fSetStaticContainingClassObject( __in CC* containgObject )
{
    //
    // save the allocated object as the static class instance
    //
    return OSCompareAndSwapPtr( NULL, containgObject, &(DldHookerCommonClass2<CC,HC>::mStaticInstance) );
}

//--------------------------------------------------------------------

template <class CC, class HC>
CC*
DldHookerCommonClass2<CC,HC>::fStaticContainerClassInstance()
{
    if( DldHookerCommonClass2<CC,HC>::mStaticInstance )
        return DldHookerCommonClass2<CC,HC>::mStaticInstance;
    

    {// start of the static instance creation
        
        //
        // an optimistic synchronization is used, see OSCompareAndSwapPtr() below
        //
        
        CC* staticInstance;
        
        //
        // allocate a new instance
        //
        staticInstance = CC::newWithDefaultSettings();
        assert( staticInstance );
        if( !staticInstance ){
            
            DBG_PRINT_ERROR(("DldHookerCommonClass2's container instantiation failed\n"));
            return NULL;
        }
       
        //
        // initialize the container's hooking class, as init() has an idempotent behaviour
        // it doesn't matter whether CC::newWithDefaultSettings() has already called it or hasn't
        //
        if( !staticInstance->mHookerCommon2->init( staticInstance ) ){
            
            DBG_PRINT_ERROR(( "staticInstance->mHookerCommon.init() failed\n" ));
            
            staticInstance->free();
            return NULL;
        }

        assert( staticInstance && staticInstance == (DldHookerCommonClass2<CC,HC>::mStaticInstance) );
        
        return DldHookerCommonClass2<CC,HC>::mStaticInstance;
        
    }// end of the static instance creation

    panic("An unreachable code was hit!");
}

//--------------------------------------------------------------------

template <class CC, class HC>
DldHookerCommonClass2<CC,HC>*
DldHookerCommonClass2<CC,HC>::fCommonHooker2()
{
    CC*                            hookerObject;
    
    hookerObject = DldHookerCommonClass2<CC,HC>::fStaticContainerClassInstance();
    assert( hookerObject );
    if( NULL == hookerObject )
        return NULL;
    
    assert( hookerObject->mHookerCommon2 );
    return hookerObject->mHookerCommon2;
}

//--------------------------------------------------------------------

template <class CC, class HC>
DldHookerCommonClass*
DldHookerCommonClass2<CC,HC>::fBaseCommonHooker()
{
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    DldHookerCommonClass*          baseCommonHooker;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return NULL;
    
    baseCommonHooker = &commonHooker2->mHookerCommon;
    
    return baseCommonHooker;
}

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::fAddHookingFunctionInternal(
    __in unsigned int hookIndex,// base is 0x0
    __in unsigned int vtableIndex,// base is 0x1
    __in DldVtableFunctionPtr HookingFunction )
{
    assert( NULL != HookingFunction );
    assert( hookIndex < sizeof( mHookedVtableFunctionsInfo )/sizeof( mHookedVtableFunctionsInfo[0] ) );
    
    if( (-1) == this->mHookedVtableFunctionsInfo[ hookIndex ].VtableIndex ||
        hookIndex >= sizeof( mHookedVtableFunctionsInfo )/sizeof( mHookedVtableFunctionsInfo[0] ) ){
        
        DBG_PRINT_ERROR(( "a wrong index %u, out of boundary, %s\n",
                          hookIndex, DldHookerCommonClass2<CC,HC>::fGetHookedClassName() ));
        return;
    }
    
    assert( 0x0  == this->mHookedVtableFunctionsInfo[ hookIndex ].VtableIndex );
    assert( NULL == this->mHookedVtableFunctionsInfo[ hookIndex ].OriginalFunction );
    assert( NULL == this->mHookedVtableFunctionsInfo[ hookIndex ].HookingFunction );
    
#if defined( DBG )
    //
    // check for a double hook registration
    //
    for(int i=0x0; (-1) != this->mHookedVtableFunctionsInfo[ i ].VtableIndex; ++i){
        
        assert( this->mHookedVtableFunctionsInfo[ i ].VtableIndex != vtableIndex );
        assert( this->mHookedVtableFunctionsInfo[ i ].HookingFunction != HookingFunction );
        
        if( this->mHookedVtableFunctionsInfo[ i ].VtableIndex == vtableIndex ||
            this->mHookedVtableFunctionsInfo[ i ].HookingFunction == HookingFunction ){
            
            DBG_PRINT_ERROR(( "a double hook registration for [%u,%u] with [%u,%u] for %s\n",
                               i,this->mHookedVtableFunctionsInfo[ i ].VtableIndex,
                               hookIndex, vtableIndex,
                               DldHookerCommonClass2<CC,HC>::fGetHookedClassName() ));
        }// end if
    }// end for
#endif//#if defined( DBG )
    
    //
    // vtable indices start at 0x1
    //
    if( 0x0 != this->mHookedVtableFunctionsInfo[ hookIndex ].VtableIndex ){
        
        DBG_PRINT_ERROR(("redefinition, a new value %u , an old value %u\n",
                         hookIndex,
                         this->mHookedVtableFunctionsInfo[ hookIndex ].VtableIndex));
    }
    
    this->mHookedVtableFunctionsInfo[ hookIndex ].VtableIndex     = vtableIndex;
    this->mHookedVtableFunctionsInfo[ hookIndex ].HookingFunction = HookingFunction;
}

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::fAddHookingFunctionExternal(
    __in int hookIndex,// base is 0x0 relative to external caller
    __in int vtableIndex,// base is 0x1
    __in DldVtableFunctionPtr HookingFunction )
{
    assert( this->mInit );
    assert( (-1) != vtableIndex );// if (-1) than the function is not virtual!
    
    //
    // cast the index to the internal value and call the internal function
    //
    this->fAddHookingFunctionInternal( hookIndex + DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks, vtableIndex, HookingFunction );
}

//--------------------------------------------------------------------

template <class CC, class HC>
OSMetaClassBase::_ptf_t
DldHookerCommonClass2<CC,HC>::fGetOriginalFunction( __in OSObject* hookedObject, __in unsigned int indx )
{
    assert( indx < ( DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks + CC::kDld_NumberOfAddedHooks ) );
    return mHookerCommon.GetOriginalFunction( hookedObject, indx );
}

//--------------------------------------------------------------------

template <class CC, class HC>
OSMetaClassBase::_ptf_t
DldHookerCommonClass2<CC,HC>::fGetOriginalFunctionExternal( __in OSObject* hookedObject, __in unsigned int indx )
{
    assert( indx < ( DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks + CC::kDld_NumberOfAddedHooks ) );
    return mHookerCommon.GetOriginalFunction( hookedObject, indx + DldHookerCommonClass2<CC,HC>::kDld_NumberOfBaseHooks);
}

//--------------------------------------------------------------------

template <class CC, class HC>
const char*
DldHookerCommonClass2<CC,HC>::fGetHookedClassName()
{
    return CC::fGetHookedClassName();
}

//--------------------------------------------------------------------

//
// a definiton for a start hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
// in case of the start the original function must be called first to get
// a full-fleged object at return with all parameteres set to valid values
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::start_hook( IOService * provider )
{
    typedef bool(*startFunc)( HC*  __this, IOService * provider );
    
    bool                           started;
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    startFunc                      Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (startFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                               DldHookerCommonClass2<CC,HC>::kDld_start_hook );

    //
    // call original
    //
    started = Original( reinterpret_cast<HC*>(this), provider );
    if( started )
        commonHooker2->mHookerCommon.start( (IOService*)this, provider );
    
    return started;
}


//--------------------------------------------------------------------

//
// a definiton for a open hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
// in case of the open the original function must be called first
//

template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::open_hook(  IOService * forClient, IOOptionBits options, void * arg )
{
    typedef bool(*openFunc)( HC*  __this, IOService * forClient, IOOptionBits options, void * arg );
    
    bool                           opened;
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    openFunc                       Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (openFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                              DldHookerCommonClass2<CC,HC>::kDld_open_hook );
    
    //
    // call original
    //
    opened = Original( reinterpret_cast<HC*>(this), forClient, options, arg );
    if( opened )
        commonHooker2->mHookerCommon.open( (IOService*)this, forClient, options, arg );
    
    return opened;
}

//--------------------------------------------------------------------

//
// a definiton for a free hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
void DldHookerCommonClass2<CC,HC>::free_hook()
{
    typedef void(*freeFunc)( HC*  __this );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    freeFunc                       Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return;
    
    Original = (freeFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                              DldHookerCommonClass2<CC,HC>::kDld_free_hook );
    
    commonHooker2->mHookerCommon.free( (IOService*)this );
    
    return Original( reinterpret_cast<HC*>(this));
}

//--------------------------------------------------------------------

//
// a definiton for a requestTerminate hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::requestTerminate_hook( IOService * provider, IOOptionBits options )
{
    typedef bool(*requestTerminateFunc)( HC*  __this, IOService * provider, IOOptionBits options );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    requestTerminateFunc           Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (requestTerminateFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                          DldHookerCommonClass2<CC,HC>::kDld_requestTerminate_hook );
    
    if( !commonHooker2->mHookerCommon.requestTerminate( (IOService*)this, provider, options ) )
        return false;
    
    return Original( reinterpret_cast<HC*>(this), provider, options );
}

//--------------------------------------------------------------------

//
// a definiton for a willTerminate hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::willTerminate_hook( IOService * provider, IOOptionBits options )
{
    typedef bool(*willTerminateFunc)( HC*  __this, IOService * provider, IOOptionBits options );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    willTerminateFunc              Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (willTerminateFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                       DldHookerCommonClass2<CC,HC>::kDld_willTerminate_hook );
    
    if( !commonHooker2->mHookerCommon.willTerminate( (IOService*)this, provider, options ) )
        return false;
    
    return Original( reinterpret_cast<HC*>(this), provider, options );
}

//--------------------------------------------------------------------

//
// a definiton for a didTerminate hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::didTerminate_hook( IOService * provider, IOOptionBits options, bool * defer )
{
    
    typedef bool(*didTerminateFunc)( HC*  __this, IOService * provider, IOOptionBits options, bool * defer );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    didTerminateFunc               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    bool originalRetVal;
    
    Original = (didTerminateFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_didTerminate_hook );
    
    originalRetVal = Original( reinterpret_cast<HC*>(this), provider, options, defer );
    
    //
    // we must call a hook after the original call as we need a *defer value after the call returns,
    // if *defer is true the underlying stack will not be torn down and a driver will call
    // didTerminate() second time later thus initiating a stack torn down
    //
    commonHooker2->mHookerCommon.didTerminate( (IOService*)this, provider, options, defer );
    
    return originalRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a terminate hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::terminate_hook( IOOptionBits options )
{
    typedef bool(*terminateFunc)( HC*  __this, IOOptionBits options );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    terminateFunc                  Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (terminateFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                   DldHookerCommonClass2<CC,HC>::kDld_terminate_hook );
    
    
    if( !commonHooker2->mHookerCommon.terminate( (IOService*)this, options ) )
        return false;
    
    return Original( reinterpret_cast<HC*>(this), options );
}

//--------------------------------------------------------------------

//
// a definiton for a terminateClient hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::terminateClient_hook( IOService * client, IOOptionBits options )
{
    
    typedef bool(*terminateClientFunc)( HC*  __this,  IOService * client, IOOptionBits options );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    terminateClientFunc            Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (terminateClientFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                         DldHookerCommonClass2<CC,HC>::kDld_terminateClient_hook );
    
    if( !commonHooker2->mHookerCommon.terminateClient( (IOService*)this, client, options ) )
        return false;
    
    return Original( reinterpret_cast<HC*>(this), client, options );
}

//--------------------------------------------------------------------

//
// a definiton for a finalize hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//

template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::finalize_hook( IOOptionBits options )
{
    typedef bool(*finalizeFunc)( HC*  __this, IOOptionBits options );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    finalizeFunc                   Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (finalizeFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                  DldHookerCommonClass2<CC,HC>::kDld_finalize_hook );
    
    if( !commonHooker2->mHookerCommon.finalize( (IOService*)this, options ) )
        return false;
    
    return Original( reinterpret_cast<HC*>(this), options );
}

//--------------------------------------------------------------------

//
// a definiton for an attach hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::attach_hook( IOService * provider )
{
    
    typedef bool(*attachFunc)( HC*  __this, IOService * provider );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    attachFunc                     Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (attachFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                DldHookerCommonClass2<CC,HC>::kDld_attach_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), provider );
    if( bRetVal )
        commonHooker2->mHookerCommon.attach( (IOService*)this, provider );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for an attachToChild hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::attachToChild_hook( IORegistryEntry * child,const IORegistryPlane * plane )
{
    typedef bool(*attachToChildFunc)( HC*  __this, IORegistryEntry * child, const IORegistryPlane * plane );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    attachToChildFunc              Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    Original = (attachToChildFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                DldHookerCommonClass2<CC,HC>::kDld_attachToChild_hook );
    
    if( !commonHooker2->mHookerCommon.attachToChild( (IOService*)this, child, plane ) )
        return false;
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), child, plane );
    if( bRetVal )
        commonHooker2->mHookerCommon.attachToChild( (IOService*)this, child, plane );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a detach hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
void DldHookerCommonClass2<CC,HC>::detach_hook( IOService * provider )
{
    typedef void(*detachFunc)( HC*  __this, IOService * provider );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    detachFunc                     Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return;
    
    Original = (detachFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                               DldHookerCommonClass2<CC,HC>::kDld_detach_hook );
    
    Original( reinterpret_cast<HC*>(this), provider );
    commonHooker2->mHookerCommon.detach( (IOService*)this, provider );
}

//--------------------------------------------------------------------

//
// a definiton for a setPropertyTable hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
void DldHookerCommonClass2<CC,HC>::setPropertyTable_hook( OSDictionary * dict )
{
    typedef void (*setPropertyTableFunc)( HC*  __this, OSDictionary * dict );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setPropertyTableFunc           Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return;
    
    Original = (setPropertyTableFunc)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                DldHookerCommonClass2<CC,HC>::kDld_setPropertyTable_hook );
    Original( reinterpret_cast<HC*>(this), dict );
    commonHooker2->mHookerCommon.setPropertyTable( (IOService*)this, dict );
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty1 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty1_hook(const OSSymbol * aKey, OSObject * anObject)
{
    typedef bool (*setProperty1Func)( HC*  __this, const OSSymbol * aKey, OSObject * anObject );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty1Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty1Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_setProperty1_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, anObject );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty1( (IOService*)this, aKey, anObject );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty2 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty2_hook(const OSString * aKey, OSObject * anObject)
{
    typedef bool (*setProperty2Func)( HC*  __this, const OSString * aKey, OSObject * anObject );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty2Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty2Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_setProperty2_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, anObject );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty2( (IOService*)this, aKey, anObject );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty3 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty3_hook(const char * aKey, OSObject * anObject)
{
    
    typedef bool (*setProperty3Func)( HC*  __this, const char * aKey, OSObject * anObject );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty3Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty3Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_setProperty3_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, anObject );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty3( (IOService*)this, aKey, anObject );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty4 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty4_hook(const char * aKey, const char * aString)
{
    
    typedef bool (*setProperty4Func)( HC*  __this, const char * aKey, const char * aString );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty4Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty4Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_setProperty4_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, aString );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty4( (IOService*)this, aKey, aString );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty5 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty5_hook(const char * aKey, bool aBoolean)
{
    typedef bool (*setProperty5Func)( HC*  __this, const char * aKey, bool aBoolean );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty5Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty5Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_setProperty5_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, aBoolean );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty5( (IOService*)this, aKey, aBoolean );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty6 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty6_hook(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits)
{
    
    typedef bool (*setProperty6Func)( HC*  __this, const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty6Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty6Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                      DldHookerCommonClass2<CC,HC>::kDld_setProperty6_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, aValue, aNumberOfBits );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty6( (IOService*)this, aKey, aValue, aNumberOfBits );
        
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty7 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::setProperty7_hook(const char * aKey, void * bytes, unsigned int length)
{
    typedef bool (*setProperty7Func)( HC*  __this, const char * aKey, void * bytes, unsigned int length );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    setProperty7Func               Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return true;
    
    Original = (setProperty7Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                     DldHookerCommonClass2<CC,HC>::kDld_setProperty7_hook );
    
    bool bRetVal;
    bRetVal = Original( reinterpret_cast<HC*>(this), aKey, bytes, length );
    if( bRetVal )
        commonHooker2->mHookerCommon.setProperty7( (IOService*)this, aKey, bytes, length );
    
    return bRetVal;
}

//--------------------------------------------------------------------

//
// a definiton for a removeProperty1 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
void DldHookerCommonClass2<CC,HC>::removeProperty1_hook(const OSSymbol * aKey)
{
    typedef void (*removeProperty1Func)( HC*  __this, const OSSymbol * aKey );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    removeProperty1Func            Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return;
    
    Original = (removeProperty1Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                     DldHookerCommonClass2<CC,HC>::kDld_removeProperty1_hook );
    
    commonHooker2->mHookerCommon.removeProperty1( (IOService*)this, aKey );
    
    Original( reinterpret_cast<HC*>(this), aKey );
}

//--------------------------------------------------------------------

//
// a definiton for a removeProperty2 hook, "this" points to a hooked HC instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
void DldHookerCommonClass2<CC,HC>::removeProperty2_hook(const OSString * aKey)
{
    typedef void (*removeProperty2Func)( HC*  __this, const OSString * aKey );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    removeProperty2Func            Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return;
    
    Original = (removeProperty2Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                        DldHookerCommonClass2<CC,HC>::kDld_removeProperty2_hook );
    
    commonHooker2->mHookerCommon.removeProperty2( (IOService*)this, aKey );
    
    Original( reinterpret_cast<HC*>(this), aKey );
}

//--------------------------------------------------------------------

//
// a definiton for a removeProperty3 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
void DldHookerCommonClass2<CC,HC>::removeProperty3_hook(const char * aKey)
{
    typedef void (*removeProperty3Func)( HC*  __this, const char * aKey );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    removeProperty3Func            Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return;
    
    Original = (removeProperty3Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                         DldHookerCommonClass2<CC,HC>::kDld_removeProperty3_hook );
    
    commonHooker2->mHookerCommon.removeProperty3( (IOService*)this, aKey );
    
    Original( reinterpret_cast<HC*>(this), aKey );
}

//--------------------------------------------------------------------

//
// a definiton for a newUserClient hook, newUserClient is an verloaded function but only one
// is used by the system, "this" points to a hooked hookedClassName instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
IOReturn DldHookerCommonClass2<CC,HC>::newUserClient1_hook( task_t owningTask, void * securityID,
                                                            UInt32 type,  OSDictionary * properties,
                                                            IOUserClient ** handler )
{
    typedef IOReturn (*newUserClient1Func)( HC*  __this, task_t owningTask, void * securityID,
                                            UInt32 type,  OSDictionary * properties,
                                            IOUserClient ** handler );
    
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    newUserClient1Func             Original;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return kIOReturnError;
    
    Original = (newUserClient1Func)commonHooker2->fGetOriginalFunction( (OSObject*)this,
                                                                        DldHookerCommonClass2<CC,HC>::kDld_newUserClient1_hook );
    
    //
    // check whether a client is allowed to connect to the device
    //
    IOReturn   securityStatus;
    securityStatus = commonHooker2->mHookerCommon.newUserClient1( (IOService*)this, owningTask, securityID, type, properties, handler );
    
    if( kIOReturnSuccess == securityStatus )
        return Original( reinterpret_cast<HC*>(this),  owningTask, securityID, type, properties, handler );
    else
        return securityStatus;

}

//--------------------------------------------------------------------

template <class CC, class HC>
IOReturn
DldHookerCommonClass2<CC,HC>::fHookObject( __inout OSObject* object, __in DldHookType type )
{
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    return commonHooker2->mHookerCommon.HookObject( object, type );
}

//--------------------------------------------------------------------

template <class CC, class HC>
IOReturn
DldHookerCommonClass2<CC,HC>::fUnHookObject( __inout OSObject* object, __in DldHookType type, __in DldInheritanceDepth Depth )
{
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    return commonHooker2->mHookerCommon.UnHookObject( object );
}

//--------------------------------------------------------------------

//
// a definiton for a first publish callback
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::fObjectFirstPublishCallback( __in IOService * newService )
{
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    return commonHooker2->mHookerCommon.ObjectFirstPublishCallback( newService );
}

//--------------------------------------------------------------------

//
// a definiton for a termination publish callback
//
template <class CC, class HC>
bool DldHookerCommonClass2<CC,HC>::fObjectTerminatedCallback( __in IOService * terminatedService )
{
    DldHookerCommonClass2<CC,HC>*  commonHooker2;
    
    commonHooker2 = DldHookerCommonClass2<CC,HC>::fCommonHooker2();
    assert( commonHooker2 );
    if( NULL == commonHooker2 )
        return false;
    
    return commonHooker2->mHookerCommon.ObjectTerminatedCallback( terminatedService );
}
/*
//--------------------------------------------------------------------

//
// a definition for a hook callback called by the hook engine when the object
// of the desired class is found or has been registered,
// the object parameter must be of a superclassName type ( this couldn't be
// an object contained in an object of derived class, i.e. this must be a leaf
// object )
//
// a notice why OSDynamicCast can't be used here and the brute force cast is used -
//  if HookedClassObj = (hookedClassName*)object would be replaced by
//  HookedClassObj = OSDynamicCast( hookedClassName, object );
//  the hooked meta class object had to be linked to the module and the corresponding
//  bundle library should be mentioned in the plist file,
//  the check after the casting was left intact as HookedClassObj = (hookedClassName*)object
//  would have been called to show that the object must be of the hookedClassName type
//
#define DldDefineCommonIOServiceHook_HookObject( className, superclassName, hookedClassName ) \
IOReturn className::HookObject( __inout OSObject* object, __in DldHookType type )\
{\
hookedClassName*   HookedClassObj;\
\
HookedClassObj = reinterpret_cast<hookedClassName*>(object);\
assert( HookedClassObj );\
if( NULL == HookedClassObj )\
return kIOReturnBadArgument;\
\
return className::HookObjectInt( HookedClassObj, type );\
}
*/

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::LockShared()
{   
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() != this->exclusiveThread );
#endif//DBG
    
    IORWLockWrite( this->rwLock );
    
#if defined(DBG)
    assert( NULL == this->exclusiveThread );
    this->exclusiveThread = current_thread();
#endif//DBG
    
};

//--------------------------------------------------------------------

template <class CC, class HC>
void
DldHookerCommonClass2<CC,HC>::UnLockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() == this->exclusiveThread );
    this->exclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

#endif//_DLDHOOKERCOMMONCLASS2_H