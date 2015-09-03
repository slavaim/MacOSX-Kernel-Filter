/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDHOOKERCOMMONCLASS_H
#define _DLDHOOKERCOMMONCLASS_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODataQueue.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include <kern/locks.h>
#include "DldCommon.h"
#include "DldCommonHashTable.h"
#include "DldIOService.h"

//--------------------------------------------------------------------

class DldIOKitHookEngine;
extern DldIOKitHookEngine* gHookEngine;

extern
IOReturn
CopyIoServicePlane(
    __in const IORegistryPlane* plane,
    __in DldIOKitHookEngine*    HookEngine
    );

//--------------------------------------------------------------------

class DldHookerBaseInterface{
    
protected:
    
    virtual bool init() = 0;
    virtual void free() = 0;
    
public:
    virtual const char* fGetClassName() = 0;
};

//--------------------------------------------------------------------

typedef enum _DldInheritanceDepth{
    DldInheritanceDepth_0 = 0,// the leaf class, i.e. not a parent or ancestor
    DldInheritanceDepth_1,
    DldInheritanceDepth_2,
    DldInheritanceDepth_3,
    DldInheritanceDepth_4,
    DldInheritanceDepth_5,
    DldInheritanceDepth_6,
    DldInheritanceDepth_7,
    DldInheritanceDepth_8,
    DldInheritanceDepth_9,
    DldInheritanceDepth_10,
    DldInheritanceDepth_Maximum
} DldInheritanceDepth;

//--------------------------------------------------------------------

typedef void (*DldVtableFunctionPtr)(void);

typedef struct _DldHookedFunctionInfo{
    
    //
    // an index for a function in the hooked class vtable,
    // (-1) is a sign of a trailing descriptor for an array
    //
    unsigned int            VtableIndex;
    
    //
    // an original function address from the hooked class vtable
    //
    DldVtableFunctionPtr    OriginalFunction;
    
    //
    // an address of hooking function which replaces the original one
    //
    DldVtableFunctionPtr    HookingFunction;
    
} DldHookedFunctionInfo;

//--------------------------------------------------------------------

typedef enum _DldHookType{
    
    DldHookTypeUnknown = 0x0,
    
    //
    // the poiter to a vtable for an object is replaced thus
    // the hook affects only the object,
    // the key is of OSObject* type
    //
    DldHookTypeObject = 0x1,
    
    //
    // the function adresses are replaced in the original vtable thus
    // affecting all objects of the same type,
    // the keys are of the DldHookTypeVtableObjKey & DldHookTypeVtableKey types
    //
    DldHookTypeVtable = 0x2,
    
    //
    // a terminating value, not an actual type
    //
    DldHookTypeMaximum
    
} DldHookType;

//
// a key for the object hooked by a direct vtable hook ( i.e. the original vtable has been tampered with ),
// the metaClass defines the class for which the vtable hook has been done
//
typedef struct _DldHookTypeVtableObjKey{
    const OSObject* Object;
    const OSMetaClass* metaClass;
    DldInheritanceDepth InheritanceDepth;
} DldHookTypeVtableObjKey;

//
// a key for the a vtable hooked by a direct vtable hook ( i.e. the original vtable has been tampered with ),
// the metaClass defines the class for which the vtable hook has been done
//
typedef struct _DldHookTypeVtableKey{
    //
    // there are two keys for Vtabe entry - one with original Vtable and the second with NULL
    //
    OSMetaClassBase::_ptf_t* Vtable;
    const OSMetaClass* metaClass;
    DldInheritanceDepth InheritanceDepth;
} DldHookTypeVtableKey;

//
// a key used only in the debug buils, saves the reverse transformation from the address
// of the hooked vtable's entry to the object which was used for hooking
//
typedef struct _DldDbgVtableHookToObject{
    struct _DldDbgVtableHookToObject* copy;
    const OSObject*           Object;
    const OSMetaClass*        MetaClass;
    OSMetaClassBase::_ptf_t*  Vtable;
    OSMetaClassBase::_ptf_t*  VtableEntry;// also a key
    DldHookedFunctionInfo*    HookInfo;
} DldDbgVtableHookToObject;

//
// this is more a structure than a class, the OSObject as a base class is used for a purpose of reference counting
//
class DldHookedObjectEntry: public OSObject{
    
    OSDeclareDefaultStructors( DldHookedObjectEntry )
    
public:
    
    static DldHookedObjectEntry* allocateNew();
    
protected:
    
    virtual void free();
    
public:
    
    union{
        
        //
        // DldHookEntryTypeObject
        //
        OSObject* Object;
        
        //
        // DldHookEntryTypeVtableObj
        //
        DldHookTypeVtableObjKey    VtableHookObj;
        
        //
        // DldHookEntryTypeVtable for non null entry, for null entry
        // a key can be constructed from this one by setting the Vtable value to NULL
        //
        DldHookTypeVtableKey       VtableHookVtable;
        
    } Key;
    
    typedef enum _DldEntryType{
        DldHookEntryTypeUnknown = 0x0,
        DldHookEntryTypeObject,   // OSObject* key
        DldHookEntryTypeVtableObj,// DldHookTypeVtableObjKey key
        DldHookEntryTypeVtable    // DldHookTypeVtableKey key
    } DldEntryType;
    
    DldEntryType   Type;
    
    union{
        
        struct CommonHeader{
            
            //
            // the value of the HookedVtableFunctionsInfo member is as follows
            //
            
            //
            // for an entry of the DldHookEntryTypeObject type:
            //    a cahed value for ClassHookerObject->HookedFunctonsInfo
            //    must NOT be freed when the object of the DldHookEntryTypeObject
            //    type is freed
            //
            
            //
            // for an entry of the DldHookEntryTypeVtableObj type:
            //    a cahed value for VtableEntry->HookedVtableFunctionsInfo,
            //    must NOT be freed when the entry of the DldHookEntryTypeVtableObj
            //    type is freed as will be freed when the related entry of the
            //    DldHookEntryTypeVtable type is freed
            //
            
            //
            // for an entry of the DldHookEntryTypeVtable type:
            //    a pointer to an array, used only if the entry describes
            //    a directly hooked vtable which can be found by DldHookTypeVtableKey,
            //    the last entry in the array contains (-1) as an index,
            //    the entry type is DldHookEntryTypeVtable, the array must be alocated
            //    by calling IOMalloc and will be freed when the entry is finalized
            //    by calling IOFree
            //
            
            DldHookedFunctionInfo*        HookedVtableFunctionsInfo;
            
        } Common;
        
        //
        // DldHookEntryTypeObject
        //
        struct{
            
            //
            // always a first member!
            //
            struct CommonHeader           Common;
            
            //
            // an original Vtable address
            //
            OSMetaClassBase::_ptf_t*      OriginalVtable;
            
        } TypeObject;
        

        //
        // DldHookEntryTypeVtableObj
        //
        struct{
            
            //
            // always a first member!
            //
            struct CommonHeader           Common;
            
            //
            // a pointer to a referenced entry for a hooked vtable which
            // can be found by DldHookTypeVtableKey, used if the entry
            // describes the object with the directly hooked vtable
            // i.e. has the DldHookEntryTypeVatbleObj type
            //
            DldHookedObjectEntry*         VtableEntry;
            
            //
            // an original Vtable address
            //
            OSMetaClassBase::_ptf_t*      OriginalVtable;
            
        } TypeVatbleObj;
        

        //
        // DldHookEntryTypeVtable
        //
        struct{
            
            //
            // always a first member!
            //
            struct CommonHeader           Common;

            unsigned int                  HookedVtableFunctionsInfoEntriesNumber;
            
            //
            // when the reference count drops to zero the entry is removed from
            // the hash and the Vtable is unhooked
            //
            unsigned int                  ReferenceCount;

            
        } TypeVtable;
        
    } Parameters;
    
    //
    // an inheritance depth
    //
    DldInheritanceDepth    InheritanceDepth;
    
    //
    // a hooking class object, not referenced
    //
    DldHookerBaseInterface* ClassHookerObject;
};

//--------------------------------------------------------------------

class DldHookedObjectsHashTable
{
    
private:
    
    ght_hash_table_t* HashTable;
    IORWLock*         RWLock;
    
#if defined(DBG)
    thread_t ExclusiveThread;
#endif//DBG
    
    //
    // returns an allocated hash table object
    //
    static DldHookedObjectsHashTable* withSize( int size, bool non_block );
    
    //
    // free must be called before the hash table object is deleted
    //
    void free();
    
    //
    // as usual for IOKit the desctructor and constructor do nothing
    // as it is impossible to return an error from the constructor
    // in the kernel mode
    //
    DldHookedObjectsHashTable()
    {
        this->HashTable = NULL;
        
#if defined(DBG)
        this->ExclusiveThread = NULL;
#endif//DBG
    }
    
    //
    // the destructor checks that the free() has been called
    //
    ~DldHookedObjectsHashTable(){ assert( !this->HashTable && !this->RWLock ); };
    
public:
    
    static bool CreateStaticTableWithSize( int size, bool non_block );
    static void DeleteStaticTable();
    
    //
    // the objEntry is referenced by AddObject, so the caller should release the object
    // if the true value is returned
    //
    bool   AddObject( __in OSObject* obj, __in DldHookedObjectEntry* objEntry, __in bool errorIfPresent = true );
    bool   AddObject( __in DldHookTypeVtableKey* vtableHookVtable, __in DldHookedObjectEntry* objEntry, __in bool errorIfPresent = true );
    bool   AddObject( __in DldHookTypeVtableObjKey* vtableHookObj, __in DldHookedObjectEntry* objEntry, __in bool errorIfPresent = true );
    
    //
    // removes the entry from the hash and returns the removed entry, NULL if there
    // is no entry for an object or vtable, the returned entry is referenced!
    //
    DldHookedObjectEntry*   RemoveObject( __in OSObject* obj );
    DldHookedObjectEntry*   RemoveObject( __in DldHookTypeVtableKey* vtableHookVtable );
    DldHookedObjectEntry*   RemoveObject( __in DldHookTypeVtableObjKey* vtableHookObj );
    
    //
    // the returned object is referenced if the reference parameter is true! the caller must release the object!
    //
    DldHookedObjectEntry*   RetrieveObjectEntry( __in OSObject* obj, __in bool reference = true );
    DldHookedObjectEntry*   RetrieveObjectEntry( __in DldHookTypeVtableKey* vtableHookVtable, __in bool reference = true );
    DldHookedObjectEntry*   RetrieveObjectEntry( __in DldHookTypeVtableObjKey* vtableHookObj, __in bool reference = true );
    
#if defined( DBG )
    //
    // used only for the debug, leaks memory in the release
    //
    bool AddObject( __in OSMetaClassBase::_ptf_t* key, __in DldDbgVtableHookToObject* DbgEntry, __in bool errorIfPresent = true );
    DldDbgVtableHookToObject* RetrieveObjectEntry( __in OSMetaClassBase::_ptf_t*    key );
#endif//DBG
    
    void
    LockShared()
    {   assert( this->RWLock );
        assert( preemption_enabled() );
        
        IORWLockRead( this->RWLock );
    };
    
    
    void
    UnLockShared()
    {   assert( this->RWLock );
        assert( preemption_enabled() );
        
        IORWLockUnlock( this->RWLock );
    };
    
    
    void
    LockExclusive()
    {
        assert( this->RWLock );
        assert( preemption_enabled() );
        
#if defined(DBG)
        assert( current_thread() != this->ExclusiveThread );
#endif//DBG
        
        IORWLockWrite( this->RWLock );
        
#if defined(DBG)
        assert( NULL == this->ExclusiveThread );
        this->ExclusiveThread = current_thread();
#endif//DBG
        
    };
    
    
    void
    UnLockExclusive()
    {
        assert( this->RWLock );
        assert( preemption_enabled() );
        
#if defined(DBG)
        assert( current_thread() == this->ExclusiveThread );
        this->ExclusiveThread = NULL;
#endif//DBG
        
        IORWLockUnlock( this->RWLock );
    };
    
    
    static DldHookedObjectsHashTable* sHashTable;
};

//--------------------------------------------------------------------

//
// a definition extracted from OSMetaClass.h,
// a definition for a single inherited class in the Apple ABI ( i.e. the same as in gcc family )
//
typedef union _DldSingleInheritingClassObjectPtr{
    const OSObject *fObj;// a pointer to the object
    OSMetaClassBase::_ptf_t **vtablep;// a pointer to the object's vtable pointer
} DldSingleInheritingClassObjectPtr;

typedef union _DldSingleInheritingHookerClassObjectPtr{
    const DldHookerBaseInterface *fObj;// a pointer to the object
    OSMetaClassBase::_ptf_t **vtablep;// a pointer to the object's vtable pointer
} DldSingleInheritingHookerClassObjectPtr;

//--------------------------------------------------------------------

#ifndef APPLE_KEXT_LEGACY_ABI// 10.5 SDK lacks this definition

    #if defined(__LP64__)
        /*! @parseOnly */
        #define APPLE_KEXT_LEGACY_ABI  0
    #elif defined(__arm__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
        #define APPLE_KEXT_LEGACY_ABI  0
    #else
        #define APPLE_KEXT_LEGACY_ABI  1
    #endif

#endif//APPLE_KEXT_LEGACY_ABI


#if APPLE_KEXT_LEGACY_ABI

//
// a definition extracted from OSMetaClass.h,
// arcane evil code interprets a C++ pointer to function as specified in the
// -fapple-kext ABI, i.e. the gcc-2.95 generated code.  IT DOES NOT ALLOW
// the conversion of functions that are from MULTIPLY inherited classes.
//
typedef union _DldClassMemberFunction{
    void (OSMetaClassBase::*fIn)(void);
    struct { 	// Pointer to member function 2.95
        unsigned short fToff;
        short  fVInd;
        union {
            OSMetaClassBase::_ptf_t fPFN;
            short  fVOff;
        } u;
    } fptmf2;
} DldClassMemberFunction;

//
// a function extracted from OSMetaClass.h,
// returns a vtable index starting from 0x1 for a function or (-1) if the function is not virtual,
// this function was crafted from the _ptmf2ptf function, also se the OSMemberFunctionCast
// macro for reference
//
// @param func The pointer to member function itself, something like &Base::func.
//

//
// Arcane evil code interprets a C++ pointer to function as specified in the
// -fapple-kext ABI, i.e. the gcc-2.95 generated code.  IT DOES NOT ALLOW
// the conversion of functions that are from MULTIPLY inherited classes.
//

static 
//#if !defined(DBG)
inline
//#endif//!DBG
unsigned int
DldConvertFunctionToVtableIndex(
    __in void (OSMetaClassBase::*func)(void)
    )
{
    DldClassMemberFunction map;
    
    map.fIn = func;
    if (map.fptmf2.fToff){
        
        DBG_PRINT_ERROR(("A non virtual function is passed to DldConvertFunctionToVtableIndex as a parameter"));
        assert( !"Multiple inheritance is not supported" );
#if defined( DBG )
        panic( "Multiple inheritance is not supported" );
#endif//DBG
        return (unsigned int)(-1);
        
    } else if (map.fptmf2.fVInd < 0) {
        
        //
        // Not a virtual, i.e. plain member func
        //
        DBG_PRINT_ERROR(("A non virtual function is passed to DldConvertFunctionToVtableIndex as a parameter"));
        assert( !"A non virtual function is passed to DldConvertFunctionToVtableIndex as a parameter" );
        return (unsigned int)(-1);
        
    } else {
        
        //
        // a virtual member function so return the index,
        // the base is 0x1
        //
        return map.fptmf2.fVInd;
    }
    
    panic( "We should not be here!" );
    return (unsigned int)(-1);
}

#else /* !APPLE_KEXT_LEGACY_ABI */

#ifdef __arm__
#error "we doesn't support ARM here"
#else /* __arm__ */

//
// a function extracted from OSMetaClass.h,
// returns a vtable index starting from 0x1 for a function or (-1) if the function is not virtual,
// this function was crafted from the _ptmf2ptf function, also se the OSMemberFunctionCast
// macro for reference
//
// @param func The pointer to member function itself, something like &Base::func.
//

//
// Slightly less arcane and slightly less evil code to do
// the same for kexts compiled with the standard Itanium C++
// ABI
//

static 
//#if !defined(DBG)
inline
//#endif//!DBG
unsigned int
DldConvertFunctionToVtableIndex(
    __in void (OSMetaClassBase::*func)(void)
    )
{
    union {
        void (OSMetaClassBase::*fIn)(void);
        uintptr_t fVTOffset;
        OSMetaClassBase::_ptf_t fPFN;
    } map;
    
    map.fIn = func;
    
    if (map.fVTOffset & 1) {
        //
        // virtual
        //
        
        //
        // Virtual member function so dereference vtable,
        // the base is 0x1
        //
        return (map.fVTOffset - 1)/sizeof( OSMetaClassBase::_ptf_t )+0x1;
    } else {
        
        //
        // Not virtual, i.e. plain member func
        //
        DBG_PRINT_ERROR(("Not virtual, i.e. plain member func has been passed to DldConvertFunctionToVtableIndex"));
        assert( !"Not virtual, i.e. plain member func has been passed to DldConvertFunctionToVtableIndex" );
#if defined( DBG )
        panic( "Not virtual, i.e. plain member func has been passed to DldConvertFunctionToVtableIndex" );
#endif//DBG
        return (unsigned int)(-1);
    }
    
    panic( "We should not be here!" );
    return (unsigned int)(-1);
}

#endif /* __arm__ */

#endif /* !APPLE_KEXT_LEGACY_ABI */


//--------------------------------------------------------------------

#define DldDeclareGetClassNameFunction()\
virtual const char* fGetClassName()\
{\
    char* name = (char*)__PRETTY_FUNCTION__; \
    int i;\
\
    for( i = 0x0; ':' != name[i] && '\0' != name[i]; ++i ){;}\
\
    if( '\0' != name[i] )\
        name[i] = '\0';\
    return (const char*)name;\
};

//--------------------------------------------------------------------

//
// the class is just a container for data and functions common for
// all hookers to avoid code duplication accross all hokers, it
// was introduced for convinience and doesn't take any reference,
// a class object is alive until containing it hooking object is alive,
// a class should not be instantiated on its own as a standalone object
//
class DldHookerCommonClass
{
    
private:
    
    IOReturn HookVtableIntWoLock( __inout OSObject* object );
    IOReturn HookObjectIntWoLock( __inout OSObject* object );
    IOReturn UnHookObjectIntWoLock( __inout OSObject* object );
    
    //
    // a type of the hook performed by the class
    //
    DldHookType                  HookType;
    
    //
    // an inheritance depth of the hooked class
    //
    DldInheritanceDepth          InheritanceDepth;
    
    //
    // a meta class and a name for the hooked class, the fields initialization is delayed
    // till the first object is being hooked
    //
    const OSMetaClass*            MetaClass;
    const OSSymbol*               ClassName; // retained
    
    //
    // set to true if this hooking class is defined through
    // a parent of the hooked class, usually this happens when the leaf class is
    // not exported for third party developers ( as in the case of AppleUSBEHCI )
    //
    //bool                          DefinedByParentClass;
    
    //
    // a number of hooked objects, valid only for DldHookTypeObject,
    //
    UInt32                        HookedObjectsCounter;
    
    //
    // the object is not referenced( can't be as not derived from OSObject )
    //
    DldHookerBaseInterface*       ClassHookerObject;
    
    OSMetaClassBase::_ptf_t*      OriginalVtable;
    OSMetaClassBase::_ptf_t*      HookClassVtable;
    OSMetaClassBase::_ptf_t*      NewVtable;// NULL if the direct vtable hook
    
    //
    // a buffer allocated for a new vtable
    //
    void*                         Buffer;
    vm_size_t                     BufferSize;                         
    
    //
    // a hooked class vtable's size, defines only the size of the table for 
    // a class which was used to define this hooking class, the actual table size
    // is defined by ( HookedVtableSize + VirtualsAdded )
    //
    unsigned int                  HookedVtableSize;
    
    //
    // defines the number of virtual functions added by children if this hooking class is defined through
    // a parent of the hooked class, usually this happens when the leaf class is
    // not exported for third party developers ( as in the case of AppleUSBEHCI )
    //
    // the VirtualsAddedSize member is initialized when the first object is being hooked as a 
    // real object is required to investigate its vtable
    //
    unsigned int                   VirtualsAddedSize;
    
    //
    // a pointer to array of vtable's functions info,
    // provided by a class containing an instance of this one as a member
    //
    DldHookedFunctionInfo*        HookedFunctonsInfo;
    
    //
    // a number of entries ( including terminating ) in the HookedFunctonsInfo array
    //
    unsigned int                  HookedFunctonsInfoEntriesNumber;
    
public:
    
    DldHookerCommonClass();   
    ~DldHookerCommonClass();
    
    //
    // saves an object pointer, the object is not referenced
    //
    void SetClassHookerObject( __in DldHookerBaseInterface*  ClassHookerObject );
    
    void SetHookedVtableSize( __in unsigned int HookedVtableSize );
    
    void SetInheritanceDepth( __in DldInheritanceDepth   Depth );
    
    //void SetMetaClass( __in const OSMetaClass*    MetaClass );
    
    //
    // NumberOfEntries includes a terminating entry
    //
    void SetHookedVtableFunctionsInfo( __in DldHookedFunctionInfo* HookedFunctonsInfo, __in unsigned int NumberOfEntries );
    
    //
    // returns unreferenced object
    //
    DldHookerBaseInterface* GetClassHookerObject(){ return this->ClassHookerObject; };
    
    DldHookType GetHookType(){ return this->HookType; };
    
    DldInheritanceDepth GetInheritanceDepth(){ return this->InheritanceDepth; };
    
    //
    // callbacks called by hooking functions ( the name defines the corresponding IOService hook )
    //
    bool start( __in IOService* serviceObject, __in IOService * provider );
    
    bool open( __in IOService* serviceObject, __in IOService * forClient, __in IOOptionBits options, __in void * arg );
    
    void free( __in IOService* serviceObject );
    
    bool terminate( __in IOService* serviceObject, __in IOOptionBits options );
    
    bool finalize( __in IOService* serviceObject, __in IOOptionBits options );
    
    bool attach( __in IOService* serviceObject, __in IOService * provider );
    
    bool attachToChild( __in IOService* serviceObject, IORegistryEntry * child,const IORegistryPlane * plane );
    
    void detach( __in IOService* serviceObject, __in IOService * provider );
    
    bool requestTerminate( __in IOService* serviceObject, __in IOService * provider, __in IOOptionBits options );
    
    bool willTerminate( __in IOService* serviceObject, __in IOService * provider, __in IOOptionBits options );
    
    bool didTerminate( __in IOService* serviceObject, __in IOService * provider, __in IOOptionBits options, __inout bool * defer );
    
    bool terminateClient( __in IOService* serviceObject, __in IOService * client, __in IOOptionBits options );
    
    void setPropertyTable( __in IOService* serviceObject, __in OSDictionary * dict );
    
    bool setProperty1( __in IOService* serviceObject, const OSSymbol * aKey, OSObject * anObject );
    bool setProperty2( __in IOService* serviceObject, const OSString * aKey, OSObject * anObject );
    bool setProperty3( __in IOService* serviceObject, const char * aKey, OSObject * anObject );
    bool setProperty4( __in IOService* serviceObject, const char * aKey, const char * aString );
    bool setProperty5( __in IOService* serviceObject, const char * aKey, bool aBoolean );
    bool setProperty6( __in IOService* serviceObject, const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits );
    bool setProperty7( __in IOService* serviceObject, const char * aKey, void * bytes, unsigned int length );
     
    void removeProperty1( __in IOService* serviceObject, const OSSymbol * aKey);
    void removeProperty2( __in IOService* serviceObject, const OSString * aKey);
    void removeProperty3( __in IOService* serviceObject, const char * aKey);
    
    IOReturn newUserClient1( __in IOService* serviceObject, task_t owningTask, void * securityID,
                            UInt32 type,  OSDictionary * properties, IOUserClient ** handler );
    
    IOReturn checkAndLogUserClientAccess( __in IOService* parentServiceObject,
                                          __in task_t owningTask,
                                          __in DldRequestedAccess* requestedAccess );
    
    IOReturn checkAndLogUserClientAccess( __in     IOService* parentServiceObject,
                                          __in     pid_t pid,
                                          __in_opt kauth_cred_t credentialRef,
                                          __in     DldRequestedAccess* requestedAccess );

    
    
    //
    // callbacks called as a reaction on some events happening with ah object
    //
    bool ObjectFirstPublishCallback( __in IOService* newService );
    
    bool ObjectTerminatedCallback( __in IOService* terminatedService );
    
    IOReturn HookObject( __inout OSObject* object, __in DldHookType type );
    
    IOReturn UnHookObject( __inout OSObject* object );
    
    OSMetaClassBase::_ptf_t GetOriginalFunction( __in OSObject* hookedObject, __in unsigned int indx );
    
    //
    // the VtableToHook and NewVtable vtables might be the same in case of a direct hook ( DldHookTypeVtable )
    //
    static void DldHookVtableFunctions( __in OSObject*                        object, // used only for the debug
                                        __inout DldHookedFunctionInfo*        HookedFunctonsInfo,
                                        __inout OSMetaClassBase::_ptf_t*      VtableToHook,
                                        __inout OSMetaClassBase::_ptf_t*      NewVtable );
    
    static void DldUnHookVtableFunctions( __inout DldHookedFunctionInfo*        HookedFunctonsInfo,
                                          __inout OSMetaClassBase::_ptf_t*      VtableToUnHook );
    
};

//--------------------------------------------------------------------

//
// the current design assumes that the hooking object is never destroyed, see GetOriginalFunction which relies
// on this behaviour for objects about which the driver is not aware and which vtables have been ubhooked
// but it happened that a call to a vtable's function was inside the hooking function waiting on the lock
// while the vtable was being unhooked ( ongoing onhooking )
//
// the class must implement the following functions
//  - static IOReturn HookObjectInt( __inout superclassName* object ) - a default DldDeclareCommonIOServiceHook_HookObjectInt
//                                                                      macro can be used instead
//  - void InitMembers() - series of default macroses ( DldInitMembers_* ) must be used for the imlementation
// 
// the following is a description of the class's members, functions and some related macroses :
//
// unsigned int  SuperVtableSize -    a super class vtable's size
//
// HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, MaximumHookingFunction )+1 ] - an array of vtable functions
//                                    descriptors for functions being hooked, the terminating enty has (-1) set as a vtable index
//
// DldHookerCommonClass    HookerCommon - an object of a class implementing a common hooking functionality
//
// After declaring and defining a hooking function its vtable index must be added to the SuperVtableIndicesToHook
// array by calling DldInitMembers_AddHookedFunctionVtableIndex in InitMembers() function or by adding the macro
// to the DldInitMembers_AddCommonHookedFunctionVtableIndices macro if this is a common hook
//

#define DldDeclareCommonIOServiceHookFunctionAndStructorsWithDepth( className, superClassName, hookedClassName, Depth ) \
\
friend class DldHookerCommonClass; \
friend class DldIOKitHookEngine; \
\
protected: \
\
    virtual bool start_hook( IOService * provider );\
    virtual bool open_hook( IOService * forClient, IOOptionBits options, void * arg );\
    virtual void free_hook();\
    virtual bool terminate_hook( IOOptionBits options = 0 ); \
    virtual bool finalize_hook( IOOptionBits options ); \
    virtual bool attach_hook( IOService * provider ); \
    virtual bool attachToChild_hook( IORegistryEntry * child,const IORegistryPlane * plane );\
    virtual void detach_hook( IOService * provider ); \
    virtual bool requestTerminate_hook( IOService * provider, IOOptionBits options );\
    virtual bool willTerminate_hook( IOService * provider, IOOptionBits options );\
    virtual bool didTerminate_hook( IOService * provider, IOOptionBits options, bool * defer );\
    virtual bool terminateClient_hook( IOService * client, IOOptionBits options );\
    virtual void setPropertyTable_hook( OSDictionary * dict );\
    virtual bool setProperty1_hook(const OSSymbol * aKey, OSObject * anObject);\
    virtual bool setProperty2_hook(const OSString * aKey, OSObject * anObject);\
    virtual bool setProperty3_hook(const char * aKey, OSObject * anObject);\
    virtual bool setProperty4_hook(const char * aKey, const char * aString);\
    virtual bool setProperty5_hook(const char * aKey, bool aBoolean);\
    virtual bool setProperty6_hook(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits);\
    virtual bool setProperty7_hook(const char * aKey, void * bytes, unsigned int length);\
    virtual void removeProperty1_hook( const OSSymbol * aKey);\
    virtual void removeProperty2_hook( const OSString * aKey);\
    virtual void removeProperty3_hook( const char * aKey);\
    virtual IOReturn newUserClient1_hook( task_t owningTask, void * securityID, UInt32 type,  OSDictionary * properties, IOUserClient ** handler );\
\
    static IOReturn HookObject( __inout OSObject* object, __in DldHookType type );\
    static IOReturn UnHookObject( __inout OSObject* object, __in DldHookType type, __in DldInheritanceDepth Depth );\
    static bool ObjectFirstPublishCallback( __in IOService * newService );\
    static bool ObjectTerminatedCallback( __in IOService * terminatedService );\
    static className* GetStaticClassInstance();\
\
private:\
\
    static className   *ClassInstance;\
    static IOReturn HookObjectInt( __inout hookedClassName* object, __in DldHookType type );\
    static IOReturn UnHookObjectInt( __inout hookedClassName* object, __in DldHookType type, __in DldInheritanceDepth Depth );\
    bool InitMembers();\
\
    unsigned int            HookedVtableSize; \
    static const DldInheritanceDepth     InheritanceDepth = Depth;\
    DldHookerCommonClass    HookerCommon; \
public:\
    DldDeclareGetClassNameFunction()

//
// the following macros is used for a leaf class hooker declaration
//
#define DldDeclareCommonIOServiceHookFunctionAndStructors( className, superClassName, hookedClassName ) \
    DldDeclareCommonIOServiceHookFunctionAndStructorsWithDepth( className, superClassName, hookedClassName, DldInheritanceDepth_0 )

//--------------------------------------------------------------------

//
// a helper calss is allowed to have only ONE new virtual function as it used to infer the vtable size
// of the based class, it is PureVirtual() now
//
#define DldDeclarePureVirtualHelperClassStart( className, hookedClassName )\
    private:\
    class className##PureVirtual: public hookedClassName{\
        friend class className;\
    public:\
        virtual bool start( IOService * provider );\
        virtual bool open( IOService * forClient, IOOptionBits options, void * arg );\
        virtual void free();\
        virtual bool terminate( IOOptionBits options = 0 ); \
        virtual bool finalize( IOOptionBits options ); \
        virtual bool attach( IOService * provider ); \
        virtual bool attachToChild_hook( IORegistryEntry * child,const IORegistryPlane * plane );\
        virtual void detach( IOService * provider ); \
        virtual bool requestTerminate( IOService * provider, IOOptionBits options );\
        virtual bool willTerminate( IOService * provider, IOOptionBits options );\
        virtual bool didTerminate( IOService * provider, IOOptionBits options, bool * defer );\
        virtual bool terminateClient( IOService * client, IOOptionBits options );\
        virtual void setPropertyTable( OSDictionary * dict );\
        virtual bool setProperty(const OSSymbol * aKey, OSObject * anObject);\
        virtual bool setProperty(const OSString * aKey, OSObject * anObject);\
        virtual bool setProperty(const char * aKey, OSObject * anObject);\
        virtual bool setProperty(const char * aKey, const char * aString);\
        virtual bool setProperty(const char * aKey, bool aBoolean);\
        virtual bool setProperty(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits);\
        virtual bool setProperty(const char * aKey, void * bytes, unsigned int length);\
        virtual void removeProperty( const OSSymbol * aKey);\
        virtual void removeProperty( const OSString * aKey);\
        virtual void removeProperty( const char * aKey);\
        virtual IOReturn newUserClient( task_t owningTask, void * securityID, UInt32 type,  OSDictionary * properties, IOUserClient ** handler );



#define DldDeclarePureVirtualHelperClassEnd( className, hookedClassName )\
        virtual void PureVirtual() = 0;\
    };

//--------------------------------------------------------------------

//
// the enum is used as an index for the HookedVtableFunctionsInfo array
//

#define DldVirtualFunctionsEnumDeclarationStart( className )\
    private:\
    enum className##HookingFunctionsEnum{

//
// returns a name for the enum index corresponding to the hooking function
//
#define DldVirtualFunctionEnumValue( className, functionName ) ( className##functionName##_EnumIndex )

#define DldAddVirtualFunctionInEnumDeclarationWithIndex( className, functionName, index )\
        className##functionName##_EnumIndex = index,

//
// normally the DldAddVirtualFunctionInEnum macros should be used
//
#define DldAddVirtualFunctionInEnumDeclaration( className, functionName )\
        className##functionName##_EnumIndex,

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

#define DldAddCommonVirtualFunctionsEnumDeclaration( className )\
        DldAddVirtualFunctionInEnumDeclarationWithIndex( className, start, 0x0 )\
        DldAddVirtualFunctionInEnumDeclaration( className, open )\
        DldAddVirtualFunctionInEnumDeclaration( className, free )\
        DldAddVirtualFunctionInEnumDeclaration( className, terminate )\
        DldAddVirtualFunctionInEnumDeclaration( className, finalize )\
        DldAddVirtualFunctionInEnumDeclaration( className, attach )\
        DldAddVirtualFunctionInEnumDeclaration( className, attachToChild )\
        DldAddVirtualFunctionInEnumDeclaration( className, detach )\
        DldAddVirtualFunctionInEnumDeclaration( className, requestTerminate )\
        DldAddVirtualFunctionInEnumDeclaration( className, willTerminate )\
        DldAddVirtualFunctionInEnumDeclaration( className, didTerminate )\
        DldAddVirtualFunctionInEnumDeclaration( className, terminateClient )\
        DldAddVirtualFunctionInEnumDeclaration( className, setPropertyTable )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty1 )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty2 )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty3 )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty4 )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty5 )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty6 )\
        DldAddVirtualFunctionInEnumDeclaration( className, setProperty7 )\
        DldAddVirtualFunctionInEnumDeclaration( className, removeProperty1 )\
        DldAddVirtualFunctionInEnumDeclaration( className, removeProperty2 )\
        DldAddVirtualFunctionInEnumDeclaration( className, removeProperty3 )\
        DldAddVirtualFunctionInEnumDeclaration( className, newUserClient1 )


//
// the className##MaximumHookingFunctionIndex must not exceed DLD_MAX_HOOKING_FUNCTIONS
//
#define DldVirtualFunctionsEnumDeclarationEnd( className )\
        DldAddVirtualFunctionInEnumDeclaration( className, MaximumHookingFunction )\
    };\
    protected:\
        DldHookedFunctionInfo   HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, MaximumHookingFunction )+1 ]; \

//--------------------------------------------------------------------

//
// a definiton for a start hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
// in case of the start the original function must be called first to get
// a full-fleged object at return with all parameteres set to valid values
//
#define DldDefineCommonIOServiceHook_start( className, superclassName, hookedClassName ) \
bool className::start_hook( IOService * provider )\
{\
    bool started;\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*startFunc)( hookedClassName*  __this, IOService * provider );\
\
    int indx = DldVirtualFunctionEnumValue( className, start );\
\
    startFunc  Original = (startFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    started = Original( reinterpret_cast<hookedClassName*>(this), provider );\
\
    if( started )\
        HookObject->HookerCommon.start( (IOService*)this, provider );\
\
    return started;\
}

//--------------------------------------------------------------------

//
// a definiton for a open hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
// in case of the open the original function must be called first
//
#define DldDefineCommonIOServiceHook_open( className, superclassName, hookedClassName ) \
bool className::open_hook(  IOService * forClient, IOOptionBits options, void * arg )\
{\
    bool opened;\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*openFunc)( hookedClassName*  __this, IOService * forClient, IOOptionBits options, void * arg );\
\
    int indx = DldVirtualFunctionEnumValue( className, open );\
\
    openFunc  Original = (openFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    opened = Original( reinterpret_cast<hookedClassName*>(this), forClient, options, arg );\
\
    if( opened )\
        HookObject->HookerCommon.open( (IOService*)this, forClient, options, arg );\
\
    return opened;\
}

//--------------------------------------------------------------------

//
// a definiton for a free hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_free( className, superclassName, hookedClassName ) \
void className::free_hook()\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return;\
\
    typedef void(*freeFunc)( hookedClassName*  __this );\
\
    int indx = DldVirtualFunctionEnumValue( className, free );\
\
    freeFunc  Original = (freeFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    HookObject->HookerCommon.free( (IOService*)this );\
\
    return Original( reinterpret_cast<hookedClassName*>(this));\
}

//--------------------------------------------------------------------

//
// a definiton for a requestTerminate hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_requestTerminate( className, superclassName, hookedClassName ) \
bool className::requestTerminate_hook( IOService * provider, IOOptionBits options )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*requestTerminateFunc)( hookedClassName*  __this, IOService * provider, IOOptionBits options );\
\
    int indx = DldVirtualFunctionEnumValue( className, requestTerminate );\
\
    requestTerminateFunc  Original = (requestTerminateFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.requestTerminate( (IOService*)this, provider, options ) )\
        return false;\
\
    return Original( reinterpret_cast<hookedClassName*>(this), provider, options );\
}

//--------------------------------------------------------------------

//
// a definiton for a willTerminate hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_willTerminate( className, superclassName, hookedClassName ) \
bool className::willTerminate_hook( IOService * provider, IOOptionBits options )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*willTerminateFunc)( hookedClassName*  __this, IOService * provider, IOOptionBits options );\
\
    int indx = DldVirtualFunctionEnumValue( className, willTerminate );\
\
    willTerminateFunc  Original = (willTerminateFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.willTerminate( (IOService*)this, provider, options ) )\
        return false;\
\
    return Original( reinterpret_cast<hookedClassName*>(this), provider, options );\
}

//--------------------------------------------------------------------

//
// a definiton for a didTerminate hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_didTerminate( className, superclassName, hookedClassName ) \
bool className::didTerminate_hook( IOService * provider, IOOptionBits options, bool * defer )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*didTerminateFunc)( hookedClassName*  __this, IOService * provider, IOOptionBits options, bool * defer );\
\
    int indx = DldVirtualFunctionEnumValue( className, didTerminate );\
\
    didTerminateFunc  Original = (didTerminateFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.didTerminate( (IOService*)this, provider, options, defer ) )\
       return false;\
\
    return Original( reinterpret_cast<hookedClassName*>(this), provider, options, defer );\
}

//--------------------------------------------------------------------

//
// a definiton for a terminate hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_terminate( className, superclassName, hookedClassName ) \
bool className::terminate_hook( IOOptionBits options )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*terminateFunc)( hookedClassName*  __this, IOOptionBits options );\
\
    int indx = DldVirtualFunctionEnumValue( className, terminate );\
\
    terminateFunc  Original = (terminateFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.terminate( (IOService*)this, options ) )\
        return false;\
\
    return Original( reinterpret_cast<hookedClassName*>(this), options );\
}

//--------------------------------------------------------------------

//
// a definiton for a terminateClient hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_terminateClient( className, superclassName, hookedClassName ) \
bool className::terminateClient_hook( IOService * client, IOOptionBits options )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*terminateClientFunc)( hookedClassName*  __this,  IOService * client, IOOptionBits options );\
\
    int indx = DldVirtualFunctionEnumValue( className, terminateClient );\
\
    terminateClientFunc  Original = (terminateClientFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.terminateClient( (IOService*)this, client, options ) )\
        return false;\
\
    return Original( reinterpret_cast<hookedClassName*>(this), client, options );\
}

//--------------------------------------------------------------------

//
// a definiton for a finalize hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_finalize( className, superclassName, hookedClassName ) \
bool className::finalize_hook( IOOptionBits options ) \
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*finalizeFunc)( hookedClassName*  __this, IOOptionBits options );\
\
    int indx = DldVirtualFunctionEnumValue( className, finalize );\
\
    finalizeFunc  Original = (finalizeFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.finalize( (IOService*)this, options ) )\
        return false;\
\
    return Original( reinterpret_cast<hookedClassName*>(this), options );\
}

//--------------------------------------------------------------------

//
// a definiton for an attach hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_attach( className, superclassName, hookedClassName ) \
bool className::attach_hook( IOService * provider )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool(*attachFunc)( hookedClassName*  __this, IOService * provider );\
\
    int indx = DldVirtualFunctionEnumValue( className, attach );\
\
    attachFunc  Original = (attachFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), provider );\
\
    if( bRetVal )\
        HookObject->HookerCommon.attach( (IOService*)this, provider );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for an attachToChild hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before the HookerCommon's calback as the
// latter might unhook and remove the hooking information
//

#define DldDefineCommonIOServiceHook_attachToChild( className, superclassName, hookedClassName ) \
bool className::attachToChild_hook( IORegistryEntry * child,const IORegistryPlane * plane )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
    return false;\
\
    typedef bool(*attachToChildFunc)( hookedClassName*  __this, IORegistryEntry * child, const IORegistryPlane * plane );\
\
    int indx = DldVirtualFunctionEnumValue( className, attachToChild );\
\
    attachToChildFunc  Original = (attachToChildFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    if( !HookObject->HookerCommon.attachToChild( (IOService*)this, child, plane ) )\
        return false;\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), child, plane );\
\
    if( bRetVal )\
        HookObject->HookerCommon.attachToChild( (IOService*)this, child, plane );\
\
    return bRetVal; \
}

//--------------------------------------------------------------------

//
// a definiton for a detach hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_detach( className, superclassName, hookedClassName ) \
void className::detach_hook( IOService * provider )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return;\
\
    typedef void(*detachFunc)( hookedClassName*  __this, IOService * provider );\
\
    int indx = DldVirtualFunctionEnumValue( className, detach );\
\
    detachFunc  Original = (detachFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    Original( reinterpret_cast<hookedClassName*>(this), provider );\
\
    HookObject->HookerCommon.detach( (IOService*)this, provider );\
}

//--------------------------------------------------------------------

//
// a definiton for a setPropertyTable hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setPropertyTable( className, superclassName, hookedClassName ) \
void className::setPropertyTable_hook( OSDictionary * dict )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return;\
\
    typedef void (*setPropertyTableFunc)( hookedClassName*  __this, OSDictionary * dict );\
\
    int indx = DldVirtualFunctionEnumValue( className, setPropertyTable );\
\
    setPropertyTableFunc  Original = (setPropertyTableFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    Original( reinterpret_cast<hookedClassName*>(this), dict );\
\
    HookObject->HookerCommon.setPropertyTable( (IOService*)this, dict );\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty1 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty1( className, superclassName, hookedClassName ) \
bool className::setProperty1_hook(const OSSymbol * aKey, OSObject * anObject)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool (*setProperty1Func)( hookedClassName*  __this, const OSSymbol * aKey, OSObject * anObject );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty1 );\
\
    setProperty1Func  Original = (setProperty1Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, anObject );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty1( (IOService*)this, aKey, anObject );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty2 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty2( className, superclassName, hookedClassName ) \
bool className::setProperty2_hook(const OSString * aKey, OSObject * anObject)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool (*setProperty2Func)( hookedClassName*  __this, const OSString * aKey, OSObject * anObject );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty2 );\
\
    setProperty2Func  Original = (setProperty2Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, anObject );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty2( (IOService*)this, aKey, anObject );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty3 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty3( className, superclassName, hookedClassName ) \
bool className::setProperty3_hook(const char * aKey, OSObject * anObject)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool (*setProperty3Func)( hookedClassName*  __this, const char * aKey, OSObject * anObject );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty3 );\
\
    setProperty3Func  Original = (setProperty3Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, anObject );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty3( (IOService*)this, aKey, anObject );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty4 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty4( className, superclassName, hookedClassName ) \
bool className::setProperty4_hook(const char * aKey, const char * aString)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool (*setProperty4Func)( hookedClassName*  __this, const char * aKey, const char * aString );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty4 );\
\
    setProperty4Func  Original = (setProperty4Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, aString );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty4( (IOService*)this, aKey, aString );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty5 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty5( className, superclassName, hookedClassName ) \
bool className::setProperty5_hook(const char * aKey, bool aBoolean)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool (*setProperty5Func)( hookedClassName*  __this, const char * aKey, bool aBoolean );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty5 );\
\
    setProperty5Func  Original = (setProperty5Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, aBoolean );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty5( (IOService*)this, aKey, aBoolean );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty6 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty6( className, superclassName, hookedClassName ) \
bool className::setProperty6_hook(const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
    return false;\
\
    typedef bool (*setProperty6Func)( hookedClassName*  __this, const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty6 );\
\
    setProperty6Func  Original = (setProperty6Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, aValue, aNumberOfBits );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty6( (IOService*)this, aKey, aValue, aNumberOfBits );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a setProperty7 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_setProperty7( className, superclassName, hookedClassName ) \
bool className::setProperty7_hook(const char * aKey, void * bytes, unsigned int length)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    typedef bool (*setProperty7Func)( hookedClassName*  __this, const char * aKey, void * bytes, unsigned int length );\
\
    int indx = DldVirtualFunctionEnumValue( className, setProperty7 );\
\
    setProperty7Func  Original = (setProperty7Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    bool bRetVal;\
    bRetVal = Original( reinterpret_cast<hookedClassName*>(this), aKey, bytes, length );\
\
    if( bRetVal )\
        HookObject->HookerCommon.setProperty7( (IOService*)this, aKey, bytes, length );\
\
    return bRetVal;\
}

//--------------------------------------------------------------------

//
// a definiton for a removeProperty1 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_removeProperty1( className, superclassName, hookedClassName ) \
void className::removeProperty1_hook(const OSSymbol * aKey)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return;\
\
    typedef void (*removeProperty1Func)( hookedClassName*  __this, const OSSymbol * aKey );\
\
    int indx = DldVirtualFunctionEnumValue( className, removeProperty1 );\
\
    removeProperty1Func  Original = (removeProperty1Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    HookObject->HookerCommon.removeProperty1( (IOService*)this, aKey );\
\
    Original( reinterpret_cast<hookedClassName*>(this), aKey );\
}

//--------------------------------------------------------------------

//
// a definiton for a removeProperty2 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_removeProperty2( className, superclassName, hookedClassName ) \
void className::removeProperty2_hook(const OSString * aKey)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return;\
\
    typedef void (*removeProperty2Func)( hookedClassName*  __this, const OSString * aKey );\
\
    int indx = DldVirtualFunctionEnumValue( className, removeProperty2 );\
\
    removeProperty2Func  Original = (removeProperty2Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    HookObject->HookerCommon.removeProperty2( (IOService*)this, aKey );\
\
    Original( reinterpret_cast<hookedClassName*>(this), aKey );\
}

//--------------------------------------------------------------------

//
// a definiton for a removeProperty3 hook, "this" points to a hooked hookedClassName
// instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_removeProperty3( className, superclassName, hookedClassName ) \
void className::removeProperty3_hook(const char * aKey)\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return;\
\
    typedef void (*removeProperty3Func)( hookedClassName*  __this, const char * aKey );\
\
    int indx = DldVirtualFunctionEnumValue( className, removeProperty3 );\
\
    removeProperty3Func  Original = (removeProperty3Func)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    HookObject->HookerCommon.removeProperty3( (IOService*)this, aKey );\
\
    Original( reinterpret_cast<hookedClassName*>(this), aKey );\
}

//--------------------------------------------------------------------

//
// a definiton for a newUserClient hook, newUserClient is an verloaded function but only one
// is used by the system, "this" points to a hooked hookedClassName instance
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_newUserClient1( className, superclassName, hookedClassName ) \
IOReturn className::newUserClient1_hook( task_t owningTask, void * securityID,\
                                        UInt32 type,  OSDictionary * properties,\
                                        IOUserClient ** handler )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return kIOReturnUnsupported;\
\
    typedef IOReturn (*newUserClientFunc)( hookedClassName*  __this, task_t owningTask, void * securityID,\
                                    UInt32 type,  OSDictionary * properties,\
                                    IOUserClient ** handler );\
\
    int indx = DldVirtualFunctionEnumValue( className, newUserClient1 );\
\
    newUserClientFunc  Original = (newUserClientFunc)HookObject->HookerCommon.GetOriginalFunction( (OSObject*)this, indx );\
\
    HookObject->HookerCommon.newUserClient1( (IOService*)this, owningTask, securityID, type, properties, handler );\
\
    return Original( reinterpret_cast<hookedClassName*>(this),  owningTask, securityID, type, properties, handler );\
}

//--------------------------------------------------------------------

//
// a definiton for a first publish callback
//
// the GetOriginalFunction must be caled before HookerCommon calback as the
// latter might unhook and remove the hooking information
//
#define DldDefineCommonIOServiceHook_FirstPublishCallback( className, superclassName, hookedClassName ) \
bool className::ObjectFirstPublishCallback( __in IOService * newService )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    return HookObject->HookerCommon.ObjectFirstPublishCallback( newService );\
}

//--------------------------------------------------------------------

//
// a definiton for a termination publish callback
//
#define DldDefineCommonIOServiceHook_TerminationCallback( className, superclassName, hookedClassName ) \
bool className::ObjectTerminatedCallback( __in IOService * terminatedService )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return false;\
\
    return HookObject->HookerCommon.ObjectTerminatedCallback( terminatedService );\
}

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

//--------------------------------------------------------------------

//
// a default implementation, a specific implementation can be used instead
// for that purpose the macro is not inserted in DldDefineCommonIOServiceHookFunctionsAndStructors
//
#define DldDefineCommonIOServiceHook_HookObjectInt( className, superclassName, hookedClassName ) \
IOReturn className::HookObjectInt( __inout hookedClassName* object, __in DldHookType type )\
{\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return kIOReturnNoMemory;\
\
    return HookObject->HookerCommon.HookObject( object, type );\
}

//--------------------------------------------------------------------

//
// a definition for an unhook callback called by the hook engine when the object
// of the desired class is being terminated,
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
// actually, this function is never called as the direct call of HookerCommon.UnHookObject() from
// the free() hook is used instead
//
#define DldDefineCommonIOServiceHook_UnHookObject( className, superclassName, hookedClassName ) \
IOReturn className::UnHookObject( __inout OSObject* object, __in DldHookType type, __in DldInheritanceDepth Depth )\
{\
    hookedClassName*  HookedClassObj;\
\
    HookedClassObj = (hookedClassName*)object;\
    assert( HookedClassObj );\
    if( NULL == HookedClassObj )\
        return kIOReturnBadArgument;\
\
    return className::UnHookObjectInt( HookedClassObj, type, Depth );\
}

//--------------------------------------------------------------------

//
// a default implementation, a specific implementation can be used instead
// for that purpose the macro is not inserted in DldDefineCommonIOServiceHookFunctionsAndStructors,
//
// actually, this function is never called as the direct call of HookerCommon.UnHookObject() from
// the free() hook is used instead
//
#define DldDefineCommonIOServiceHook_UnHookObjectInt( className, superclassName, hookedClassName ) \
IOReturn className::UnHookObjectInt( __inout hookedClassName* object, __in DldHookType type, __in DldInheritanceDepth Depth )\
{\
    assert( !"not supposed to be called" );\
    className* HookObject = className::GetStaticClassInstance();\
    assert( HookObject );\
    if( NULL == HookObject )\
        return kIOReturnNoMemory;\
    assert( type  == HookObject->HookerCommon.GetHookType() );\
    assert( Depth == HookObject->HookerCommon.GetInheritanceDepth() );\
\
    return HookObject->HookerCommon.UnHookObject( object );\
}

//--------------------------------------------------------------------

//
// a definition for a static function which returns a pointer for a static instance of a class,
// the returned object is not referenced,
// Actually, I don't need a full fledged IOKit object and can skip superclassName::init(), but as
// most of the classes are derived from IOService there is no harm in calling IOService::init(),
// nevertheless I forged a surrogat for an init() by calling InitMembers(), if the hooking class
// is derived from a complicated class it may be undesirable to call a super::init() and this call
// must be removed
//
#define DldDefineCommonIOServiceHook_GetStaticClassInstance( className, superclassName, hookedClassName ) \
className* className::GetStaticClassInstance()\
{\
    assert( gHookEngine );\
    assert( gDldRootEntry );\
\
    if( NULL != className::ClassInstance )\
        return className::ClassInstance;\
\
    className::ClassInstance = new className();\
    assert( NULL != className::ClassInstance );\
    if( NULL == className::ClassInstance ){\
        DBG_PRINT_ERROR( ( "("#className"::ClassInstance = new className() the allocation failed\n" ) );\
        return NULL;\
    }\
\
    if( !(className::ClassInstance)->init() ){\
        assert( !"("#className"::ClassInstance)->init() failed\n" );\
        DBG_PRINT_ERROR( ( "("#className"::ClassInstance)->init() failed\n" ) );\
        className::ClassInstance->release();\
        className::ClassInstance = NULL;\
        return NULL;\
    }\
\
    if( !(className::ClassInstance)->InitMembers() ){\
        assert( !"("#className"::ClassInstance)->InitMembers() failed\n" );\
        DBG_PRINT_ERROR( ( "("#className"::ClassInstance)->InitMembers() failed\n" ) );\
        className::ClassInstance->release();\
        className::ClassInstance = NULL;\
        return NULL;\
    }\
\
    return className::ClassInstance;\
}

//--------------------------------------------------------------------

//
// a definition for a staic members of the class and standard functions,
// the following functions are not defined by this macro, if the default
// implementation should be used the respective macro must be applied
//  - HookObjectInt() with a defaut definition by DldDefineCommonIOServiceHook_HookObjectInt
//  - UnHookObjectInt() with a defaut definition by DldDefineCommonIOServiceHook_UnHookObjectInt
//  - InitMembers() with a default definiton by DldDefineCommonIOServiceHook_InitMembers
//
// do not forget to add a function's vtable index to the array by calling thr
// DldInitMembers_AddHookedFunctionVtableIndex macro for a vtable hooking functions
//
#define DldDefineCommonIOServiceHookFunctionsAndStructors( className, superclassName, hookedClassName ) \
    className* className::ClassInstance = NULL;\
    DldDefineCommonIOServiceHook_start( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_open( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_free( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_requestTerminate( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_willTerminate( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_didTerminate( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_terminateClient( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_terminate( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_finalize( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_attach( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_attachToChild( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_detach( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setPropertyTable( className, superclassName, hookedClassName ) \
    DldDefineCommonIOServiceHook_setProperty1( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setProperty2( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setProperty3( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setProperty4( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setProperty5( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setProperty6( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_setProperty7( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_removeProperty1( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_removeProperty2( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_removeProperty3( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_newUserClient1( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_FirstPublishCallback( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_TerminationCallback( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_HookObject( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_UnHookObject( className, superclassName, hookedClassName )\
    DldDefineCommonIOServiceHook_GetStaticClassInstance( className, superclassName, hookedClassName )

//--------------------------------------------------------------------

//
// the hooker class must have only one virtual funcion, so its vtable is one element bigger than the super class's vtable
//

#define DldInitMembers_Enter( className, superClassName, hookedClassName )\
    unsigned int indx = 0x0;\
    this->HookerCommon.SetClassHookerObject( (OSObject*)this ); \
    this->HookedVtableSize = sizeof(_ptf_t)*( DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &className::className##PureVirtual::PureVirtual ) - 0x1 );\
    this->HookerCommon.SetHookedVtableSize( this->HookedVtableSize );\
    this->HookerCommon.SetInheritanceDepth( this->InheritanceDepth );

#define DldInitMembers_Exit( className, superClassName, hookedClassName )\
    if( indx > DldVirtualFunctionEnumValue( className, MaximumHookingFunction ) ) panic( #className"::InitMembers() indx >= DLD_MAX_HOOKING_FUNCTIONS");\
    assert( DldVirtualFunctionEnumValue( className, MaximumHookingFunction ) == indx );\
    this->HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, MaximumHookingFunction ) ].VtableIndex = (unsigned int)(-1);\
    this->HookerCommon.SetHookedVtableFunctionsInfo( this->HookedVtableFunctionsInfo, DldVirtualFunctionEnumValue( className, MaximumHookingFunction )+0x1 );\
    ++indx;

//
// DldInitMembers_AddFunctionInfoForHookedClass uses a virtual class derived from the hooked class to
// retrive a hooked class's virtual function index ( the trick of creating a dummy derived class for which 
// hooking class is a friend helps to overcome the c++ confinement on protected members access from
// non-derivied classes and in the same time to avoid hookong class dependence on a module implementing
// a hooked class - overwise the kxld will try to load the hooked class module even if there is no any
// devices in the system which are handled by this module, and the worse - if the module file is not present
// in the file system the module implementing the hooking class becames unloadable!  )
//

#define DldInitMembers_AddFunctionInfoForHookedClass( className, functionName, functionName_hook, superClassName, hookedClass )\
    if( indx >= DldVirtualFunctionEnumValue( className, MaximumHookingFunction ) ) panic( #className"::InitMembers() indx >= DLD_MAX_HOOKING_FUNCTIONS for "#hookedClass"::"#functionName );\
    this->HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, functionName ) ].VtableIndex = DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) &className##PureVirtual::functionName );\
    this->HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, functionName ) ].HookingFunction = OSMemberFunctionCast( DldVtableFunctionPtr, this, (void (OSMetaClassBase::*)(void)) &className::functionName_hook );\
    indx++;

//
// the same as DldInitMembers_AddFunctionInfoForHookedClass but is used for class's overloaded functions
// the name conflict is resolved by applying the function's number and its return type and the list of the parameters 
// 
#define DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, functionName, functionName_hook, superClassName, hookedClass, functionNumber, functionRetType, functionListOfParameters )\
    if( indx >= DldVirtualFunctionEnumValue( className, MaximumHookingFunction ) ) panic( #className"::InitMembers() indx >= DLD_MAX_HOOKING_FUNCTIONS for "#hookedClass"::"#functionName );\
    this->HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, functionName##functionNumber ) ].VtableIndex = DldConvertFunctionToVtableIndex( (void (OSMetaClassBase::*)(void)) ( functionRetType ( className##PureVirtual::* ) functionListOfParameters )&className##PureVirtual::functionName );\
    this->HookedVtableFunctionsInfo[ DldVirtualFunctionEnumValue( className, functionName##functionNumber ) ].HookingFunction = OSMemberFunctionCast( DldVtableFunctionPtr, this, (void (OSMetaClassBase::*)(void)) &className::functionName_hook );\
    indx++;

//
// a filling of the SetHookedVtableIndicesToHook array by the indices of the standard(common) hooking functions
//
#define DldInitMembers_AddCommonHookedVtableFunctionsInfo( className, superClassName, hookedClassName )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, start, start_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, open, open_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, free, free_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, requestTerminate, requestTerminate_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, willTerminate, willTerminate_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, didTerminate, didTerminate_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, terminateClient, terminateClient_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, terminate, terminate_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, finalize, finalize_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, attach, attach_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, attachToChild, attachToChild_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, detach, detach_hook, superClassName, hookedClass )\
    DldInitMembers_AddFunctionInfoForHookedClass( className, setPropertyTable, setPropertyTable_hook, superClassName, hookedClass )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty1_hook, superClassName, hookedClass, 1, bool, (const OSSymbol * aKey, OSObject * anObject) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty2_hook, superClassName, hookedClass, 2, bool, (const OSString * aKey, OSObject * anObject) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty3_hook, superClassName, hookedClass, 3, bool, (const char * aKey, OSObject * anObject) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty4_hook, superClassName, hookedClass, 4, bool, (const char * aKey, const char * aString) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty5_hook, superClassName, hookedClass, 5, bool, (const char * aKey, bool aBoolean) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty6_hook, superClassName, hookedClass, 6, bool, (const char * aKey, unsigned long long aValue, unsigned int aNumberOfBits) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, setProperty, setProperty7_hook, superClassName, hookedClass, 7, bool, (const char * aKey, void * bytes, unsigned int length) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, newUserClient, newUserClient1_hook, superClassName, hookedClass, 1, IOReturn,( task_t owningTask, void * securityID,UInt32 type,  OSDictionary * properties, IOUserClient ** handler ) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, removeProperty, removeProperty1_hook, superClassName, hookedClass, 1, void, ( const OSSymbol * aKey) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, removeProperty, removeProperty2_hook, superClassName, hookedClass, 2, void, ( const OSString * aKey) )\
    DldInitMembers_AddOverloadedFunctionInfoForHookedClass( className, removeProperty, removeProperty3_hook, superClassName, hookedClass, 3, void, ( const char * aKey ) )

//
// a default definition for className::InitMembers(), an optional for use,
// can be used if a hooker class doesn't define any hook except the default ones
// and doesn't need to initialize any class members
//
#define DldDefineCommonIOServiceHook_InitMembers( className, superclassName, hookedClassName ) \
bool className::InitMembers()\
{\
    DldInitMembers_Enter( className, superclassName, hookedClassName )\
    DldInitMembers_AddCommonHookedVtableFunctionsInfo( className, superclassName, hookedClassName )\
    DldInitMembers_Exit( className, superclassName, hookedClassName )\
    return true;\
}

//--------------------------------------------------------------------

//
// do not forget to maintain syncronization for the follwoing macroses
// if you change DldAddObjectHookClassToHookEngine you should change 
// DldDefineInitHookClassEntryData acordingly, the same holds for
// DldAddVtableHookClassToHookEngine and DldDefineInitVtableHookClassEntryData
//

#define DldAddVtableHookClassToHookEngine( hookingClass, classToHook )\
{\
    DldHookDictionaryEntryData    HookEntryData = { 0x0 };\
    HookEntryData.HookType = DldHookTypeVtable;\
    HookEntryData.IOKitClassName = #classToHook;\
    HookEntryData.HookFunction = hookingClass::HookObject;\
    HookEntryData.UnHookFunction = hookingClass::UnHookObject;\
    HookEntryData.ObjectFirstPublishCallback = NULL;\
    HookEntryData.ObjectTerminatedCallback = NULL;\
    HookEntryData.Depth = hookingClass::InheritanceDepth;\
    this->AddNewIOKitClass( &HookEntryData );\
}

#define DldAddObjectHookClassToHookEngine( hookingClass, classToHook )\
{\
    DldHookDictionaryEntryData    HookEntryData = { 0x0 };\
    HookEntryData.HookType = DldHookTypeObject;\
    HookEntryData.IOKitClassName = #classToHook;\
    HookEntryData.HookFunction = hookingClass::HookObject;\
    HookEntryData.UnHookFunction = hookingClass::UnHookObject;\
    HookEntryData.ObjectFirstPublishCallback = hookingClass::ObjectFirstPublishCallback;\
    HookEntryData.ObjectTerminatedCallback = hookingClass::ObjectTerminatedCallback;\
    HookEntryData.Depth = DldInheritanceDepth_0;\
    assert( DldInheritanceDepth_0 == hookingClass::InheritanceDepth );\
    this->AddNewIOKitClass( &HookEntryData );\
}

//
// see comments for DldDefineInitHookClassEntryData macro
//
#define DldAddObjectHookClassToHookEngine2( hookingClass, classToHook )\
{\
    extern void DldInitHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr );\
    DldHookDictionaryEntryData    HookEntryData = { 0x0 };\
    DldInitHookEntryData_##hookingClass( &HookEntryData );\
    this->AddNewIOKitClass( &HookEntryData );\
}

//
// see comments for DldDefineInitVtableHookClassEntryData macro
//
#define DldAddVtableHookClassToHookEngine2( hookingClass, classToHook )\
{\
    extern void DldInitVtableHookEntryData##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr );\
    DldHookDictionaryEntryData    HookEntryData = { 0x0 };\
    DldInitVtableHookEntryData##hookingClass( &HookEntryData );\
    this->AddNewIOKitClass( &HookEntryData );\
}

//
// the following macros are used to facilitate the DldIOKitHookEngine class in
// the hooking class registration when the DldAddVtableHookClassToHookEngine and 
// DldAddObjectHookClassToHookEngine macroses can't be used directly because of
// the conflicts when header files for different hooking classes are included
// in the DldIOKitHookEngine.cpp file ( Apple redefines some macroses, enumes and
// structures so the SDK headers are incompatible ), if you are using these macroses
// include "DldIOKitHookDictionaryEntry.h"
//
#define DldDeclareInitVtableHookClassEntryData( hookingClass, classToHook )\
static void DldInitVtableHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr );

#define DldDefineInitVtableHookClassEntryData( hookingClass, classToHook )\
void hookingClass::DldInitVtableHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr )\
{\
    _HookEntryDataPtr->HookType = DldHookTypeVtable;\
    _HookEntryDataPtr->IOKitClassName = #classToHook;\
    _HookEntryDataPtr->HookFunction = hookingClass::HookObject;\
    _HookEntryDataPtr->UnHookFunction = hookingClass::UnHookObject;\
    _HookEntryDataPtr->ObjectFirstPublishCallback = NULL;\
    _HookEntryDataPtr->ObjectTerminatedCallback = NULL;\
    _HookEntryDataPtr->Depth = hookingClass::InheritanceDepth;\
}\
\
void DldInitVtableHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr )\
{\
    hookingClass::DldInitVtableHookEntryData_##hookingClass( _HookEntryDataPtr );\
}

#define DldDeclareInitHookClassEntryData( hookingClass, classToHook )\
static void DldInitHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr );

#define DldDefineInitHookClassEntryData( hookingClass, classToHook )\
void hookingClass::DldInitHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr )\
{\
    _HookEntryDataPtr->HookType = DldHookTypeObject;\
    _HookEntryDataPtr->IOKitClassName = #classToHook;\
    _HookEntryDataPtr->HookFunction = hookingClass::HookObject;\
    _HookEntryDataPtr->UnHookFunction = hookingClass::UnHookObject;\
    _HookEntryDataPtr->ObjectFirstPublishCallback = hookingClass::ObjectFirstPublishCallback;\
    _HookEntryDataPtr->ObjectTerminatedCallback = hookingClass::ObjectTerminatedCallback;\
    _HookEntryDataPtr->Depth = DldInheritanceDepth_0;\
    assert( DldInheritanceDepth_0 == hookingClass::InheritanceDepth );\
}\
\
void DldInitHookEntryData_##hookingClass( DldHookDictionaryEntryData* _HookEntryDataPtr )\
{\
    hookingClass::DldInitHookEntryData_##hookingClass( _HookEntryDataPtr );\
}

//--------------------------------------------------------------------

#endif//_DLDHOOKERCOMMONCLASS_H
