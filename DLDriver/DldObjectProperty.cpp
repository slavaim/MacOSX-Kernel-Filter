/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include <libkern/OSAtomic.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <IOKit/IOBSD.h>
#include "DldObjectProperty.h"
#include "DldIOService.h"
#include "md5_hash.h"
#include "DldWhiteList.h"
#include "DldIOVnode.h"
#include "DldVnodeHashTable.h"
#include "IOBSDSystem.h"
#include "DldIOKitHookEngine.h"
#include "DldDVDWhiteList.h"
#include "DldNetworkInterfaceFilter.h"

#define super OSObject

#if defined( DBG )
static SInt32  gPropertyCount = 0x0;
#endif//DBG

//--------------------------------------------------------------------

OSDefineMetaClassAndStructors( DldObjectPropertyEntry, OSObject )

//--------------------------------------------------------------------

static DldObjectPropertyTypeDescriptor   gTypeDesc[] =
{
    { DldObjectPopertyType_UsbDevice,           "IOUSBDevice",      kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPopertyType_UsbInterface,        "IOUSBDevice",      kDldObjectPopertyTypeFlavor_Ancillary },
    { DldObjectPopertyType_UsbInterface,        "IOUSBInterface",   kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPopertyType_IOMedia,             "IOMedia",          kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPopertyType_IODVDMedia,          "IODVDMedia",       kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPopertyType_IOCDMedia,           "IOCDMedia",        kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPopertyType_Serial,              "IOSerialStreamSync", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPopertyType_SCSIPeripheralDeviceType05, "IOSCSIPeripheralDeviceType05", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_BluetoothDevice,    "IOBluetoothDevice", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_IOHIDSystem,        "IOHIDSystem",      kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_AppleMultitouchDeviceUserClient, "AppleMultitouchDeviceUserClient", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_Net80211Interface,  "IO80211Interface", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_IONetworkInterface, "IONetworkInterface", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_IOFireWireDevice,   "IOFireWireDevice", kDldObjectPopertyTypeFlavor_Primary },
    { DldObjectPropertyType_IOFireWireSBP2,     "IOFireWireSBP2Target", kDldObjectPopertyTypeFlavor_Primary },
    
    //
    // the last element
    //
    { DldObjectPopertyType_Unknown,           "Unknown", kDldObjectPopertyTypeFlavor_Primary }   
};

//
// init the static member
//
DldObjectPropertyTypeDescriptor*   DldObjectPropertyEntry::typeDescr = gTypeDesc;

//--------------------------------------------------------------------

DldPropertyCommonData::DldPropertyCommonData( __in DldObjectPropertyEntry* cont )
{
    //
    // as it is impossibkle to return an error from the constructor ( exceptions are not allowed in the kernel )
    // every user of rwLock must check for NULL before referencing the pointer
    //
    this->rwLock = IORWLockAlloc();
    if( !this->rwLock ){
        
        assert( !"this->rwLock = IORWLockAlloc() failed" );
        DBG_PRINT_ERROR(("this->rwLock = IORWLockAlloc() failed\n"));
    }
    
    this->container = cont;
}

//--------------------------------------------------------------------

DldPropertyCommonData::~DldPropertyCommonData()
{
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
    if( this->parentProperty )
        this->parentProperty->release();
    
    if( this->virtualBackingServices )
        DldFreeServicesArray( this->virtualBackingServices );
    
    if( this->whiteListState.acl )
        this->whiteListState.acl->release();
}

//--------------------------------------------------------------------

void
DldPropertyCommonData::LockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    //
    // check is required as the allocation is made in the constructor
    // and can fail without notification
    //
    if( !this->rwLock )
        return;
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

void
DldPropertyCommonData::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    //
    // check is required as the allocation is made in the constructor
    // and can fail without notification
    //
    if( !this->rwLock )
        return;
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldPropertyCommonData::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() != this->exclusiveThread );//recursive lock, a dead lock
#endif//DBG
    
    //
    // check is required as the allocation is made in the constructor
    // and can fail without notification
    //
    if( !this->rwLock )
        return;
    
    IORWLockWrite( this->rwLock );
    
#if defined(DBG)
    assert( NULL == this->exclusiveThread );
    this->exclusiveThread = current_thread();
#endif//DBG
    
};

//--------------------------------------------------------------------

void
DldPropertyCommonData::UnLockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    //
    // check is required as the allocation is made in the constructor
    // and can fail without notification
    //
    if( !this->rwLock )
        return;
    
#if defined(DBG)
    assert( current_thread() == this->exclusiveThread );
    this->exclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldObjectPropertyEntry::LockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

void
DldObjectPropertyEntry::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldObjectPropertyEntry::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() != this->exclusiveThread );//recursive lock, a dead lock
#endif//DBG
    
    IORWLockWrite( this->rwLock );
    
#if defined(DBG)
    assert( NULL == this->exclusiveThread );
    this->exclusiveThread = current_thread();
#endif//DBG
    
};

//--------------------------------------------------------------------

void
DldObjectPropertyEntry::UnLockExclusive()
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

bool
DldObjectPropertyEntry::allocatePropertyDataWithType(
    __in DldObjectPropertyTypeDescriptor*   typeDsc
    )
{
    assert( preemption_enabled() );
    assert( DldObjectPopertyType_Unknown < typeDsc->type && typeDsc->type < DldObjectPopertyType_Max );
    assert( !this->dataU.property );
    
    DldPropertyCommonData* data;
    
    switch( typeDsc->type ){
            
        case DldObjectPopertyType_UsbDevice:
            data = new DldIoUsbDevicePropertyData( this );
            break;
            
        case DldObjectPopertyType_UsbInterface:
            data = new DldIoUsbInterfacePropertyData( this );
            break;
            
        case DldObjectPopertyType_IOMedia:
            data = new DldIOMediaPropertyData( this );
            break;
            
        case DldObjectPopertyType_IOCDMedia:
        case DldObjectPopertyType_IODVDMedia:
            data = new DldIODVDMediaPropertyData( this );
            break;
            
        case DldObjectPopertyType_Serial:
            data = new DldSerialPropertyData( this );
            break;
            
        case DldObjectPopertyType_SCSIPeripheralDeviceType05:
            data = new DldIOSCSIPeripheralDeviceType05PropertyData( this );
            break;
            
        case DldObjectPropertyType_BluetoothDevice:
            data = new DldIoBluetoothDevicePropertyData( this );
            break;
            
        case DldObjectPropertyType_IOHIDSystem:
            data = new DldIOHIDSystemPropertyData( this );
            break;
            
        case DldObjectPropertyType_AppleMultitouchDeviceUserClient:
            data = new DldAppleMultitouchDeviceUserClientPropertyData( this );
            break;
            
        case DldObjectPropertyType_Net80211Interface:
            data = new DldNet80211InterfacePropertyData( this );
            break;
            
        case DldObjectPropertyType_IONetworkInterface:
            data = new DldNetIOInterfacePropertyData( this );
            break;
            
        case DldObjectPropertyType_IOFireWireDevice:
            data = new DldIOFireWireDevicePropertyData( this );
            break;
            
        case DldObjectPropertyType_IOFireWireSBP2:
            data = new DldIOFireWireSBP2DeviceProperty( this );
            break;
            
        default:
            assert( !"an unknown property type" );
            DBG_PRINT_ERROR(( "DldPropertyCommon::allocatePropertyData( %u ) received an uknown property type for the %s class\n",
                              (int)typeDsc->type, typeDsc->className ));
            return NULL;
            
    }// end switch( typeDsc->type )
    
    assert( data );
    
    //
    // set the descriptor type properties
    //
    if( data ){
        
        data->typeDsc = *typeDsc;
        this->dataU.property = data;
        
        assert( typeDsc->type == this->dataU.property->typeDsc.type );
        
#if defined( DBG )
        OSIncrementAtomic( &gPropertyCount );
#endif//DBG
        
    }// end if( data )
    
    return ( NULL != data );
}

//--------------------------------------------------------------------

bool
DldObjectPropertyEntry::updateDescriptor(
    __in_opt IOService* service,
    __in     DldIOService* dldService,
    __in     bool forceUpdate
    )
/*
 
 the function must be called each time the corresponding device's state or
 property changes
 
 the logic for the input parameters is quite tricky, generally
 the dldService should be used as a cache for the device service's
 properties thus the service parameter is not used too often and
 might be NULL for most of the cases
 
 a return value should be ignored
 */
{
    
    assert( preemption_enabled() );
    assert( dldService );
    assert( this->dataU.property );
    assert( DldObjectPopertyType_Unknown < this->dataU.property->typeDsc.type &&
           this->dataU.property->typeDsc.type < DldObjectPopertyType_Max );
    assert( this->dataU.property->dldService == dldService );
    
    if( !this->dataU.property->dldService ){
        
        assert( !"we should have not been here as the fields are initialized when the object is created" );
        this->dataU.property->dldService = dldService;
        this->dataU.property->onBootDevicePath = DldIsBootMedia( dldService->getSystemService() );
    }
    
    //
    // do not fill the device property until the full start or
    // the last call before a full device start as not
    // all properties might have been initialized or have correct values
    //
    if( kDldPnPStateReadyToStart != dldService->getPnPState() &&
        kDldPnPStateStarted != dldService->getPnPState() )
        return true;
    
    //
    // so wea are here only when the original service has started
    //
    this->dataU.property->fillPropertyData( dldService, forceUpdate );
    assert( this->dataU.property->initialized );
#if defined(DBG)
    assert( 0x1 == this->dataU.property->flags.baseClassPropertyFilled );
#endif // DBG
    
    if( kDldObjectPopertyTypeFlavor_Primary == this->dataU.property->typeDsc.flavor ){
        
        //
        // no need for locking as the properties is for the information purpose only and the setProperty is atomic
        //
        dldService->setProperty( DldStrPropertyMajorType, DldDeviceTypeToString( this->dataU.property->deviceType.type.major ) );
        dldService->setProperty( DldStrPropertyMinorType, DldDeviceTypeToString( this->dataU.property->deviceType.type.minor ) );
        dldService->setProperty( DldStrPropertyPropertyType, DldPropertyTypeToString( this->dataU.property->typeDsc.type ) );
        
    } else if( kDldObjectPopertyTypeFlavor_Ancillary == this->dataU.property->typeDsc.flavor ){
        
        //
        // it is supposed that there is one abcillary property, if not true then only the last updated will be reflected in the object properties
        //
        
        //
        // no need for locking as the properties is for the information purpose only and the setProperty is atomic
        //
        dldService->setProperty( DldStrPropertyMajorTypeAncillary, DldDeviceTypeToString( this->dataU.property->deviceType.type.major ) );
        dldService->setProperty( DldStrPropertyMinorTypeAncillaey, DldDeviceTypeToString( this->dataU.property->deviceType.type.minor ) );
        dldService->setProperty( DldStrPropertyPropertyTypeAncillary, DldPropertyTypeToString( this->dataU.property->typeDsc.type ) );
    }
    
    //
    // update ancillary properties
    //
    if( this->ancillaryObjectPropertyEntries ){
        
        //
        // the lock acquiring combined with the following updateDescriptor() calling
        // might lead to a deadlock, be vigilant!
        //
        this->LockShared();
        if( this->ancillaryObjectPropertyEntries ){
             
            int ancCount;
            
            ancCount = this->ancillaryObjectPropertyEntries->getCount();
            for( int m = 0x0; m < ancCount; ++m ){
                
                DldObjectPropertyEntry* ancProp;
                
                assert( OSDynamicCast( DldObjectPropertyEntry, this->ancillaryObjectPropertyEntries->getObject( m ) ) );
                
                ancProp = OSDynamicCast( DldObjectPropertyEntry, this->ancillaryObjectPropertyEntries->getObject( m ) );
                assert( ancProp );
                
                ancProp->updateDescriptor( service, dldService, forceUpdate );
                
            }// end for( int m = 0x0; m < ancCount; ++m )
            
        }// end  if( this->ancillaryObjectPropertyEntries )
        this->UnLockShared();
    }
    
    return true;
}

//--------------------------------------------------------------------

void DldObjectPropertyEntry::freePropertyData( DldPropertyCommonData* data )
{
    assert( preemption_enabled() );
    assert( data && DldObjectPopertyType_Unknown < data->typeDsc.type && data->typeDsc.type < DldObjectPopertyType_Max );
    
    delete data;
}

//--------------------------------------------------------------------

void DldObjectPropertyEntry::removeAncillaryProperties()
{
    //
    // remove all ancillary properties as they might contain back references to
    // the primary property thus creating a reference loop which will prevent
    // from both objects being destroyed as the reference counts don't drop to zero
    //
    
    if( this->ancillaryObjectPropertyEntries ){
        
        OSArray*  ancillaryProperties;
        
        this->LockExclusive();
        {// start of the lock
            
            ancillaryProperties = this->ancillaryObjectPropertyEntries;
            this->ancillaryObjectPropertyEntries = NULL;
            
        }// end of the lock
        this->UnLockExclusive();
        
        if( ancillaryProperties ){
            
            //
            // OSArray::free() calls flushCollection() thus releasing all objects
            //
            ancillaryProperties->release();
            
        }// end if( ancillaryProperties )
        
    }// end if( this->property && ...
}

//--------------------------------------------------------------------

/*
 an example for caling stack when an ancillary property is being freed, just FYI
 #0  DldObjectPropertyEntry::free (this=0x98e49c0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldObjectProperty.cpp:398
 #1  0x004fba02 in OSObject::taggedRelease (this=0x98e49c0, tag=0x85ff18) at /SourceCache/xnu/xnu-1456.1.25/libkern/c++/OSObject.cpp:183
 #2  0x004fc858 in OSArray::flushCollection (this=0x8bfb300) at /SourceCache/xnu/xnu-1456.1.25/libkern/c++/OSArray.cpp:222
 #3  0x004fc9c0 in OSArray::free (this=0x8bfb300) at /SourceCache/xnu/xnu-1456.1.25/libkern/c++/OSArray.cpp:166
 #4  0x004fba02 in OSObject::taggedRelease (this=0x8bfb300, tag=0x0) at /SourceCache/xnu/xnu-1456.1.25/libkern/c++/OSObject.cpp:183
 #5  0x004fba1d in OSObject::release (this=0x8bfb300) at /SourceCache/xnu/xnu-1456.1.25/libkern/c++/OSObject.cpp:255
 #6  0x71bfda77 in DldObjectPropertyEntry::removeAncillaryProperties (this=0x98e7ea0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldObjectProperty.cpp:385
 #7  0x71bd2103 in DldIOService::finalize (this=0xa2bf900, options=5) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOService.cpp:386
 #8  0x71bc5e0d in DldHookerCommonClass::finalize (this=0x8dcee08, serviceObject=0x98df500, options=5) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldHookerCommonClass.cpp:829
 #9  0x71bbc558 in finalize_hook (this=0x98df500, options=5) at DldHookerCommonClass2.h:1145
 #10 0x00546b33 in IOWorkLoop::runAction (this=0x7ebb580, inAction=0x534ebc <IOService::actionFinalize(IOService*, unsigned long)>, target=0x98df500, arg0=0x5, arg1=0x0, arg2=0x0, arg3=0x0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOWorkLoop.cpp:437
 #11 0x00531a8a in _workLoopAction (action=0x534ebc <IOService::actionFinalize(IOService*, unsigned long)>, service=0x98df500, p0=0x5, p1=0x0, p2=0x0, p3=0x0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOService.cpp:1645
 #12 0x00535f41 in IOService::terminateWorker (options=5) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOService.cpp:2099
 #13 0x00536353 in IOService::terminateThread (arg=0x5, waitResult=0) at /SourceCache/xnu/xnu-1456.1.25/iokit/Kernel/IOService.cpp:1833
 */
void
DldObjectPropertyEntry::free( void )
{
    
    if( this->dataU.property )
        this->freePropertyData( this->dataU.property );
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
    if( this->ancillaryObjectPropertyEntries ){
     
        //
        // flushCollection is called by OSArray::free()
        //
        this->ancillaryObjectPropertyEntries->release();
    }
    
#if defined( DBG )
    OSDecrementAtomic( &gPropertyCount );
#endif//DBG
    
    super::free();
}

//--------------------------------------------------------------------

DldObjectPropertyEntry*
DldObjectPropertyEntry::withObject(
    __in IOService* service,
    __in DldIOService* dldService,
    __in bool postponePropertyUpdate,
    __in DldObjectPopertyTypeFlavor flavor
    )
{
    assert( preemption_enabled() );
    assert( dldService && service );
    
    DldObjectPropertyEntry*            newEntry = NULL;
    DldObjectPropertyTypeDescriptor*   desc;
    const OSMetaClass*                 pMetaClass;
    
    pMetaClass = service->getMetaClass();
    assert( pMetaClass );
    
    //
    // find the most specific class in the hierarchy for which there is
    // a property descriptor, so perform traversing from a derived
    // class to a super class,
    // each iteration moves to the one level below in the inheritance chain,
    // i.e. to the super class
    //
    for( pMetaClass = service->getMetaClass();
         pMetaClass && !newEntry;
         pMetaClass = pMetaClass->getSuperClass() ){
        
        assert( !newEntry );
        
        const char * className = pMetaClass->getClassName();
        
        //
        // iterate through all descriptors comparing the name
        // in a descriptor with the class's name
        //
        for( desc = DldObjectPropertyEntry::typeDescr;
             DldObjectPopertyType_Unknown != desc->type;
             ++desc ){
            
            assert( !newEntry );
            
            if( flavor != desc->flavor )
                continue;
            
            if( 0x0 != strcmp( className, desc->className ) )
                continue;
            
            //
            // use the cast by name as casting by type requires a class's MetaCast
            // object binding to this kernel extension thus requiring the corresponding
            // extension being loaded and present and linked to this extension,
            // the possible optimization is converting the names to MetaClass objects
            // for the existing classes but this requires to update dynamicaly the new
            // classes registration and derigistration which seems complicated
            //
            assert( OSMetaClass::checkMetaCastWithName( desc->className, service ) );
            if( !OSMetaClass::checkMetaCastWithName( desc->className, service ) )
                continue;
            
            newEntry = new DldObjectPropertyEntry();
            assert( newEntry );
            
            //
            // exit the inner "for" loop
            //
            break;
            
        }// end for( desc )
        
        if( newEntry ){
            
            //
            // exit the outer "for" loop
            //
            assert( 0x0 == strcmp( className, desc->className ) );
            break;
        }
        
    }// end for( pMetaClass )
    
    
    if( !newEntry )
        return NULL;
    
    newEntry->rwLock = IORWLockAlloc();
    if( !newEntry->rwLock ){
        
        assert( !"newEntry->rwLock = IORWLockAlloc() failed" );
        
        newEntry->release();
        return NULL;
    }
    
    
    if( !newEntry->init() ){
        
        assert( !"newEntry->init() failed" );
        
        newEntry->release();
        return NULL;
    }
    
    assert( pMetaClass && 0x0 == strcmp( pMetaClass->getClassName(), desc->className ) );
    
    newEntry->allocatePropertyDataWithType( desc );
    assert( newEntry->dataU.property );
    if( !newEntry->dataU.property ){
        
        newEntry->release();
        return NULL;
    }
    
     assert( flavor == newEntry->dataU.property->typeDsc.flavor );
    
    //
    // initialize the property's data
    //
    newEntry->dataU.property->dldService       = dldService;
    newEntry->dataU.property->service          = dldService->getSystemService();
    newEntry->dataU.property->onBootDevicePath = DldIsBootMedia( dldService->getSystemService() );
    
    if( newEntry->dataU.property->onBootDevicePath )
        dldService->deviceIsOnBootPath();
    
    //
    // update the descriptor with the data
    //
    if( !postponePropertyUpdate )
        newEntry->updateDescriptor( service, dldService, true );
    
    if( kDldObjectPopertyTypeFlavor_Primary == flavor ){
        
        //
        // add an ancillary property entry
        //
        DldObjectPropertyEntry* ancillaryProperty;
        
        //
        // it is OK to get NULL - this simply means that there is no ancillary descriptor for this type
        //
        ancillaryProperty = DldObjectPropertyEntry::withObject( service,
                                                                dldService,
                                                                postponePropertyUpdate,
                                                                kDldObjectPopertyTypeFlavor_Ancillary );
        if( ancillaryProperty ){
            
            //
            // OK, there is an ancillary property type descriptor
            //
            assert( kDldObjectPopertyTypeFlavor_Ancillary == ancillaryProperty->dataU.property->typeDsc.flavor );
            
            if( NULL == newEntry->ancillaryObjectPropertyEntries ){
                
                newEntry->ancillaryObjectPropertyEntries = OSArray::withCapacity( 0x1 );
                assert( newEntry->ancillaryObjectPropertyEntries );
            }
            
            if( newEntry->ancillaryObjectPropertyEntries ){
                
                bool added;
                
                added = newEntry->ancillaryObjectPropertyEntries->setObject( ancillaryProperty );
                assert( added );
            }
            
            //
            // the objet is retained by OSArray if setObject() was successfull
            //
            ancillaryProperty->release();
            DLD_DBG_MAKE_POINTER_INVALID( ancillaryProperty );
            
        }// end if( ancillaryProperty )
        
    }// end if( kDldObjectPopertyTypeFlavor_Primary == flavor )
    
    return newEntry;
}

//--------------------------------------------------------------------

void
DldObjectPropertyEntry::setPnPState( __in DldPnPState pnpState )
{
    assert( pnpState < kDldPnPStateMax );
    
    this->dataU.property->pnpState = pnpState;
    
    //
    // set the PnP state for the ancillary properties
    //
    if( this->ancillaryObjectPropertyEntries ){
        
        this->LockShared();
        if( this->ancillaryObjectPropertyEntries ){
            
            int ancCount;
            
            ancCount = this->ancillaryObjectPropertyEntries->getCount();
            for( int m = 0x0; m < ancCount; ++m ){
                
                DldObjectPropertyEntry* ancProp;
                
                assert( OSDynamicCast( DldObjectPropertyEntry, this->ancillaryObjectPropertyEntries->getObject( m ) ) );
                
                ancProp = OSDynamicCast( DldObjectPropertyEntry, this->ancillaryObjectPropertyEntries->getObject( m ) );
                assert( ancProp );
                
                ancProp->setPnPState( pnpState );
                
            }// end for( int m = 0x0; m < ancCount; ++m )
            
        }// end  if( this->ancillaryObjectPropertyEntries )
        this->UnLockShared();
    }
}

//--------------------------------------------------------------------

void
DldPropertyCommonData::setMinorType(
    __in UInt32 requestedParentType,
    __in DldIOService* dldService
    )
{
    assert( dldService );
    
    if( !dldService )
        return;
    
    if( requestedParentType == this->deviceType.type.major ){
        
        //
        // there is no point in searching for the same type,
        // this doesn't add any new information even if being found
        //
        return;
    }
    
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    DldObjectPropertyEntry* parentPropertyRef = NULL;
    
    dldService->LockShared();
    {// start of the lock
        
        const OSArray*  parentProperties;
        
        parentProperties = dldService->getParentProperties();
        if( parentProperties ){
            
            for( unsigned int i = 0x0; i < parentProperties->getCount(); ++i ){
                
                OSObject* object = parentProperties->getObject( i );
                
                assert( object && OSDynamicCast( DldObjectPropertyEntry, object ) );
                
                if( !object || !OSDynamicCast( DldObjectPropertyEntry, object ) )
                    continue;
                
                DldObjectPropertyEntry* parentProperty = OSDynamicCast( DldObjectPropertyEntry, object );
                assert( parentProperty );
                
                if( requestedParentType == parentProperty->dataU.property->deviceType.type.major ){
                    
                    //
                    // set the minor type, do not set the parent's "minor type" as this is incorrect - there might
                    // be multiple children of different types and the security check supposes that the "minor type"
                    // is a type of a real parent, nonetheless if you change the parent minor type here you must update
                    // the corresponding DldIOService object's property to reflect the updated type in the IORegistryExplorer,
                    // TO DO move a DldIOService's property update here
                    //
                    this->deviceType.type.minor = parentProperty->dataU.property->deviceType.type.major;
                    
                    parentPropertyRef = parentProperty;
                    parentPropertyRef->retain();
                                        
                    break;
                }// end if( requestedParentType == parentProperty->dataU.property->deviceType.type.major )
                
            }// end for
            
        }// end if( parentProperties )
        
    }// end of the lock
    dldService->UnLockShared();
    
    //
    // save the parent
    //
    if( parentPropertyRef ){
        
        if( NULL == this->parentProperty ){
            
            this->LockExclusive();// atomic compare-exchange can be used instead
            {// start of the lock
                
                if( NULL == this->parentProperty ){
                    
                    this->parentProperty = parentPropertyRef;
                    parentPropertyRef = NULL;// we lost the ownership as it was transfered
                    
                } else {
                    
                    assert( parentPropertyRef == this->parentProperty );
                }
                
            }// end of the lock
            this->UnLockExclusive();
            
        }// end if( NULL == this->dataU.property->parentProperty )
        
        if( parentPropertyRef )
            parentPropertyRef->release();
        
    }// end if( parentPropertyRef )
    
}

//--------------------------------------------------------------------

bool
DldPropertyCommonData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    //
    // the base class function is incomplete in the sence
    // that it doesn't completely initialize the object
    // and should be called only after the object's
    // necessary fields have been initialized by a
    // derived oject's fillPropertyData()
    //
    assert( this->initialized );
    
    //
    // check whether the device is attached to a USB bus
    //
    this->setMinorType( DLD_DEVICE_TYPE_USB, dldService );
    
    //
    // check whether the device is attached to a IEEE1394 bus
    //
    this->setMinorType( DLD_DEVICE_TYPE_IEEE1394, dldService );
    
#if defined(DBG)
    this->flags.baseClassPropertyFilled = 0x1;
#endif // DBG
    
    return true;
}

//--------------------------------------------------------------------

bool
DldIOMediaPropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    bool           IsDVD = false;
    DldIOService*  lowestBackingMedia = NULL;
    
    //
    // the isUnderlyingStackProcessed value defines whether
    // the driver intercepts all requests to build the underlying
    // stack, it is so on to occasions
    //  - the second scan on the driver start
    //  - a new device arrival ivent after the driver initialized
    //
    bool           isUnderlyingStackProcessed = gFirstScanCompleted;
    
    assert( dldService );
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    this->initialized = true;
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    
    //
    // the BSD subsystem initialization might be delayed, so check BSDName on each invokation
    //
    if( NULL == this->BSDName ){
        
        dldService->LockExclusive();
        {
            if( NULL == this->BSDName && dldService->getProperty( kIOBSDNameKey ) ){
                
                OSObject*  property;
                property = dldService->getProperty( kIOBSDNameKey );
                assert( property );
                if( property ){
                    
                    OSString* string;
                    string = OSDynamicCast( OSString, property );
                    assert( string );
                    
                    //
                    // make a copy
                    //
                    if( string ){
                        //
                        // check for a full name /dev/(r)diskX
                        //
                        if( string->getChar(0) == '/' ){
                            
                            this->BSDName = string;
                            this->BSDName->retain();
                            
                        } else {
                            
                            size_t    requiredLength = sizeof("/dev/") + string->getLength();
                            char*     newCString = (char*)IOMalloc(requiredLength);
                            assert( newCString );
                            if( newCString ){
                                
                                memcpy( newCString, "/dev/", sizeof( "/dev/" ) - sizeof('\0') );
                                memcpy( newCString + sizeof( "/dev/" ) - sizeof('\0'), string->getCStringNoCopy(), string->getLength() );
                                newCString[ requiredLength - sizeof('\0') ] = '\0';
                                
                                OSString*  newString = OSString::withCString(newCString);
                                assert( newString );
                                if( newString ){
                                    
                                    this->BSDName = newString; // already retained by OSString::withString
                                    newString = NULL; // just to catch bugs
                                }
                                
                                IOFree( newCString, requiredLength);
                            } // end if( newCString )
                        }
                    } // if( string )
                } // end if( property )
            } // end if( dldService->getProperty( kIOBSDNameKey ) )
        }
        dldService->UnLockExclusive();
        
    } // end if( NULL == this->BSDName )
    
    
    //
    // all this booleans are set together
    //
    assert( ( this->parentInfoGathered &&
              this->removableTypeDefined &&
              this->protocolCharacteristicsProcessed )
           ||
           ( !this->parentInfoGathered &&
             !this->removableTypeDefined &&
             !this->protocolCharacteristicsProcessed )
           );
    
    if( !forceUpdate &&
        ( this->parentInfoGathered &&
          this->removableTypeDefined &&
          this->protocolCharacteristicsProcessed ) )
        return true;
    
    //
    // get the protocol characterisitcs which includes the
    // external or internal location characterisitc and
    // the physical entity's type to which the media is attached,
    // this call also sets the lowest backing media value
    //
    this->processProtocolCharacteristics( dldService );
    this->protocolCharacteristicsProcessed = isUnderlyingStackProcessed;
    
    //
    // get the lowest media in the chain for virtual devices, this media
    // defines the actual type of the virtual media, NULL is returned
    // for non-virtual devices, the returned object is referenced
    //
    lowestBackingMedia = this->getLowestBackingObjectReferenceForVirtualObject();
    if( lowestBackingMedia ){
        
        OSArray*                   parentProperties = NULL;
        DldObjectPropertyEntry*    objectProperty = NULL;
        
        
        //__asm__ volatile( "int $0x3" );
        
        //
        // so the current virtual stack is supported by a real physical one,
        // copy the properties from the latter as the parent properties
        // for the current, this allows correctly define the device type
        // even if the physical stack will be removed because of a physical
        // device surprise removal, so the sequence of properties will
        // be like the physical stack is above the virtual one - this is
        // unusual but it seems this won't result in any mishap
        //
        lowestBackingMedia->LockExclusive();
        {// start of the lock
            
            parentProperties = const_cast<OSArray*>(lowestBackingMedia->getParentProperties());
            if( parentProperties )
                parentProperties->retain();
            
            objectProperty = lowestBackingMedia->getObjectProperty();
            if( objectProperty )
                objectProperty->retain();
            
        }// end of the lock
        lowestBackingMedia->UnLockExclusive();
        
        
        if( parentProperties ){
            
            //
            // addParentProperties() check for duplication so 
            // calling isObjectInParentProperties() is not required
            //
            dldService->addParentProperties( parentProperties );
            parentProperties->release();
            parentProperties = NULL;
        }
        

        if( objectProperty ){
           
            //
            // as lowestBackingMedia is shared by all devices in the stack and is found independently
            // for each device ( by the current function for several layers of IOMedia ( disk->volume) )
            // the check is required to skip an adding if it was copied from the lower iOMedia ( i.e. disk 
            // and we are on the volume level )
            //
            if( !dldService->isObjectInParentProperties( objectProperty ) )
                dldService->addParentProperty( objectProperty );
            
            objectProperty->release();
            objectProperty = NULL;
        }
            
        
    }// end if( lowestBackingMedia )
    
    //
    // IOMedia  property might be a super class for other properties, so we need to check
    // the already existing type
    //
    if( DldObjectPopertyType_IODVDMedia == this->typeDsc.type ||
        DldObjectPopertyType_IOCDMedia == this->typeDsc.type ){
        
        IsDVD = true;
        
    } else {
        
        //
        // IOMedia is mounted on IOPartitionScheme and can be a disk or CD/DVD mefia,
        // so we need to check the parent to find whether the parent is a DVD or a disk
        //
        dldService->LockShared();
        {// start of the lock
            
            const OSArray*  parentProperties;
            
            parentProperties = dldService->getParentProperties();
            if( parentProperties ){
                
                for( unsigned int i = 0x0; i < parentProperties->getCount(); ++i ){
                    
                    OSObject* object = parentProperties->getObject( i );
                    
                    assert( object && OSDynamicCast( DldObjectPropertyEntry, object ) );
                    
                    if( !object || !OSDynamicCast( DldObjectPropertyEntry, object ) )
                        continue;
                    
                    DldObjectPropertyEntry* property = OSDynamicCast( DldObjectPropertyEntry, object );
                    assert( property );
                    
                    //
                    // actually DldObjectPopertyType_IOCDMedia is supported by the same property
                    // class as DldObjectPopertyType_IODVDMedia therefore the DldObjectPopertyType_IOCDMedia
                    // value won't be incountered
                    //
                    if( DldObjectPopertyType_IODVDMedia == property->dataU.property->typeDsc.type ||
                        DldObjectPopertyType_IOCDMedia == property->dataU.property->typeDsc.type ){
                        
                        IsDVD = true;
                        break;
                        
                    }// end if( DldObjectPopertyType_IODVDMedia == property->dataU.property->typeDsc.type )
                    
                }// end for
                
            }// end if( parentProperties )
            
        }// end of the lock
        dldService->UnLockShared();
    }
    //
    // no need to repeat the parents info gathering, a new parent can't appear
    //
    this->parentInfoGathered = isUnderlyingStackProcessed;
    
    //
    // lock the descriptor if making non-atomic value change,
    // all aligned 1,2 and 4 bytes access are atomic on 32 and 64 bit Intel CPUs
    //
    
    assert( DldObjectPopertyType_IOMedia == this->typeDsc.type ||
            DldObjectPopertyType_IODVDMedia == this->typeDsc.type || 
            DldObjectPopertyType_IOCDMedia == this->typeDsc.type );
    assert( dldService );
    
    dldService->LockShared();
    {// start of the lock
        
        IORegistryEntry*  objectForQuery;
        bool              isRemovable = false;
        bool              isRemovableByLower = false;
        
        //
        // it is preferable to use the DldIOService object as we can control its integrity
        // and it contains the service object's properties snapshot
        //
        objectForQuery = dldService;
        
        //
        // check for a removable capability
        //
        OSObject*  property;
        
        property = objectForQuery->getProperty( kIOMediaRemovableKey );
        if( property ){
            
            if( OSDynamicCast( OSBoolean, property ) )
                isRemovable = ( (FALSE == this->onBootDevicePath) &&
                                (OSDynamicCast(OSBoolean, property))->getValue() );
            
            DLD_DBG_MAKE_POINTER_INVALID( property );
        }// if( property )
        
        //
        // there is a subtlety here - FileVault works by creating a vitual
        // removable disk backed by a file, in that case we need to find
        // the actual place of the data destination which defines
        // the actual disk type
        //
        if( kPI_Virtual == this->physicalInterconnect ){
            
            //
            // a virtual device, check the backing media to know whether the device
            // is backed by removable media, a first scan can find a virtual
            // device's stack before a physical device's stack
            //
            assert( !( isUnderlyingStackProcessed && !lowestBackingMedia ) );
            
            if( lowestBackingMedia ){
                
                DldObjectPropertyEntry*   lowestPropertyEntry;
                
                lowestPropertyEntry = lowestBackingMedia->getObjectProperty();
                assert( lowestPropertyEntry );
                
                if( lowestPropertyEntry && DldObjectPopertyType_IOMedia == lowestPropertyEntry->dataU.property->typeDsc.type )
                    isRemovableByLower = lowestPropertyEntry->dataU.ioMediaProperty->isRemovable;
                
            }
            
        }// end if( kPI_Virtual == this->dataU.ioMediaProperty->physicalInterconnect )
        
        
        if( kPIL_External == this->physicalInterconnectLocation ||
            kPI_USB == this->physicalInterconnect ||
            kPI_FireWire == this->physicalInterconnect ){
            
            isRemovable = ( FALSE == this->onBootDevicePath );
        }
        
            
        if( lowestBackingMedia ){
            
            //
            // a physical backing device overwrites any virtual device settings
            //
            this->isRemovable = isRemovableByLower;
            isRemovable = isRemovableByLower;
            
        } else {
            
            this->isRemovable = isRemovable;
        }

        if( IsDVD )
            this->deviceType.type.major = DLD_DEVICE_TYPE_CD_DVD;
        else
            this->deviceType.type.major = isRemovable ? DLD_DEVICE_TYPE_REMOVABLE : DLD_DEVICE_TYPE_HARDDRIVE;
            
        //
        // no need to repeat the above gathering again next time,
        // as for example the backing disk for a FileVault
        // disk might have gone and in that case we hit
        // a wrong path as the backing disk has not been
        // found, so for a short period of time until ejecting
        // the disk type will be wrong
        //
        this->removableTypeDefined = isUnderlyingStackProcessed;
        
    }// end of the lock
    dldService->UnLockShared();
    
    
    if( lowestBackingMedia )
        lowestBackingMedia->release();
    
    this->checkSecurity = isUnderlyingStackProcessed;
    
    //
    // check for CoreStorage property for encrypted drives, this exists only on Lion and newer versions of OS X
    //
    dldService->LockShared();
    {// start of the lock
        
        OSObject*  property;
        
        property = dldService->getProperty( "CoreStorage Encrypted" );;
        if( property ){
            
            if( OSDynamicCast( OSBoolean, property ) ){
                
                this->encryptionProvider = ( (OSDynamicCast( OSBoolean, property ))->isTrue() ) ? ProviderMacOsEncryption : ProviderUnknown;
            }
            
            DLD_DBG_MAKE_POINTER_INVALID( property );
        }// if( object )
        
    }// end of the lock
    dldService->UnLockShared();
    
    return true;
}

//--------------------------------------------------------------------

bool
DldIODVDMediaPropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    if( !this->initialized ){
        
        this->deviceType.type.major = DLD_DEVICE_TYPE_CD_DVD;
        this->initialized = true;
        
    }
    
    return this->DldIOMediaPropertyData::fillPropertyData( dldService, forceUpdate );
}

//--------------------------------------------------------------------

DldDeviceUID  gZeroUid = { {0x0} };

bool
DldIsUidValid( __in DldDeviceUID* uid)
{
    return( 0x0 != memcmp( uid->uid, gZeroUid.uid, sizeof( gZeroUid.uid ) ) );
}

//--------------------------------------------------------------------

//
// the output UID is mdContext->digest,
// the caller allocated a room for *mdContext
// but don't have to initialize it
//
bool
DldCreateUsbUniqueDeviceId(
    __in    OSString*        uidString,
    __in    DldUsbVidPid*    VidPid,
    __inout MD5_CTX*         mdContext
    )
{
    UInt16*      unicodeStr;
    unsigned int strLength;
    
    assert( 0x0 != VidPid->idVendor && 0x0 != VidPid->idProduct );
    
    //
    // to provide the Windows Dl compatibility we need an upcased Unicode string, 
    // let's assume that USB UID is always in the ASCII format so we can easily transform
    // it to a Unicode string
    //
    
    strLength  = uidString->getLength();
    
    unicodeStr = (UInt16*)IOMalloc( strLength*sizeof( UInt16 ) );
    assert( unicodeStr );
    if( !unicodeStr )
        return false;
    
    bzero( unicodeStr, strLength*sizeof( UInt16 ) );
    
    //
    // ASCII to Unicode
    //
    for( int i = 0x0; i < strLength; ++i ){
        
        char  ch;
        
        ch = uidString->getChar( i );
        
        //
        // upcase a character
        //
        if( (ch >= 'a') && (ch <= 'z') ){

            ch = ch - 32;
        } // end if
        
        unicodeStr[ i ] = ch;
        
    }// end for
    
    DldMD5Init( mdContext );
    
    DldMD5Update( mdContext, 
               (unsigned char*)unicodeStr, 
               strLength*sizeof( UInt16 ) );
    
    DldMD5Update( mdContext, 
               (unsigned char*)VidPid, 
               sizeof( *VidPid ) );
    
    DldMD5Final( mdContext );
    
    IOFree( unicodeStr, strLength*sizeof( UInt16 ) );
    return true;
}

//--------------------------------------------------------------------

bool
DldIoUsbDevicePropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    
    bool  updateWhiteListProperty = false;
    
    assert( this->container );
    assert( DldObjectPopertyType_UsbDevice == this->typeDsc.type );
    assert( dldService );
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    if( !this->initialized ){
        
        this->deviceType.type.major = DLD_DEVICE_TYPE_USB;
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    //
    // the checkSecurity value is set when attached IOUsbInterfaces will be discovered,
    // by default the value is "false"
    //
    
    //
    // the ACL objects declaration are moved outsie the lock to
    // not call release() property under the lock
    //
    DldAclObject*   acl    = NULL;
    DldAclObject*   oldAcl = NULL;
    
    //
    // locking shared provide us with a consistent property snapshot
    //
    dldService->LockShared();
    {// start of the lock
        
        IORegistryEntry*  objectForQuery;
        
        //
        // it is preferable to use the DldIOService object as we can control its integrity
        // and it contains the service object's properties snapshot
        //
        objectForQuery = dldService;
        
        //
        // get a VID
        //
        if( 0x0 == this->flags.vidValid ){
            
            OSObject*  property;
            
            property = objectForQuery->getProperty( kUSBVendorID );
            if( property ){
                
                if( OSDynamicCast( OSNumber, property ) ){
                    
                    this->VidPid.idVendor = (OSDynamicCast(OSNumber, property))->unsigned16BitValue();
                    this->flags.vidValid = 0x1;
                }
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
            }// if( object )
            
        }
        
        
        //
        // get a PID
        //
        if( 0x0 == this->flags.pidValid ){
            
            OSObject*  property;
            
            property = objectForQuery->getProperty( kUSBProductID );
            if( property ){
                
                if( OSDynamicCast( OSNumber, property ) ){
                    
                    this->VidPid.idProduct = (OSDynamicCast(OSNumber, property))->unsigned16BitValue();
                    this->flags.pidValid = 0x1;
                }
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
            }// if( object )
            
        }
        
        //
        // get a USB address( uniquely identifies a device for each port )
        //
        if( 0x0 == this->usbDeviceAddress ){
            
            OSObject*  property;
            
            property = objectForQuery->getProperty( kUSBDevicePropertyAddress );
            if( property ){
                
                if( OSDynamicCast( OSNumber, property ) )
                    this->usbDeviceAddress = (OSDynamicCast(OSNumber, property))->unsigned16BitValue();
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
            }// if( object )
            
        }
        
        //
        // get a unique ID, creating a UID requires a valid VidPid
        //
        if( !DldIsUidValid( &this->uid ) && this->flags.vidValid && this->flags.pidValid ){
            
            OSObject*  property;
            OSString*  uidString = NULL;
            
            property = objectForQuery->getProperty( kUSBSerialNumberString );
            if( !property ){
                
                //
                // this is 10.5 which doesn't set UID string property,
                // use a UID string index to retrieve a UID string
                //
                IOService*       ioService;
                
                ioService = dldService->getSystemServiceRef();
                assert( ioService );
                if( ioService ){
                    
                    IOUSBDevice*    ioUSBDevice;
                    
                    ioUSBDevice = (IOUSBDevice*)OSMetaClass::checkMetaCastWithName( "IOUSBDevice", ioService );
                    assert( ioUSBDevice );
                    if( ioUSBDevice ){
                        
                        OSObject*            indexObject;
                        unsigned long long   index = 0x0;
                        
                        indexObject = ioUSBDevice->getProperty( kUSBSerialNumberStringIndex );
                        if( indexObject ){
                            
                            assert( OSDynamicCast( OSNumber, indexObject ) );
                            
                            if( OSDynamicCast( OSNumber, indexObject ) ){
                                
                                index = (OSDynamicCast( OSNumber, indexObject ))->unsigned64BitValue();
                                
                            }// end if( OSDynamicCast( OSNumber, indexObject ) )
                            
                        }// end if( indexObject )
                        
                        //
                        // Valid string index?
                        //
                        if( 0x0 != index ){
                            
                            char 	   strBuf[256];
                            UInt16 	   strLen = sizeof(strBuf) - 1;	// GetStringDescriptor MaxLen = 255
                            IOReturn   err;
                            
                            //
                            // Default language is US English
                            //
                            err = ioUSBDevice->GetStringDescriptor((UInt8)index, strBuf, strLen, (UInt16)0x409);
                            if( kIOReturnSuccess == err ){
                                
                                uidString = OSString::withCString( strBuf );
                                property = uidString;
                            }
                            
                        }// end if( 0x0 != index )
                        
                    }// end if( ioUSBDevice )
                    
                    dldService->putSystemServiceRef( ioService );
                    
                }//end if( ioService )
                
            }// end if( !property )
            
            
            if( property ){
                
                OSString* uidString;
                
                uidString = OSDynamicCast( OSString, property );
                assert( uidString );
                if( uidString ){
                    
                    MD5_CTX  mdContext;
                    if( DldCreateUsbUniqueDeviceId( uidString, &this->VidPid, &mdContext ) ){
                        
                        assert( sizeof( this->uid ) == sizeof( mdContext.digest ) );
                        memcpy( this->uid.uid, mdContext.digest, sizeof( mdContext.digest ) );
                        this->container->setUIDProperty();
                        
                        DLD_COMM_LOG( WHITE_LIST, ( "USB UID for 0x%p(V_0x%04x/P_0x%04x) is %08x:%08x:%08x:%08x\n",
                                                       this->service,
                                                       this->VidPid.idVendor,
                                                       this->VidPid.idProduct,
                                                       *(unsigned int*)&this->uid.uid[0],
                                                       *(unsigned int*)&this->uid.uid[4],
                                                       *(unsigned int*)&this->uid.uid[8],
                                                       *(unsigned int*)&this->uid.uid[12] ) );
                        
                    } else {
                        
                        assert( !"DldCreateUsbUniqueDeviceId() failed" );
                        DBG_PRINT_ERROR(( "DldCreateUsbUniqueDeviceId failed for VID=0x%X PID=0x%X\n",
                                          this->VidPid.idVendor,
                                          this->VidPid.idProduct ));
                    }
                    
                }// end if( uidString )
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
                
            }// end if( property )
            
            if( uidString )
                uidString->release();
            
        }// end if( !DldIsUidValid( &this->dataU.usbDeviceProperty->uid ) &&
        
        UInt32 currentWatermark;
        
        do{
            
            bool                checkWhiteList;
            bool                inWhiteList = false;
            bool                propagateUp = false;
            DldWhiteListType    wlType = kDldWhiteListUnknown;
            
            //
            // release the objects from the previous iteration,
            // an extremely rare situation as a single iteration is common
            //
            if( acl ){
                
                acl->release();
                acl = NULL;
            }
            
            if( oldAcl ){
                
                oldAcl->release();
                oldAcl = NULL;
            }
            
            //
            // kDld(Temp)WhiteListUSBVidPid and kDld(Temp)WhiteListUSBUid share the same watermark
            //
            currentWatermark = gWhiteList->getWatermark( kDldWhiteListUSBVidPid );// it is important to use a watermark snapshot across the following code!
            checkWhiteList = ( this->whiteListWatermark != currentWatermark );
            
            //
            // the first is more restrictive case - check for white listing by UID ( a specific device )
            //
            if( checkWhiteList && DldIsUidValid( &this->uid ) ){
                
                DldDeviceUIDEntry  entryCopy;
                
                assert( sizeof( entryCopy.uid.uid ) == sizeof( this->uid.uid ) );
                assert( NULL == acl );
                
                if( gWhiteList->getUsbUIDEnrty( &this->uid, &entryCopy, &acl ) ){
                    
                    //
                    // found in white list
                    //
                    
                    assert( 0x0 == memcmp( this->uid.uid, entryCopy.uid.uid, sizeof( entryCopy.uid.uid ) ) );
                    
                    inWhiteList = true;
                    propagateUp = ( 0x0 != entryCopy.flags.propagateUp );
                    
                    wlType = kDldWhiteListUSBUid;
                    
                }
            }
            
            //
            // check for white listing by VidPid ( a group of devices ) if it has not been white listed by ID
            //
            if( checkWhiteList && !inWhiteList &&
                this->flags.vidValid &&
                this->flags.pidValid ){
                
                DldUsbVidPidEntry  entryCopy;
                
                assert( NULL == acl );
                
                if( gWhiteList->getVidPidEnrty( &this->VidPid, &entryCopy, &acl ) ){
                    
                    //
                    // found in white list
                    //
                    
                    assert( this->VidPid.idVendor == entryCopy.VidPid.idVendor &&
                            this->VidPid.idProduct == entryCopy.VidPid.idProduct );
                    
                    inWhiteList = true;
                    propagateUp = ( 0x0 != entryCopy.flags.propagateUp );
                    
                    wlType = kDldWhiteListUSBVidPid;
                    
                }
                
            }
            
            assert( !( !inWhiteList && propagateUp ) );
            
            //
            // upgrade a watermark value, a concurrecy is not an issue, in the worst case
            // yet another check will be made
            //
            if( checkWhiteList && currentWatermark == gWhiteList->getWatermark( kDldWhiteListUSBVidPid ) ){
                
                this->LockExclusive();
                {// start of the lock
                    
                    //
                    // actually the second check for the watermak consistency
                    // can be skipped but it has a positive impact as
                    // waiting for a lock can take a significant time
                    // and the settings might have been changed while
                    // a thread was blocked
                    //
                    
                    if( currentWatermark == gWhiteList->getWatermark( kDldWhiteListUSBVidPid ) ){
                        
                        this->whiteListWatermark = currentWatermark;
                        
                        updateWhiteListProperty = true;
                        
                        oldAcl = this->whiteListState.acl;
                        
                        if( inWhiteList ){
                            
                            assert( kDldWhiteListUSBVidPid == wlType || kDldWhiteListUSBUid == wlType );
                            
                            //
                            // N.B. the values are set so that if there is a code
                            // which doesn't honor the lock the possibility
                            // of error because of a race condition will be reduced
                            //
                            
                            //
                            // at first save the acl as it never checked if the device is not whitelisted
                            //
                            this->whiteListState.acl = acl;
                            acl = NULL; // the ownership has been transferred
                            
                            //
                            // propagate flag must be set first ( though this is not an issue
                            // on the processing in the start() routine as the device is not marked
                            // as started )
                            //
                            this->whiteListState.propagateUp = propagateUp;
                            
                            //
                            // full memory barrier to prevent CPU or compiler from reodering
                            //
                            DldMemoryBarrier();
                            
                            this->whiteListState.inWhiteList = true;
                            
                        } else {
                            
                            assert( kDldWhiteListUnknown == wlType );
                            
                            this->whiteListState.inWhiteList = false;
                            this->whiteListState.propagateUp = false;
                            this->whiteListState.acl = NULL;
                        }
                        
                        this->whiteListState.type = wlType;
                        this->whiteListState.currentWLApplied = true;
                        
                    }// end if( currentWatermark == gWhiteList->getWatermark( kDldWhiteListUSBVidPid ) )
                    
                    
                }// end of the lock
                this->UnLockExclusive();
                
            }// end if( checkWhiteList && currentWatermark == gWhiteList->getWatermark() )
            
            
        } while( currentWatermark != gWhiteList->getWatermark( kDldWhiteListUSBVidPid ) );
        
    }// end of the lock
    dldService->UnLockShared();
    
    //
    // release the acl if the acl ownership has not been transferred
    //
    if( acl ){
        
        acl->release();
        acl = NULL;
    }
    
    //
    // release the old ACL
    //
    if( oldAcl ){
        
        oldAcl->release();
        oldAcl = NULL;
    }
    
    if( updateWhiteListProperty ){
        
        //
        // no need for the locking as this is only for information, so might provide a wrong information in case of contention,
        // but this is unlikely situation so using a lock here is an overkill solution
        //
        dldService->setProperty( DldStrPropertyWhiteList,   this->whiteListState.inWhiteList );
        dldService->setProperty( DldStrPropertyPropagateUp, this->whiteListState.propagateUp );
        
    }
    
    return true;
}

//--------------------------------------------------------------------

bool
DldIoUsbInterfacePropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    
    //
    // lock the descriptor if making non-atomic value change,
    // all aligned 1,2 and 4 bytes access are atomic on 32 and 64 bit Intel CPUs
    //
    
    assert( this->container );
    assert( DldObjectPopertyType_UsbInterface == this->typeDsc.type );
    assert( dldService );
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    if( !this->initialized ){
        
        this->deviceType.type.major = DLD_DEVICE_TYPE_USB;
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    dldService->LockShared();
    {// start of the lock
        
        IORegistryEntry*  objectForQuery;
        
        //
        // it is preferable to use the DldIOService object as we can control its integrity
        // and it contains the service object's properties snapshot
        //
        objectForQuery = dldService;
        
        
        //
        // get an InterfaceClass
        //
        if( 0x0 == this->usbInterfaceClass ){
            
            OSObject*  property;
            
            //
            // get the interface class
            //
            property = objectForQuery->getProperty( kUSBInterfaceClass );
            if( !property ){
                
                //
                // this might be an IOUSBDevice, try to get its device class which
                // is the same as interface class
                //
                property = objectForQuery->getProperty( kUSBDeviceClass );
            }
            
            if( property ){
                
                if( OSDynamicCast( OSNumber, property ) )
                    this->usbInterfaceClass = (OSDynamicCast(OSNumber, property))->unsigned8BitValue();
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
            }// if( property )
            
            //
            // and now get the subclass
            //
            property = objectForQuery->getProperty( kUSBInterfaceSubClass );
            if( !property ){
                
                //
                // this might be an IOUSBDevice, try to get its device subclass which
                // is the same as interface class
                //
                property = objectForQuery->getProperty( kUSBDeviceSubClass );
            }
            
            if( property ){
                
                if( OSDynamicCast( OSNumber, property ) )
                    this->usbInterfaceSubClass = (OSDynamicCast(OSNumber, property))->unsigned8BitValue();
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
            }// if( property )
            
            //
            // get the protocol
            //
            property = objectForQuery->getProperty( kUSBInterfaceProtocol );
            if( !property ){
                
                //
                // this might be an IOUSBDevice, try to get its device subclass which
                // is the same as interface class
                //
                property = objectForQuery->getProperty( kUSBDeviceProtocol );
            }
            
            if( property ){
                
                if( OSDynamicCast( OSNumber, property ) )
                    this->usbInterfaceProtocol = (OSDynamicCast(OSNumber, property))->unsigned8BitValue();
                
                DLD_DBG_MAKE_POINTER_INVALID( property );
            }// if( property )
        }
        
    }// end of the lock
    dldService->UnLockShared();
    
    //
    // the following code is only for real interface descriptors
    //
    if( kDldObjectPopertyTypeFlavor_Primary == this->typeDsc.flavor ){
        
        //
        // find the underlying IOUSBDevice property ( i.e. the property for the whole device
        // to which different interfaces are attached ),
        // skip a fake ancillary interface from consideration
        //
        if( NULL == this->usbDeviceProperty ){
            
            //
            // traverse the parent properties to find IOUSBDevice property,
            // the lock must be acquired exclusive as the found property
            // will be referenced so we have to protect ourself 
            //
            dldService->LockExclusive();
            {// start of the lock
                
                const OSArray* parenProperties = dldService->getParentProperties();
                if( parenProperties && NULL == this->usbDeviceProperty ){
                    
                    for( int i = 0x0; i < parenProperties->getCount(); ++i ){
                        
                        DldObjectPropertyEntry*  entry = OSDynamicCast( DldObjectPropertyEntry,
                                                                        parenProperties->getObject(i) );
                        
                        assert( entry );
                        
                        if( !entry->dataU.property || DldObjectPopertyType_UsbDevice != entry->dataU.property->typeDsc.type )
                            continue;
                        
                        this->usbDeviceProperty = entry;
                        entry->retain();
                        
                        //
                        // account for this interface
                        //
                        OSIncrementAtomic( &entry->dataU.usbDeviceProperty->numberOfInterfaces );
                        
                        break;
                        
                    }// end for
                    
                }// end if
                
            }// end of the lock
            dldService->UnLockExclusive();
            
            assert( !( dldService->getParentProperties() && NULL == this->usbDeviceProperty ) );
            
        }// end if( NULL == this->dataU.usbInterfaceProperty.usbDeviceProperty )
        
        
        if( this->usbDeviceProperty ){
            
            bool checkSecurityAtIOUsbDevice;
            
            assert( kDldObjectPopertyTypeFlavor_Primary == this->typeDsc.flavor );
            
            //
            // for a complete list of USB interface classes see http://www.usb.org/developers/defined_class
            /*
             Base   Descriptor      Description
             Class  Usage
             
             00h    Device          Use class information in the Interface Descriptors
             01h    Interface       Audio  
             02h    Both            Communications and CDC Control
             03h    Interface       HID (Human Interface Device)
             05h    Interface       Physical
             06h    Interface       Image
             07h    Interface       Printer
             08h    Interface       Mass Storage
             09h    Device          Hub
             0Ah    Interface       CDC-Data
             0Bh    Interface       Smart Card
             0Dh    Interface       Content Security
             0Eh    Interface       Video
             0Fh    Interface       Personal Healthcare
             DCh    Both            Diagnostic Device
             E0h    Both            Wireless Controller( including Bluetooth, depends on subclass and protocol bits )
             EFh    Both            Miscellaneous
             FEh    Interface       Application Specific
             FFh    Both            Vendor Specific
             */
            //
            switch( this->usbInterfaceClass ){
                    
                //case kUSBHIDInterfaceClass: we control USB HID, only Apple USB HIDs's are not controleed, see DldApplySecurityQuirks
                case kUSBMassStorageInterfaceClass:// controlled at FSD level
                case 0x9:// a hub, we NEVER control hubs
                    checkSecurityAtIOUsbDevice = false;
                    break;
                    
                default:
                    checkSecurityAtIOUsbDevice = true;
                    break;
            }
            //
            // the real check is never done at this level as the direct call
            // to the controller is used but the checkSecurity value
            // from interface is used in case of a device
            // with multiple interfaces
            //
            this->checkSecurity = checkSecurityAtIOUsbDevice;
            
            //
            // if there is at least one interface which must be controlled on the IOUsbDevice
            // level then all device's interfacess fall in this class, but do this mapping
            // only for the started USB interfaces to not use an interface which doesn't have
            // all properties set thus distorting the behaviour
            //
            if( checkSecurityAtIOUsbDevice &&
                ( kDldPnPStateStarted == this->pnpState || kDldPnPStateReadyToStart == this->pnpState ) ){
                
                this->usbDeviceProperty->dataU.property->checkSecurity = checkSecurityAtIOUsbDevice;
            }
            
        }
        
        //
        // get the interface's endpoint descriptors for real inerfaces
        //
        if( NULL == this->endpoints ){
            
            IOService*  service = dldService->getSystemServiceRef();
            assert( service );
            if( service ){
                
                IOUSBInterface*    interface;
                
                assert( service->metaCast( "IOUSBInterface" ) );
                
                if( service->metaCast( "IOUSBInterface" ) ){
                    
                    interface = (IOUSBInterface*)service;
                    
                } else {
                    
                    interface = NULL;
                    DBG_PRINT_ERROR(("service->metaCast( \"IOUSBInterface\" ) failed\n"));
                }
                
                
                if( interface ){
                    
                    unsigned int                count = 0x0;
                    IOUSBFindEndpointRequest	request = { 0x0 };
                    
                    request.type      = kUSBAnyType;
                    request.direction = kUSBAnyDirn;
                    
                    //
                    // get a list of endpoint descriptors for the interface
                    //
                    for( IOUSBPipe* pipe = interface->FindNextPipe( NULL, &request );
                        NULL != pipe;
                        pipe = interface->FindNextPipe( pipe, &request ), ++count ){
                        
                        //
                        // the request struct is updated by FindNextPipe
                        //
                        request.type      = kUSBAnyType;
                        request.direction = kUSBAnyDirn;
                        
                    }// end while
                    
                    
                    if( 0x0 != count ){
                        
#if defined( DBG )
                        //__asm__ volatile( "int $0x3" );
#endif
                        IOUSBController::Endpoint* endpoints;
                        
                        endpoints = (IOUSBController::Endpoint*)IOMalloc( sizeof( endpoints[0] )*count );
                        assert( endpoints );
                        if( endpoints ){
                            
                            bzero( endpoints, sizeof( endpoints[0] )*count );
                            
                            unsigned int validEntriesCount = 0x0;
                            
                            for( IOUSBPipe* pipe = interface->FindNextPipe( NULL, &request );
                                (validEntriesCount < count) && NULL != pipe;
                                pipe = interface->FindNextPipe( pipe, &request ), ++validEntriesCount ){
                                
                                assert( pipe->GetEndpoint() );
                                memcpy( &endpoints[validEntriesCount], pipe->GetEndpoint(), sizeof( endpoints[0] ) );
                                
                                //
                                // the request struct is updated by FindNextPipe
                                //
                                request.type      = kUSBAnyType;
                                request.direction = kUSBAnyDirn;
                            }
                            
                            assert( validEntriesCount <= count );
                            
                            bool   free = false;
                            
                            //
                            // save the collected endpoints info
                            //
                            this->LockExclusive();
                            {// start of the lock
                                
                                if( NULL == this->endpoints ){
                                    
                                    this->endpointsArrayEntriesCount = count;
                                    this->endpointsArrayValidEntriesCount = validEntriesCount;
                                    this->endpoints = endpoints;
                                    
                                } else {
                                    
                                    free = true;
                                }
                            }// end of the lock
                            this->UnLockExclusive();
                            
                            assert( NULL != this->endpoints );
                            assert( 0x0 != this->endpointsArrayEntriesCount );
                            assert( this->endpointsArrayValidEntriesCount <= this->endpointsArrayEntriesCount );
                            
                            if( free ){
                                
                                assert( this->endpoints != endpoints );
                                IOFree( endpoints, sizeof( endpoints[0] )*count );
                            }
                            
                        } else {
                            
                            DBG_PRINT_ERROR(("IOMalloc( sizeof( endpoints[0] ) * %u ) failed\n", count ));
                        }
                        
                    }// end if( 0x0 != count )
                    
                } // end if( interface )
                
                dldService->putSystemServiceRef( service );
                
            } else { // end if( service )
                
                DBG_PRINT_ERROR(("dldService->getSystemServiceRef() failed\n"));
            }
            
        }// end if( NULL == this->dataU.usbInterfaceProperty->endpoints )
        
    } else {// for if( kDldObjectPopertyTypeFlavor_Primary == this->dataU.usbInterfaceProperty->typeDsc.flavor )
        
        //
        // this is an ancillary descriptor,
        // set a back pointer to the primary IOUSBDevice descriptor
        //
        assert( kDldObjectPopertyTypeFlavor_Ancillary == this->typeDsc.flavor );
        
        if( NULL == this->usbDeviceProperty ){
            
            dldService->LockShared();
            {// start of the lock
                
                if( NULL == this->usbDeviceProperty ){
                    
                    assert( dldService->getObjectProperty() &&
                            kDldObjectPopertyTypeFlavor_Primary == dldService->getObjectProperty()->dataU.property->typeDsc.flavor &&
                            DldObjectPopertyType_UsbDevice == dldService->getObjectProperty()->dataU.property->typeDsc.type );
                    
                    this->usbDeviceProperty = dldService->getObjectProperty();
                    //
                    // reference it, at first glance there is a refernce loop, and it is correct,
                    // but the loop will be broken by didTerminate(), detach() or finalize()
                    //
                    if( this->usbDeviceProperty )
                        this->usbDeviceProperty->retain();
                }
                
            }// end of the lock
            dldService->UnLockShared();
        }
        
    }// end else for if( kDldObjectPopertyTypeFlavor_Primary == this->dataU.usbInterfaceProperty->typeDsc.flavor )
    
    return true;
}

//--------------------------------------------------------------------

DldIoUsbInterfacePropertyData::~DldIoUsbInterfacePropertyData()
{
    if( this->usbDeviceProperty )
        this->usbDeviceProperty->release();
    
    if( this->endpoints ){
        
        assert( this->endpointsArrayEntriesCount > 0x0 );
        
        IOFree( this->endpoints, this->endpointsArrayEntriesCount * sizeof( this->endpoints[ 0 ] ) );
        
    }// end if( this->endpoints )
    
}

//--------------------------------------------------------------------

bool
DldSerialPropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    
    assert( dldService );
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    if( !this->initialized ){
        
        this->checkSecurity = true;
        
        //
        // lock the descriptor if making non-atomic value change,
        // all aligned 1,2 and 4 bytes access are atomic on 32 and 64 bit Intel CPUs
        //
        
        assert( DldObjectPopertyType_Serial == this->typeDsc.type );
        
        this->deviceType.type.major = DLD_DEVICE_TYPE_SERIAL;
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//--------------------------------------------------------------------

bool
DldIOSCSIPeripheralDeviceType05PropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{    
    assert( kDldPnPStateReadyToStart == dldService->getPnPState() ||
            kDldPnPStateStarted == dldService->getPnPState() );
    
    if( !this->initialized ){
        
        this->checkSecurity = true;
        this->deviceType.type.major = DLD_DEVICE_TYPE_CD_DVD;
        
        //
        // the type is hardcoded here ( for USB it dynamicaly changes as
        // device moves from one WL to another ) as the type is used
        // to find a properties which can be whitelisted by the CD/DVD whitelist
        //
        this->whiteListState.type = kDldWhiteListDVDUid;
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//-------------------------------------------------------------------

bool
DldIoBluetoothDevicePropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    //
    // nothing to do
    //
    if( !this->initialized ){
        
        this->deviceType.type.major = DLD_DEVICE_TYPE_BLUETOOTH;
        
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//-------------------------------------------------------------------

bool
DldIOHIDSystemPropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    //
    // nothing to do
    //
    if( !this->initialized ){
        
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//-------------------------------------------------------------------

bool
DldAppleMultitouchDeviceUserClientPropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    //
    // nothing to do
    //
    if( !this->initialized ){
        
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//-------------------------------------------------------------------

bool
DldNetIOInterfacePropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    bool attachFilter;
    
    assert( preemption_enabled() );
    
    //
    // nothing to do
    //
    if( !this->initialized ){
        
        DldObjectPropertyEntry*   wifiPropertyObj;
        
        wifiPropertyObj = dldService->retrievePropertyByTypeRef( DldObjectPropertyType_Net80211Interface );
        assert( !( wifiPropertyObj && this->container != wifiPropertyObj ) );
        
        this->deviceType.type.major = wifiPropertyObj ? DLD_DEVICE_TYPE_WIFI : DLD_DEVICE_TYPE_UNKNOWN;
        this->interfaceFilterState = kNetInterfaceFilterNotRegisterd;
        this->initialized = true;
        
        if( wifiPropertyObj )
            wifiPropertyObj->release();
    }
    
    //
    // the call also sets this->deviceType.type.minor
    //
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    //
    // if the state is kDldPnPStateReadyToStart then the related IOService object has been started
    // and has been initialized, see DldHookerCommonClass2<CC,HC>::start_hook, so it is safe to query
    // its properties and attach a filter to BSD network interface
    //
    if( kNetInterfaceFilterRegisterd != this->interfaceFilterState &&
        ( kDldPnPStateReadyToStart == dldService->getPnPState() || kDldPnPStateStarted == dldService->getPnPState() ) ){
        
#if defined (DBG)
        DldObjectPropertyEntry*   usbPropertyObj;
        
        usbPropertyObj  = dldService->retrievePropertyByTypeRef( DldObjectPopertyType_UsbDevice );
        assert( !( usbPropertyObj && DLD_DEVICE_TYPE_USB != this->deviceType.type.minor ) );
#endif // DBG
        
        //
        // the filter is attached to USB and WiFi to not interfere with a PCI Ethernet Card
        //
        attachFilter = ( DLD_DEVICE_TYPE_WIFI == this->deviceType.type.major ) ||
                       ( DLD_DEVICE_TYPE_USB == this->deviceType.type.minor );
        
        if( attachFilter ){
            
            //
            // attach an interface filter
            //
            if( OSCompareAndSwap( kNetInterfaceFilterNotRegisterd,
                                  kNetInterfaceFilterBeingRegisterd,
                                  &this->interfaceFilterState ) ){
                
                this->interfaceFilter = DldNetworkInterfaceFilter::withNetworkInterface( dldService );
                //assert( this->interfaceFilter ); allowed to fail, see comments in withNetworkInterface()
                if( this->interfaceFilter ){
                    
                    errno_t   error;
                    
                    //
                    // attach
                    //
                    error = this->interfaceFilter->attachToNetworkInterface();
                    assert( !error );
                    if( KERN_SUCCESS == error ){
                        
                        DLD_COMM_LOG(COMMON,("interfaceFilter->attachToNetworkInterface() attached to %p\n", dldService));
                        //
                        // no need to use an atomic operation as the code is protected by OSCompareAndSwap
                        // and the kNetInterfaceFilterBeingRegisterd filter's state can be changed only
                        // by a thread that set it before
                        //
                        this->interfaceFilterState = kNetInterfaceFilterRegisterd;
                        
                    } else {
                        
                        DBG_PRINT_ERROR(("interfaceFilter->attachToNetworkInterface() failed for %p with an error %d\n", dldService, error));
                        
                        this->interfaceFilterState = kNetInterfaceFilterNotRegisterd;
                        
                        //
                        // delete the interface object
                        //
                        this->interfaceFilter->release();
                        this->interfaceFilter = NULL;
                    }
                    
                } else {
                    
                    this->interfaceFilterState = kNetInterfaceFilterNotRegisterd;
                }
                
            } // end if( OSAtomicCompareAndSwap32()
            
            // allowed to fail, see comments in withNetworkInterface()
            //assert( kNetInterfaceFilterRegisterd == this->interfaceFilterState );
        } // if( attachFilter )
        
#if defined (DBG)
        if( usbPropertyObj )
            usbPropertyObj->release();
#endif // DBG
        
    } // end if( kNetInterfaceFilterRegisterd != this->interfaceFilterState && ... )
    
    return true;
}

//-------------------------------------------------------------------

DldNetIOInterfacePropertyData::~DldNetIOInterfacePropertyData()
{
    if( this->interfaceFilter )
        this->interfaceFilter->release();
}

//-------------------------------------------------------------------

bool
DldNet80211InterfacePropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    bool filledSuccessfully;
    
    //
    // do not set this->initialized to true before calling the super class initialization
    // as the super class checks this value and if it is true some initialization is skipped,
    // this is in contrast to calling ldPropertyCommonData::fillPropertyData that is usually
    // called with the value set to true
    //
    filledSuccessfully = this->DldNetIOInterfacePropertyData::fillPropertyData( dldService, forceUpdate );
    if( filledSuccessfully && !this->initialized )
        this->initialized = true;
    
    return filledSuccessfully;
}

//-------------------------------------------------------------------

bool DldIOFireWireDevicePropertyData::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    //
    // nothing to do
    //
    if( !this->initialized ){
        
        this->deviceType.type.major = DLD_DEVICE_TYPE_IEEE1394;
        this->initialized = true;
    }
    
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//-------------------------------------------------------------------

bool DldIOFireWireSBP2DeviceProperty::fillPropertyData(
    __in DldIOService* dldService,
    __in bool forceUpdate
    )
{
    //
    // nothing to do
    //
    this->initialized = true;
    this->DldPropertyCommonData::fillPropertyData( dldService, forceUpdate );
    
    return true;
}

//-------------------------------------------------------------------

#define MEDIA_UID_REQUEST_SIGNATURE   0xABCDBEEF

typedef struct _MEDIA_UID_REQUEST{
    ULONG                     Signature;
    IOReturn                  RC;
    DldObjectPropertyEntry*   ioSCSIPeripheralType05PropertyEntry;
    IOService*                mediaService;
    CDDVDDiskID*              diskID;
    UInt32                    completionEvent;
} MEDIA_UID_REQUEST;

void
DldObjectPropertyEntry::getMediaUID_WR( void* _mediaUidRequest )
{
    MEDIA_UID_REQUEST*  mediaUidRequest = (MEDIA_UID_REQUEST*)_mediaUidRequest;
    
    assert( MEDIA_UID_REQUEST_SIGNATURE == mediaUidRequest->Signature );
    assert( DldObjectPopertyType_SCSIPeripheralDeviceType05 == mediaUidRequest->ioSCSIPeripheralType05PropertyEntry->dataU.property->typeDsc.type );
    
    //
    // remember the current thread, as for the case of newUser() invokation
    // for the IOSCSIPeripheralDeviceType05 class the attached object inherited
    // from IOCDBlockStorageDevice will be used to issue commands ( as IOSCSIPeripheralDeviceType05
    // can't be used as it suppose that the user client SCSITaskUserClient calss
    // issues asynchronous read requests and imlicitly uses the user class callback
    // for completion, the synchronous read is not implemented ), so there will be
    // the recursion with a stack showed below when the check was not made and
    // the assert failed ( in release there would be a deadlock )
    //
    /*
     this the old stack taken before the call was moved to a separate thread
     #0  Debugger (message=0x6c0c9c "panic") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/AT386/model_dep.c:845
     #1  0x00223792 in panic (str=0x6731e8 "%s:%d Assertion failed: %s") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/debug.c:294
     #2  0x0022353f in Assert (file=0x465b3f10 "/work/DeviceLockProject/DeviceLockIOKitDriver/DldObjectProperty.cpp", line=79, expression=0x465ac2b8 "current_thread() != this->exclusiveThread") at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/debug.c:160
     #3  0x46569c56 in DldObjectPropertyEntry::LockExclusive (this=0x6b15840) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldObjectProperty.cpp:79
     #4  0x4656b430 in DldObjectPropertyEntry::initDVDMediaWL (this=0x6b15840, dldMediaIOService=0x6023bc0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldObjectProperty.cpp:1686
     #5  0x46571f35 in DldApplySecurityQuirks (dldIOService=0x6023bc0, param=0x31b6a8f0, accesEval=0x31b6a718) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKernAuthorization.cpp:1127
     #6  0x46572da5 in isAccessAllowed (param=0x31b6a8f0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKernAuthorization.cpp:1463
     #7  0x465975b1 in DldIOSCSIProtocolInterface::ExecuteCommand (this=0x6af1c44, service=0x57b7000, request=0xb751500) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldIOSCSIProtocolInterface.cpp:106
     #8  0x4659e125 in IOSCSIPeripheralDeviceType05DldHook<(_DldInheritanceDepth)0>::ExecuteCommand_hook (this=0x57b7000, request=0xb751500) at IOSCSIPeripheralDeviceType05DldHook.h:161
     #9  0x0148fdf5 in ?? ()
     #10 0x015edaa6 in ?? ()
     #11 0x015f0a5a in ?? ()
     #12 0x465a8f42 in DldDVDWhiteList::WIN_ReadTOC (this=0x6b6c410, device=0x57d4d80, formatAsTime=0 '\0', toc=0x6fa3400, actualByteCount=0x31b6abbc) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldDVDWhiteList.cpp:437
     #13 0x465a67e1 in DldDVDWhiteList::WIN_ProcessTocAndConvertToLbaInLittleEndian (this=0x6b6c410, device=0x57d4d80, CdromToc=0x6fa3400, DiskFlags=0x31b6adac) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldDVDWhiteList.cpp:277
     #14 0x465a6d6f in DldDVDWhiteList::initCDDVDDiskID (this=0x6b6c410, device=0x57d4d80, diskID=0x31b6aecc) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldDVDWhiteList.cpp:649
     #15 0x465a9d95 in DldDVDWhiteList::getMediaUID (this=0x6b6c410, media=0x57b7000, outDiskID=0x31b6afa8) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldDVDWhiteList.cpp:1376
     #16 0x4656b468 in DldObjectPropertyEntry::initDVDMediaWL (this=0x6b15840, dldMediaIOService=0x6023bc0) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldObjectProperty.cpp:1699
     #17 0x46571f35 in DldApplySecurityQuirks (dldIOService=0x6023bc0, param=0x31b6b300, accesEval=0x31b6b158) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKernAuthorization.cpp:1127
     #18 0x46572da5 in isAccessAllowed (param=0x31b6b300) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldKernAuthorization.cpp:1463
     #19 0x4652d722 in DldHookerCommonClass::newUserClient1 (this=0x6da4408, serviceObject=0x57d4d80, owningTask=0x63845dc, securityID=0x63845dc, type=12, properties=0x0, handler=0x31b6bd08) at /work/DeviceLockProject/DeviceLockIOKitDriver/DldHookerCommonClass.cpp:1621
     #20 0x46511351 in DldHookerCommonClass2<IOServiceDldHook2<(_DldInheritanceDepth)4>, IOService>::newUserClient1_hook (this=0x57d4d80, owningTask=0x63845dc, securityID=0x63845dc, type=12, properties=0x0, handler=0x31b6bd08) at DldHookerCommonClass2.h:1608
     #21 0x0064ae67 in is_io_service_open_extended (_service=0x57d4d80, owningTask=0x63845dc, connect_type=12, ndr={mig_vers = 0 '\0', if_vers = 0 '\0', reserved1 = 0 '\0', mig_encoding = 0 '\0', int_rep = 1 '\001', char_rep = 0 '\0', float_rep = 0 '\0', reserved2 = 0 '\0'}, properties=0x0, propertiesCnt=0, result=0x5f6aeb8, connection=0x31b6bd80) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/iokit/Kernel/IOUserClient.cpp:2395
     #22 0x002c1bbb in _Xio_service_open_extended (InHeadP=0x629eb68, OutHeadP=0x5f6ae84) at device/device_server.c:14130
     #23 0x00226d74 in ipc_kobject_server (request=0x629eb00) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/kern/ipc_kobject.c:339
     #24 0x002126b1 in ipc_kmsg_send (kmsg=0x629eb00, option=0, send_timeout=0) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/ipc_kmsg.c:1371
     #25 0x0021e193 in mach_msg_overwrite_trap (args=0x54bd708) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:505
     #26 0x0021e37d in mach_msg_trap (args=0x54bd708) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/ipc/mach_msg.c:572
     #27 0x002d8b06 in mach_call_munger64 (state=0x54bd704) at /work/Mac_OS_X_kernel/10_6_4/xnu-1504.7.4/osfmk/i386/bsd_i386.c:765                     
     */
    assert( NULL == mediaUidRequest->ioSCSIPeripheralType05PropertyEntry->dataU.ioSCSIPeripheralType05Property->currentUidRetrievalThread );
    
    mediaUidRequest->ioSCSIPeripheralType05PropertyEntry->dataU.ioSCSIPeripheralType05Property->currentUidRetrievalThread = current_thread();
    {
        mediaUidRequest->RC = gCDDVDWhiteList->getMediaUID( mediaUidRequest->mediaService, mediaUidRequest->diskID );
    }
    mediaUidRequest->ioSCSIPeripheralType05PropertyEntry->dataU.ioSCSIPeripheralType05Property->currentUidRetrievalThread = NULL;
    
    DldSetNotificationEvent( &mediaUidRequest->completionEvent );
    DLD_DBG_MAKE_POINTER_INVALID( mediaUidRequest);
}

//
// a very slow function, blocks up to dozens of seconds!
//
IOReturn
DldObjectPropertyEntry::initDVDMediaWL( __in DldIOService* dldMediaIOService )
{
    
    assert( preemption_enabled() );
    assert( dldMediaIOService );
    
    if( this->dataU.property->whiteListState.currentWLApplied &&
       this->dataU.ioSCSIPeripheralType05Property->uidValid )
        return KERN_SUCCESS;
    
    if( DldObjectPopertyType_SCSIPeripheralDeviceType05 != this->dataU.property->typeDsc.type ){
        
        assert( !"an attempt to apply initDVDMediaWL to a wrong type" );
        DBG_PRINT_ERROR(( "an attempt to apply initDVDMediaWL to a wrong type %s \n", 
                          DldPropertyTypeToString( this->dataU.property->typeDsc.type ) ));
        
        return kIOReturnBadArgument;
    }
    
    //
    // authorize the CD/DVD media
    //
    IOReturn      RC = KERN_SUCCESS;
    
    UInt32 currentWatermark;
    currentWatermark = gWhiteList->getWatermark( kDldWhiteListDVDUid );
    
    assert( gCDDVDWhiteList );
    
    if( !this->dataU.ioSCSIPeripheralType05Property->uidValid ){
        
        //
        // calculate the UID
        //
        IOService*  mediaServiceRef;
        
        mediaServiceRef = dldMediaIOService->getSystemServiceRef();
        assert( mediaServiceRef );
        
        if( mediaServiceRef ){
            
            //
            // the exclusive lock is used to avoid a stampede
            // if several concurrent threads try to get access,
            // should there be problems with deadlocking a
            // specially dedicated lock must be introduced
            //
            this->LockExclusive();
            {// start of the lock
                
                if( !this->dataU.ioSCSIPeripheralType05Property->uidValid ){
                    
                    //
                    // calculate UID
                    //
                    CDDVDDiskID   diskID;
                    
                    assert( sizeof( diskID ) == 16 );
                    assert( NULL == this->dataU.ioSCSIPeripheralType05Property->currentUidRetrievalThread );
                    
                    _MEDIA_UID_REQUEST   mediaUidRequest = {0x0};

                    mediaUidRequest.Signature = MEDIA_UID_REQUEST_SIGNATURE;
                    mediaUidRequest.ioSCSIPeripheralType05PropertyEntry = this;
                    mediaUidRequest.mediaService = mediaServiceRef;
                    mediaUidRequest.diskID = &diskID;
                    DldInitNotificationEvent( &mediaUidRequest.completionEvent );
                    
                    //
                    // run in the separate thread as gCDDVDWhiteList->getMediaUID consumes a good portion
                    // of the thread stack and results in the stack depletion resulting in a kernel panic
                    /*
                     0xffffff803ea2dec0 : 0xffffff800a01d626 mach_kernel : _panic + 0xc6
                     0xffffff803ea2df30 : 0xffffff800a0b83c9 mach_kernel : _panic_64 + 0x1f9
                     0xffffff803ea2e0a0 : 0xffffff800a0ceddf mach_kernel : _hndl_double_fault + 0xf
                     0xffffff8049710340 : 0xffffff7f8c42dab0 com.devicelock.agent.driver : __Z19DldKauthAclEvaluateP14_DldAccessEvalP12DldAclObject + 0x140
                     0xffffff8049710410 : 0xffffff7f8c42ed67 com.devicelock.agent.driver : __ZN23DldTypePermissionsArray15isAccessAllowedEP14_DldAccessEval + 0xcb
                     0xffffff8049710440 : 0xffffff7f8c43018c com.devicelock.agent.driver : __Z15isAccessAllowedP20_DldAccessCheckParam + 0x13fc
                     0xffffff8049710960 : 0xffffff7f8c3f9174 com.devicelock.agent.driver : __ZN26DldIOSCSIProtocolInterface14ExecuteCommandEP23IOSCSIProtocolInterfaceP8OSObject + 0x654
                     0xffffff80497111f0 : 0xffffff7f8c491d04 com.devicelock.agent.driver : __ZN35IOSCSIPeripheralDeviceType05DldHookIL20_DldInheritanceDepth0EE19ExecuteCommand_hookEP8OSObject + 0x26a
                     0xffffff8049711290 : 0xffffff7f8a66fb80 com.apple.iokit.IOSCSIArchitectureModelFamily : __ZN27IOSCSIPrimaryCommandsDevice11SendCommandEP8OSObjectj + 0x126
                     0xffffff80497112d0 : 0xffffff7f8b1e4acd com.apple.iokit.IOSCSIMultimediaCommandsDevice : __ZN30IOSCSIMultimediaCommandsDevice7ReadTOCEP18IOMemoryDescriptorhhjPt + 0x169
                     0xffffff8049711350 : 0xffffff7f8b1ea5dd com.apple.iokit.IOSCSIMultimediaCommandsDevice : __ZN13IODVDServices7readTOCEP18IOMemoryDescriptorhhhPt + 0x69
                     0xffffff8049711390 : 0xffffff7f8c30289c com.devicelock.agent.driver : __ZN15DldDVDWhiteList11WIN_ReadTOCEP22IOCDBlockStorageDevicehP10_CDROM_TOCPt + 0x108
                     0xffffff80497114d0 : 0xffffff7f8c2ff5a8 com.devicelock.agent.driver : __ZN15DldDVDWhiteList43WIN_ProcessTocAndConvertToLbaInLittleEndianEP22IOCDBlockStorageDeviceP10_CDROM_TOCPj + 0xa0
                     0xffffff80497115f0 : 0xffffff7f8c2ffda2 com.devicelock.agent.driver : __ZN15DldDVDWhiteList15initCDDVDDiskIDEP22IOCDBlockStorageDeviceP13_DldDeviceUID + 0x152
                     0xffffff8049711c30 : 0xffffff7f8c30476a com.devicelock.agent.driver : __ZN15DldDVDWhiteList11getMediaUIDEP9IOServiceP13_DldDeviceUID + 0x8d2
                     0xffffff8049711e80 : 0xffffff7f8c43886c com.devicelock.agent.driver : __ZN22DldObjectPropertyEntry14initDVDMediaWLEP12DldIOService + 0x462
                     0xffffff8049711f80 : 0xffffff7f8c42e7d3 com.devicelock.agent.driver : __Z22DldApplySecurityQuirksP12DldIOServiceP20_DldAccessCheckParamP14_DldAccessEvalP16_WhiteListStatus + 0x8a3
                     0xffffff8049712030 : 0xffffff7f8c4301f1 com.devicelock.agent.driver : __Z15isAccessAllowedP20_DldAccessCheckParam + 0x1461
                     0xffffff8049712550 : 0xffffff7f8c3f9174 com.devicelock.agent.driver : __ZN26DldIOSCSIProtocolInterface14ExecuteCommandEP23IOSCSIProtocolInterfaceP8OSObject + 0x654
                     0xffffff8049712de0 : 0xffffff7f8c491d04 com.devicelock.agent.driver : __ZN35IOSCSIPeripheralDeviceType05DldHookIL20_DldInheritanceDepth0EE19ExecuteCommand_hookEP8OSObject + 0x26a
                     0xffffff8049712e80 : 0xffffff7f8b1e830d com.apple.iokit.IOSCSIMultimediaCommandsDevice : __ZN30IOSCSIMultimediaCommandsDevice9IssueReadEP18IOMemoryDescriptorPvyy + 0xad
                     0xffffff8049712ef0 : 0xffffff7f8b1b8c66 com.apple.iokit.IOCDStorageFamily : __ZN22IOCDBlockStorageDriver14executeRequestEyP18IOMemoryDescriptorP19IOStorageAttributesP19IOStorageCompletionPN20IOBlockStorageDriver7ContextE + 0x16a
                     0xffffff8049712f60 : 0xffffff7f8a582b42 com.apple.iokit.IOStorageFamily : __ZN20IOBlockStorageDriver14prepareRequestEyP18IOMemoryDescriptorP19IOStorageAttributesP19IOStorageCompletion + 0x11c
                     0xffffff8049712fc0 : 0xffffff7f8a5883b3 com.apple.iokit.IOStorageFamily : __ZL11dkreadwritePv9dkrtype_t + 0x4f1
                     0xffffff8049713060 : 0xffffff7f8b9f33f4 com.apple.BootCache : _BC_strategy + 0xd09
                     0xffffff8049713190 : 0xffffff800a120985 mach_kernel : _spec_strategy + 0x265
                     0xffffff80497131f0 : 0xffffff800a0dbd11 mach_kernel : _buf_strategy + 0x3c1
                     0xffffff80497132e0 : 0xffffff7f8c9b8b77 com.apple.filesystems.udf : __ZN7UDFNode12VnopStrategyEP18vnop_strategy_args + 0xcd
                     0xffffff8049713460 : 0xffffff7f8c9a760e com.apple.filesystems.udf : __ZL12udf_strategyP18vnop_strategy_args + 0x285
                     0xffffff80497134e0 : 0xffffff7f8c44b214 com.devicelock.agent.driver : __Z19DldFsdStrategytHookP18vnop_strategy_args + 0xe4
                     0xffffff8049713520 : 0xffffff800a113a5e mach_kernel : _VNOP_STRATEGY + 0x2e
                     0xffffff8049713550 : 0xffffff800a0dd69c mach_kernel : _buf_bread + 0x9c
                     0xffffff8049713580 : 0xffffff800a0dd7f7 mach_kernel : _buf_meta_bread + 0x17
                     0xffffff80497135a0 : 0xffffff7f8c9abfef com.apple.filesystems.udf : __ZN8UDFBlock9ReadBlockEP8UDFMountP5vnodexjb19UDFBlockReleaseHint + 0x3f
                     0xffffff80497135c0 : 0xffffff7f8c9ac1ea com.apple.filesystems.udf : __ZN8UDFBlock17ReadFileFromVnodeEP5vnodex19UDFBlockReleaseHintb + 0x6a
                     0xffffff8049713610 : 0xffffff7f8c9b927f com.apple.filesystems.udf : __ZN7UDFNode8VnopReadEP3uioi + 0x1cf
                     0xffffff80497136a0 : 0xffffff7f8c9ba254 com.apple.filesystems.udf : __ZN7UDFNode4ReadExmPh + 0x48
                     0xffffff80497136d0 : 0xffffff7f8c9ad0f3 com.apple.filesystems.udf : __ZN22UDFVarSizeDataIterator12ReadCurEntryEv + 0x6f
                     0xffffff8049713700 : 0xffffff7f8c9bab76 com.apple.filesystems.udf : __ZN7UDFNode15ExtendedReadDirEP3uioPiS2_bbPKcbS4_mbPm + 0x430
                     0xffffff8049713920 : 0xffffff7f8c9c211d com.apple.filesystems.udf : __ZN10UDFDirNode11VnopReadDirEP3uioPiS2_ + 0x77
                     0xffffff8049713980 : 0xffffff7f8c9a72b0 com.apple.filesystems.udf : __ZL11udf_readdirP17vnop_readdir_args + 0x61
                     0xffffff80497139a0 : 0xffffff800a11334e mach_kernel : _VNOP_READDIR + 0x3e
                     0xffffff80497139f0 : 0xffffff800a10194c mach_kernel : _getdirentries + 0x3bc
                     0xffffff8049713f20 : 0xffffff800a101c7a mach_kernel : _getdirentries64 + 0x5a
                     0xffffff8049713f50 : 0xffffff800a3e182a mach_kernel : _unix_syscall64 + 0x20a
                     0xffffff8049713fb0 : 0xffffff800a0ced33 mach_kernel : _hndl_unix_scall64 + 0x13
                     Kernel Extensions in backtrace:
                     com.apple.iokit.IOStorageFamily(1.8)[A3CC4E44-8E10-3D9A-BA8E-95743E79D125]@0xffffff7f8a57c000->0xffffff7f8a5a0fff
                     com.apple.iokit.IOSCSIArchitectureModelFamily(3.5.1)[4CCC048B-060B-3C07-8D85-A84CEA02CD25]@0xffffff7f8a66d000->0xffffff7f8a697fff
                     com.apple.iokit.IOCDStorageFamily(1.7.1)[17C8A086-3427-3010-982E-CA78B78EF4BF]@0xffffff7f8b1b7000->0xffffff7f8b1c4fff
                     dependency: com.apple.iokit.IOStorageFamily(1.8)[A3CC4E44-8E10-3D9A-BA8E-95743E79D125]@0xffffff7f8a57c000
                     com.apple.iokit.IOSCSIMultimediaCommandsDevice(3.5.1)[A06C6513-68BA-3575-8711-88C5BA095597]@0xffffff7f8b1e3000->0xffffff7f8b1fcfff
                     dependency: com.apple.iokit.IODVDStorageFamily(1.7.1)[C315781B-FD35-3CF1-8668-31C4DF5BBE97]@0xffffff7f8b1c8000
                     dependency: com.apple.iokit.IOBDStorageFamily(1.7)[CF94CB41-FFF7-3188-8652-1B0EEFF32998]@0xffffff7f8b1d6000
                     dependency: com.apple.iokit.IOStorageFamily(1.8)[A3CC4E44-8E10-3D9A-BA8E-95743E79D125]@0xffffff7f8a57c000
                     dependency: com.apple.iokit.IOSCSIArchitectureModelFamily(3.5.1)[4CCC048B-060B-3C07-8D85-A84CEA02CD25]@0xffffff7f8a66d000
                     dependency: com.apple.iokit.IOCDStorageFamily(1.7.1)[17C8A086-3427-3010-982E-CA78B78EF4BF]@0xffffff7f8b1b7000
                     com.apple.BootCache(34.0)[A98928A7-297F-38F2-BF29-7BC7EF1FEB4D]@0xffffff7f8b9f1000->0xffffff7f8b9fbfff
                     com.devicelock.agent.driver(7.2.7)[361C23C3-B3BB-0C61-0A21-9056A442E15D]@0xffffff7f8c2d8000->0xffffff7f8c9a3fff
                     dependency: com.apple.iokit.IOSCSIArchitectureModelFamily(3.5.1)[4CCC048B-060B-3C07-8D85-A84CEA02CD25]@0xffffff7f8a66d000
                     com.apple.filesystems.udf(2.3)[F5D2E2AF-05A1-3BAD-961A-A0BA73464700]@0xffffff7f8c9a4000->0xffffff7f8c9e5fff
                     */
                    //
                    thread_t  thread;
                    RC = kernel_thread_start( ( thread_continue_t ) &DldObjectPropertyEntry::getMediaUID_WR,
                                                &mediaUidRequest,
                                                &thread );
                    assert( KERN_SUCCESS == RC );
                    if( KERN_SUCCESS == RC ){
                        
                        //
                        // block waiting for the operation completion
                        //
                        DldWaitForNotificationEvent( &mediaUidRequest.completionEvent );
                        
                        RC = mediaUidRequest.RC;
                        
                        //
                        // release the thread object
                        //
                        thread_deallocate( thread );
                        thread = NULL;
                        
                        assert( kIOReturnSuccess == RC || kIOReturnNoMedia == RC ); // kIOReturnNoMedia is okay if the access was denied
                        if( kIOReturnSuccess == RC ){
                            
                            memcpy( &this->dataU.ioSCSIPeripheralType05Property->mediaUID,
                                    &diskID,
                                    sizeof(diskID) );
                            
                            this->dataU.ioSCSIPeripheralType05Property->uidValid = true;
                        }
                    }
                    
                }
            }// end of the lock
            this->UnLockExclusive();
            
            dldMediaIOService->putSystemServiceRef( mediaServiceRef );
            
        }// end if( mediaServiceRef )
        
    }// end if( !scsi05Prop->dataU.ioSCSIPeripheralType05Property->uidValid )
    
    //
    // apply the WL to the CD/DVD UID
    //
    if( this->dataU.ioSCSIPeripheralType05Property->uidValid ){
        
        do{
            
            bool                  inWhiteList;
            DldDeviceUIDEntry     entryCopy;
            DldAclObject*         acl = NULL;
            DldAclObject*         oldAcl = NULL;
            
            assert( sizeof( entryCopy.uid ) == sizeof( this->dataU.ioSCSIPeripheralType05Property->mediaUID ) );
            
            currentWatermark = gWhiteList->getWatermark( kDldWhiteListDVDUid );
            
            inWhiteList = gWhiteList->getDvdUIDEnrty( &this->dataU.ioSCSIPeripheralType05Property->mediaUID,
                                                      &entryCopy,
                                                      &acl );
            //
            // the CD/DVD is extremely slow so it is OK to acquire the lock for a long time,
            // this help to avoid a complicated synchronization with a media change code
            //
            
            this->dataU.property->LockExclusive();
            {// start of the lock
                
                //
                // check that the media is still the same
                //
                bool  mediaNotChanged;
                
                mediaNotChanged = ( 0x0 == memcmp( &entryCopy.uid,
                                                   &this->dataU.ioSCSIPeripheralType05Property->mediaUID,
                                                   sizeof( entryCopy.uid ) ) );
                
                if( !this->dataU.property->whiteListState.currentWLApplied && mediaNotChanged ){
                    
                    oldAcl = this->dataU.property->whiteListState.acl;
                    
                    if( inWhiteList )
                    {
                        //
                        // in the white list
                        //
                        //
                        // N.B. the values are set so that if there is a code
                        // which doesn't honor the lock the possibility
                        // of error because of a race condition will be reduced
                        //
                        
                        //
                        // at first save the acl as it never checked if the device is not whitelisted
                        //
                        this->dataU.property->whiteListState.acl = acl;
                        acl = NULL; // the ownership has been transferred
                        
                        //
                        // full memory barrier to prevent CPU or compiler from reodering
                        //
                        DldMemoryBarrier();
                        
                        this->dataU.property->whiteListState.inWhiteList = true;
                        
                    } else {
                        
                        //
                        // not in the white list
                        //
                        this->dataU.property->whiteListState.inWhiteList = false;
                        this->dataU.property->whiteListState.acl = NULL;
                    }
                    
                }// end if( !scsi05Prop->dataU.property->whiteListState.currentWLApplied && mediaNotChanged )
                
                assert( false == this->dataU.property->whiteListState.propagateUp );
                
            }// end of the lock
            this->dataU.property->UnLockExclusive();
            
            if( acl )
                acl->release();
            
            if( oldAcl )
                oldAcl->release();
            
        } while( currentWatermark != gWhiteList->getWatermark( kDldWhiteListDVDUid ) );
        
    }// end if( scsi05Prop->dataU.ioSCSIPeripheralType05Property->uidValid )
    
    this->dataU.property->whiteListState.currentWLApplied = ( currentWatermark == gWhiteList->getWatermark( kDldWhiteListDVDUid ) );
    
    return RC;
}

//--------------------------------------------------------------------

//
// the caller must set intData->logDataSize to the size the allocated *intData->logData,
// the caller must allocate a room for *intData->logData of at least sizeof( intData->logData->device ),
// on return the function initializes the fields for *intData->logData
// and sets intData->logDataSize according to the length of the apropriate initialized data,
// so the caller must not consider this field as a real full size of the 
// intData->logData memory which he has allocated before calling this function
//
bool DldObjectPropertyEntry::initDeviceLogData(
    __in     DldRequestedAccess* action,
    __in     DldFileOperation    operation, // a very limited use, valid only for IOMedia and its derivatives
    __in     int32_t        pid,// BSD process ID
    __in     kauth_cred_t   credential,
    __in     bool           accessDisabled,
    __inout  DldDriverDataLogInt*  intData// at least sizeof( intData->logData->device )
    )
{
    UInt32              maximumLogDataSize = intData->logDataSize;
    bool                success = true;
    static const char*  fakeOpticalDriveName = "/dev/diskXXX";
    
    assert( preemption_enabled() );
    assert( intData->logDataSize >= sizeof( intData->logData->device) );
    assert( credential );
    
    bzero( &intData->logData->device, sizeof( intData->logData->device ) );
    intData->logDataSize = 0x0;// an initial value, must be changed
    
    //
    // init the common header
    //
    
    //
    // type of the device for this log entry
    //
    intData->logData->Header.deviceType = this->dataU.property->deviceType;
    
    //
    // a user's GUID,
    // kauth_cred_getguid can return an error, disregard it
    //
    kauth_cred_getguid( credential, &intData->logData->device.Header.userGuid );
    intData->logData->Header.userUid = kauth_cred_getuid( credential );
    intData->logData->Header.userGid = kauth_cred_getgid( credential );
    
    intData->logData->Header.isDisabledByDL = accessDisabled;
    intData->logData->Header.action = *action;
    
    //
    // set the minimum allowed log data size, you can increase it for any particular type
    // while processing it in the below switch-case statement
    //
    intData->logDataSize = sizeof( intData->logData->device.Header );
    
    //
    // init per device specific fields
    //
    switch( this->dataU.property->typeDsc.type ){
            
        case DldObjectPopertyType_UsbInterface:
        case DldObjectPopertyType_UsbDevice:
        {
            DldObjectPropertyEntry* usbDeviceProperty;
            
            //
            // set the actual log data type
            //
            intData->logData->Header.type = DLD_LOG_TYPE_USB;
            assert( intData->logData->Header.type == intData->logData->device.Header.type );
            
            //
            // set the actual data size
            //
            intData->logDataSize = sizeof( intData->logData->device.USB );
            
            //
            // we need an IOUSBDevice property, the interface property doesn't contain
            // the required information
            //
            if( DldObjectPopertyType_UsbInterface == this->dataU.property->typeDsc.type )
                usbDeviceProperty = this->dataU.usbInterfaceProperty->usbDeviceProperty;
            else
                usbDeviceProperty = this;
            
            assert( usbDeviceProperty );
            if( !usbDeviceProperty ){
                
                //
                // the log is incomplete but contains enough data fo logging
                //
                DBG_PRINT_ERROR(("usbDeviceProperty is NULL\n"));
                break;
            }
            
            //
            // fill in with the USB device data
            //
            intData->logData->device.USB.vidPid = usbDeviceProperty->dataU.usbDeviceProperty->VidPid;
            intData->logData->device.USB.uid = usbDeviceProperty->dataU.usbDeviceProperty->uid;
            
            DLD_COMM_LOG( DEVICE_ACCESS, ("Device acess for USB(%s) with VID(0x%X) PID(0x%X) is %s\n",
                                           usbDeviceProperty->dataU.property->typeDsc.className,
                                           usbDeviceProperty->dataU.usbDeviceProperty->VidPid.idVendor,
                                           usbDeviceProperty->dataU.usbDeviceProperty->VidPid.idProduct,
                                           (accessDisabled ? "Diabled" : "Allowed") ) );
            
            break;
        }// end case DldObjectPopertyType_UsbDevice:
            
        case DldObjectPopertyType_IOMedia:
        case DldObjectPopertyType_IODVDMedia: // DldIODVDMediaPropertyData inherits from DldIOMediaPropertyData
        case DldObjectPopertyType_IOCDMedia:  // DldIOCDMediaPropertyData inherits from DldIOMediaPropertyData
        {
            vm_size_t   size;
            vm_size_t   nameBuffLength;
            vm_size_t   name_len;
            
            intData->logData->Header.type = DLD_LOG_TYPE_FSD;
            intData->logData->Fsd.scopeID = DLD_KAUTH_SCOPE_VNODE_ID; // we are cheating on the service!
                
            intData->logData->Fsd.operation = operation;
            
            //
            // a process' PID and a short name
            //
            intData->logData->Fsd.pid = pid;
            proc_name( pid, intData->logData->Fsd.p_comm, sizeof( intData->logData->Fsd.p_comm ) );
            
            assert( maximumLogDataSize > (size_t)&(((DldDriverDataLog*)0)->Fsd.path[0]) );
            
            size = (vm_size_t)&(((DldDriverDataLog*)0)->Fsd.path[0]);
            nameBuffLength = (vm_size_t)maximumLogDataSize - size;
            name_len = nameBuffLength;
            
            //
            // as the logging subsystem doesn't save the vnode state ( open-close-reclaim balance )
            // the name should be reported with each log entry
            //
            assert( name_len == nameBuffLength );
            if( this->dataU.ioMediaProperty->BSDName ){
                
                OSString*    fileName = this->dataU.ioMediaProperty->BSDName;
                
                assert( fileName );
                assert( name_len > sizeof( '\0' ) );
                
                name_len = min( fileName->getLength(), name_len - sizeof( '\0' ) );
                memcpy( intData->logData->Fsd.path, fileName->getCStringNoCopy(), name_len );
                
            } else {
                
                name_len = min( strlen( fakeOpticalDriveName ), name_len - sizeof( '\0' ) );
                memcpy( intData->logData->Fsd.path, fakeOpticalDriveName, name_len );
                
            }
            
            //
            // set rhe terminating zero
            //
            intData->logData->Fsd.path[ name_len ] = '\0';
            
            //
            // account for the terminating zero
            //
            name_len += sizeof( '\0' );
            
            assert( name_len <= MAXPATHLEN );
            size += name_len;
            
            //
            // set the actual size of the valid data
            //
            assert( size <= maximumLogDataSize );// check for overwrite
            intData->logDataSize = size;
            
            DLD_COMM_LOG( DEVICE_ACCESS, ( "Device acess for %s(%s) is %s\n",
                                           DldDeviceTypeToString(this->dataU.property->typeDsc.type),
                                           this->dataU.property->typeDsc.className,
                                           (accessDisabled ? "Disabled" : "Allowed") ));
            break;
        } // end case DldObjectPopertyType_IOMedia:
            
        case DldObjectPopertyType_SCSIPeripheralDeviceType05:
        {
            DldObjectPropertyEntry*   mediaPropertyToAudit = NULL;
            DldIOService*             dldIOService;
            
            dldIOService = this->getReferencedDldIOService();
            assert( dldIOService );
            if( dldIOService ){
                
                //
                // look for actual CD/DVD properties
                //
                mediaPropertyToAudit = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_IODVDMedia );
                if( NULL == mediaPropertyToAudit ){
                    
                    mediaPropertyToAudit = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_IOCDMedia );
                    if( NULL == mediaPropertyToAudit ){
                        
                        mediaPropertyToAudit = dldIOService->retrievePropertyByTypeRef( DldObjectPopertyType_IOMedia );
                    } // end if( NULL == mediaPropertyToAudit )
                } // end if( NULL == mediaPropertyToAudit )
                
                dldIOService->release();
                DLD_DBG_MAKE_POINTER_INVALID( dldIOService );
            } // end if( dldIOService )
            
            if( mediaPropertyToAudit ){
                
                intData->logDataSize = maximumLogDataSize;
                
                mediaPropertyToAudit->initDeviceLogData(action,
                                                        operation,
                                                        pid,
                                                        credential,
                                                        accessDisabled,
                                                        intData);
                
                mediaPropertyToAudit->release();
                DLD_DBG_MAKE_POINTER_INVALID( mediaPropertyToAudit );
                
            } else {
                
                assert( ! mediaPropertyToAudit );
                /*
                 when burning a disk the BSD subsystem is detached from the IOKit objects, so we do not have the BSD name,
                 this is a case of DldObjectPopertyType_SCSIPeripheralDeviceType05
                 
                 | | |   |           +-o IOSCSIPeripheralDeviceType05  <class DldIOService, id 0x100000556, retain 6>
                 | | |   |             | {
                 | | |   |             |   "IOClass" = "IOSCSIPeripheralDeviceType05"
                 | | |   |             |   "CFBundleIdentifier" = "com.apple.iokit.IOSCSIMultimediaCommandsDevice"
                 | | |   |             |   "IOProviderClass" = "IOSCSIPeripheralDeviceNub"
                 | | |   |             |   "IOMaximumBlockCountRead" = 65535
                 | | |   |             |   "IOPowerManagement" = {"DevicePowerState"=4,"CurrentPowerState"=4,"MaxPowerState"=4,"DriverPowerState"=1}
                 | | |   |             |   "IOMaximumBlockCountWrite" = 65535
                 | | |   |             |   "CD Features" = 2047
                 | | |   |             |   "IOProbeScore" = 5000
                 | | |   |             |   "Peripheral Device Type" = 5
                 | | |   |             |   "BD Features" = 0
                 | | |   |             |   "IOMatchCategory" = "IODefaultMatchCategory"
                 | | |   |             |   "DVD Features" = 503
                 | | |   |             |   "Dld: DldIOService" = 18446743524215540480
                 | | |   |             |   "Dld: Class Name" = "IOSCSIPeripheralDeviceType05"
                 | | |   |             |   "Dld: MajorType" = "DLD_DEVICE_TYPE_CD_DVD"
                 | | |   |             |   "Dld: MinorType" = "DLD_DEVICE_TYPE_UNKNOWN"
                 | | |   |             |   "Dld: PropertyType" = "DldObjectPopertyType_SCSIPeripheralDeviceType05"
                 | | |   |             |   "Dld: UID" = <00000000000000000000000000000000>
                 | | |   |             |   "Dld: PnPState" = "kDldPnPStateStarted"
                 | | |   |             | }
                 | | |   |             | 
                 | | |   |             +-o IODVDServices  <class DldIOService, id 0x100000557, retain 8>
                 | | |   |               | {
                 | | |   |               |   "IOCFPlugInTypes" = {"97ABCF2C-23CC-11D5-A0E8-003065704866"="IOSCSIArchitectureModelFamily.kext/Contents/PlugIns/SCSITaskUserClient.kext/Contents/$
                 | | |   |               |   "Dld: DldIOService" = 18446743524215541504
                 | | |   |               |   "SCSITaskDeviceCategory" = "SCSITaskAuthoringDevice"
                 | | |   |               |   "Dld: Class Name" = "IODVDServices"
                 | | |   |               |   "IOUserClientClass" = "SCSITaskUserClient"
                 | | |   |               |   "IOGeneralInterest" = "IOCommand is not serializable"
                 | | |   |               |   "IOMatchCategory" = "SCSITaskUserClientIniter"
                 | | |   |               |   "device-type" = "DVD"
                 | | |   |               |   "Protocol Characteristics" = {"Write Time Out Duration"=15000,"AHCI Port Number"=0,"Read Time Out Duration"=15000,"Physical Interconnect"="SATA","$
                 | | |   |               |   "IOMinimumSegmentAlignmentByteCount" = 4
                 | | |   |               |   "SCSITaskUserClient GUID" = <00f4e20b80ffffff122f182200000000>
                 | | |   |               |   "Device Characteristics" = {"Power Off"=Yes,"Product Name"="DVDRW  GS23N","Fast Spindown"=Yes,"CD Features"=2047,"Low Power Polling"=No,"DVD Featu$
                 | | |   |               |   "Dld: PnPState" = "kDldPnPStateStarted"
                 | | |   |               | }
                 | | |   |               | 
                 | | |   |               +-o SCSITaskUserClientIniter  <class DldIOService, id 0x100000558, retain 5>
                 | | |   |               |   {
                 | | |   |               |     "IOProviderMergeProperties" = {"IOCFPlugInTypes"={"97ABCF2C-23CC-11D5-A0E8-003065704866"="IOSCSIArchitectureModelFamily.kext/Contents/PlugIns/SC$
                 | | |   |               |     "CFBundleIdentifier" = "com.apple.iokit.SCSITaskUserClient"
                 | | |   |               |     "IOProviderClass" = "IODVDServices"
                 | | |   |               |     "IOClass" = "SCSITaskUserClientIniter"
                 | | |   |               |     "IOMatchCategory" = "SCSITaskUserClientIniter"
                 | | |   |               |     "IOProbeScore" = 0
                 | | |   |               |     "Dld: PnPState" = "kDldPnPStateStarted"
                 | | |   |               |     "Dld: Class Name" = "SCSITaskUserClientIniter"
                 | | |   |               |     "Dld: DldIOService" = 18446743524215534336
                 | | |   |               |   }
                 | | |   |               |   
                 | | |   |               +-o IODVDBlockStorageDriver  <class DldIOService, id 0x100000559, retain 5>
                 | | |   |               |   {
                 | | |   |               |     "IOPropertyMatch" = {"device-type"="DVD"}
                 | | |   |               |     "IOProbeScore" = 0
                 | | |   |               |     "IOProviderClass" = "IODVDBlockStorageDevice"
                 | | |   |               |     "IOClass" = "IODVDBlockStorageDriver"
                 | | |   |               |     "CFBundleIdentifier" = "com.apple.iokit.IODVDStorageFamily"
                 | | |   |               |     "Statistics" = {"Operations (Write)"=0,"Latency Time (Write)"=0,"Bytes (Read)"=515984,"Errors (Write)"=0,"Total Time (Read)"=28328203330,"Retrie$
                 | | |   |               |     "IOMatchCategory" = "IODefaultMatchCategory"
                 | | |   |               |     "IOGeneralInterest" = "IOCommand is not serializable"
                 | | |   |               |     "Dld: Class Name" = "IODVDBlockStorageDriver"
                 | | |   |               |     "Dld: PnPState" = "kDldPnPStateStarted"
                 | | |   |               |     "Dld: DldIOService" = 18446743524215533824
                 | | |   |               |   }
                 | | |   |               |   
                 | | |   |               +-o SCSITaskUserClient  <class DldIOService, id 0x10000061a, retain 5>
                 | | |   |                   {
                 | | |   |                     "Dld: DldIOService" = 18446743524245695488
                 | | |   |                     "IOUserClientCreator" = "pid 310, Disk Utility"
                 | | |   |                     "Dld: Class Name" = "SCSITaskUserClient"
                 | | |   |                     "Dld: PnPState" = "kDldPnPStateStarted"
                 | | |   |                   }
                 
                 */
                vm_size_t   size;
                vm_size_t   nameBuffLength;
                vm_size_t   name_len;
                
                intData->logData->Header.type = DLD_LOG_TYPE_FSD;
                intData->logData->Fsd.scopeID = DLD_KAUTH_SCOPE_VNODE_ID; // we are cheating on the service!
                
                intData->logData->Fsd.operation = operation;
                
                //
                // a process' PID and a short name
                //
                intData->logData->Fsd.pid = pid;
                proc_name( pid, intData->logData->Fsd.p_comm, sizeof( intData->logData->Fsd.p_comm ) );
                
                assert( maximumLogDataSize > (size_t)&(((DldDriverDataLog*)0)->Fsd.path[0]) );
                
                size = (vm_size_t)&(((DldDriverDataLog*)0)->Fsd.path[0]);
                nameBuffLength = (vm_size_t)maximumLogDataSize - size;
                name_len = nameBuffLength;
                
                //
                // as the logging subsystem doesn't save the vnode state ( open-close-reclaim balance )
                // the name should be reported with each log entry
                //
                assert( name_len == nameBuffLength );
                
                name_len = min( strlen( fakeOpticalDriveName ), name_len - sizeof( '\0' ) );
                memcpy( intData->logData->Fsd.path, fakeOpticalDriveName, name_len );
                
                //
                // set rhe terminating zero
                //
                intData->logData->Fsd.path[ name_len ] = '\0';
                
                //
                // account for the terminating zero
                //
                name_len += sizeof( '\0' );
                
                assert( name_len <= MAXPATHLEN );
                size += name_len;
                
                //
                // set the actual size of the valid data
                //
                assert( size <= maximumLogDataSize );// check for overwrite
                intData->logDataSize = size;
                
                DLD_COMM_LOG( DEVICE_ACCESS, ( "Device acess for %s(%s) is %s\n",
                                               DldDeviceTypeToString(this->dataU.property->typeDsc.type),
                                               this->dataU.property->typeDsc.className,
                                               (accessDisabled ? "Disabled" : "Allowed") ));
            } // end for else if( mediaPropertyToAudit )
            
            break;
        } // end case DldObjectPopertyType_SCSIPeripheralDeviceType05
            
        default:
            
            //
            // the header contains a suffifcient amount of data for the logging to be possible
            //
            //
            // set the actual log data type
            //
            intData->logData->Header.type = DLD_LOG_TYPE_DEVCE;
            assert( intData->logData->Header.type == intData->logData->device.Header.type );
            
            /*
            assert( !"unprocessed device type for log" );
            DBG_PRINT_ERROR(("unprocessed device type (%u)(%s) for log\n",
                              this->dataU.property->typeDsc.type,
                              DldPropertyTypeToString( this->dataU.property->typeDsc.type ) ));
            return false;
             */
            
            DLD_COMM_LOG( DEVICE_ACCESS, ( "Device acess for %s(%s) is %s\n",
                                           DldDeviceTypeToString(this->dataU.property->typeDsc.type),
                                           this->dataU.property->typeDsc.className,
                                           (accessDisabled ? "Diabled" : "Allowed") ));
            
            break;
    }
    
    //
    // the service might depend on this particular size, it can't be made smaller
    //
    assert( intData->logDataSize >= sizeof( intData->logData->device.Header ) );
    
    return success;
}

//--------------------------------------------------------------------

static DldPhysicalInterconnectString   gPhysicalInterconnectStrings[]=
{
    { kPI_ATA       , kIOPropertyPhysicalInterconnectTypeATA },
    { kPI_SATA      , kIOPropertyPhysicalInterconnectTypeATA },
    { kPI_SAS       , kIOPropertyPhysicalInterconnectTypeSerialAttachedSCSI },
    { kPI_ATAPI     , kIOPropertyPhysicalInterconnectTypeATAPI },
    { kPI_USB       , kIOPropertyPhysicalInterconnectTypeUSB },
    { kPI_FireWire  , kIOPropertyPhysicalInterconnectTypeFireWire },
    { kPI_SCSIParal , kIOPropertyPhysicalInterconnectTypeSCSIParallel },
    { kPI_FibreChnl , kIOPropertyPhysicalInterconnectTypeFibreChannel },
    { kPI_Virtual   , kIOPropertyPhysicalInterconnectTypeVirtual }
};


static DldPhysicalInterconnectLocationString gPhysicalInterconnectLocationStrings[]=
{
    { kPIL_Internal, kIOPropertyInternalKey },
    { kPIL_External, kIOPropertyExternalKey },
    { kPIL_IntExt,   kIOPropertyInternalExternalKey },// this key is usually set for a bus itself, ignore it!
    { kPIL_File,     kIOPropertyInterconnectFileKey },
    { kPIL_RAM,      kIOPropertyInterconnectRAMKey }
};

//--------------------------------------------------------------------

DldPhysicalInterconnect
DldStringToPhysicalInterconnect(
    __in OSString*   string
    )
{
    for( int i=0x0; i < DLD_STATIC_ARRAY_SIZE(gPhysicalInterconnectStrings); ++i ){
        
        if( string->isEqualTo( gPhysicalInterconnectStrings[i].string ) )
            return gPhysicalInterconnectStrings[i].physicalInterconnect;
        
    }// end for
    
    return kPI_Unknown;
}


DldPhysicalInterconnectLocation
DldStringToPhysicalInterconnectLocation(
    __in OSString*   string
    )
{
    for( int i=0x0; i < DLD_STATIC_ARRAY_SIZE(gPhysicalInterconnectLocationStrings); ++i ){
        
        if( string->isEqualTo( gPhysicalInterconnectLocationStrings[i].string ) )
            return gPhysicalInterconnectLocationStrings[i].physicalInterconnectLocation;
        
    }// end for
    
    return kPIL_Unknown;
}

//--------------------------------------------------------------------

//
// context is a pointer to a DldObjectPropertyEntry* object
//
void
DldIOMediaPropertyData::processProtocolCharacteristicsApplierFunction( 
    __in IORegistryEntry * entry,
    __in void * context// DldIOMediaPropertyData*
   )
{
    //
    // a physical interconnect and physical interconnect location define
    // whether the device is external or internal
    /*
     Example:
     <pre>
     @textblock
     <dict>
        <key>Protocol Characteristics</key>
            <dict>
                <key>Physical Interconnect</key>
                    <string>ATAPI</string>
                <key>Physical Interconnect Location</key>
                    <string>Internal</string>
            </dict>
        </dict>
     @/textblock
     </pre>
     */
    //
    DldIOMediaPropertyData*  objectProperty = (DldIOMediaPropertyData*)context;
    DldIOService*            entryDldService;
    
    assert( preemption_enabled() );
    assert( DldObjectPopertyType_IOMedia == objectProperty->typeDsc.type ||
            DldObjectPopertyType_IODVDMedia == objectProperty->typeDsc.type || 
            DldObjectPopertyType_IOCDMedia == objectProperty->typeDsc.type );
    
    //
    // the root is not of the DldIOService type
    //
    entryDldService = OSDynamicCast( DldIOService, entry );
    
    if( entryDldService )
        entryDldService->LockShared();
    {// start of the lock
        
        OSObject*                object = NULL;// uncasted object
        OSDictionary*            protocolCharacteristics = NULL;
        OSString*                physicalInterconnect = NULL;
        OSString*                physicalInterconnectLocation = NULL;
        
        //
        // get the Protocol Characteristics dictionary, might absent or contains
        // useless info on old ATA stacks, in the latter case the information is 
        // present as registry object properties with the same names as in the dictionary
        //
        object = entry->getProperty( kIOPropertyProtocolCharacteristicsKey );
        if( object ){
            
            //
            // check the dictionary for required properties
            //
            
            protocolCharacteristics = OSDynamicCast( OSDictionary, object );
            assert( protocolCharacteristics );
            if( !protocolCharacteristics )
                goto __bail_from_dictionairy;
            
            //
            // get the Physical Interconnect key, if fails the assert checks
            // that this is a case of an old ATA stack which has nearly
            // empty dictionary, see below for an example
            //
            object = protocolCharacteristics->getObject( kIOPropertyPhysicalInterconnectTypeKey );
            assert( object || ( protocolCharacteristics->getCount() <= 0x1 ) );
            if( !object )
                goto __bail_from_dictionairy;
            
            physicalInterconnect = OSDynamicCast( OSString, object );
            assert( physicalInterconnect );
            if( !physicalInterconnect )
                goto __bail_from_dictionairy;
            
            objectProperty->physicalInterconnect = DldStringToPhysicalInterconnect( physicalInterconnect );
            
            //
            // get the Physical Interconnect Location key
            //
            object = protocolCharacteristics->getObject( kIOPropertyPhysicalInterconnectLocationKey );
            assert( object );
            if( !object )
                goto __bail_from_dictionairy;
            
            physicalInterconnectLocation = OSDynamicCast( OSString, object );
            assert( physicalInterconnectLocation );
            if( !physicalInterconnectLocation )
                goto __bail_from_dictionairy;
            
        __bail_from_dictionairy:;
            
        } // end if( object )
        
        //
        // check the registry object's properties,
        // below is an excerpt from the 2007 15 MacBook Pro's DVD drive stack
        //
        /*
         +-o IOATAPIProtocolTransport  <object 0x080fb800, id 0x10000022d, vtable 0x1287f80, registered, matched, active, busy 1, retain count 5>
         | {
         |   "Write Time Out Duration" = 15000
         |   "CFBundleIdentifier" = "com.apple.iokit.IOATAPIProtocolTransport"
         |   "IOProviderClass" = "IOATADevice"
         |   "IOClass" = "IOATAPIProtocolTransport"
         |   "Physical Interconnect" = "ATAPI" <--- actual value should be in "Protocol Characteristics"
         |   "Read Time Out Duration" = 15000
         |   "Physical Interconnect Location" = "Internal" <--- actual value should be in "Protocol Characteristics"
         |   "Retry Count" = 1
         |   "ata device type" = "atapi"
         |   "IOProbeScore" = 0
         |   "IOMatchCategory" = "IODefaultMatchCategory"
         |   "IOPowerManagement" = `object 0x080f6a00, vt 0x608360 <vtable for IOServicePM>`
         |   "Protocol Characteristics" = {"unit number"=0} <--- useless "Protocol Characteristics"
         | }
         +--o IOSCSIPeripheralDeviceNub  <object 0x08124b00, id 0x100000242, vtable 0x1280380, registered, matched, active, busy 1, retain count 3>
            | {
            |   "IOClass" = "IOSCSIPeripheralDeviceNub"
            |   "IOProviderClass" = "IOSCSIProtocolServices"
            |   "CFBundleIdentifier" = "com.apple.iokit.IOSCSIArchitectureModelFamily"
            |   "IOProbeScore" = 0
            |   "IOMatchCategory" = "SCSITaskUserClientIniter"
            |   "Protocol Characteristics" = {"unit number"=0,"Physical Interconnect"="ATAPI","Physical Interconnect Location"="Internal","Read Time Out Duration"=15000,"Write Time Out Duration"=15000,"Retry Count"=1}
            |   "Peripheral Device Type" = 5
            |   "Vendor Identification" = "HL-DT-ST"
            |   "Product Identification" = "DVDRW  GSA-S10N"
            |   "Product Revision Level" = "AP09"
            | }             
         */
        if( NULL == physicalInterconnectLocation ){
            
            OSObject* property;
            
            property = entry->getProperty( kIOPropertyPhysicalInterconnectTypeKey );
            if( property )
                physicalInterconnectLocation = OSDynamicCast( OSString, object );
            
        }// end if( NULL == physicalInterconnectLocation )
        
        //
        // process the found data
        //
        
        if( physicalInterconnectLocation ){
            
            objectProperty->physicalInterconnectLocation = 
                DldStringToPhysicalInterconnectLocation( physicalInterconnectLocation );
            
        }// end if( physicalInterconnectLocation )
        
        
        if( protocolCharacteristics ){
            
            objectProperty->physicalInterconnectionApplierFunction( entry, protocolCharacteristics );
            objectProperty->fileVaultApplierFunction( entry, protocolCharacteristics );
            
        }// end if( protocolCharacteristics )
        
        //__asm__ volatile( "int $0x3" );
        
    }// end of the lock
    if( entryDldService )
        entryDldService->UnLockShared();
    
    //
    // apply recursively to the object's parents, so this is a DFS traversing
    //
    entry->applyToParents( DldIOMediaPropertyData::processProtocolCharacteristicsApplierFunction,
                           (void*)objectProperty,
                           gDldDeviceTreePlan );
    
}

//--------------------------------------------------------------------

//
// if the entry is of DldIoService type then it is locked shared by the caller
// to retain the protocolCharacteristics value
//
void
DldIOMediaPropertyData::physicalInterconnectionApplierFunction(
    __in IORegistryEntry *        entry,
    __in OSDictionary*            protocolCharacteristics
    )
{
    //
    // a physical interconnect and physical interconnect location define
    // whether the device is external or internal, the exception
    // is a boot device - it bever considered as external, for
    // example VmWare sets External property for a boot device
    /*
     Example:
     <pre>
        @textblock
            <dict>
                <key>Protocol Characteristics</key>
                    <dict>
                        <key>Physical Interconnect</key>
                            <string>ATAPI</string>
                        <key>Physical Interconnect Location</key>
                            <string>Internal</string>
                    </dict>
            </dict>
        @/textblock
     </pre>
     */
    //    
    
    if( kPIL_External == this->physicalInterconnectLocation )
        this->isRemovable = (FALSE == this->onBootDevicePath);

}

//--------------------------------------------------------------------

//
// if the entry is of DldIoService type then it is locked shared by the caller
// to retain the protocolCharacteristics value
//
void DldIOMediaPropertyData::fileVaultApplierFunction(
    __in IORegistryEntry *         entry,
    __in OSDictionary*             protocolCharacteristics
    )
{
    //
    // FileVault is supported by the IODiskImageBlockStorageDeviceOutKernel class
    // which adds a path to the backing file in the Potocol Characteristics property
    //
    /*
     Example:
     <pre>
        @textblock
        <dict>
            <key>Protocol Characteristics</key>
            <dict>
                <key>Physical Interconnect</key>
                    <string>Virtual Interface</string>
                <key>Physical Interconnect Location</key>
                    <string>File</string>
                <key>Virtual Interface Locaton Path</key>
                    <Data> <....path to the file as a binary data.....>
            </dict>
        </dict>
     @/textblock
     </pre>
     */
    
    if( kPI_Virtual != this->physicalInterconnect || this->fileVaultInfoGathered )
        return;
    
    //
    // so this is a virtual device
    //
    
    //
    // get the path to the backing file
    //
    DldIOService*   dldService;
    OSObject*       object;
    OSData*         data;
    OSData*         dataCopy = NULL;
    OSString*       path = NULL;
    vnode_t         vnode = NULL;
    DldIOVnode*     dldVnode = NULL;
    DldIOService*   dldBackingIOMediaService = NULL;
    
    assert( preemption_enabled() );
    
    dldService = OSDynamicCast( DldIOService, entry );
    assert( dldService );
    if( NULL == dldService )
        return;
    
    object = protocolCharacteristics->getObject( "Virtual Interface Location Path" );
    assert( object );
    if( !object ){
        
        DBG_PRINT_ERROR(("a \"Virtual Interface Location Path\" doesn't exist for a vitual interface\n"));
        return;
    }
    
    data = OSDynamicCast( OSData, object );
    assert( data );
    if( !data )
        return;
    
    //
    // create a zero terminated string
    //
    dataCopy = OSData::withCapacity( data->getLength() + sizeof( L'\0' ) );
    assert( dataCopy );
    if( !dataCopy )
        return;
    
    if( !dataCopy->initWithData( data ) ){
        
        assert( !"dataCopy->initWithData( data ) failed" );
        goto __exit;
    }
    
    if( !dataCopy->appendByte( 0x0, sizeof( L'\0' ) ) ){
        
        assert( !"dataCopy->appendByte( 0x0, sizeof( L'\0' ) ) failed" );
        goto __exit;
    }
    
    path = OSString::withCString( (const char*)dataCopy->getBytesNoCopy() );
    assert( path );
    if( !path )
        goto __exit;
    
    //__asm__ volatile( "int $0x3" );
    
    //
    // try to get the file's vnode
    //
    int error;
    
    error = vnode_lookup( path->getCStringNoCopy(), 0x0, &vnode, vfs_context_current() );
    if( error ){
        
        DBG_PRINT_ERROR(("an error = 0x%x while looking for the %s vnode\n", error, path->getCStringNoCopy()));
        goto __exit;
    }
    
    if( NULLVP == vnode ){
        
        DBG_PRINT_ERROR(("a vnode for the %s file can't be found\n", path->getCStringNoCopy()));
        goto __exit;
    }
    
    DLD_COMM_LOG( FILE_VAULT, ("a vnode for the %s file has been retrieved\n", path->getCStringNoCopy()));
    
    //
    // add the vnode in the hash and hook the FSD if this has not been done
    //
    dldVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode( vnode );
    assert( dldVnode );
    if( !dldVnode ){
        
        DBG_PRINT_ERROR(("CreateAndAddIOVnodByBSDVnode() failed for the %s file\n", path->getCStringNoCopy()));
        goto __exit;
    }
    
    //
    // mark the vnode as backing a virtual disk
    //
    if( 0x0 == dldVnode->flags.virtualDiskFile )
        dldVnode->flags.virtualDiskFile = 0x1;
    
    dldBackingIOMediaService = DldGetReferencedDldIOServiceFoMediaByVnode( vnode );
    if( !dldBackingIOMediaService ){
        
        DBG_PRINT_ERROR(("DldGetReferencedDldIOServiceFoMediaByVnode() failed for the %s file\n", path->getCStringNoCopy()));
        goto __exit;
    }
    
    if( !this->addVirtualBackingObject( dldBackingIOMediaService->getSystemService() ) ){
        
        DBG_PRINT_ERROR(("addVirtualBackingObject() failed for the %s file\n", path->getCStringNoCopy()));
        goto __exit;
    }
    
    //
    // all information required to support a FileVault disk have been gathered,
    // there is no need to repeat the process and incurring an additional overhead
    // of file opening
    //
    this->fileVaultInfoGathered = true;
    
__exit:
    
    if( dldBackingIOMediaService )
        dldBackingIOMediaService->release();
    
    if( dldVnode )
        dldVnode->release();
    
    if( vnode )
        vnode_put( vnode );
    
    if( path )
        path->release();
    
    if( dataCopy )
        dataCopy->release();
    
}

//--------------------------------------------------------------------

//
// Protocol Characteristic is a property created by some class in the stack which
// describes the property of a physical connection
//
bool
DldIOMediaPropertyData::processProtocolCharacteristics(
   __in DldIOService* dldService
   )
{
    assert( preemption_enabled() );
    assert( DldObjectPopertyType_IOMedia == this->typeDsc.type ||
            DldObjectPopertyType_IODVDMedia == this->typeDsc.type || 
            DldObjectPopertyType_IOCDMedia == this->typeDsc.type );
    assert( dldService );
    assert( OSDynamicCast( DldIOService, dldService ) );// to be sure IOService is not passed as a parameter
    
    //
    // apply to the object
    //
    DldIOMediaPropertyData::processProtocolCharacteristicsApplierFunction( dldService, this );
    
    return true;
}

//--------------------------------------------------------------------

bool
DldIsServiceInServicesArray(
    __in IOService*  service,
    __in DldIOServicesArray*  servicesArray
    )
{
    assert( servicesArray? ( servicesArray->validEntries <= servicesArray->maxCapacity ): true );
    
    for( int i=0x0; servicesArray && i<servicesArray->validEntries; ++i ){
        
        if( service == servicesArray->objects[ i ] )
            return true;
        
    }// end for
    
    return false;
}

//--------------------------------------------------------------------

DldIOServicesArray*
DldAlocateAndCopyServicesArray(
    __in int capacity,
    __in_opt DldIOServicesArray*  copyFrom
    )
{
    DldIOServicesArray*   newArray;
    vm_size_t             size;
    
    assert( capacity >= 0x1 );
    assert( copyFrom? (copyFrom->validEntries <= copyFrom->maxCapacity): true );
    
    size = sizeof( *newArray ) + (capacity-1)*sizeof( newArray->objects[0] );
    newArray = (DldIOServicesArray*)IOMalloc( size );
    assert( newArray );
    if( !newArray )
        return NULL;
    
    //
    // it is enough to zero header, we also zero the first array's element
    //
    bzero( newArray, sizeof( *newArray ) );
    
    newArray->maxCapacity  = capacity;
    newArray->validEntries = 0x0;
    
    if( copyFrom ){
        
        int   objectsToCopy;
        
        objectsToCopy = min( newArray->maxCapacity, copyFrom->validEntries );
        
        memcpy( newArray->objects, copyFrom->objects, objectsToCopy*sizeof( newArray->objects[0] ) );
        newArray->validEntries = objectsToCopy;
    }
    
    assert( newArray->validEntries <= newArray->maxCapacity );
    
    return newArray;
}

//--------------------------------------------------------------------

void
DldFreeServicesArray(
    __in DldIOServicesArray*  servicesArray
    )
{
    vm_size_t    size;
    
    assert( servicesArray->maxCapacity >= 0x1 );
    
    size = sizeof( *servicesArray ) + 
    sizeof( servicesArray->objects[0])*( servicesArray->maxCapacity-1 );
    
    IOFree( servicesArray, size );
    
}

//--------------------------------------------------------------------

bool
DldAddNewServiceInServicesArray(
    __in IOService*  service,
    __in DldIOServicesArray*  servicesArray
    )
{
    if( servicesArray->validEntries == servicesArray->maxCapacity ){
        
        //
        // the array is full
        //
        return false;
    }
    
    servicesArray->objects[ servicesArray->validEntries ] = service;
    servicesArray->validEntries += 0x1;
    
    return true;
}

//--------------------------------------------------------------------

bool
DldPropertyCommonData::addVirtualBackingObject(
    __in IOService* service
    )
/*
 the function is used to add the object which can't be reached by children-parents
 links traversal in the tree, it is used manly for the virtual storages backing by file,
 backing objects must be added in the descending order while traversing from the
 leaves to the root, the getLowestBackingMediaReferenceForVirtualMedia() uses
 this property to retrieve the lowest device in the path with assumption that
 there is only one parent for each object
 */
{
    //
    // never refernce the service object as it is passed without
    // any guarantee on validity, the value is a handle to be used
    // while searching in the hash table
    //
    
    bool   inArray = false;
    
    assert( preemption_enabled() );
    
    while( !inArray ){
        
        DldIOServicesArray* newArray = NULL;
        DldIOServicesArray* oldArray = NULL;
        
        this->LockShared();
        {// start of the lock
            
            oldArray = this->virtualBackingServices;
            inArray = DldIsServiceInServicesArray( service, oldArray );
            
            //
            // allocate a new array with a capacity for a new object
            //
            if( !inArray ){
                
                int newCapacity;
                
                newCapacity = ( oldArray?(oldArray->validEntries): 0x0 ) + 0x1;
                newArray = DldAlocateAndCopyServicesArray( newCapacity, oldArray );
                assert( newArray );
            }
            
        }// end of the lock
        this->UnLockShared();
        
        //
        // there are three outcome
        //   - the service was in the array ( exit )
        //   - the service was not in the array and a new array allocation failed ( exit )
        //   - the service was not in the array and a new array was allocated ( continue )
        //
        if( inArray || !newArray)
            break;
        
        assert( newArray );
        assert( !inArray );
        
        if( !DldAddNewServiceInServicesArray( service, newArray ) ){
            
            assert( !"a serious logic bug!" );
            
            DldFreeServicesArray( newArray );
            break;
        }
        
        //
        // set the new array
        //
        this->LockExclusive();
        {// start of the lock
            
            //
            // continue with the new array only if we updating the same old array,
            // else repeat the allocation and the new array initialization
            //
            if( oldArray == this->virtualBackingServices ){
                
                this->virtualBackingServices = newArray;
                inArray = true;
                
                assert( DldIsServiceInServicesArray( service, newArray ) );
            }
        }// end of the lock
        this->UnLockExclusive();
        
        if( !inArray ){
            
            //
            // a concurrent thread managed to update the array, repeat allocation with an updated array
            //
            DldFreeServicesArray( newArray );
            continue;
            
        } else {
            
            //
            // the update was successful
            //
            
            if( oldArray )
                DldFreeServicesArray( oldArray );
            
            break;
        }
        
        assert( !"an unreachable point in the code!" );
        
    }// end while
    
    assert( inArray );
    return inArray;
}

//--------------------------------------------------------------------

//
// get the lowest media in the chain for virtual devices, this media
// defines the actual type of the virtual media, NULL is returned
// for non-virtual devices, the returned object is referenced,
// the caller must dereference the returned object when it is no longer needed
//
DldIOService*
DldPropertyCommonData::getLowestBackingObjectReferenceForVirtualObject()
{
    DldIOService*   lowestDldService = NULL;// will be referenced if found
    IOService*      lowestService = NULL;// not referenced, shoul not be considered as valid!
    
    assert( preemption_enabled() );
    
    if( !this->virtualBackingServices )
        return NULL;
    
    this->LockShared();
    {// start of the lock
        
        if( this->virtualBackingServices &&
            0x0 != this->virtualBackingServices->validEntries ){
            
            int lowestIndex;
            
            lowestIndex = this->virtualBackingServices->validEntries - 0x1;
            
            lowestService = this->virtualBackingServices->objects[ lowestIndex ];
            assert( lowestService );
        }// end if( this->dataU.property->virtualBackingServices )
        
    }// end of the lock
    this->UnLockShared();
    
    if( lowestService )
        lowestDldService = DldIOService::RetrieveDldIOServiceForIOService( lowestService );
    
    return lowestDldService;
}

//--------------------------------------------------------------------

DldIOService*
DldObjectPropertyEntry::getLowestBackingObjectReferenceForVirtualObject()
{
    return this->dataU.property->getLowestBackingObjectReferenceForVirtualObject();
}

//--------------------------------------------------------------------

DldIOService*
DldObjectPropertyEntry::getReferencedDldIOService()
{
    return DldIOService::RetrieveDldIOServiceForIOService( this->dataU.property->service );
}

//--------------------------------------------------------------------

void
DldObjectPropertyEntry::setUIDProperty()
{
    
    OSData*    mediaUIDData = NULL;
    
    //
    // currently only the DldObjectPopertyType_SCSIPeripheralDeviceType05 and 
    // DldObjectPopertyType_UsbDevice properties are processed
    //
    if( DldObjectPopertyType_SCSIPeripheralDeviceType05 == this->dataU.property->typeDsc.type ){
        
        mediaUIDData = OSData::withBytes( this->dataU.ioSCSIPeripheralType05Property->mediaUID.uid,
                                          sizeof( this->dataU.ioSCSIPeripheralType05Property->mediaUID.uid ) );
        
        assert( mediaUIDData );
        
    } else if( DldObjectPopertyType_UsbDevice == this->dataU.property->typeDsc.type ){
        
        mediaUIDData = OSData::withBytes( this->dataU.usbDeviceProperty->uid.uid,
                                          sizeof( this->dataU.usbDeviceProperty->uid.uid ) );
        
        assert( mediaUIDData );
    }
    
    if( !mediaUIDData )
        return;
    
    DldIOService*  dldIOService;
    
    dldIOService = this->getReferencedDldIOService();
    assert( dldIOService );
    
    //
    // set an object's property
    //
    if( dldIOService ){
        
        dldIOService->setProperty( DldStrPropertyMediaUID, mediaUIDData );
        dldIOService->release();
        DLD_DBG_MAKE_POINTER_INVALID( dldIOService );
        
    }//end if( scsi05Service )
    
    mediaUIDData->release();
    DLD_DBG_MAKE_POINTER_INVALID( mediaUIDData );
            
}

//--------------------------------------------------------------------
