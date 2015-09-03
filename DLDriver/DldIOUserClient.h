/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOUSERCLIENT_H
#define _DLDIOUSERCLIENT_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODataQueue.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DeviceLockIOKitDriver.h"
#include "DldIOLog.h"
#include "DldIOUserClientRef.h"

//--------------------------------------------------------------------

class com_devicelock_driver_DeviceLockIOKitDriver;

//--------------------------------------------------------------------

class DldIOUserClient : public IOUserClient
{
    OSDeclareDefaultStructors( DldIOUserClient )
    
private:
    task_t                           fClient;
    proc_t                           fClientProc;
    pid_t                            fClientPID;
    
    //
    // data queues for kernel-user data transfer
    //
    IODataQueue*                     fDataQueue[ kt_DldNotifyTypeMax ];
    
    //
    // memory descriptors for the queues
    //
    IOMemoryDescriptor*              fSharedMemory[ kt_DldNotifyTypeMax ];
    
    //
    // mach port for notifications that the data is available in he queues
    //
    mach_port_t                      fNotificationPorts[ kt_DldNotifyTypeMax ];
    
    //
    // locks to serialize access to the queues
    //
    IOLock*                          fLock[ kt_DldNotifyTypeMax ];
    
    //
    // size of the queues, must be initialized before creating queues
    //
    UInt32                           fQueueSize[ kt_DldNotifyTypeMax ];
    //kauth_listener_t                 fListener;
    
    //
    // true if a user client calls kt_DldUserClientClose operation
    //
    Boolean                          clientClosedItself;
    
    //
    // an object to which this client is attached
    //
    com_devicelock_driver_DeviceLockIOKitDriver *fProvider;
    
protected:
    
    virtual void freeAllocatedResources();
    
    virtual void free();
    
private:
    
    IOReturn setVidPidWhiteListEx( __in  void *vInBuffer,//DldDeviceVidPidDscr
                                   __out void *vOutBuffer,
                                   __in  void *vInSize,
                                   __in  void *vOutSizeP,
                                   __in DldWhiteListType  listType);
    
    IOReturn setUIDWhiteListEx( __in  void *vInBuffer,//DldDeviceUIDDscr
                                __out void *vOutBuffer,
                                __in  void *vInSize,
                                __in  void *vOutSizeP,
                                __in DldWhiteListType  listType );
    
public:
    virtual bool     start( __in IOService *provider );
    virtual void     stop( __in IOService *provider );
    virtual IOReturn open(void);
    virtual IOReturn clientClose(void);
    virtual IOReturn close(void);
    virtual bool     terminate(IOOptionBits options);
    virtual IOReturn startLogging(void);
    virtual IOReturn stopLogging(void);
    
    virtual pid_t getPid(){ return this->fClientPID; }
    virtual bool isClientProc( __in proc_t proc ){ assert(this->fClientProc); return ( proc == this->fClientProc ); };
    
    virtual IOReturn registerNotificationPort( mach_port_t port, UInt32 type, UInt32 refCon);
    
    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                         IOMemoryDescriptor **memory);
    
    virtual IOReturn logData( __in DldDriverDataLogInt* data );
    virtual IOReturn shadowNotification( __in DldDriverShadowNotificationInt* data );
    virtual IOReturn diskCawlNotification( __in DldDriverDiskCAWLNotificationInt* data );
    virtual IOReturn socketFilterNotification( __in DldSocketFilterNotification* data );
    virtual IOReturn eventNotification( __in DldDriverEventData* data );
    
    virtual IOReturn setQuirks( __in  void *vInBuffer,//DldAclDescriptorHeader
                                __out void *vOutBuffer,
                                __in  void *vInSize,
                                __in  void *vOutSizeP,
                                void *, void *);
    
    virtual IOReturn setACL( __in  void *vInBuffer,
                             __out void *vOutBuffer,
                             __in  void *vInSize,
                             __in  void *vOutSizeP,
                             void *, void *);
    
    virtual IOReturn setShadowFile( __in  void *vInBuffer,//DldShadowFileDescriptorHeader
                                    __out void *vOutBuffer,
                                    __in  void *vInSize,
                                    __in  void *vOutSizeP,
                                    void *, void *);
    
    virtual IOReturn setUserState( __in  void *vInBuffer,//DldAclDescriptorHeader
                                   __out void *vOutBuffer,
                                   __in  void *vInSize,
                                   __in  void *vOutSizeP,
                                   void *, void *);
    
    virtual IOReturn setVidPidWhiteList( __in  void *vInBuffer,//DldDeviceVidPidDscr
                                        __out void *vOutBuffer,
                                        __in  void *vInSize,
                                        __in  void *vOutSizeP,
                                        void *, void *);
    
    virtual IOReturn setTempVidPidWhiteList( __in  void *vInBuffer,//DldDeviceVidPidDscr
                                             __out void *vOutBuffer,
                                             __in  void *vInSize,
                                             __in  void *vOutSizeP,
                                             void *, void *);
    
    virtual IOReturn setUIDWhiteList( __in  void *vInBuffer,//DldDeviceUIDDscr
                                      __out void *vOutBuffer,
                                      __in  void *vInSize,
                                      __in  void *vOutSizeP,
                                      void *, void *);
    
    virtual IOReturn setTempUIDWhiteList( __in  void *vInBuffer,//DldDeviceUIDDscr
                                          __out void *vOutBuffer,
                                          __in  void *vInSize,
                                          __in  void *vOutSizeP,
                                          void *, void *);
     
    
    virtual IOReturn setDVDWhiteList( __in  void *vInBuffer,//DldDeviceUIDDscr
                                      __out void *vOutBuffer,
                                      __in  void *vInSize,
                                      __in  void *vOutSizeP,
                                      void *, void *);
    
    virtual IOReturn processServiceDiskCawlResponse( __in  void *vInBuffer, // DldServiceDiskCawlResponse
                                                     __out void *vOutBuffer,
                                                     __in  void *vInSize,
                                                     __in  void *vOutSizeP,
                                                     void *, void *);
    
    virtual IOReturn processServiceSocketFilterResponse( __in  void *vInBuffer, // DldSocketFilterServiceResponse
                                                         __out void *vOutBuffer,
                                                         __in  void *vInSize,
                                                         __in  void *vOutSizeP,
                                                        void *, void *);
    
    virtual IOReturn setProcessSecurity( __in  void *vInBuffer, // DldProcessSecurity
                                         __out void *vOutBuffer,
                                         __in  void *vInSize,
                                         __in  void *vOutSizeP,
                                         void *, void *);
    
    virtual IOReturn setFileSecurity( __in  void *vInBuffer, // DldFileSecurity
                                      __out void *vOutBuffer,
                                      __in  void *vInSize,
                                      __in  void *vOutSizeP,
                                      void *, void *);
    
    virtual IOReturn setDLAdminProperties( __in  void *vInBuffer,
                                           __out void *vOutBuffer,
                                           __in  void *vInSize,
                                           __in  void *vOutSizeP,
                                           void *, void *);
    
    virtual IOReturn setEncryptionProviderProperties( __in  void *vInBuffer,
                                                      __out void *vOutBuffer,
                                                      __in  void *vInSize,
                                                      __in  void *vOutSizeP,
                                                      void *, void *);
    
    virtual IOReturn getDriverProperties( __in  void *vInBuffer,
                                          __out void *vOutBuffer,
                                          __in  void *vInSize,
                                          __in  void *vOutSizeP,
                                          void *, void *);
    
    virtual IOReturn reportIPCCompletion( __in  void *vInBuffer,
                                          __out void *vOutBuffer,
                                          __in  void *vInSize,
                                          __in  void *vOutSizeP,
                                         void *, void *);
    
    virtual IOExternalMethod *getTargetAndMethodForIndex(IOService **target,
                                                         UInt32 index);
    
    virtual DldAclObject* userModeFilesecToKernelACL( __in mach_vm_address_t filesecUserMode,//kauth_filesec
                                                      __in mach_vm_size_t    filesecSize,
                                                      __out bool*            isNullACL );
    
    //
    // alocates a memory in the kernel mode and copies a content of a user mode memory, the allocated memory is of
    // the same size as provided by the second parameter, the memory is allocated by a call to IOMalloc, a caller must free
    //
    virtual void* userModeMemoryToKernelMode( __in mach_vm_address_t  userAddress, __in mach_vm_size_t bufferSize );
    
    static DldIOUserClient* withTask( __in task_t owningTask );
    
};

//--------------------------------------------------------------------

#endif//_DLDIOUSERCLIENT_H