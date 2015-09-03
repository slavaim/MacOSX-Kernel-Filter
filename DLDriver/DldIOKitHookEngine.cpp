/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include "DldIOKitHookEngine.h"
#include "IOUSBMassStorageClassDldHook.h"
#include "IOUSBInterfaceDldHook.h"
#include "IOSCSIPeripheralDeviceNubDldHook.h"
#include "IOUSBDeviceDldHook.h"
#include "IOUSBCompositeDriverDldHook.h"
#include "IOBlockStorageDriverDldHook.h"
#include "IOBlockStorageServicesDldHook.h"
#include "IOMediaDldHook.h"
#include "IOMediaBSDClientDldHook.h"
#include "IOFDiskPartitionSchemeDldHook.h"
#include "IOServiceDldHook.h"
#include "IOSerialStreamSyncDldHook.h"
#include "AppleUSBEHCIDldHook.h"
#include "AppleUSBOHCIDldHook.h"
#include "IOUserClientDldHook.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIOKitHookEngine, OSObject )


//--------------------------------------------------------------------

bool
DldIOKitHookEngine::init()
{
    if( !super::init() )
        return false;
    
    if( !DldHookedObjectsHashTable::CreateStaticTableWithSize( 100, false ) )
        return false;
    
    return true;
}

//--------------------------------------------------------------------

void
DldIOKitHookEngine::free(void)
{
    if( this->DictionaryPerObjectHooks )
        this->DictionaryPerObjectHooks->release();
    
    for( unsigned int i = 0x0; i < DldInheritanceDepth_Maximum; ++i ){
        
        if( this->DictionaryVtableClassHooks[ i ] )
            this->DictionaryVtableClassHooks[ i ]->release();
    
    }// end for
    
    DldHookedObjectsHashTable::DeleteStaticTable();
    
    super::free();
    return;
}

//--------------------------------------------------------------------

OSDictionary*
DldIOKitHookEngine::HookTypeToDictionary( __in DldHookType HookType, __in DldInheritanceDepth Depth )
{
    if( DldHookTypeVtable == HookType ){
        
        assert( DldHookTypeVtable == HookType );
        assert( Depth < DldInheritanceDepth_Maximum );
        
        return this->DictionaryVtableClassHooks[ Depth ];
        
    } else {
        
        assert( DldHookTypeObject == HookType );
        assert( DldInheritanceDepth_0 == Depth );
        
        return this->DictionaryPerObjectHooks;
        
    }
}

//--------------------------------------------------------------------

bool
DldIOKitHookEngine::AddNewIOKitClass(
    __in DldHookDictionaryEntryData* HookData 
    )
{   
    bool bRet = false;
    
    assert( preemption_enabled() );
    assert( ( DldHookTypeUnknown < HookData->HookType ) && ( HookData->HookType < DldHookTypeMaximum ) );
    assert( HookData->Depth < DldInheritanceDepth_Maximum ); 
    DBG_PRINT( ( "DldIOKitHookEngine::AddNewIOKitClass( %s )\n", HookData->IOKitClassName ) );
    
    OSString*    IOKitClassNameStr = OSString::withCStringNoCopy( HookData->IOKitClassName );
    assert( NULL != IOKitClassNameStr );
    if( NULL == IOKitClassNameStr )
        return false;
    
    //
    // IOKitClassNameStr will be referenced by the withIOKitClassName constructor
    //
    
    OSDictionary*                  Dictionary = NULL;
    DldIOKitHookDictionaryEntry*   pEntry = DldIOKitHookDictionaryEntry::withIOKitClassName( IOKitClassNameStr,
                                                                                             HookData );
    assert( NULL != pEntry );
    if( NULL == pEntry ){
        
        assert( false == bRet );
        goto __exit;
    }
    
    Dictionary = this->HookTypeToDictionary( HookData->HookType, HookData->Depth );
    
    assert( Dictionary && ( ( DldHookTypeVtable == HookData->HookType && Dictionary == this->DictionaryVtableClassHooks[ HookData->Depth ] ) || 
                            ( DldHookTypeObject == HookData->HookType && Dictionary == this->DictionaryPerObjectHooks ) ) );
    
    //
    // add the hooking class in the dictionary, the pEntry will be retained
    //
    if( !Dictionary->setObject( IOKitClassNameStr, pEntry ) ){
        
        assert( !"DldIOKitHookEngine::AddNewIOKitClass : entry insertion has failed!" );
        DBG_PRINT_ERROR(("\'%s\' an entry insertion has failed!", HookData->IOKitClassName));
        
        assert( false == bRet );
        goto __exit;
    }
    
    assert( preemption_enabled() );
    
    //
    // there is no need to call the remove() routine to remove a notification
    // if it has been successfully saved in the pEntry object, the notification will be
    // removed when the pEntry is being finalized
    //
    
    if( HookData->ObjectTerminatedCallback ){
       
        //
        // Create a notification object to call a 'C' function, every time an
        // instance of object is terminated.
        //
        
        IONotifier*    ObjectTerminatedNotifier;
        
#ifdef DLD_MACOSX_10_5
        ObjectTerminatedNotifier = IOService::addNotification( gIOTerminatedNotification,                                 /* type   */
                                                               IOService::serviceMatching( HookData->IOKitClassName ),    /* match  */
                                                               DldIOKitHookEngine::ObjectTerminatedCallback,              /* action */
                                                               this                                                       /* param  */
                                                            );
#else
        ObjectTerminatedNotifier = IOService::addMatchingNotification( gIOTerminatedNotification,                                 /* type   */
                                                                       IOService::serviceMatching( HookData->IOKitClassName ),    /* match  */
                                                                       DldIOKitHookEngine::ObjectTerminatedCallback,              /* action */
                                                                       this                                                       /* param  */
                                                                      );
#endif
        //
        // ObjectTerminatedNotifier is created with a refrence set to 0x1 and released while remove() is called
        //
        assert( NULL != ObjectTerminatedNotifier );
        if( NULL == ObjectTerminatedNotifier ||
           !pEntry->setNotifier( gIOTerminatedNotification, ObjectTerminatedNotifier ) ){
            
            DBG_PRINT_ERROR( ( "\'%s\' IOService::addMatchingNotification( gIOTerminatedNotification ) failed! \n", HookData->IOKitClassName ) );
            assert( !"DldIOKitHookEngine::AddNewIOKitClass : entry insertion has failed!" );
            
            //
            // if ObjectTerminatedNotifier is not NULL then the pEntry->setNotifier() failed,
            // so a registration described by ObjectTerminatedNotifier must be removed
            //
            if( ObjectTerminatedNotifier )
                ObjectTerminatedNotifier->remove();
            
            assert( false == bRet );
            goto __exit;
        }
        
    }// end if( HookData->ObjectTerminatedCallback )
    
    
    if( HookData->ObjectFirstPublishCallback ){
        
        //
        // Create a notification object to call a 'C' function, every time an
        // instance of object is first published
        //
        IONotifier*    ObjectPublishNotifier;
#ifdef DLD_MACOSX_10_5
        ObjectPublishNotifier = IOService::addNotification( gIOFirstPublishNotification,                               /* type   */
                                                            IOService::serviceMatching( HookData->IOKitClassName ),    /* match  */
                                                            DldIOKitHookEngine::ObjectPublishCallback,                 /* action */
                                                            this                                                       /* param  */
                                                           );
#else
        ObjectPublishNotifier = IOService::addMatchingNotification( gIOFirstPublishNotification,                               /* type   */
                                                                    IOService::serviceMatching( HookData->IOKitClassName ),    /* match  */
                                                                    DldIOKitHookEngine::ObjectPublishCallback,                 /* action */
                                                                    this                                                       /* param  */
                                                                   );
#endif
        //
        // ObjectPublishNotifier is created with a refrence set to 0x1 and released while remove() is called
        //
        assert( NULL != ObjectPublishNotifier );
        if( NULL == ObjectPublishNotifier ||
           !pEntry->setNotifier( gIOFirstPublishNotification, ObjectPublishNotifier ) ){
            
            DBG_PRINT_ERROR( ( "\'%s\' IOService::addMatchingNotification( gIOFirstPublishNotification ) failed! \n", HookData->IOKitClassName ) );
            assert( !"DldIOKitHookEngine::AddNewIOKitClass : entry insertion has failed!" );
            
            
            //
            // if ObjectPublishNotifier is not NULL then the pEntry->setNotifier() failed,
            // so a registration described by ObjectPublishNotifier must be removed
            //
            if( ObjectPublishNotifier )
                ObjectPublishNotifier->remove();
            
            assert( false == bRet );
            goto __exit;
        }
        
    }// end if( HookData->ObjectFirstPublishCallback )
    
    bRet = true;
    
__exit:
    
    assert( true == bRet );
    if( !bRet ){
        
        if( Dictionary )
            Dictionary->removeObject( IOKitClassNameStr );
        
    }// end if
    
    //
    // the pEntry object is retained by the dictionary
    //
    if( NULL != pEntry )
        pEntry->release();
    
    IOKitClassNameStr->release();
    
    return bRet;
    
}

//--------------------------------------------------------------------

DldIOKitHookEngine*
DldIOKitHookEngine::withNoHooks()
//
// actually this function must not be called more than once or the behaviour is unpredictable
// when more than one hooking object exist
//
{
    assert( preemption_enabled() );
    
    DldIOKitHookEngine*    pEngine = new DldIOKitHookEngine();
    assert( NULL != pEngine );
    if( NULL == pEngine ){
        
        return NULL;
    }
    
    //
    // IOKit's base classes standard itnitialization
    //
    if( !pEngine->init() ){
        
        pEngine->release();
        return NULL;
    }
    
    
    pEngine->DictionaryPerObjectHooks = OSDictionary::withCapacity( 20 );
    assert( pEngine->DictionaryPerObjectHooks );
    if( NULL == pEngine->DictionaryPerObjectHooks ){
        
        pEngine->release();
        return NULL;
    }// end if
    
    for( unsigned int i = 0x0; i < DldInheritanceDepth_Maximum; ++i ){
        
        pEngine->DictionaryVtableClassHooks[ i ] = OSDictionary::withCapacity( 20 );
        if( NULL == pEngine->DictionaryVtableClassHooks[ i ] ){
            
            pEngine->release();
            return NULL;
        }// end if
        
        
    }// end for
    
    return pEngine;
}

//--------------------------------------------------------------------

extern bool DldHookEngine_Add_IOSCSIPeripheralDeviceType05DldHooks( __in DldIOKitHookEngine* hookEngine );

bool DldIOKitHookEngine::startHookingWithPredefinedClasses()
//
// the gHookEngine global variable must be initialized before calling
// this function as the callback called inside this function will require
// gHookEngine to be initialized to be used for recursive hooking
//
{
    assert( gHookEngine );
    assert( gDldRootEntry );
    
    //
    // fill the dictionary with the predefined IOKit classes and the functions to be called for hooking,
    // direct vtable hooks must be defined first or else they won't be applied - see DldIOKitHookEngine::HookObject
    //
    
    /*
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_0 ), IOService )// direct Vtable hook!
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_1 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_2 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_3 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_4 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_5 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_6 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_7 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_8 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_9 ), IOService )
    DldAddVtableHookClassToHookEngine( IoServiceVtableDldHookClass( DldInheritanceDepth_10 ), IOService )
     */
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_0>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_1>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_2>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_3>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_4>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_5>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_6>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_7>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_8>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_9>, IOService >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOServiceDldHook2<DldInheritanceDepth_10>, IOService >( DldHookTypeVtable );
    
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_0>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_1>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_2>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_3>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_4>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_5>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_6>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_7>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_8>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_9>, IOUserClient >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOUserClientDldHook<DldInheritanceDepth_10>, IOUserClient >( DldHookTypeVtable );
    
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_0>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_1>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_2>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_3>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_4>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_5>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_6>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_7>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_8>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_9>, IOSerialStreamSync >( DldHookTypeVtable );
    this->DldAddHookingClassInstance< IOSerialStreamSyncDldHook<DldInheritanceDepth_10>, IOSerialStreamSync >( DldHookTypeVtable );
    
    //
    // as IOSCSIPeripheralDeviceType05 hook is a vtable hook it must be declared before the first IOService plan traversing
    //
    DldHookEngine_Add_IOSCSIPeripheralDeviceType05DldHooks( this );
    
    //
    // now we have to copy the current IOService tree to prepare it as the HookObject will be called
    // immediatelly by the class registration callbacks while being registered by DldAddObjectHookClassToHookEngine
    //
    CopyIoServicePlane( gIOServicePlane, this );
    gFirstScanCompleted = true;
    
    //
    // repeat the copying as there is an interdependence between objects not connected directly
    // by the parent-child relations, e.g. a dependence between a virtual media ( FileVailt ) and
    // a physical media supporting it, such decoupled stacks might be processed by the first scan
    // in a wrong order - from a dependent to a master
    //
    CopyIoServicePlane( gIOServicePlane, this );
    
    //
    // pay attention that the order of registration does matter as for example the properties
    // for an IOUSBDevice must exist when a IOUSBInterface device is being hooked, this requirements
    // might be relaxed to some extent as the following call to CopyIoServicePlane() will traverse
    // the whole IOService plan again, but it is still a good idea to adhere to the above rule
    //
    /*
    DldAddObjectHookClassToHookEngine( IOServiceDldHook, IOService )
    DldAddObjectHookClassToHookEngine( AppleUSBEHCIDldHook, AppleUSBEHCI )
    DldAddObjectHookClassToHookEngine( AppleUSBOHCIDldHook, AppleUSBOHCI )
    DldAddObjectHookClassToHookEngine( IOUSBDeviceDldHook, IOUSBDevice )
    DldAddObjectHookClassToHookEngine( IOUSBCompositeDriverDldHook, IOUSBCompositeDriver )
    DldAddObjectHookClassToHookEngine( IOUSBInterfaceDldHook, IOUSBInterface )
    //DldAddObjectHookClassToHookEngine( IOUSBMassStorageClassDldHook, IOUSBMassStorageClass )
    DldAddObjectHookClassToHookEngine( IOSCSIPeripheralDeviceNubDldHook, IOSCSIPeripheralDeviceNub )
    DldAddObjectHookClassToHookEngine( IOBlockStorageDriverDldHook, IOBlockStorageDriver )
    DldAddObjectHookClassToHookEngine( IOBlockStorageServicesDldHook, IOBlockStorageServices )
    DldAddObjectHookClassToHookEngine2( IOSCSIPeripheralDeviceType05DldHook, IOSCSIPeripheralDeviceType05 )
    DldAddObjectHookClassToHookEngine( IOMediaDldHook, IOMedia )
    DldAddObjectHookClassToHookEngine( IOMediaBSDClientDldHook, IOMediaBSDClient )
    DldAddObjectHookClassToHookEngine( IOFDiskPartitionSchemeDldHook, IOFDiskPartitionScheme )
    DldAddObjectHookClassToHookEngine( IOModemSerialStreamSyncDldHook, IOModemSerialStreamSync )
     */
    
    this->DldAddHookingClassInstance< IOUSBMassStorageClassDldHook2<DldInheritanceDepth_0>, IOUSBMassStorageClass >( DldHookTypeObject );
    this->DldAddHookingClassInstance< AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeEHCI >, IOUSBControllerV3 >( DldHookTypeObject );
    this->DldAddHookingClassInstance< AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeOHCI >, IOUSBControllerV3 >( DldHookTypeObject );
    this->DldAddHookingClassInstance< AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeUHCI >, IOUSBControllerV3 >( DldHookTypeObject );
    this->DldAddHookingClassInstance< AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeXHCI >, IOUSBControllerV3 >( DldHookTypeObject );
    
    //
    // and check the tree again
    //
    CopyIoServicePlane( gIOServicePlane, this );
    
    return true;
}

//--------------------------------------------------------------------

//
// A static member function that is called by a notification object when an 
// object is published. This function is called with arbitration lock of
// the published object held
//

#ifdef DLD_MACOSX_10_5
bool
DldIOKitHookEngine::ObjectPublishCallback(
    void*        target,
    void*        refCon,
    IOService*   NewService
    )
#else
bool
DldIOKitHookEngine::ObjectPublishCallback(
    void*        target,
    void*        refCon,
    IOService*   NewService,
    IONotifier*  notifier
    )
#endif
{
    assert( target );
    bool                  bRet;
    DldIOKitHookEngine*   __this = (DldIOKitHookEngine*)target;
    
    bRet = __this->CallObjectFirstPublishCallback( NewService );
    if( bRet )
        __this->HookObject( (OSObject*)NewService );
    
    return bRet;
}

//--------------------------------------------------------------------

//
// A static member function that is called by a notification object when an 
// object is being terminated in its finalize routine. This function is called
// with arbitration lock of the published object held.
//

#ifdef DLD_MACOSX_10_5
bool
DldIOKitHookEngine::ObjectTerminatedCallback(
    void*        target,
    void*        refCon,
    IOService*   NewService
    )
#else
bool
DldIOKitHookEngine::ObjectTerminatedCallback(
    void*        target,
    void*        refCon,
    IOService*   NewService,
    IONotifier*  notifier
    )
#endif
{
    assert( target );
    bool                  bRet;
    DldIOKitHookEngine*   __this = (DldIOKitHookEngine*)target;
    
    bRet = __this->CallObjectTerminatedCallback( NewService );
    
    //
    // the object is unhooked in free()
    //
    
    return bRet;
}

//--------------------------------------------------------------------

IOReturn
DldIOKitHookEngine::HookObject(
    __inout OSObject* object
    )
{
    OSObject*                      pRawEntry;
    DldIOKitHookDictionaryEntry*   pEntry;
    const OSMetaClass*             pMetaClass;
    OSDictionary*                  Dictionary;
    DldInheritanceDepth            Depth;
    
    //
    // at first call the direct vtable hooker for the nearest class in the inheritance classes chain,
    // so only the nearest direct vtable hooks are applied
    //
    
    assert( object->getMetaClass() );
    
    //
    // each steps moves to the one level below in the inheritance chain, i.e. to the super class,
    // the start is the leaf class
    //
    for( pMetaClass = object->getMetaClass(), Depth = DldInheritanceDepth_0;
         pMetaClass && Depth < DldInheritanceDepth_Maximum;
         pMetaClass = pMetaClass->getSuperClass(), Depth = (DldInheritanceDepth)( (int)Depth + 0x1 ) ){
        
        Dictionary = this->HookTypeToDictionary( DldHookTypeVtable, Depth );
        assert( Dictionary );
        
        pRawEntry = Dictionary->getObject( pMetaClass->getClassName() );
        
        if( pRawEntry ){
            
            pEntry = OSDynamicCast( DldIOKitHookDictionaryEntry, pRawEntry );
            
            assert( pEntry && pEntry->getHookFunction() );
            assert( DldHookTypeVtable == pEntry->getHookType() );
            
    
            OSString* className;
            className = pEntry->getIOKitClassNameCopy();
            
            //
            // check that the found class can be reached in the hierarchy, this must be
            // always true, this is just a double check
            //
            assert( className && OSMetaClass::checkMetaCastWithName( className, object ) );
            if( !( className && OSMetaClass::checkMetaCastWithName( className, object ) ) ){
                
                //
                // actualy this is an error of the "panic" severity
                //
                assert( "the object can't be cast by class name" );
                pRawEntry = NULL;
                pEntry = NULL;
            }
            
            if( className )
                className->release();
            DLD_DBG_MAKE_POINTER_INVALID( className );
            
            if( !pEntry )
                continue;
            
            assert( DldHookTypeVtable == pEntry->getHookType() );
            
            if( DldHookTypeVtable == pEntry->getHookType() ){
                
                //
                // a direct vtable hook is found
                //
                ( pEntry->getHookFunction() )( object, pEntry->getHookType() );
                
                //
                // end the chain processing
                //
                break;
            }
            
        }// end if( pRawEntry )
        
        
    }// end for( pMetaClass )
    
    
    //
    // now hook using a per object hook
    //
    Dictionary = this->HookTypeToDictionary( DldHookTypeObject, DldInheritanceDepth_0 );
    assert( Dictionary );
    pRawEntry = Dictionary->getObject( object->getMetaClass()->getClassName() );
    if( NULL == pRawEntry )
        return kIOReturnSuccess;
    
    pEntry = OSDynamicCast( DldIOKitHookDictionaryEntry, pRawEntry );
    assert( pEntry && pEntry->getHookFunction() && DldHookTypeObject == pEntry->getHookType() );
    
    //
    // this must be a leaf class
    //
    
    //
    // check that the name defines the leaf class,
    // this is a double check in addition to the above, just to be 100% sure
    //
    
    OSString*  classNameStr;
    classNameStr = pEntry->getIOKitClassNameCopy();
    assert( classNameStr );
    if( classNameStr ){
        
        const OSSymbol*  classNameSym;
        
        classNameSym = OSSymbol::withCStringNoCopy( classNameStr->getCStringNoCopy() );
        assert( classNameSym );
        if( classNameSym ){
            
            assert( OSMetaClass::getMetaClassWithName( classNameSym ) == object->getMetaClass() );
            if( OSMetaClass::getMetaClassWithName( classNameSym ) != object->getMetaClass() ){
                
                assert( !"it seems that this is something wrong with the leaf class hooking code" );
                pEntry = NULL;
            }
            
            classNameSym->release();
            DLD_DBG_MAKE_POINTER_INVALID( classNameSym );
            
        }// end if( classNameSym )
        
        classNameStr->release();
        DLD_DBG_MAKE_POINTER_INVALID( classNameStr );
        
    }// end if( classNameStr )
    
    assert( pEntry );
    if( !pEntry )
        return kIOReturnSuccess;
           
    //
    // if the hook type is DldHookTypeVtable then its has been already
    // applied while processing a parent classes chain for a direct
    // vtable hooks
    //
    assert( DldHookTypeObject == pEntry->getHookType() );
    
    if( DldHookTypeObject == pEntry->getHookType() )
        return ( pEntry->getHookFunction() )( object, pEntry->getHookType() );
    else
        return kIOReturnSuccess;

}

//--------------------------------------------------------------------
 
/*
IOReturn
DldIOKitHookEngine::UnHookObject(
    __inout OSObject* object
    )
{
    OSObject *pRawEntry;
    
    #error Ambiguity! the direct vtable unhooks won't be called!
    pRawEntry = this->Dictionary->getObject( object->getMetaClass()->getClassName() );
    if( NULL == pRawEntry )
        return kIOReturnSuccess;
    
    DldIOKitHookDictionaryEntry*   pEntry;
    
    pEntry = OSDynamicCast( DldIOKitHookDictionaryEntry, pRawEntry );
    assert( pEntry && pEntry->getUnHookFunction() );
    
    return ( pEntry->getUnHookFunction() )( object, pEntry->getHookType() );
}
*/

//--------------------------------------------------------------------

bool
DldIOKitHookEngine::CallObjectFirstPublishCallback(
    __in IOService* NewService
    )
{
    OSObject *pRawEntry;
    
    //
    // this callback is called only for leaf classes
    //
    pRawEntry = this->DictionaryPerObjectHooks->getObject( NewService->getMetaClass()->getClassName() );
    if( NULL == pRawEntry )
        return true;
    
    DldIOKitHookDictionaryEntry*   pEntry;
    
    pEntry = OSDynamicCast( DldIOKitHookDictionaryEntry, pRawEntry );
    assert( pEntry );
    
    if( pEntry->getFirstPublishCallback() ){
        
        return ( pEntry->getFirstPublishCallback() )( NewService );
        
    } else {
        
        return true;
    }
}

//--------------------------------------------------------------------
       
bool
DldIOKitHookEngine::CallObjectTerminatedCallback(
    __in IOService* TerminatedService 
    )
{
    OSObject *pRawEntry;
    
    //
    // this callback is called only for leaf classes
    //
    pRawEntry = this->DictionaryPerObjectHooks->getObject( TerminatedService->getMetaClass()->getClassName() );
    if( NULL == pRawEntry )
        return true;
    
    DldIOKitHookDictionaryEntry*   pEntry;
    
    pEntry = OSDynamicCast( DldIOKitHookDictionaryEntry, pRawEntry );
    assert( pEntry );
    
    if( pEntry->getTerminationCallback() ){
        
        return ( pEntry->getTerminationCallback() )( TerminatedService );
        
    } else {
        
        return true;
    }
}
//--------------------------------------------------------------------
