/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOSERVICE_H
#define _DLDIOSERVICE_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldCommonHashTable.h"
#include "DldIORegistryEntry.h"
#include "DldObjectProperty.h"

#define DLD_NOT_IN_ARRAY ((unsigned int)(-1))

#define DldStrPropertyDldIOService              "Dld: DldIOService"
#define DldStrPropertyKmodeName                 "Dld: Kmode Name"
#define DldStrPropertyClassName                 "Dld: Class Name"
#define DldStrPropertyPnPState                  "Dld: PnPState"
#define DldStrPropertyWhiteList                 "Dld: WhiteListed"
#define DldStrPropertyPropagateUp               "Dld: PropagateUp"
#define DldStrPropertyMajorType                 "Dld: MajorType"
#define DldStrPropertyMinorType                 "Dld: MinorType"
#define DldStrPropertyPropertyType              "Dld: PropertyType"
#define DldStrPropertyMajorTypeAncillary        "Dld: MajorTypeAnclillary"
#define DldStrPropertyMinorTypeAncillaey        "Dld: MinorTypeAncillary"
#define DldStrPropertyPropertyTypeAncillary     "Dld: PropertyTypeAncillary"
#define DldStrPropertyMediaUID                  "Dld: UID"
#define DldStrPropertyBootDevice                "Dld: BootDevice"

class DldIOService: public DldIORegistryEntry
{
    OSDeclareDefaultStructors( DldIOService )
    
private:
    
    //
    // a unique ID
    //
    UInt32        serviceID;
    
    //
    // the corresponding system's IOService object, not retained
    //
    IOService*    service;
    
    //
    // the object's properties, the retained( referenced ) object
    // might be NULL if there is no any properties class for the object,
    // the object's content is mutable and access must be protected
    // by the DldObjectPropertyEntry's lock ( not by the DldIOService's lock )
    //
    DldObjectPropertyEntry*   property;
    
    //
    // the children object properties array ( i.e. objects which are transitevly attached 
    // in the DldPlane ), the lesser index of the property the near the child in the chain
    // of the attached objects, the pointer might be NULL if there are no child properties,
    // the array's content is mutable and access must be protected by the rwLock lock
    //
    OSArray*      childProperties;
    
    //
    // the parents properties array ( i.e. objects to which the object is transitevly attached ),
    // the order of objects in the array is the same as the order of parents - the less index
    // the closer parent, the pointer might be NULL if there are no parent properties,
    // the array's content is mutable and access must be protected by the rwLock lock
    //
    OSArray*      parentProperties;
    
    //
    // number of parents
    //
    volatile SInt32 parentsCount;
    
    //
    // a RW lock to protect the internal data
    //
    IORWLock*     rwLock;
    
#if defined(DBG)
    thread_t      exclusiveThread;
#endif//DBG
    
    //
    // a PnP state of the object, affects the security checking - not started devices are not checked,
    // the copy of the state is un the property class ( if any )
    //
    DldPnPState    pnpState;
    
    //
    // flags to control a state of the IOService object
    //
    struct{
        unsigned int start:0x1;
        unsigned int willTerminate:0x1;
        unsigned int terminate:0x1;
        unsigned int didTerminate:0x1;
        unsigned int detached:0x1;
    } ioServiceFlags;
    
    //
    // counter for the number of outstanding reference to the service object
    // provided by this DldIOService object
    //
    SInt32         ioServiceRefCount;
    
    //
    // only one waiter at any time is allowed, if not NULL
    // then set to a signal state when ioServiceRefCount
    // drops to zero
    //
    UInt32*        ioServiceReleaseEvent;
    
    typedef enum _DldIOServiceOperation{
        kDldIOSO_Unknown = 0x0,    // 0x0
        kDldIOSO_init,             // 0x1
        kDldIOSO_probe,            // 0x2
        kDldIOSO_attach,           // 0x3
        kDldIOSO_detach,           // 0x4
        kDldIOSO_start,            // 0x5
        kDldIOSO_terminate,        // 0x6
        kDldIOSO_finalize,         // 0x7
        kDldIOSO_terminateClient,  // 0x8
        kDldIOSO_didTerminate,     // 0x9
        kDldIOSO_willTerminate,    // 0xA
        kDldIOSO_requestTerminate, // 0xB
        kDldIOSO_open              // 0xC
    } DldIOServiceOperation;
    
    //
    // an array of PnP operations performed on the service or by the service
    //
    DldIOServiceOperation   serviceOperationsLog[ 20 ];
    
    //
    // number of valid entires in the serviceOperations array
    //
    volatile SInt32         serviceOperationslogValidEntries;
    
    void logServiceOperation( __in DldIOServiceOperation  op );
    
    //
    // the function retrives and saves the device information for the device specific class
    //
    IOReturn setDeviceParameters();
    
    bool isObjectInArrayWOLock( __in_opt OSArray* array, __in_opt OSObject* object );
    unsigned int getObjectArrayIndex( __in_opt OSArray* array, __in_opt OSObject* object );
    
    static void RemoveChildPropertyApplierFunction( __in IORegistryEntry * entry, __in void * context );
    
    static void AddChildPropertyApplierFunction( __in IORegistryEntry * entry, __in void * context );
    
    static void AddParentPropertiesApplierFunction( __in IORegistryEntry * entry, __in void * context );
    
    static void AddParentPropertyApplierFunction( __in IORegistryEntry * entry, __in void * context );
    
    static void RemoveParentPropertiesApplierFunction( __in IORegistryEntry * entry, __in void * context );
    
protected:
    
    virtual bool init();
    virtual bool init( __in OSDictionary * PropertyDictionary );
    
    /*! @function free
     @abstract Frees data structures that were allocated by init()*/
    
    virtual void free( void );
    
    //
    // the following two functions work with ioServiceRefCount
    // and ioServiceReleaseEvent providing a mechanism for
    // tracking the outstanding references to the IOService object
    // provided through this DldIoService object
    //
    virtual void zeroReferenceNotification();
    virtual void waitForZeroReferenceNotification();
    
public:
   
    //
    // if the postponePropertyUpdate value is "true" the property for the service will be created
    // but won't be updated so the caller must call updateProperty later, the created
    // property will not affect the security or shadowing status as the object type
    // will be undefined until updateDescriptor is called
    //
    static DldIOService* withIOService( IOService* service, __in bool postponePropertyUpdate = false );
    
    //
    // if the postponePropertyUpdate value is "true" the property for the service will be created
    // but won't be updated so the caller must call updateProperty later, the created
    // property will not affect the security or shadowing status as the object type
    // will be undefined until updateDescriptor is called
    //
    static DldIOService* withIOServiceAddToHash( __in IOService* service, __in bool postponePropertyUpdate = false );
    
    //
    // returns a corresponding system IOService, the returned object is not guaranted
    // to be valid as the DldIOService may outlive the system IOService object
    //
    IOService* getSystemService(){ return this->service; }
    
    //
    // returns a referenced system IOService or NULL if it is unsafe to touch the object,
    // the returned object must be released by callin putSystemServiceRef();
    //
    IOService* getSystemServiceRef();
    void       putSystemServiceRef( IOService* service);
    
    UInt32     getServiceID(){ return this->serviceID; };
    
    //
    // returns a referenced( retained ) object for a DldIOService object corresponding
    // to a system's IOService object
    //
    static DldIOService* RetrieveDldIOServiceForIOService( __in IOService* service );
    
    //
    // the returned property is not referenced and might be NULL
    //
    virtual DldObjectPropertyEntry* getObjectProperty(){ return this->property;};
    
    //
    // the returned arrays are not referenced,
    // a returned array contains objects of the DldObjectPropertyEntry type,
    // the caller must acquire the DldIOService lock before calling this
    // function as the parent and children properties might change dynamically
    //
    virtual const OSArray* getParentProperties(){ return this->parentProperties; };
    virtual const OSArray* getChildProperties(){ return this->childProperties; };
    
    
    //
    // the following functions for properties adding and removing acquire
    // the service lock internaly so a callr must not acquire lock
    //
    virtual bool addParentProperty( __in_opt DldObjectPropertyEntry*  property );
    virtual bool addParentProperties( __in_opt const OSArray*  properties );
    virtual void removeParentProperties( __in OSArray* propertiesToRemove );
    virtual bool addChildProperty( __in_opt DldObjectPropertyEntry*  property );
    virtual void removeChildProperty( __in_opt DldObjectPropertyEntry*  property );
    
    //
    // returns true if the object has multiple parents
    //
    bool hasMultipleParents() { return ( this->parentsCount > 0x1 ); };
    
    //
    // returns true if the object in the parents array, the function
    // acquires the lock internaly so a caller must not acquire the lock
    //
    virtual bool isObjectInParentProperties( __in_opt DldObjectPropertyEntry*  property );
    
    //
    // returns a referenced property from the property array for the object
    // ( i.e. a parent, this object's property or a child property ),
    // the previous index allows to iterate through the entries of the same type,
    // to start from the beginning start from (-1) which is a default value
    //
    virtual DldObjectPropertyEntry* retrievePropertyByTypeRef( __in DldObjectPopertyType  type, __in int previousIndex = (-1) );
    
    //
    // remove this entry from the hash table, the entry is released when is being removed from the hash table
    //
    virtual void removeFromHashTable();
    
    /* methods available in Mac OS X 10.1 or later */
    /*! @function requestTerminate
     @abstract Passes a termination up the stack.
     @discussion When an IOService is made inactive the default behavior is to also make any of its clients
                 that have it as their only provider also inactive, in this way recursing the termination up
                 the driver stack. Returning <code>true</code> from this method when passed a just terminated
                 provider will cause the client to also be terminated.
     @param provider The terminated provider of this object.
     @param options Options originally passed to terminate, plus <code>kIOServiceRecursing</code>.
     @result <code>true</code> if this object should be terminated now that its provider has been. */
    
    virtual bool requestTerminate( DldIOService * provider, IOOptionBits options );
    
    /*! @function willTerminate
     @abstract Passes a termination up the stack.
     @discussion Notification that a provider has been terminated, sent before recursing up the stack,
                 in root-to-leaf order.
     @param provider The terminated provider of this object.
     @param options Options originally passed to terminate.
     @result <code>true</code>. */
    
    virtual bool willTerminate( DldIOService * provider, IOOptionBits options );
    
    /*! @function didTerminate
     @abstract Passes a termination up the stack.
     @discussion Notification that a provider has been terminated, sent after recursing up the stack,
                 in leaf-to-root order.
     @param provider The terminated provider of this object.
     @param options Options originally passed to terminate.
     @param defer If there is pending I/O that requires this object to persist, and the provider is not opened
            by this object set <code>defer</code> to <code>true</code> and call
            the <code>IOService::didTerminate()</code> implementation when the I/O completes.
            Otherwise, leave <code>defer</code> set to its default value of <code>false</code>.
     @result <code>true</code>. */
    
    virtual bool didTerminate( DldIOService * provider, IOOptionBits options, bool * defer );
    
    /*! @function terminateClient
     @abstract Passes a termination up the stack.
     @discussion When an IOService object is made inactive the default behavior is to also make any of its
                 clients that have it as their only provider inactive, in this way recursing the termination up
                 the driver stack.
     @param client The client of the terminated provider.
     @param options Options originally passed to @link terminate terminate@/link, plus <code>kIOServiceRecursing</code>.
     @result result of the terminate request on the client. */
    
    virtual bool terminateClient( DldIOService * client, IOOptionBits options );
    
    /*! @function terminate
     @abstract Makes an IOService object inactive and begins its destruction.
     @discussion Registering an IOService object informs possible clients of its existance and instantiates drivers
                 that may be used with it; <code>terminate</code> involves the opposite process of informing clients
                 that an IOService object is no longer able to be used and will be destroyed. By default, if any
                 client has the service open, <code>terminate</code> fails. If the <code>kIOServiceRequired</code>
                 flag is passed however, <code>terminate</code> will be successful though further progress in
                 the destruction of the IOService object will not proceed until the last client has closed it.
                 The service will be made inactive immediately upon successful termination, and all its clients
                 will be notified via their @link message message@/link method with a message of type
                 <code>kIOMessageServiceIsTerminated</code>. Both these actions take place on the caller's thread.
                 After the IOService object is made inactive, further matching or attach calls will fail on it.
                 Each client has its @link stop stop@/link method called upon their close of an inactive IOService
                 object , or on its termination if they do not have it open. After <code>stop</code>, @link detach
                 detach@/link is called in each client. When all clients have been detached, the @link finalize finalize@/link
                 method is called in the inactive service. The termination process is inherently asynchronous because
                 it will be deferred until all clients have chosen to close.
     @param options In most cases no options are needed. <code>kIOServiceSynchronous</code> may be passed to cause 
            <code>terminate</code> to not return until the service is finalized. */
    
    virtual bool terminate( IOOptionBits options = 0 );
    
    /*! @function finalize
     @abstract Finalizes the destruction of an IOService object.
     @discussion The <code>finalize</code> method is called in an inactive (ie. terminated) IOService object after the
                 last client has detached. IOService's implementation will call @link stop stop@/link, @link close close@/link,
                 and @link detach detach@/link on each provider. When <code>finalize</code> returns, the object's retain
                 count will have no references generated by IOService's registration process.
     @param options The options passed to the @link terminate terminate@/link method of the IOService object are passed
                 on to <code>finalize</code>.
     @result <code>true</code>. */
    
    virtual bool finalize( IOOptionBits options );
    
    /*! @function attach
     @abstract Attaches an IOService client to a provider in the I/O Registry.
     @discussion This function called in an IOService client enters the client into the I/O Registry as a child of
                 the provider in the service plane. The provider must be active or the attach will fail.
                 Multiple attach calls to the same provider are no-ops and return success.
                 A client may be attached to multiple providers. Entering an object into the I/O Registry
                 retains both the client and provider until they are detached.
     @param provider The IOService object which will serve as this object's provider.
     @result <code>false</code> if the provider is inactive or on a resource failure; otherwise <code>true</code>. */
    
    virtual bool attach( DldIOService * provider );
    
    /*! @function start
     @abstract During an IOService object's instantiation, starts the IOService object that has been selected to run on the provider.
     @discussion The <code>start</code> method of an IOService instance is called by its provider when it has been selected (due to its probe score and match category) as the winning client. The client is already attached to the provider when <code>start</code> is called.<br>Implementations of <code>start</code> must call <code>start</code> on their superclass at an appropriate point. If an implementation of <code>start</code> has already called <code>super::start</code> but subsequently determines that it will fail, it must call <code>super::stop</code> to balance the prior call to <code>super::start</code> and prevent reference leaks.
     @result <code>true</code> if the start was successful; <code>false</code> otherwise (which will cause the instance to be detached and usually freed). */
    
    virtual bool start( __in_opt IOService* service, __in_opt DldIOService * provider );
    
    /*! @function detach
     @abstract Detaches an IOService client from a provider in the I/O Registry.
     @discussion This function called in an IOService client removes the client as a child of the provider
                 in the service plane of the I/O Registry. If the provider is not a parent of the client
                 this is a no-op, otherwise the I/O Registry releases both the client and provider.
     @param provider The IOService object to detach from. */
    
    virtual void detach( DldIOService * provider );
    
    /*! @function open
     @abstract Requests active access to a provider.
     @discussion IOService provides generic open and close semantics to track clients of a provider that have established an active datapath. The use of <code>open</code> and @link close close@/link, and rules regarding ownership are family defined, and defined by the @link handleOpen handleOpen@/link and @link handleClose handleClose@/link methods in the provider. Some families will limit access to a provider based on its open state.
     @param forClient Designates the client of the provider requesting the open.
     @param options Options for the open. The provider family may implement options for open; IOService defines only <code>kIOServiceSeize</code> to request the device be withdrawn from its current owner.
     @result <code>true</code> if the open was successful; <code>false</code> otherwise. */
    
    virtual bool open( __in IOService*    service,
                       __in IOService *   forClient,
                       __in IOOptionBits  options,
                       __in void *		  arg );
    
    virtual void setPnPState( __in DldPnPState pnpState );
    virtual DldPnPState getPnPState();
    
    //
    // called when a first user client is attached, or there is no user
    // clients for a stack ( e.g. disks ) so we consider a stack having
    // a fake user client, the value must not be considered as an actual
    // testimony for an attachet user client, the value is used to find
    // whether the securities must be checked as the data might be sent
    // to a user or received from a user
    //
    virtual void userClientAttached();
    
    //
    // called to mark the device as processing requests to boot device
    //
    virtual void deviceIsOnBootPath();
    
    //
    // returns true if the device is on the boot device path
    //
    virtual Boolean isOnBootDevicePath();
    
    virtual void LockShared();
    virtual void UnLockShared();
    virtual void LockExclusive();
    virtual void UnLockExclusive();
    
    //
    // the caller must set intData->logDataSize to the size the allocated *intData->logData,
    // the caller must allocate a room for *intData->logData of at least sizeof( intData->logData->device ),
    // on return the function initializes the fields for *intData->logData
    // and sets intData->logDataSize according to the length of the apropriate initialized data,
    // so the caller must not consider this field as a real full size of the 
    // intData->logData memory which he has allocated before calling this function
    //
    virtual bool initDeviceLogData( __in     DldRequestedAccess* action,
                                    __in     DldFileOperation    operation, // a very limited use, valid only for IOMedia and its derivatives
                                    __in     int32_t        pid,// BSD process ID
                                    __in     kauth_cred_t   credential,
                                    __in     bool           accessDisabled,
                                    __inout  DldDriverDataLogInt*  intData// at least sizeof( intData->logData->device )
                                  );
    
    //
    // the same as initDeviceLogData but inits log data for a parent which defines the minor device type
    //
    virtual bool initParentDeviceLogData( __in     DldRequestedAccess* action,
                                          __in     DldFileOperation    operation, // a very limited use, valid only for IOMedia and its derivatives
                                          __in     int32_t        pid,// BSD process ID
                                          __in     kauth_cred_t   credential,
                                          __in     bool           accessDisabled,
                                          __inout  DldDriverDataLogInt*  intData// at least sizeof( intData->logData->device )
                                         );
    
    //
    // we need to provide our own serialization function as DldIOService objects holds a reference to the property
    // objects of IOService object and calling serialize() on them might have a side effect as happened with
    // network properties - a network property class contained a pointer to a callback which became invalid
    // on device removal but was called from DldIOService object's property as an application saved an object
    // port in its IPC namespace and the object somehow preserved all properties
    //
    virtual bool serializeProperties( OSSerialize * s ) const;
    
    //
    // this functions attaches bluetooth HCI and SerialManager's objects to a USB device object
    //
    virtual void processBluetoothStack();
    
 };

#endif//_DLDIOSERVICE_H