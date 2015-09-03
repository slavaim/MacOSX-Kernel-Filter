/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDCOMMON_H
#define _DLDCOMMON_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <libkern/OSAtomic.h>
#include "DldUserToKernel.h"

#if !defined(__i386__) && !defined(__x86_64__)
    #error "Unsupported architecture"
#endif

extern DldDriverProperties  gDriverProperties;
extern SInt32 gLogIndx;

//
// DBG for debug
// _DLD_LOG - for log
// _DLD_LOG_ERRORS - for error log
//
#if defined(DBG)
    #define DLD_INVALID_POINTER_VALUE ((long)(-1))
    #define DLD_DBG_MAKE_POINTER_INVALID( _ptr ) do{ (*(long*)&_ptr) = DLD_INVALID_POINTER_VALUE; }while(0);
    #define DLD_IS_POINTER_VALID( _ptr ) ( NULL != _ptr && DLD_INVALID_POINTER_VALUE != (long)_ptr )
#else//DBG
    #define DLD_DBG_MAKE_POINTER_INVALID( _ptr )  do{void(0);}while(0);
    #define DLD_IS_POINTER_VALID( _ptr ) ( NULL != _ptr )
#endif//DBG


//
// the sleep is required to allow the logger to catch up,
// the macro requires a boolean variable isError being defined in the outer scope,
// the macro uses the internal DldLog if available or the Apple System Logger( ASL ) in the other case
//
#define DLD_COMM_LOG_EXT( _S_ ) do{\
        if( gInternalLog ){\
            SInt32 logIndex = OSIncrementAtomic( &gLogIndx ); \
            gInternalLog->Log(" [%-7d] DldLog:", (int)logIndex ); \
            gInternalLog->Log( "%s %s(%u):%s: ", isError?"ERROR!!":"", __FILE__ , __LINE__, __PRETTY_FUNCTION__ );\
            gInternalLog->Log _S_ ; \
        } else { SInt32 logIndex = OSIncrementAtomic( &gLogIndx ); \
            IOLog(" [%-7d] DldLog:", (int)logIndex ); \
            IOLog("%s %s(%u):%s: ", isError?"ERROR!!":"", __FILE__ , __LINE__, __PRETTY_FUNCTION__ );\
            IOLog _S_ ; \
            IOSleep( gGlobalSettings.millisecondsForLogToCatchUp );\
        }\
    }while(0);

//
// ASL - Apple System Logger,
// the macro requires a boolean variable isError being defined in the outer scope,
// the macro uses only ASL
//
#define DLD_COMM_LOG_EXT_TO_ASL( _S_ ) do{\
    SInt32 logIndex = OSIncrementAtomic( &gLogIndx ); \
    IOLog(" [%-7d] DldLog:", (int)logIndex ); \
    IOLog("%s %s(%u):%s: ", isError?"ERROR!!":"", __FILE__ , __LINE__, __PRETTY_FUNCTION__ );\
    IOLog _S_ ; \
    IOSleep( gGlobalSettings.millisecondsForLogToCatchUp );\
}while(0);

//
// a common log
//
#if !defined(_DLD_LOG)

 #define DBG_PRINT( _S_ )   do{ void(0); }while(0);// { kprintf _S_ ; }

#else

 //
 // IOSleep called after IOLog allows the system to replenish the log buffer by retrieving the existing entries using syslogd
 //
 #define DBG_PRINT( _S_ )  do{ bool  isError = false; DLD_COMM_LOG_EXT( _S_ ); }while(0);

#endif


//
// errors log
//
#if !defined(_DLD_LOG_ERRORS)

    #define DBG_PRINT_ERROR( _S_ )   do{ void(0); }while(0);//DBG_PRINT( _S_ )

#else

    #define DBG_PRINT_ERROR( _S_ )   do{ if( gGlobalSettings.logErrors ){ bool  isError = true; DLD_COMM_LOG_EXT( _S_ ); } }while(0);

#endif

//
// errors log to ASL
//
#if !defined(_DLD_LOG_ERRORS)

    #define DBG_PRINT_ERROR_TO_ASL( _S_ )   do{ void(0); }while(0);//DBG_PRINT( _S_ )

#else

    #define DBG_PRINT_ERROR_TO_ASL( _S_ )   do{ if( gGlobalSettings.logErrors ){ bool  isError = true; DLD_COMM_LOG_EXT_TO_ASL( _S_ ); } }while(0);

#endif

//
// common log, should not be used for errors
//
#define DLD_COMM_LOG( _TYPE_ , _S_ ) do{ if( gGlobalSettings.logFlags._TYPE_ ){ bool  isError = false; DLD_COMM_LOG_EXT( _S_ ); } }while(0);

//
// ASL common log
//
#define DLD_COMM_LOG_TO_ASL( _TYPE_ , _S_ ) do{ if( gGlobalSettings.logFlags._TYPE_ ){ bool  isError = false; DLD_COMM_LOG_EXT_TO_ASL( _S_ ); } }while(0);

//--------------------------------------------------------------------

#define __in
#define __out
#define __inout
#define __in_opt
#define __out_opt
#define __opt

//--------------------------------------------------------------------

#include "DldInternalLog.h"
class DldInternalLog;
extern DldInternalLog*   gInternalLog;

//--------------------------------------------------------------------

#define DLD_STATIC_ARRAY_SIZE( _ARR_ ) ( (unsigned int)( sizeof(_ARR_)/sizeof(_ARR_[0] ) ) )

//--------------------------------------------------------------------

//
// an addition to the native sloppy circle implementation
//
#define	CIRCLEQ_INIT_WITH_TYPE(head, type) do {  \
    (head)->cqh_first = (type *)(head); \
    (head)->cqh_last = (type *)(head);  \
} while (0)


#define CIRCLEQ_INSERT_HEAD_WITH_TYPE(head, elm, field, type) do {			\
    (elm)->field.cqe_next = (head)->cqh_first;			\
    (elm)->field.cqe_prev = (type *)(head);				\
    if ((head)->cqh_last == (type *)(head))				\
        (head)->cqh_last = (elm);				\
    else								\
        (head)->cqh_first->field.cqe_prev = (elm);		\
    (head)->cqh_first = (elm);					\
} while (0)

#define CIRCLEQ_INSERT_TAIL_WITH_TYPE(head, elm, field, type) do {			\
    (elm)->field.cqe_next = (type *)(head);				\
    (elm)->field.cqe_prev = (head)->cqh_last;			\
    if ((head)->cqh_first == (type *)(head))			\
        (head)->cqh_first = (elm);				\
    else								\
        (head)->cqh_last->field.cqe_next = (elm);		\
    (head)->cqh_last = (elm);					\
} while (0)

//--------------------------------------------------------------------

#define FIELD_OFFSET(Type,Field) (reinterpret_cast<unsigned long long>( (&(((Type *)(0))->Field)) ) )

//--------------------------------------------------------------------

//
// Double linked list manipulation functions, the same as on Windows
//

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

inline
void
InitializeListHead(
    __inout PLIST_ENTRY ListHead
    )
{
    ListHead->Flink = ListHead->Blink = ListHead;
}

inline
bool
IsListEmpty(
    __in const LIST_ENTRY * ListHead
    )
{
    return (bool)(ListHead->Flink == ListHead);
}

inline
bool
RemoveEntryList(
    __in PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Flink;
    
    Flink = Entry->Flink;
    Blink = Entry->Blink;
    Blink->Flink = Flink;
    Flink->Blink = Blink;
    return (bool)(Flink == Blink);
}

inline
PLIST_ENTRY
RemoveHeadList(
    __in PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Flink;
    PLIST_ENTRY Entry;
    
    Entry = ListHead->Flink;
    Flink = Entry->Flink;
    ListHead->Flink = Flink;
    Flink->Blink = ListHead;
    return Entry;
}

inline
PLIST_ENTRY
RemoveTailList(
    __in PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Entry;
    
    Entry = ListHead->Blink;
    Blink = Entry->Blink;
    ListHead->Blink = Blink;
    Blink->Flink = ListHead;
    return Entry;
}

inline
void
InsertTailList(
    __in PLIST_ENTRY ListHead,
    __in PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Blink;
    
    Blink = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Blink;
    Blink->Flink = Entry;
    ListHead->Blink = Entry;
}


inline
void
InsertHeadList(
    __in PLIST_ENTRY ListHead,
    __in PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Flink;
    
    Flink = ListHead->Flink;
    Entry->Flink = Flink;
    Entry->Blink = ListHead;
    Flink->Blink = Entry;
    ListHead->Flink = Entry;
}

inline
void
AppendTailList(
    __in PLIST_ENTRY ListHead,
    __in PLIST_ENTRY ListToAppend
    )
{
    PLIST_ENTRY ListEnd = ListHead->Blink;
    
    ListHead->Blink->Flink = ListToAppend;
    ListHead->Blink = ListToAppend->Blink;
    ListToAppend->Blink->Flink = ListHead;
    ListToAppend->Blink = ListEnd;
}

//---------------------------------------------------------------------

//
// Calculate the address of the base of the structure given its type, and an
// address of a field within the structure.
//

#define CONTAINING_RECORD(address, type, field) ((type *)( \
                                        (char*)(address) - \
                                        reinterpret_cast<vm_address_t>(&((type *)0)->field)))

//---------------------------------------------------------------------

#define DlSetFlag( Flag, Value )    ((Flag) |= (Value))
#define DlClearFlag( Flag, Value )    ((Flag) &= ~(Value))
#define DlIsFlagOn( Flag, Value )   ( ( (Flag) & (Value) ) != 0x0 )

//--------------------------------------------------------------------

#ifndef OSCompareAndSwapPtr
    /*
     10.5 SDK doesn't define OSCompareAndSwapPtr, so this is an easy way to find that this is a 10.5 compilation,
     the Apple document http://developer.apple.com/library/ios/#documentation/DeveloperTools/Conceptual/cross_development/Using/using.html#//apple_ref/doc/uid/20002000-SW6
     is incomplete and misguiding
     */
    #define DLD_MACOSX_10_5 (0x1)
#endif//

#ifdef DLD_MACOSX_10_5

    //
    // 10.6 SDK uses SAFE_CAST_PTR to cast pointers, 10.5 doesn't
    //
    #define OSCompareAndSwap(a, b, c) \
    (OSCompareAndSwap((UInt32)a, (UInt32)b, (volatile UInt32*)c ))

    /*!
     * @function OSCompareAndSwapPtr
     *
     * @abstract
     * Compare and swap operation, performed atomically with respect to all devices that participate in the coherency architecture of the platform.
     *
     * @discussion
     * The OSCompareAndSwapPtr function compares the pointer-sized value at the specified address with oldVal. The value of newValue is written to the address only if oldValue and the value at the address are equal. OSCompareAndSwapPtr returns true if newValue is written to the address; otherwise, it returns false.
     *
     * This function guarantees atomicity only with main system memory. It is specifically unsuitable for use on noncacheable memory such as that in devices; this function cannot guarantee atomicity, for example, on memory mapped from a PCI device. Additionally, this function incorporates a memory barrier on systems with weakly-ordered memory architectures.
     * @param oldValue The pointer value to compare at address.
     * @param newValue The pointer value to write to address if oldValue compares true.
     * @param address The pointer-size aligned address of the data to update atomically.
     * @result true if newValue was written to the address.
     *
     extern Boolean OSCompareAndSwapPtr(
     void            * oldValue,
     void            * newValue,
     void * volatile * address);
     */
    #if defined(__x86_64__)

        #error "we don't support 10.5 64 bit kernel"

        /*!
         * @function OSCompareAndSwap64
         *
         * @abstract
         * 64-bit compare and swap operation.
         *
         * @discussion
         * See OSCompareAndSwap.
         */
        #define OSCompareAndSwapPtr(a, b, c) \
        (OSCompareAndSwap64((UInt64)a, (UInt64)b, (volatile UInt64*)c))

    #endif//defined(__x86_64__)


    #if defined(__i386__)

        #define OSCompareAndSwapPtr(a, b, c) \
        (OSCompareAndSwap((UInt32)a, (UInt32)b, (volatile UInt32*)c ))

    #endif//defined(__i386__)

    // 10.6 SDK uses SAFE_CAST_PTR for OSIncrementAtomic's parameter, 10.5 doesn't
    #define OSIncrementAtomic(a) \
    (OSIncrementAtomic((volatile SInt32*)a))

    #define OSDecrementAtomic(a) \
    (OSDecrementAtomic((volatile SInt32*)a))

    #define vnode_getwithref(v) (vnode_get(v))

    extern "C"
    int vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base,
                    int len, off_t offset, enum uio_seg segflg, int ioflg,
                    kauth_cred_t cred, int *aresid, struct proc *p);

    // actually this property doesn't exists on 10.5 , the ID should be retrieved by inquiring a device
    #define kUSBSerialNumberString		"USB Serial Number"

#endif//DLD_MACOSX_10_5

//--------------------------------------------------------------------

typedef struct _DldDriverDataLogInt{
    
    //
    // the size of the logData buffer
    //
    UInt32             logDataSize;
    
    //
    // log data, the size of the buffer might be smaller than
    // defined by sizeof( DldDriverDataLog ) as the structure's
    // name buffer member might be cut
    //
    DldDriverDataLog*  logData;
    
} DldDriverDataLogInt;

//--------------------------------------------------------------------

typedef struct _DldDriverShadowNotificationInt{
    
    //
    // the size of the notifyData buffer
    //
    UInt32             notifyDataSize;
    
    //
    // notification data
    //
    DldDriverShadowNotification*  notifyData;
    
} DldDriverShadowNotificationInt;

//--------------------------------------------------------------------

typedef struct _DldDriverDiskCAWLNotificationInt{

#if defined(DBG)
#define DLD_CAWL_NOTIFY_SIGNATURE   0xAB1398CD
    UInt32              signature;
#endif(DBG)
    
    //
    // the size of the notifyData buffer
    //
    UInt32              notifyDataSize;
    
    //
    // notification data, removed after sending, so the DldDriverDiskCAWLNotificationInt
    // can outlive *notifyData for a prolonged period of time, when removed the pointer set
    // to NULL
    //
    DldDriverDiskCAWLNotification*  notifyData;
    
    //
    // used to anchor entries in the list headed in the
    // DldIOVnode object, the list is used to wait for
    // the service responce for vnode create and open
    //
    LIST_ENTRY          listEntry;
    
    //
    // a notification ID, the copy from the notifyData->notificationID,
    // used to find the notification data in the list by ID as the notifyData
    // might be removed to save the pool
    //
    UInt64              notificationID;
    
    //
    // service response flags
    //
    bool                responseReceived;
    bool                disableAccess;
    bool                logOperation;
    
} DldDriverDiskCAWLNotificationInt;

//--------------------------------------------------------------------

//
// log flags are used by DLD_COMM_LOG() to filter the log output,
// the structure is fiiled from a property list by
// com_devicelock_driver_DeviceLockIOKitDriver::fillLogFlags()
//
typedef struct _DldLogFlags{
    
    //
    // general type messages or that do not fall in any known type
    //
    bool    COMMON;
    
    bool    QUIRKS;
    
    //
    // print SCSI requests, slightly verbose
    //
    bool    SCSI;
    
    bool    DVD_WHITE_LIST;
    
    //
    // turns on the white list subsystem log
    //
    bool    WHITE_LIST;
    
    //
    // turns on a log for DldKauthAclEvaluate, an extremely verbose output! you were warned!
    //
    bool    ACL_EVALUATUION;
    
    //
    // logs denied access from isAccessAllowed
    //
    bool    ACCESS_DENIED;
    
    bool    FILE_VAULT;
    
    bool    POWER_MANAGEMENT;
    
    //
    // logs access to the devices from the log routine, this allows to determine whether
    // the log entry has been created or not
    //
    bool    DEVICE_ACCESS;
    
    //
    // logs socket packets dynamics
    //
    bool    NET_PACKET_FLOW;
    
} DldLogFlags;

//
// the global settings structure is used to store
// the global settings of uncommon nature in one place
// to not disperse them through the code that
// makes them hard to track and easy to forget
// of their existence
//
typedef struct _DldGlobalSettings{
    
    //
    // contains a number of milliseconds to wait before
    // adding a new entry in the log, this pause allows
    // a user mode part to catch up and avoid log entries
    // lossing, the value influences on DLD_COMM_LOG,
    // used only for the ASL log, the internal log
    // is fast enougth to handle a short flood of messages
    //
    unsigned        millisecondsForLogToCatchUp;
    
    //
    // contains a number of milliseconds to wait
    // before repeatng an attempt to send shadow or
    // log notifications, the service should remove
    // some entries while a thread trying to put
    // a new entry has been put in a sleep state
    //
    unsigned        millisecondsWaitForFreeSpaceInNoificationsBuffer;
    
    //
    // works together with millisecondsWaitForFreeSpaceInNoificationsBuffer
    // and is a number of attempts to send a buffer after waiting
    // for millisecondsWaitForFreeSpaceInNoificationsBuffer, if 0x0
    // no resend attempts will be made
    //
    SInt32          maxAttemptsToResendBuffer;      
    
    //
    // print out errors, changes DBG_PRINT_ERROR behaviour
    //
    bool            logErrors;
    
    //
    // logFlags are used by DLD_COMM_LOG
    //
    DldLogFlags     logFlags;
    
    //
    // a path for the log file
    //
    OSString*       logFilePath;
    
    //
    // if true the CAWL flushes data synchronously, only for the test
    //
    bool            flushCAWL;
    
    //
    // if true the VFS hooks are skipped
    //
    bool            doNotHookVFS;
    
} DldGlobalSettings;

extern DldGlobalSettings           gGlobalSettings;

//--------------------------------------------------------------------

//
// the structure is used to facilitate the debug as sometimes gdb
// pick ups a wrong address for a global variable ( I suspect mainly static ones )
//
typedef struct _DldDbgData{
    
    void*           com_devicelock_driver_DeviceLockIOKitDriver;
    void*           gGlobalSettings;
    void*           spawnThread1;
    void*           gDldRootEntry;
    void*           userClient;
    void*           gDldDeviceTreePlan;
    void*           sVnodesHashTable;
    void*           sVnodeHooksHashTable;
    void*           sSparseFilesHashTable;
    void*           sRegistryEntriesHashTable;
    void*           gSCSITaskListHead;
    void*           gLog;
    void*           gShadow;
    void*           gDiskCAWL;
    void*           gWhiteList;
    void*           gCredsArray;
    void*           gHookEngine;
    void*           gVnodeGate;
    void*           gMntCache;
    void*           gArrayOfAclArrays;
    void*           gInternalLog;
    void*           gCDDVDWhiteList;
    void*           gPMNotifier;
    void*           gSocketFilter;
    void*           gServiceProtection;
    vm_offset_t     gTaskOffset;
    vm_offset_t     gProcOffset;
    vm_offset_t     gScsiCallbackOffset;
} DldDbgData;

extern DldDbgData   gDldDbgData;

//--------------------------------------------------------------------

#define ALL_READ_NOENCRYPT  ( DEVICE_DIRECT_READ | \
                              DEVICE_EJECT_MEDIA | \
                              DEVICE_DIR_LIST |    \
                              DEVICE_READ | \
                              DEVICE_EXECUTE | \
                              DEVICE_PLAY_AUDIO_CD )

#define ALL_WRITE_NOENCRYPT  ( DEVICE_DIRECT_WRITE | \
                               DEVICE_VOLUME_DEFRAGMENT | \
                               DEVICE_DIR_CREATE | \
                               DEVICE_WRITE | \
                               DEVICE_RENAME | \
                               DEVICE_DELETE )

#define ALL_FORMAT_NOENCRYPT  ( DEVICE_DISK_FORMAT )

//--------------------------------------------------------------------

#define ALL_READ  ( ALL_READ_NOENCRYPT | \
                    DEVICE_ENCRYPTED_READ )

#define ALL_WRITE  ( ALL_WRITE_NOENCRYPT | \
                     DEVICE_ENCRYPTED_WRITE | \
                     DEVICE_ENCRYPTED_DIRECT_WRITE )

#define ALL_FORMAT  ( ALL_FORMAT_NOENCRYPT | \
                      DEVICE_ENCRYPTED_DISK_FORMAT )

//--------------------------------------------------------------------

//
// do not add encrypted rights here, this rights will be converted to encrypted ones for encrypted vnodes
//
#define READ_IO  ( DEVICE_DIRECT_READ | \
                   DEVICE_READ )

#define WRITE_IO ( DEVICE_DIRECT_WRITE | \
                   DEVICE_WRITE | \
                   DEVICE_DISK_FORMAT )

//--------------------------------------------------------------------

#endif//_DLDCOMMON_H