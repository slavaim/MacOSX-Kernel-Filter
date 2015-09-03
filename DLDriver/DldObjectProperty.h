/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDOBJECTPROPERTY_H
#define _DLDOBJECTPROPERTY_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/usb/USBSpec.h>
#include <IOKit/usb/IOUSBHubDevice.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldWhiteList.h"
#include "DldAclObject.h"

class DldIOService;
class DldObjectPropertyEntry;

//--------------------------------------------------------------------

typedef enum _DldPnPState{
    kDldPnPStateUnknown = 0x0,
    kDldPnPStateReadyToStart,// the same as kDldPnPStateStarted but the security setting must not be applied as
                             // the object's properties is being initialized
    kDldPnPStateStarted, // IOService::start() has been called and returned success
    
    // always the last
    kDldPnPStateMax
} DldPnPState;

//--------------------------------------------------------------------

static
inline
const char*
DldPnPStateToString( __in DldPnPState state )
{
    switch( state ){
            
        case kDldPnPStateUnknown:
            return "kDldPnPStateUnknown";
        case kDldPnPStateReadyToStart:
            return "kDldPnPStateReadyToStart";
        case kDldPnPStateStarted:
            return "kDldPnPStateStarted";
        case kDldPnPStateMax:
            return "kDldPnPStateMax";
        default:
            return "out of range";
            
    }// end state
}

//--------------------------------------------------------------------

//
// the enum is related to the gTypeDesc array
// and the allocatePropertyDataWithType function
// udate them when adding a new type
//
typedef enum _DldObjectPopertyType{
    
    DldObjectPopertyType_Unknown = 0x0,
    
    DldObjectPopertyType_UsbDevice,// IOUSBDevice class
    DldObjectPopertyType_UsbInterface,// IOUSBInterface class
    DldObjectPopertyType_IOMedia,// IOMedia class
    DldObjectPopertyType_IODVDMedia,// IODVDMedia class
    DldObjectPopertyType_IOCDMedia,//IOCDMedia class
    DldObjectPopertyType_Serial, // IOSerialStreamSync class
    DldObjectPopertyType_SCSIPeripheralDeviceType05, // IOSCSIPeripheralDeviceType05 class, SCSI multimedia
    DldObjectPropertyType_BluetoothDevice, // IOBluetoothDevice class, created for each paired device
    DldObjectPropertyType_IOHIDSystem, // IOHIDSystem is created for every HID device
    DldObjectPropertyType_AppleMultitouchDeviceUserClient, // DldObjectPropertyType_AppleMultitouchDeviceUserClient , Apple MagicMouse surface
    DldObjectPropertyType_Net80211Interface, // WiFi interface
    DldObjectPropertyType_IONetworkInterface, // Network interface
    DldObjectPropertyType_IOFireWireDevice, // IOFireDevice class
    DldObjectPropertyType_IOFireWireSBP2, // IOFireWireSBP2Target class
    
    //
    // always the last!
    //
    DldObjectPopertyType_Max
} DldObjectPopertyType;

//--------------------------------------------------------------------

static
inline
const char*
DldPropertyTypeToString( __in DldObjectPopertyType type )
{
    switch( type ){
        case DldObjectPopertyType_Unknown:
            return "DldObjectPopertyType_Unknown";
        case DldObjectPopertyType_UsbDevice:
            return "DldObjectPopertyType_UsbDevice";
        case DldObjectPopertyType_UsbInterface:
            return "DldObjectPopertyType_UsbInterface";
        case DldObjectPopertyType_IOMedia:
            return "DldObjectPopertyType_IOMedia";
        case DldObjectPopertyType_IODVDMedia:
            return "DldObjectPopertyType_IODVDMedia";
        case DldObjectPopertyType_IOCDMedia:
            return "DldObjectPopertyType_IOCDMedia";
        case DldObjectPopertyType_Serial:
            return "DldObjectPopertyType_Serial";
        case DldObjectPopertyType_SCSIPeripheralDeviceType05:
            return "DldObjectPopertyType_SCSIPeripheralDeviceType05";
        case DldObjectPropertyType_BluetoothDevice:
            return "DldObjectPropertyType_BluetoothDevice";
        case DldObjectPropertyType_IOHIDSystem:
            return "DldObjectPropertyType_IOHIDSystem";
        case DldObjectPropertyType_AppleMultitouchDeviceUserClient:
            return "DldObjectPropertyType_AppleMultitouchDeviceUserClient";
        case DldObjectPropertyType_Net80211Interface:
            return "DldObjectPropertyType_Net80211Interface";
        case DldObjectPropertyType_IONetworkInterface:
            return "DldObjectPropertyType_IONetworkInterface";
        case DldObjectPropertyType_IOFireWireDevice:
            return "DldObjectPropertyType_IOFireWireDevice";
        case DldObjectPropertyType_IOFireWireSBP2:
            return "DldObjectPropertyType_IOFireWireSBP2";
        default:
            return "out of range";
    }
}

typedef enum _DldObjectPopertyTypeFlavor{
    kDldObjectPopertyTypeFlavor_Primary = 0x0,
    kDldObjectPopertyTypeFlavor_Ancillary
} DldObjectPopertyTypeFlavor;

//--------------------------------------------------------------------

typedef struct _DldObjectPropertyTypeDescriptor{
    
    DldObjectPopertyType        type;
    const char*                 className;
    DldObjectPopertyTypeFlavor  flavor;
    
} DldObjectPropertyTypeDescriptor;

//--------------------------------------------------------------------

//
// the following definitions are more structures than classes,
// I do not want any complication in data access
//

//--------------------------------------------------------------------

typedef struct _DldWhiteListState{
    
    //
    // a variable foed! for example a USB device can
    // move from the PidVid to UID class
    //
    DldWhiteListType   type;
    
    //
    // true if the current WL has been applied so we
    // can consider the structure contains up to date infoemation
    //
    bool               currentWLApplied;
    
    //
    // "true" if the device is in white list
    //
    bool               inWhiteList;
    
    //
    // "true" if white list setting must be propagated up through all stack to the children,
    // used for USB
    //
    bool               propagateUp;
    
    //
    // an ACL object which defines to whom the WL state must be applied
    // if NULL the WL state is appled to every user,
    // protected by a DldObjectPropertyEntry's lock
    //
    DldAclObject*      acl;
    
} DldWhiteListState;

//--------------------------------------------------------------------

typedef struct _DldIOServicesArray{
    
    //
    // a maximum capacity, in elements number, must be at least 0x1
    //
    int  maxCapacity;
    
    //
    // a number of valid elements ( there must be no gaps between elements )
    //
    int  validEntries;
    
    //
    // an array itself
    //
    IOService*   objects[ 1 ];
    
} DldIOServicesArray;

bool
DldIsServiceInServicesArray(
    __in IOService*  service,
    __in DldIOServicesArray*  servicesArray
    );

DldIOServicesArray*
DldAlocateAndCopyServicesArray(
    __in int capacity,
    __in_opt DldIOServicesArray*  copyFrom
    );

void
DldFreeServicesArray(
    __in DldIOServicesArray*  servicesArray
    );

bool
DldAddNewServiceInServicesArray(
    __in IOService*  service,
    __in DldIOServicesArray*  servicesArray
    );

//--------------------------------------------------------------------

class DldPropertyCommonData{
    
    friend class DldObjectPropertyEntry;
    
public:
    
    //
    // a copy of the corresponding descriptor from the gTypeDesc array
    //
    DldObjectPropertyTypeDescriptor   typeDsc;
    
    //
    // a type for the device, a minor type correlates with the parentProperty
    // major type
    //
    DldDeviceType         deviceType;
    
    //
    // the encrypted property is valid only for an IOMedia( or derived class )
    // object that creates a BSD device ( like /dev/disk5s1 or /dev/disk5 for CoreStorage )
    // on which an FSD is moonted, for all other objects it is always 'ProviderUnknown'
    //
    EncryptionProviderEnum   encryptionProvider;
    
    //
    // a dld object which owns the object, the object is not referenced
    // or there will be a cycle in the reference graph as DldIOService
    // objects retain property objects, the pointer must be used only
    // as a guide and never being treated as a valid object pointer
    //
    class DldIOService*   dldService;
    
    //
    // the same as dldService but for the system IOService,
    // can be used to find a corresponding DldIOService,
    // so this is a handle but not an object which can be dereferenced
    //
    IOService*            service;
    
    //
    // an array of services ( usually only one service ) which
    // can't be directly tracked by the child-parent relations,
    // for example in case of FileVault an array contains a service
    // for IOMedia object containing a file backing a virtual device,
    // the above notes for the service field is applied to every
    // service in this array
    //
    DldIOServicesArray*   virtualBackingServices;
    
    //
    // a parent's property entry, a parent notion is a vague one
    // and can't be defined exactly, a definition might be
    // "a bus to which the device is attached" so for a USB pen drive
    // at the Removable level the parent is an IOUSBDevice object,
    // might be NULL ( and NULL for the most of the cases ),
    // if not NULL then the object is referenced( retained ),
    // the field is protected by the entry's RW lock containing
    // this property, but actually the value is set only once and
    // never changed so the access can be optimized
    //
    class DldObjectPropertyEntry*  parentProperty;
    
    //
    // defines whether the securities must be checked at this level,
    // the value is mainly ignored except by the USB controller hook
    // where it palys a crucial role in determine whether the acccess to the drvice
    // has been already checked at the upper level or not
    //
    Boolean               checkSecurity;
    
    //
    // set to true if at least one user has connected to the stack in
    // which the device is situated, the value must not be considered as an actual
    // testimony for an attachet user client, the value is used to find
    // whether the securities must be checked as the data might be sent
    // to a user or received from a user    
    //
    Boolean               userClientAttached;
    
    //
    // if true the device is on the boot and/or system disk path or
    // associated with the boot device, the current processing algorithm is very
    // simple - it processes only the IOMedia for the boot disk and its parents,
    // the value is also propagated to the children but not to siblings
    //
    Boolean               onBootDevicePath;
    
    //
    // the object's white list state
    //
    DldWhiteListState     whiteListState;
    
    //
    // a current PnP state for the object
    //
    DldPnPState           pnpState;
    
    //
    // if true the base intialization has been performed and should not be repeated
    //
    bool                  initialized;
    
    struct{
        
        //
        // 0x1 if at least one data stream was or is under CAWL control,
        //
        unsigned int      cawlControlled:0x1;
#if defined(DBG)
        unsigned int      baseClassPropertyFilled:0x1;
#endif // DBG
    } flags;
    
    //
    // each implementation must have an idempotent behaviour!
    //
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
    
    virtual void LockShared();
    virtual void UnLockShared();
    virtual void LockExclusive();
    virtual void UnLockExclusive();
    
    //
    // sets a service as a backing media for a virtual device,
    // the backing medium must be set in the descending order
    // to the tree root
    //
    virtual bool addVirtualBackingObject( __in IOService* service );
    
    //
    // get the lowest media in the chain for virtual devices, this media
    // defines the actual type of the virtual media, NULL is returned
    // for non-virtual devices, the returned object is referenced
    //
    virtual DldIOService* getLowestBackingObjectReferenceForVirtualObject();
    
    virtual void setMinorType( __in UInt32 requestedParentType, __in DldIOService* dldService );
    
    DldPropertyCommonData( __in DldObjectPropertyEntry* container );
    ~DldPropertyCommonData();
    
protected:
    
    //
    // an object containing this class instance's pointer
    // and managing the the instance lifetime
    //
    DldObjectPropertyEntry* container;
    
    //
    // a RW lock to protect the data
    //
    IORWLock*     rwLock;
    
#if defined(DBG)
    thread_t      exclusiveThread;
#endif//DBG
    
private:
    
    //
    // not allowed to create an instance without a container
    //
    DldPropertyCommonData();
    
};

//--------------------------------------------------------------------

//
// the USB device properties, i.e. a device on a usb bus
//
class DldIoUsbDevicePropertyData: public DldPropertyCommonData{
    
public:
    
    //
    // the device is described by its VID/PID/ID,
    // the device interfaces are described by its class
    //
    DldUsbVidPid        VidPid;
    
    //
    // a unique ID
    //
    DldDeviceUID        uid;
    
    USBDeviceAddress    usbDeviceAddress;
    
    //
    // a white list watermark is used to find whether
    // the most recent database state was used to 
    // set a device white list state
    //
    UInt32              whiteListWatermark;
    
    //
    // number of IOUSBInterfaces exported by the device
    //
    UInt32              numberOfInterfaces;
    
    struct{
        
        //
        // as USB PID can be 0x0 so it is impossible to define the pid validity
        // in terms of its value, vid validity flag was added for consistency
        //
        UInt32   vidValid:0x1;
        UInt32   pidValid:0x1;
        
    } flags;
    
    DldIoUsbDevicePropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// the usb device's interface property, a single device might expose
// several interfaces, i.e. interface-to-device relation is a multiple
// to one relation
//
class DldIoUsbInterfacePropertyData: public DldPropertyCommonData{
    
public:
    
    //
    // the property for a IOUSBDevce device which exhibits this interface,
    // the object is referenced,
    // the object is NULL if the interface property is an ancillary property
    // for the IOUSBDevice, this is done to avoid cycle in reference pointers
    //
    class DldObjectPropertyEntry*  usbDeviceProperty;
    
    //
    // the interface specific properties
    //
    UInt8               usbInterfaceClass;//the 0xFF value means "unknown"
    UInt8               usbInterfaceSubClass;
    UInt8               usbInterfaceProtocol;
    
    //
    // an array of endpoints for the interface, protected by the rwLock lock
    // of the property object which contains this property, zeroed entries are
    // allowed to be in the array,
    // memory is allocated by IOMalloc
    //
    IOUSBController::Endpoint* endpoints;
    
    //
    // number of entries in the endpoints array including zeroed entries
    //
    unsigned int               endpointsArrayEntriesCount;
    
    //
    // number of valid entries in the endpoints array
    // 
    unsigned int               endpointsArrayValidEntriesCount;
    
    DldIoUsbInterfacePropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    ~DldIoUsbInterfacePropertyData();
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// kPI_ prefix stands for Physical Interconnect
//
typedef enum {
    kPI_Unknown = 0x0,
    kPI_ATA,       // kIOPropertyPhysicalInterconnectTypeATA
    kPI_SATA,      // kIOPropertyPhysicalInterconnectTypeATA
    kPI_SAS,       // kIOPropertyPhysicalInterconnectTypeSerialAttachedSCSI
    kPI_ATAPI,     // kIOPropertyPhysicalInterconnectTypeATAPI
    kPI_USB,       // kIOPropertyPhysicalInterconnectTypeUSB
    kPI_FireWire,  // kIOPropertyPhysicalInterconnectTypeFireWire
    kPI_SCSIParal, // kIOPropertyPhysicalInterconnectTypeSCSIParallel
    kPI_FibreChnl, // kIOPropertyPhysicalInterconnectTypeFibreChannel
    kPI_Virtual,   // kIOPropertyPhysicalInterconnectTypeVirtual
} DldPhysicalInterconnect;

typedef struct _DldPhysicalInterconnectString{
    DldPhysicalInterconnect  physicalInterconnect;
    const char*              string;
} DldPhysicalInterconnectString;

//
// kPIL_ prefix stands for Physical Interconnect Location
//
typedef enum {
    kPIL_Unknown = 0x0,
    kPIL_Internal,  // kIOPropertyInternalKey
    kPIL_External,  // kIOPropertyExternalKey
    kPIL_IntExt,    // kIOPropertyInternalExternalKey
    kPIL_File,      // kIOPropertyInterconnectFileKey
    kPIL_RAM        // kIOPropertyInterconnectRAMKey
} DldPhysicalInterconnectLocation;

typedef struct _DldPhysicalInterconnectLocationString{
    DldPhysicalInterconnectLocation  physicalInterconnectLocation;
    const char*                      string;
} DldPhysicalInterconnectLocationString;

//
// the IOMedia property
//
class DldIOMediaPropertyData: public DldPropertyCommonData{
    
public:
    
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
    
    bool                              isRemovable;
    bool                              removableTypeDefined;
    
    bool                              fileVaultInfoGathered;
    bool                              parentInfoGathered;
    bool                              protocolCharacteristicsProcessed;
    
    DldPhysicalInterconnect           physicalInterconnect;
    DldPhysicalInterconnectLocation   physicalInterconnectLocation;
    
    OSString*                         BSDName; // a referenced object
    
    DldIOMediaPropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ){;};
    virtual ~DldIOMediaPropertyData(){ if( this->BSDName ) this->BSDName->release(); }
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
    
private:
    
    bool processProtocolCharacteristics( __in DldIOService* dldService );
    
    //
    // context is a pointer to a DldIOMediaPropertyData* object
    //
    static void processProtocolCharacteristicsApplierFunction( __in IORegistryEntry*  entry,
                                                               __in void*  context );
    
    void physicalInterconnectionApplierFunction( __in IORegistryEntry*  entry,
                                                 __in OSDictionary*     protocolCharacteristics );
    
    void fileVaultApplierFunction( __in IORegistryEntry*   entry,
                                   __in OSDictionary*      protocolCharacteristics );
}; 

//--------------------------------------------------------------------

//
// the IODVDMedia property
//
class DldIODVDMediaPropertyData: public DldIOMediaPropertyData{
    
public:
    
    DldIODVDMediaPropertyData( __in DldObjectPropertyEntry* container ): DldIOMediaPropertyData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// the IOSerialStreamSync property
//
class DldSerialPropertyData: public DldPropertyCommonData{
    
public:
    
    DldSerialPropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// the IOSCSIPeripheralDeviceType05 class device property, the Apple provided
// classes are
//  -	The IOSCSIPeripheralDeviceType00 driver supports block storage devices that comply
//      with the SCSI block commands specification.
//  -   The IOSCSIPeripheralDeviceType05 driver supports multimedia devices that comply
//      with the SCSI multimedia commands specification.
//  -   The IOSCSIPeripheralDeviceType07 driver supports magneto-optical devices that comply
//      with the SCSI block commands specification.
//  -   The IOSCSIPeripheralDeviceType0E driver supports reduced block command devices that comply
//      with the SCSI reduced block commands specification.
//
class DldIOSCSIPeripheralDeviceType05PropertyData: public DldPropertyCommonData{
    
public:
    
    //
    // a unique media ID, contains zeros if there is no media
    //
    DldDeviceUID    mediaUID;
    
    bool            uidValid;
    
    //
    // a thread which performs a UID retrieval, used
    // to skip check for the recursive calls
    //
    volatile thread_t        currentUidRetrievalThread;

    DldIOSCSIPeripheralDeviceType05PropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// for IOBluetoothDevice class, which instantiated for each paired device and
// attached to Bluetooth controller driver
//
class DldIoBluetoothDevicePropertyData: public DldPropertyCommonData{
    
public:

    DldIoBluetoothDevicePropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// a property for every HID device ( Mouse, Keyboard, Touchpad )
//
class DldIOHIDSystemPropertyData: public DldPropertyCommonData{
    
public:
    
    DldIOHIDSystemPropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// a property for Apple HID devices surface, like buttonless Apple MagicMouse
//
//
class DldAppleMultitouchDeviceUserClientPropertyData: public DldPropertyCommonData{
    
public:
    
    DldAppleMultitouchDeviceUserClientPropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// forward declaration
//
class DldNetworkInterfaceFilter;

typedef enum{
    kNetInterfaceFilterNotRegisterd = 0x0,  // the filter is not registered
    kNetInterfaceFilterBeingRegisterd,      // the filter is being registered
    kNetInterfaceFilterRegisterd            // the filter is registered
} DldNetInterfaceFilterState;

//
// a property for a network device
//
class DldNetIOInterfacePropertyData: public DldPropertyCommonData{
    
public:
    
    //
    // one of the DldNetInterfaceFilterState enum values
    //
    UInt32                      interfaceFilterState;
    
    //
    // a referenced object, may be NULL
    //
    class DldNetworkInterfaceFilter*  interfaceFilter;
    
    DldNetIOInterfacePropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    ~DldNetIOInterfacePropertyData();
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};


//
// a property for WiFi device, the WiFi interface class is a derivative from the IONetworkInterface class
// so the property object substitutes the more general network device property
//
class DldNet80211InterfacePropertyData: public DldNetIOInterfacePropertyData{
    
public:
    
    DldNet80211InterfacePropertyData( __in DldObjectPropertyEntry* container ): DldNetIOInterfacePropertyData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// a property for IEEE1394 devices
//
class DldIOFireWireDevicePropertyData: public DldPropertyCommonData{
    
public:
    
    DldIOFireWireDevicePropertyData( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

//
// a property for IEEE1394 SBP2 devices, a separate property is used instead checking DldIOFireWireDevicePropertyData for Unit_Spec_ID with 0x609E and
// Unit_SW_Version for 0x10483 that represens the SBP2 devices, if you decide to switch to Unit_Spec_ID and Unit_SW_Version checking it looks that a
// correct approach is creating a new property for IOFireWireUnit objects and associating the checks with the new property
//
class DldIOFireWireSBP2DeviceProperty: public DldPropertyCommonData{
    
public:
    
    DldIOFireWireSBP2DeviceProperty( __in DldObjectPropertyEntry* container ): DldPropertyCommonData( container ) {;};
    
    virtual bool fillPropertyData( __in DldIOService* dldService,
                                   __in bool forceUpdate );
};

//--------------------------------------------------------------------

class DldObjectPropertyEntry: public OSObject
{
    OSDeclareDefaultStructors( DldObjectPropertyEntry )
    
private:
    
    //
    // a RW lock to protect the data
    //
    IORWLock*     rwLock;
    
#if defined(DBG)
    thread_t      exclusiveThread;
#endif//DBG
    
    
    bool allocatePropertyDataWithType( __in DldObjectPropertyTypeDescriptor* typeDsc );
    void freePropertyData( DldPropertyCommonData* data );
    
public:
    
    //
    // if the postponePropertyUpdate value is "true" the property for the service will be created
    // but won't be updated so the caller must call updateProperty later, the created
    // property will not affect the security or shadowing status as the object type
    // will be undefined until updateDescriptor is called
    //
    static DldObjectPropertyEntry* withObject( __in IOService* service,
                                               __in DldIOService* dldService,
                                               __in bool postponePropertyUpdate = false,
                                               __in DldObjectPopertyTypeFlavor flavor = kDldObjectPopertyTypeFlavor_Primary );
    
    virtual void free( void );
    
    virtual void setPnPState( __in DldPnPState pnpState );
    virtual bool updateDescriptor( __in_opt IOService* service,
                                   __in DldIOService* dldService,
                                   __in bool forceUpdate );
    
private:
    //
    // the lock is not in the use, so the lock access functions have been declared private
    // to not be misused instead of dataU.property's lock
    //
    virtual void LockShared();
    virtual void UnLockShared();
    virtual void LockExclusive();
    virtual void UnLockExclusive();
    
    static void getMediaUID_WR( void* _mediaUidRequest ); // a worker routine that calls gCDDVDWhiteList->getMediaUID
    
public:
    
    virtual bool initDeviceLogData(
                  __in     DldRequestedAccess* action,
                  __in     DldFileOperation    operation, // a very limited use, valid only for IOMedia and its derivatives
                  __in     int32_t        pid,// BSD process ID
                  __in     kauth_cred_t   credential,
                  __in     bool           accessDisabled,
                  __inout  DldDriverDataLogInt*  intData// at least sizeof( intData->logData->device )
                  );
    
    //
    // the function can be applied only for the DldIOSCSIPeripheralDeviceType05PropertyData type,
    // if the media UID has not been initialized the function will start media UID initialization
    // which take from seconds to dozens of seconds as requires direct media read,
    // the dldMediaIOService must point to an IOMedia object or an object attached to a IOMedia one
    //
    virtual IOReturn initDVDMediaWL( __in DldIOService* dldMediaIOService );
    
    //
    // sets an object UID property for a DldIOService's IORegistryEntry
    //
    virtual void setUIDProperty();
    
    //
    // remove tha eaary of ancillary ptoperties, used to break the reference loop
    // betwen primary and ancillary properties before releasing the primary
    // property entry reference in DldIOService
    //
    virtual void removeAncillaryProperties();
    
    //
    // might return NULL, a caller is responsible for a retuned object releasing
    //
    virtual DldIOService* getReferencedDldIOService();
    
    //
    // get the lowest media in the chain for virtual devices, this media
    // defines the actual type of the virtual media, NULL is returned
    // for non-virtual devices, the returned object is referenced
    //
    virtual DldIOService* getLowestBackingObjectReferenceForVirtualObject();
    
    //
    // a USB device driver can be attached directly to
    // an IOUSBDevice nub instead of attaching to an IOUSBInterface nub,
    // for example see the bluetooth stack, in that case we will
    // hang a fake IOUSBDevice interface from an IOUSBDevice,
    // Ancillary properties is a simplified version of a real
    // property of the same type, only necessary fields are
    // valid and consistent,
    //
    // the array is protected by the lock
    //
    __opt OSArray*  ancillaryObjectPropertyEntries;
    
    //
    // a union of pointers to data for different device types,
    // the pointer can't change over the object life but
    // the content of the data can change and must be
    // protected by the rwLock lock
    //
    union{
        DldPropertyCommonData*                          property;
        DldIoUsbDevicePropertyData*                     usbDeviceProperty;
        DldIoUsbInterfacePropertyData*                  usbInterfaceProperty;
        DldIOMediaPropertyData*                         ioMediaProperty;
        DldIODVDMediaPropertyData*                      ioDVDMediaProperty;
        DldSerialPropertyData*                          serialProperty;
        DldIOSCSIPeripheralDeviceType05PropertyData*    ioSCSIPeripheralType05Property;
        DldIoBluetoothDevicePropertyData*               bluetoothDeviceProperty;
        DldIOHIDSystemPropertyData*                     hidSystemProperty;
        DldAppleMultitouchDeviceUserClientPropertyData* appleMultitouchProperty;
        DldNet80211InterfacePropertyData*               net80211InterfaceProperty;
    } dataU;
    
    //
    // an array of enum-to-name desriptors
    //
    static DldObjectPropertyTypeDescriptor*   typeDescr;
    
};

//--------------------------------------------------------------------

#endif// _DLDOBJECTPROPERTY_H