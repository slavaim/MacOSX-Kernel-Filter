/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDUSERTOKERNEL_H
#define _DLDUSERTOKERNEL_H

#include <sys/param.h>
#include <sys/kauth.h>
#include <sys/vnode.h>
#include <IOKit/scsi/SCSITask.h>
#include <netinet/in.h>

//
// ATTENTION!
// If the driver interface is changed the DldDriverInterfaceVersion must be increased!
//

//--------------------------------------------------------------------

//
// because of 32 bit driver and 64 bit service the alignment and pack attribute is required for all structures
// that are shared by the service and the driver
//
#define DLD_ALIGNMENT  __attribute__((aligned(4),packed))

#if 0

///////////////////////////////////////////////////////////////////////
//
// the following copy is made to facilitate KAUTH rights transformation to DL rights
//
///////////////////////////////////////////////////////////////////////

/* Actions, also rights bits in an ACE */

#define KAUTH_VNODE_READ_DATA			(1<<1)
#define KAUTH_VNODE_LIST_DIRECTORY		KAUTH_VNODE_READ_DATA
#define KAUTH_VNODE_WRITE_DATA			(1<<2)
#define KAUTH_VNODE_ADD_FILE			KAUTH_VNODE_WRITE_DATA
#define KAUTH_VNODE_EXECUTE			(1<<3)
#define KAUTH_VNODE_SEARCH			KAUTH_VNODE_EXECUTE
#define KAUTH_VNODE_DELETE			(1<<4)
#define KAUTH_VNODE_APPEND_DATA			(1<<5)
#define KAUTH_VNODE_ADD_SUBDIRECTORY		KAUTH_VNODE_APPEND_DATA
#define KAUTH_VNODE_DELETE_CHILD		(1<<6)
#define KAUTH_VNODE_READ_ATTRIBUTES		(1<<7)
#define KAUTH_VNODE_WRITE_ATTRIBUTES		(1<<8)
#define KAUTH_VNODE_READ_EXTATTRIBUTES		(1<<9)
#define KAUTH_VNODE_WRITE_EXTATTRIBUTES		(1<<10)
#define KAUTH_VNODE_READ_SECURITY		(1<<11)
#define KAUTH_VNODE_WRITE_SECURITY		(1<<12)
#define KAUTH_VNODE_TAKE_OWNERSHIP		(1<<13)

/* backwards compatibility only */
#define KAUTH_VNODE_CHANGE_OWNER		KAUTH_VNODE_TAKE_OWNERSHIP

/* For Windows interoperability only */
#define KAUTH_VNODE_SYNCHRONIZE			(1<<20)

/* (1<<21) - (1<<24) are reserved for generic rights bits */

/* Actions not expressed as rights bits */
/*
 * Authorizes the vnode as the target of a hard link.
 */
#define KAUTH_VNODE_LINKTARGET			(1<<25)

/*
 * Indicates that other steps have been taken to authorise the action,
 * but authorisation should be denied for immutable objects.
 */
#define KAUTH_VNODE_CHECKIMMUTABLE		(1<<26)

/* Action modifiers */
/*
 * The KAUTH_VNODE_ACCESS bit is passed to the callback if the authorisation
 * request in progress is advisory, rather than authoritative.  Listeners
 * performing consequential work (i.e. not strictly checking authorisation)
 * may test this flag to avoid performing unnecessary work.
 *
 * This bit will never be present in an ACE.
 */
#define KAUTH_VNODE_ACCESS			(1<<31)

/*
 * The KAUTH_VNODE_NOIMMUTABLE bit is passed to the callback along with the
 * KAUTH_VNODE_WRITE_SECURITY bit (and no others) to indicate that the
 * caller wishes to change one or more of the immutable flags, and the
 * state of these flags should not be considered when authorizing the request.
 * The system immutable flags are only ignored when the system securelevel
 * is low enough to allow their removal.
 */
#define KAUTH_VNODE_NOIMMUTABLE			(1<<30)


/*
 * fake right that is composed by the following...
 * vnode must have search for owner, group and world allowed
 * plus there must be no deny modes present for SEARCH... this fake
 * right is used by the fast lookup path to avoid checking
 * for an exact match on the last credential to lookup
 * the component being acted on
 */
#define KAUTH_VNODE_SEARCHBYANYONE		(1<<29)


/*
 * when passed as an 'action' to "vnode_uncache_authorized_actions"
 * it indicates that all of the cached authorizations for that
 * vnode should be invalidated 
 */
#define	KAUTH_INVALIDATE_CACHED_RIGHTS		((kauth_action_t)~0)



/* The expansions of the GENERIC bits at evaluation time */
#define KAUTH_VNODE_GENERIC_READ_BITS	(KAUTH_VNODE_READ_DATA |		\
KAUTH_VNODE_READ_ATTRIBUTES |		\
KAUTH_VNODE_READ_EXTATTRIBUTES |	\
KAUTH_VNODE_READ_SECURITY)

#define KAUTH_VNODE_GENERIC_WRITE_BITS	(KAUTH_VNODE_WRITE_DATA |		\
KAUTH_VNODE_APPEND_DATA |		\
KAUTH_VNODE_DELETE |			\
KAUTH_VNODE_DELETE_CHILD |		\
KAUTH_VNODE_WRITE_ATTRIBUTES |		\
KAUTH_VNODE_WRITE_EXTATTRIBUTES |	\
KAUTH_VNODE_WRITE_SECURITY)

#define KAUTH_VNODE_GENERIC_EXECUTE_BITS (KAUTH_VNODE_EXECUTE)

#define KAUTH_VNODE_GENERIC_ALL_BITS	(KAUTH_VNODE_GENERIC_READ_BITS |	\
KAUTH_VNODE_GENERIC_WRITE_BITS |	\
KAUTH_VNODE_GENERIC_EXECUTE_BITS)

/*
 * Some sets of bits, defined here for convenience.
 */
#define KAUTH_VNODE_WRITE_RIGHTS	(KAUTH_VNODE_ADD_FILE |				\
KAUTH_VNODE_ADD_SUBDIRECTORY |			\
KAUTH_VNODE_DELETE_CHILD |			\
KAUTH_VNODE_WRITE_DATA |			\
KAUTH_VNODE_APPEND_DATA |			\
KAUTH_VNODE_DELETE |				\
KAUTH_VNODE_WRITE_ATTRIBUTES |			\
KAUTH_VNODE_WRITE_EXTATTRIBUTES |		\
KAUTH_VNODE_WRITE_SECURITY |			\
KAUTH_VNODE_TAKE_OWNERSHIP |			\
KAUTH_VNODE_LINKTARGET |			\
KAUTH_VNODE_CHECKIMMUTABLE)

///////////////////////////////////////////////////////////////////////
//
// end of the copy
//
///////////////////////////////////////////////////////////////////////

#endif//0

///////////////////////////////////////////////////////////
// Make up some private access rights. Only 24 bits.
///////////////////////////////////////////////////////////

// DL MAC definition                    // DL Windows value     // KAUTH          
#define DEVICE_DIRECT_READ              (0x0004)                //KAUTH_VNODE_READ_DATA 
#define DEVICE_DIRECT_WRITE             (0x0008)                //KAUTH_VNODE_WRITE_DATA
#define DEVICE_EJECT_MEDIA              (0x0010)                //KAUTH_VNODE_READ_DATA
#define DEVICE_DISK_FORMAT              (0x0080)                //( KAUTH_VNODE_WRITE_DATA | KAUTH_VNODE_DELETE_CHILD )
#define DEVICE_VOLUME_DEFRAGMENT        (0x0100)                //KAUTH_VNODE_WRITE_DATA
#define DEVICE_DIR_LIST                 (0x0200)                //KAUTH_VNODE_LIST_DIRECTORY
#define DEVICE_DIR_CREATE               (0x0400)                //KAUTH_VNODE_ADD_FILE
#define DEVICE_READ                     (0x0800)                //KAUTH_VNODE_READ_DATA
#define DEVICE_WRITE                    (0x1000)                //KAUTH_VNODE_WRITE_DATA
#define DEVICE_EXECUTE                  (0x2000)                //KAUTH_VNODE_EXECUTE
#define DEVICE_RENAME                   (0x4000)                //KAUTH_VNODE_WRITE_DATA
#define DEVICE_DELETE                   (0x8000)                //KAUTH_VNODE_DELETE_CHILD
#define DEVICE_PLAY_AUDIO_CD            (0x200000L)             //KAUTH_VNODE_READ_DATA

#define DEVICE_ENCRYPTED_READ         (0x010000L)
#define DEVICE_ENCRYPTED_WRITE        (0x020000L)
#define DEVICE_ENCRYPTED_DIRECT_WRITE (0x040000L)
#define DEVICE_PLAY_AUDIO_CD          (0x200000L)
#define DEVICE_ENCRYPTED_DISK_FORMAT  (0x080000L)

#define DEVICE_RIGHTS_OPEN_END       (0x1000000L)

#define DEVICE_FULL_WRITE  ( DEVICE_WRITE | DEVICE_DIRECT_WRITE | DEVICE_DELETE | DEVICE_DIR_CREATE  | DEVICE_DISK_FORMAT )

//
// admin rights, they do not map to any KAUTH rights as they are internal to the service
//
#define ADMIN_SHADOW_DATA_ACCESS        (0x00000004)
#define ADMIN_FULL_ACCESS               (0x00FFFF67) // already contains ADMIN_SHADOW_DATA_ACCESS
#define ADMIN_CHANGE_ACCESS             (0x00203A04 & ~(ADMIN_SHADOW_DATA_ACCESS))
#define ADMIN_READ_ACCESS               (0x00202A04 & ~(ADMIN_SHADOW_DATA_ACCESS))

//////////////////////////////////////////////////////////////
// access rights for protected processes
//////////////////////////////////////////////////////////////
#define DL_PROCESS_TERMINATE            (ADMIN_FULL_ACCESS)

//////////////////////////////////////////////////////////

typedef UInt32 dld_classic_rights_t;         //  DEVICE_****  rights
typedef UInt32 dld_classic_rights_process_t; // DL_PROCESS_*** rights

//
// some classes have Class::convertKauthToWin() function which sets the structure,
// if the transformation has not been done ( determined by zeroed win rights ) the
// global ::convertKauthToWin() is called to perform transformation
//
typedef struct _DldRequestedAccess{
    
    //
    // classic Windows DL access rights
    //
    dld_classic_rights_t winRequestedAccess;
    
    //
    // MAC OS X KAuth access rights ( might be 0x0 if winRequestedAccess was detrmined w/o help from the KAUTH subsystem )
    //
    kauth_ace_rights_t   kauthRequestedAccess;
    
} DldRequestedAccess;

//--------------------------------------------------------------------

inline
const char*
DldWinRightsToStr( dld_classic_rights_t winRequestedAccess )
{
    switch( winRequestedAccess ){
        case DEVICE_DIRECT_READ:
            return "DEVICE_DIRECT_READ";
        case DEVICE_DIRECT_WRITE:
            return "DEVICE_DIRECT_WRITE";
        case DEVICE_EJECT_MEDIA:
            return "DEVICE_EJECT_MEDIA";
        case DEVICE_DISK_FORMAT:
            return "DEVICE_DISK_FORMAT";
        case DEVICE_VOLUME_DEFRAGMENT:
            return "DEVICE_VOLUME_DEFRAGMENT";
        case DEVICE_DIR_LIST:
            return "DEVICE_DIR_LIST";
        case DEVICE_DIR_CREATE:
            return "DEVICE_DIR_CREATE";
        case DEVICE_READ:
            return "DEVICE_READ";
        case DEVICE_WRITE:
            return "DEVICE_WRITE";
        case DEVICE_EXECUTE:
            return "DEVICE_EXECUTE";
        case DEVICE_RENAME:
            return "DEVICE_RENAME";
        case DEVICE_DELETE:
            return "DEVICE_DELETE";
        case DEVICE_PLAY_AUDIO_CD:
            return "DEVICE_PLAY_AUDIO_CD";
        case DEVICE_ENCRYPTED_READ:
            return "DEVICE_ENCRYPTED_READ";
        case DEVICE_ENCRYPTED_WRITE:
            return "DEVICE_ENCRYPTED_WRITE";
        case DEVICE_ENCRYPTED_DIRECT_WRITE:
            return "DEVICE_ENCRYPTED_DIRECT_WRITE";
        case DEVICE_ENCRYPTED_DISK_FORMAT:
            return "DEVICE_ENCRYPTED_DISK_FORMAT";
        default:
            return "UNKNOWN";
    }// end switch
}

//--------------------------------------------------------------------

///////////////////////////////////////////////////////////
// Device Type from the DL Windows. Maximum number 0xFFFF.
///////////////////////////////////////////////////////////

#define DEVICE_TYPE_UNKNOWN        0
#define DEVICE_TYPE_FLOPPY         1
#define DEVICE_TYPE_REMOVABLE      2
#define DEVICE_TYPE_HARD_DRIVE     3
#define DEVICE_TYPE_REMOTE         4
#define DEVICE_TYPE_NETWORK_DISK   4
#define DEVICE_TYPE_DVD            5
#define DEVICE_TYPE_CD_ROM         5
#define DEVICE_TYPE_RAM_VOL        6  
#define DEVICE_TYPE_SERIAL_PORT    7
#define DEVICE_TYPE_MOUSE_PORT     7
#define DEVICE_TYPE_LPT            8
#define DEVICE_TYPE_TAPE           9
#define DEVICE_TYPE_USBHUB         10
#define DEVICE_TYPE_IRDA           11
#define DEVICE_TYPE_1394           12
#define DEVICE_TYPE_BLUETOOTH      13
#define DEVICE_TYPE_WIFI           14
#define DEVICE_TYPE_WINDOWS_MOBILE 15
#define DEVICE_TYPE_PALM           16
#define DEVICE_TYPE_PRINTER        17
#define DEVICE_TYPE_IPHONE         18
#define DEVICE_TYPE_BLACKBERRY     19
#define TYPES_COUNT                20 // total number of device types

#define DEVICE_TYPE_SERVICE        255 // mainly needed for audit log viewer. "Service" pseudo device type.

//
// Special Device Types, i.e. actually not a device types
//
#define DEVICE_SPECIAL_TYPE_FIRST  256

#define DEVICE_SPECIAL_TYPE_RESERVED (DEVICE_SPECIAL_TYPE_FIRST + 0) // reserved device type
#define DEVICE_SPECIAL_TYPE_ADMINS   (DEVICE_SPECIAL_TYPE_FIRST + 1) // This type is used to save DL Admins security descriptor.
#define SPECIAL_TYPES_COUNT          2

//
// the DL Mac type of the device, do not forget to udate DldDeviceTypeToString
// if adding a new type
//
#define DLD_DEVICE_TYPE_UNKNOWN     ((UInt16)DEVICE_TYPE_UNKNOWN)
#define DLD_DEVICE_TYPE_HARDDRIVE   ((UInt16)DEVICE_TYPE_HARD_DRIVE)
#define DLD_DEVICE_TYPE_REMOVABLE   ((UInt16)DEVICE_TYPE_REMOVABLE)
#define DLD_DEVICE_TYPE_USB         ((UInt16)DEVICE_TYPE_USBHUB)
#define DLD_DEVICE_TYPE_IEEE1394    ((UInt16)DEVICE_TYPE_1394)
#define DLD_DEVICE_TYPE_SERIAL      ((UInt16)DEVICE_TYPE_SERIAL_PORT)
#define DLD_DEVICE_TYPE_CD_DVD      ((UInt16)DEVICE_TYPE_DVD)
#define DLD_DEVICE_TYPE_BLUETOOTH   ((UInt16)DEVICE_TYPE_BLUETOOTH)
#define DLD_DEVICE_TYPE_WIFI        ((UInt16)DEVICE_TYPE_WIFI)

//
// always the last
//
#define DLD_DEVICE_TYPE_MAX         ((UInt16)TYPES_COUNT)

//--------------------------------------------------------------------

typedef union _DldDeviceType{
    
    struct{
        UInt16   major;//DLD_DEVICE_TYPE_XXX
        UInt16   minor;//DLD_DEVICE_TYPE_XXX
    } type;
    
    UInt32       combined;
    
} DldDeviceType;

//#define DLD_DEVICE_TYPE_COMBINED( major, minor ) ( (UInt32)( ((UInt32)(major)) | (((UInt32)(minor))<<16) ) )

inline
const char*
DldDeviceTypeToString( UInt16 type )
{
    switch( type ){
        case DLD_DEVICE_TYPE_UNKNOWN:
            return "DLD_DEVICE_TYPE_UNKNOWN"; // it is OK, we just do not process this device
        case DLD_DEVICE_TYPE_HARDDRIVE:
            return "DLD_DEVICE_TYPE_HARDDRIVE";
        case DLD_DEVICE_TYPE_REMOVABLE:
            return "DLD_DEVICE_TYPE_REMOVABLE";
        case DLD_DEVICE_TYPE_USB:
            return "DLD_DEVICE_TYPE_USB";
        case DLD_DEVICE_TYPE_SERIAL:
            return "DLD_DEVICE_TYPE_SERIAL";
        case DLD_DEVICE_TYPE_CD_DVD:
            return "DLD_DEVICE_TYPE_CD_DVD";
        case DLD_DEVICE_TYPE_IEEE1394:
            return "DLD_DEVICE_TYPE_IEEE1394";
        case DLD_DEVICE_TYPE_BLUETOOTH:
            return "DLD_DEVICE_TYPE_BLUETOOTH";
        case DLD_DEVICE_TYPE_WIFI:
            return "DLD_DEVICE_TYPE_WIFI";
        case DLD_DEVICE_TYPE_MAX:
            return "DLD_DEVICE_TYPE_MAX"; // an invalid type
        default:
            return "UNDEFINED"; // an invalid type ( has somebody added a new type? )
    }
}

//--------------------------------------------------------------------

typedef enum _EncryptionProviderEnum{
    ProviderUnknown = 0x0,
    ProviderPGP = 0x1,
    ProviderLexarFlash = 0x2,
    ProviderTrueCrypt = 0x3,
    ProviderSafeDisk = 0x4,
    ProviderDcppVolume = 0x5,
    ProviderLexarS3000 = 0x6,
    ProviderLexarS3000Fips = 0x7,
    ProviderBitLocker = 0x8,
    ProviderSafeGuardEasy = 0x9,
    ProviderDiskGoSecure = 0xA,
	ProviderMacOsEncryption = 0xB,
    //
    // do not forget to increase the following number 
    // when a new provider is added
    //
    MaximumEncryptionProviderNumber = 0xC
} EncryptionProviderEnum;

//--------------------------------------------------------------------

//
// ACL types
//
typedef enum {
    kDldAclTypeUnknown = 0x0,
    kDldAclTypeSecurity,
    kDldAclTypeLogAllowed,
    kDldAclTypeLogDenied,
    kDldAclTypeShadow,
    kDldAclTypeDiskCAWL,
    
    //
    // always the last
    //
    kDldAclTypeMax
    
} DldAclType;

inline
const char*
DldAclTypeToString( DldAclType type )
{
    switch( type ){
            
        case kDldAclTypeUnknown:
            return "Unknown0";
        case kDldAclTypeSecurity:
            return "kDldAclTypeSecurity";
        case kDldAclTypeLogAllowed:
            return "kDldAclTypeLogAllowed";
        case kDldAclTypeLogDenied:
            return "kDldAclTypeLogDenied";
        case kDldAclTypeShadow:
            return "kDldAclTypeShadow";
        case kDldAclTypeDiskCAWL:
            return "kDldAclTypeDiskCAWL";
        case kDldAclTypeMax:
            return "kDldAclTypeMax";
        default:
            return "Undefined";
    }
}

//--------------------------------------------------------------------

#define DLDRIVER_IOKIT_CLASS "com_devicelock_driver_DeviceLockIOKitDriver"
#define DLDRIVER_IPC_CLASS   "com_devicelock_driver_DeviceLockIPCDriver"

//--------------------------------------------------------------------

#define kDldUserClientCookie     ( (UInt32)0xFFFABC12 )
#define kDldUserIPCClientCookie  ( (UInt32)0xFFFABC13 )

//--------------------------------------------------------------------

enum { kt_MaximumEventsToHold = 512 };

//--------------------------------------------------------------------

enum {
    kt_DldUserClientOpen = 0x0,             // 0x0
    kt_DldUserClientClose,                  // 0x1
    kt_DldUserClientSetQuirks,              // 0x2
    kt_DldUserClientSetACL,                 // 0x3
    kt_DldUserClientSetShadowFile,          // 0x4
    kt_DldUserClientSetUserState,           // 0x5
    kt_DldUserClientSetVidPidWhiteList,     // 0x6
    kt_DldUserClientSetUIDWhiteList,        // 0x7
    kt_DldUserClientSetDVDWhiteList,        // 0x8
    kt_DldUserClientDiskCawlResponse,       // 0x9
    kt_DldUserClientSocketFilterResponse,   // 0xA
    kt_DldUserClientSetProcessSecurity,     // 0xB
    kt_DldUserClientSetFileSecurity,        // 0xC
    kt_DldUserClientQueryDriverProperties,  // 0xD
    kt_DldUserClientSetTempVidPidWhiteList, // 0xE
    kt_DldUserClientSetTempUIDWhiteList,    // 0xF
    kt_DldUserClientSetDLAdminSettings,     // 0x10
    kt_DldUserClientSetEncryptionProvider,  // 0x11
    kt_DldUserClientReportIPCCompletion,    // 0x12
    
    //
    // the number of methods
    //
    kt_DldUserClientMethodsMax,
    
    //
    // a fake method
    //
    kt_DldStopListeningToMessages = 0xFFFFFFFF,
};

//--------------------------------------------------------------------

enum {
    kt_DldUserIPCClientInvalidRequest = 0x0,    // 0x0
    kt_DldUserIPCClientIpcRequest,              // 0x1
    
    //
    // the number of methods
    //
    kt_DldUserIPCClientMethodsMax
};

//--------------------------------------------------------------------

typedef enum{
    DldDriverInterfaceVersion0 = 0x0,  // not valid
    DldDriverInterfaceVersionCurrent1, // not used, a fake version value
    DldDriverInterfaceVersionCurrent2,
    // <-- add here DldDriverInterfaceVersionN where N is increased sequentially for any interface change.
    DldDriverInterfaceVersionCurrent
} DldDriverInterfaceVersion;


typedef union _DldDriverProperties{
    
    //
    // the size of the structure ( might vary for different versions )
    //
    UInt32   size;
    
    //
    // the structure is extended by adding new fields at the end!
    // refrain from removing unused fields
    //
    DldDriverInterfaceVersion   interfaceVersion;
    
    //
    // add new fields below
    //
    
} DLD_ALIGNMENT DldDriverProperties;

//--------------------------------------------------------------------

typedef enum{
    kt_DldNotifyTypeUnknown = 0x0,
    kt_DldNotifyTypeLog,
    kt_DldNotifyTypeShadow,
    kt_DldNotifyTypeDiskCAWL,
    kt_DldNotifyTypeEvent, // events that do not fall into any other categoty
#ifdef _DLD_SOCKET_FILTER
    kt_DldNotifyTypeSocketFilter,
#endif // _DLD_SOCKET_FILTER
    
    //
    // always the last
    //
    kt_DldNotifyTypeMax
    
    // but this is not the end of the story
    // [ kt_DldAclTypeSocketDataBase, kt_DldAclTypeSocketDataBase + kt_DldSocketBuffersNumber ) is a range reserved for socket buffers!
    
} DldNotifyType;

//
// there are kt_DldSocketBuffersNumber buffer, this value is a starting base for IOConnectMapMemory,
// this out of range values can't be added to DldNotifyType as kt_DldNotifyTypeMax is used for a shared
// queue implementation
//
#define kt_DldAclTypeSocketDataBase  (kt_DldNotifyTypeMax+0x1000)

// static char StaticAssert_NotifyTypeMax[ (kt_DldNotifyTypeMax < kt_DldAclTypeSocketDataBase) ? 0 : -1 ];

//--------------------------------------------------------------------

typedef enum _DldShadowType{
    DldShadowTypeUnknown = 0x0, // an invalid type, if received then something went wrong
    DldShadowTypeFileWrite,     // kernel mode - header for DldShadowWriteVnodeArgs
    DldShadowTypeFilePageout,   // kernel mode - header for DldShadowWriteVnodeArgs
    DldShadowTypeFileOpen,
    DldShadowTypeFileClose,
    DldShadowTypeFileReclaim,
    DldShadowTypeSCSICommand,
    DldShadowTypeOperationCompletion, // kernel mode - header for DldShadowOperationCompletion
    
    //
    // always the the last
    //
    DldShadowTypeStopShadow,
    _DldShadowTypeInflateMe = ~(UInt64)(0)
} DldShadowType;

//--------------------------------------------------------------------

inline
const char*
DldShadowTypeToStr( DldShadowType shadowType )
{
    switch( shadowType ){
            
        case DldShadowTypeUnknown:
            return "DldShadowTypeUnknown";
        case DldShadowTypeFileWrite:
            return "DldShadowTypeFileWrite";
        case DldShadowTypeFilePageout:
            return "DldShadowTypeFilePageout";
        case DldShadowTypeFileOpen:
            return "DldShadowTypeFileOpen";
        case DldShadowTypeFileClose:
            return "DldShadowTypeFileClose";
        case DldShadowTypeFileReclaim:
            return "DldShadowTypeFileReclaim";
        case DldShadowTypeStopShadow:
            return "DldShadowTypeStopShadow";
        case DldShadowTypeSCSICommand:
            return "DldShadowTypeSCSICommand";
        case DldShadowTypeOperationCompletion:
            return "DldShadowTypeOperationCompletion";
        default:
            return "Unknown";
    }
    
}

//--------------------------------------------------------------------

#define DLD_SN_TYPE_GENERAL  ((UInt32)0x0)
#define DLD_SN_TYPE_CUTOFF   ((UInt32)0x1)

typedef union _DldDriverShadowNotification{
    
    struct{
        
        //
        // always the first, one of the DLD_SN_TYPE_XXX
        //
        UInt32         type;

        //
        // a shadow file's ID
        //
        UInt32         shadowFileID;
        
        //
        // a type of the shadow data, actually used only for the debug
        // purposes and can be removed
        //
        DldShadowType  shadowType;
        
        //
        // offset in a shadow file
        //
        off_t          offset;
        
        //
        // size of the dta at the above offset
        //
        UInt32         dataSize;
        
        //
        // result of the shadowing, actually of IOReturn type
        //
        UInt32         kRC;
        
    } GeneralNotification;
    
} DldDriverShadowNotification;

//--------------------------------------------------------------------

//
// CAWL operations
//
typedef enum _DldCawlOpcode{
    
    kDldCawlOpUnknown   = 0x0,  // invalid, something went wrong if received
    kDldCawlOpAccessRequest,    // the only operation that allows to disable access and requires a response
    kDldCawlOpFileOpen,         // notification
    kDldCawlOpFileClose,        // notification
    kDldCawlOpFileReclaim,      // notification, vnode becomes invalid and must be removed from any database
    kDldCawlOpFileWrite,        // notification
    kDldCawlOpFileRemove,       // notification TO DO
    kDldCawlOpFileRename,       // notification TO DO
    
    //
    // the last entry
    //
    kDldCawlOpMax,
    _DldCawlOpcodeInflateMe = ~(UInt64)(0)
} DldCawlOpcode;

//--------------------------------------------------------------------

//
// a responce from the service to the driver,
// currently used only for open operation
//
typedef struct _DldServiceDiskCawlResponse{
    
    //
    // an ID provided by the driver
    //
    SInt64              notificationID;
    
    //
    // a unique ID for a vnode
    //
    UInt64              vnodeID;
    
    //
    // a vnode handle, can be reused by the kernel,
    // so only the pair of vnodeID and vnodeHandle 
    // allows to uniquely identify a vnode object,
    // the reason for vnode handle introduction is
    // a complexity in finding vnode by ID
    //
    UInt64              vnodeHandle;
    
    //
    // the operation code, the same as in the notification from the driver
    //
    DldCawlOpcode       opcode;
    
    union{
        
        struct{
            
            //
            // if true the service disabled access to the file for a user application
            //
            bool    disableAccess;
            bool    logOperation;
        } accessRequest;
        
    } operationData;
    
} DldServiceDiskCawlResponse;

//--------------------------------------------------------------------

//
// kDldCawlOpFileOpen
//
typedef struct _FileOpen{
    
    //
    // device type for a device where a file is being opened
    //
    DldDeviceType       deviceType;
    
    //
    // DL and kauth_action_t
    //
    DldRequestedAccess  action;
    
    //
    // a user's GUID, zeroed if unknown
    //
    guid_t              effectiveUserGuid;
    guid_t              realUserGuid;
    
    //
    // a user's UID, zeroed if unknown
    //
    uid_t               effectiveUserUid;
    uid_t               realUserUid;
    
    //
    // a user's GID, zeroed if unknown
    //
    gid_t               effectiveUserGid;
    gid_t               realUserGid;
    
} FileOpen;

//
// a CAWL notification from the driver to the service
//
typedef struct _DldDriverDiskCAWLNotification{
    
    //
    // this notification entry ID, must be unique
    //
    SInt64              notificationID;
    
    //
    // a unique ID for a vnode
    //
    UInt64              vnodeID;
    
    //
    // a vnode handle, can be reused by the kernel,
    // so only pair of vnodeID and vnodeHandle 
    // allows uniquely identify a vnode object,
    // the reason for vnode handle introduction is
    // a complexity in finding vnode by ID
    //
    UInt64              vnodeHandle;
    
    //
    // the operation code
    //
    DldCawlOpcode       opcode;

    union{

        FileOpen        open;
        FileOpen        accessRequest;
        
        struct{
            
            UInt64              timeStamp;
            UInt64              offset;
            UInt32              size;
            unsigned char       backingSparseFileID[16];
        } write;
        
    } operationData;
    
    //
    // the length in bytes including the zero termibator,
    // might be 0x0, usually provided only for open notification
    //
    UInt32              nameLength;
    char                fileName[1];
    // no new fields after the fileName
} DldDriverDiskCAWLNotification;

//
// the DldDriverDiskCAWLNotificationMax defines the bug=ffer big enought to accomodate any
// cawl notification but does not accurately defines layout, never use it to get access
// to data after the header, use the information from the header to get the actual data offset and size
//
typedef struct _DldDriverDiskCAWLNotificationMax{
    
    DldDriverDiskCAWLNotification  header;
    char   fileName[ MAXPATHLEN ]; // never use as a valid file name as the first character is in the header!
    
} DldDriverDiskCAWLNotificationMax;

//--------------------------------------------------------------------

typedef struct _DldSCSITaskResults{
    SCSIServiceResponse			serviceResponse;
    SCSITaskStatus				taskStatus;
    UInt64						realizedDataTransferCount;
} DldSCSITaskResults;

//--------------------------------------------------------------------

#define DLD_SHADOW_DATA_HEADER_SIGNATURE  (0xABCDF65DEC56195Cll)

typedef struct _DldShadowDataHeader{
    
    //
    // a signature, must be DLD_SHADOW_DATA_HEADER_SIGNATURE
    //
    UInt64           signature;
    
    //
    // a full size for the data including this header
    //
    UInt64           size;
    
    //
    // an operation ID, a unique value until the operation completion
    //
    UInt64           operationID;
    
    //
    // type of the operation
    //
    DldShadowType    type;
    
    union {
        
        //
        // DldShadowTypeFileWrite
        // DldShadowTypeFilePageout
        //
        struct{
            
            //
            // a unique ID for a vnode or device,
            // in case of vnode its address is used
            //
            UInt64           vnodeID;
            
            //
            // offset for data in a file( a file which was shadowed! )
            //
            UInt64           offset;
            
            //
            // size of data
            //
            UInt64           size;
            
        }  write;
        
        
        //
        // DldShadowTypeFileOpen
        //
        struct{
            
            //
            // a unique ID for a vnode or device,
            // in case of vnode its address is used
            //
            UInt64           vnodeID;
            
        } open;
        
        //
        // DldShadowTypeFileClose
        //
        struct{
            
            //
            // a unique ID for a vnode or device,
            // in case of vnode its address is used
            //
            UInt64           vnodeID;
            
        } close;
        
        //
        // DldShadowTypeFileReclaim
        //
        struct{
            
            //
            // a unique ID for a vnode or device,
            // in case of vnode its address is used
            //
            UInt64           vnodeID;
            
        } reclaim;
        
        //
        // DldShadowTypeSCSICommand
        //
        struct {
            //
            // a DldIOService->serviceID value
            //
            UInt32                       serviceID;
            
            //
            // one of the kSCSICDBSize_XXByte values
            //
            UInt32                       commandSize;
            
            //
            // a command block
            //
            SCSICommandDescriptorBlock   cdb;
            
            //
            // offset of the data if the request is written in chunks
            //
            UInt64                       offset;
            
            //
            // a size of the data
            //
            UInt64                       dataSize;
            
        } SCSI;
        
        //
        // DldShadowTypeOperationCompletion, completions
        // with the shadowingRetval value set to some
        // error can be sent without the corresponding
        // preciding operation, this means that an error
        // happened while some operation was being shadowed
        //
        struct{
            
            //
            // a unique ID for a vnode or device,
            // in case of vnode its address is used
            //
            UInt64           vnodeID;
            
            //
            // a status of the shadowed operation execution, 0x0 is a success value,
            // the status is of the BSD kernel status type or IOReturn type, for both
            // the 0x0 is a success value,
            // for SCSI the value is of the SCSITaskStatus type
            //
            int              retval;
            
            //
            // a return value for the shadowing operation
            // if the value is not KERN_SUCCESS then the shadowing
            // failed
            //
            int              shadowingRetval;
            
            //
            // some additional data depending of the request's nature,
            // must be synchronized with DldShadowOperationCompletion::additionalData
            //
            union{
                
                //
                // bytes written by FSD in a file( a shadowed file! ) when processing
                // a write request, used for:
                // DldShadowTypeFileWrite
                // DldShadowTypeFilePageout
                //
                user_ssize_t   bytesWritten;
                
                //
                // a completion status for a SCSI operation, DldShadowTypeSCSICommand request
                //
                DldSCSITaskResults   scsiOperationStatus;
                
            } additionalData;
            
        } completion;
        
    } operationData;
    
    //
    // char          data[]
    //
} DldShadowDataHeader;

//--------------------------------------------------------------------

typedef struct _DldAclDescriptorHeader{
    
    //
    // the size of the buffer containing this header, i.e. header+tail
    //
    UInt32          size;
    
    DldDeviceType   deviceType;
    
    DldAclType      aclType;
    
    //
    // the space after the header is occupied by the ACL data,
    // it contains the kauth_filesec structure
    // kauth_filesec filesec;
    //
} DldAclDescriptorHeader;

//--------------------------------------------------------------------

#define DLD_IGNR_FSIZE  ((off_t)(-1ll))
#define DLD_SHADOW_FILE_DEFAULT_MINSIZE  ((off_t)(0x10000000ll)) // 256 MB

typedef struct _DldShadowFileDescriptorHeader{
    
    //
    // the size of the buffer containing this header, i.e. header+tail
    //
    UInt32          size;
    
    //
    // a shadow file's ID
    //
    UInt32          shadowFileID;
    
    //
    // the maximum allowed size, DLD_IGNR_FSIZE id there is no limit
    //
    UInt64           maxFileSize;
    
    //
    // a file size at which the driver tries to switch to the next
    // shadow file, if DLD_IGNR_FSIZE then ignored and the file
    // will never be switched if there is no file to switch for
    //
    UInt64           switchFileSize;
    
    //
    // the name length in bytes including the zero terminator
    //
    UInt32          nameLength;
    
    //
    // the name itself
    //
    char            name[1];
} DldShadowFileDescriptorHeader;

//--------------------------------------------------------------------

//
// a terminator placed after the last entry in a shadow file,
// the terminator is the last data written in the file,
// the terminator is written iff there is enough data after the last entry,
// if there is not enough place ( e.g. the last entry terminates at the file boundary )
// the terminator is not written, the termination write operation
// is allowed to fail - nothing will be made in this case, no notification
// is sent, so the terminator is not a reliable source of information
//
#define DLD_SHADOW_FILE_TERMINATOR  ((UInt64)(0xABCDEFAB79862463ll))

//--------------------------------------------------------------------

typedef enum{
    kDldUserSessionUnknown = 0x0,
    kDldUserSessionGUI,// GUI session( Aqua ), GUI also a TTY sessions
    kDldUserSessionRemote,// SSH session or similar over network ( 
    kDldUserSessionTTY, // access to tty but no access to GUI ( SSH local or remote )
    
    // always the last
    kDldUserSessionMax
} DldUserSessionType;

typedef enum{
    kDldUserLoginStateUnknown = 0x0,
    kDldUserLoginStateLoggedIn,
    kDldUserLoginStateLoggedOut,
    
    // always the last
    kDldUserLoginStateMax
} DldUserLoginState;

typedef enum{
    kDldUserSessionActivityUnknown = 0x0,
    kDldUserSessionActive,
    kDldUserSessionDeactivated,
    
    // always the last
    kDldUserSessionActivityMax
} DldUserSessionActivityState;

typedef struct _DldLoggedUserInfo{
    
    //
    // a unique ID for the session
    //
    UInt32                       sessionID;
    
    //
    // a PID for the user's agent, (-1) for invalid value, if the value
    // is invalid the userID and groupID is used to create  user context
    //
    int                          agentPID;
    
    //
    // a raw SessionAttributeBits representing the client info, used only for a debug purposes
    //
    UInt32                       sessionAttributeBits;
    
    //
    // type of the session
    //
    DldUserSessionType           sessionType;
    
    //
    // the user login state
    //
    DldUserLoginState            loginState;
    
    //
    // user session state ( fast user switch )
    //
    DldUserSessionActivityState  activityState;
    
    //
    // user and group ID are used when agentPID is invalid
    //
    UInt32                       userID;
    UInt32                       groupID;
    
} DldLoggedUserInfo;

//--------------------------------------------------------------------

typedef struct _DldCommonWL{
    
    //
    // an opaque value, the user application set it to oxo,
    // the kernel extension sets it to the descriptor type (DldWhiteListType)
    //
    UInt64           kernelReserved;
    
    //
    // an ACL describes users to which
    // a white list should be applied,
    // if NULL the white list is
    // applied to all users,
    // the rights mask must be DEVICE_READ | DEVICE_WRITE,
    // if a WL is applied to a user the ACL must ALLOW explicitly the user or a group,
    // for reference see the DldApplySecurityQuirks() code
    //
    // a user space pointer to the kauth_filesec structure
    //
    mach_vm_address_t filesec;//kauth_filesec
    mach_vm_size_t    filesecSize;
    
} DldCommonWL;


typedef struct _DldDeviceUID{
    UInt8   uid[ 16 ];
} DldDeviceUID;

typedef DldDeviceUID CDDVDDiskID;

typedef struct _DldDeviceUIDEntry{
    
    DldDeviceUID   uid;
    
    struct{
        UInt32 propagateUp:0x1;
    } flags;
    
} DldDeviceUIDEntry;


typedef struct _DldDeviceUIDDscr{
    
    //
    // must always be the first entry!
    //
    DldCommonWL   common;
    
    //
    // number of entries in the arrUID array
    //
    UInt32 count;
    
    //
    // an array ot Unique ID entries, must be the last field!
    //
    DldDeviceUIDEntry  arrUID[ 1 ];
    
} DldDeviceUIDDscr;

#define kDLD_APPLE_USB_VID (0x5AC)

//
// the definition must be compatible with
// the Windows DL's definition
//
// typedef struct _PID_VID{
//      USHORT  Pid;
//      USHORT  Vid;
//  } PID_VID,*PPID_VID;
//
typedef struct _DldUsbVidPid
{
    UInt16              idProduct; // must be the first to be Windows Dl compatible
    UInt16              idVendor;
} DldUsbVidPid;


typedef struct _DldUsbVidPidEntry{
    
    DldUsbVidPid    VidPid;
    
    struct{
        UInt32 propagateUp:0x1;
    } flags;
    
} DldUsbVidPidEntry;


typedef struct _DldDeviceVidPidDscr{
    
    //
    // must always be the first entry!
    //
    DldCommonWL   common;
    
    //
    // number of entries in the arrUID array
    //
    UInt32 count;
    
    //
    // an array of VidPid entries, must be the last field!
    //
    DldUsbVidPidEntry  arrVidPid[ 1 ];
    
} DldDeviceVidPidDscr;


typedef struct _DldQuirksOfSecuritySettings{
    
    //
    // USB
    //
    Boolean controlUsbHid;
    Boolean controlUsbMassStorage;
    Boolean controlUsbBluetooth;
    Boolean controlUsbNetwork;
    
    //
    // IEEE 1394
    //
    Boolean controlIEE1394Storage;
    
    //
    // Bluetooth
    //
    Boolean controlBluetoothHid;
    
    //
    // Shadow
    //
    Boolean disableOnShadowErrors;
    
    //
    // CAWL
    //
    Boolean disableAccessOnCawlError;
    
    //
    // behaviour on service crash, if the service(daemon) process
    // terminates incorrectly ( w/o calling kt_DldUserClientClose )
    // access to all devices will be disabled( except the root volume
    // for admins and devices that are allowed by the special setting )
    //
    Boolean disableAccessOnServiceCrash;
    
} DLD_ALIGNMENT DldQuirksOfSecuritySettings;

//--------------------------------------------------------------------

#define DLD_LOG_TYPE_UNKNOWN        ((UInt32)0x0) // invalid value
#define DLD_LOG_TYPE_FSD            ((UInt32)0x1) // file system
#define DLD_LOG_TYPE_DEVCE          ((UInt32)0x2) // Common device log
#define DLD_LOG_TYPE_USB            ((UInt32)0x3) // USB

#define DLD_KAUTH_SCOPE_UNLNOWN_ID  ((UInt32)0x0) // invalid value
#define DLD_KAUTH_SCOPE_VNODE_ID    ((UInt32)0x1) // KAUTH_SCOPE_VNODE
#define DLD_KAUTH_SCOPE_FILEOP_ID   ((UInt32)0x2) // KAUTH_SCOPE_FILEOP

typedef enum _DldFileOperation{
    DldFileOperationUnknown = 0x0,
    DldFileOperationOpen,
    DldFileOperationRead,
    DldFileOperationWrite,
    DldFileOperationRename,
    
    DldFileOperationMax = ~(UInt32)0x0
} DldFileOperation;

//
// the common header that contains enough infrmation for "minimal log"
//
typedef struct _DldLogHeader{
    
    //
    // always the first, one of the DLD_LOG_TYPE_XXX,
    // set by the log subsystem client to define
    // the type of the log data
    //
    UInt32              type;
    
    //
    // a counter can wrap around, it should be used
    // only to find whether there were lost entries,
    // tha value set internaly by the log subsystem,
    // the value set by a subsystem client is ignored
    //
    UInt32              logEntryNumber;
    
    //
    // device type for a device related with the log entry
    //
    DldDeviceType       deviceType;
    
    //
    // DL and kauth_action_t
    //
    DldRequestedAccess  action;
    
    //
    // a user's GUID, zeroed if unknown
    //
    guid_t              userGuid;
    
    //
    // a user's UID, zeroed if unknown
    //
    uid_t               userUid;
    
    //
    // a user's GID, zeroed if unknown
    //
    gid_t               userGid;
    
    //
    // true if the request was disabled by DL
    //
    bool                isDisabledByDL;
    
    //
    // if true the device is encrypted, makes sense only for removable storages
    //
    bool                isEncrypted;
    
    //
    // if not zero there was an error in the CAWL subsystem,
    // if zero then should be ignored as this doesn't mean that
    // the CAWL subsystem processed the request
    //
    UInt32               cawlError;
    
} DLD_ALIGNMENT DldLogHeader;


typedef union _DldDriverDataLog{
    
    //
    // a common header must be placed at the start
    // of any log data structure
    //
    DldLogHeader      Header;
    
    
    struct{// for DLD_LOG_TYPE_FSD
        
        DldLogHeader        Header;
        
        //
        // a process ID
        //
        UInt32              pid;
        
        //
        // one of the DLD_KAUTH_SCOPE_XXXX_ID definitions
        //
        UInt32              scopeID;
        
        //
        // vnode type
        //
        enum vtype          v_type;
        
        //
        // a type of FSD operation
        //
        DldFileOperation    operation;
        
        //
        // a process name
        //
        char                p_comm[MAXCOMLEN + 1];
        
        //
        // must be the last member, when passing
        // to the user space the buffer is cut
        // up to the terminating zero
        //
        char                path[MAXPATHLEN];
    } DLD_ALIGNMENT Fsd;
    
    union{
        
        DldLogHeader  Header;
        
        struct {// for DLD_LOG_TYPE_USB
            
            DldLogHeader  Header;
            
            //
            // device's IDs, uid might not exist for a device
            //
            DldUsbVidPid  vidPid;
            DldDeviceUID  uid;
            
        } DLD_ALIGNMENT USB;
        
    } DLD_ALIGNMENT device;
    
} DLD_ALIGNMENT DldDriverDataLog, DldDriverLogData;

//--------------------------------------------------------------------

#define DLD_EVENT_UNKNOWN      (0x0) // an invalid value
#define DLD_EVENT_SIGNAL       (0x1) // the driver caught a signal
#define DLD_EVENT_FORCED_STOP  (0x2) // stop the service
#define DLD_EVENT_IMPORT_DLS   (0x3) // import DLS from the file
#define DLD_EVENT_GPUPDATE     (0x4) // forcely update group policy
#define DLD_EVENT_RESPONSE     (0x5) // a response from the service to the driver

typedef struct _DldEventHeader{
    
    //
    // an unique number for the event, must not be consequitive!
    //
    SInt32    id;
    
    //
    // type of the event, one of DLD_EVENT_XXXX
    //
    UInt32     type;
    
    //
    // 0x1 if the client is trusted, 0x0 otherwise
    //
    UInt32     trustedClient;
    
    //
    // an index for a waiting block the driver is waiting reply on,
    // 0x0 means there is no waiting block and the driver is not
    // waiting for response
    //
    UInt32     waitBlockIndex;
    
    //
    // the size of the data including the header
    //
    UInt32     size;
    
} DLD_ALIGNMENT DldEventHeader;

typedef struct _DldDriverEventData{
    
    //
    // a common header must be placed at the start
    // of any log data structure
    //
    DldEventHeader      Header;
    
    union{
        
        // DLD_EVENT_SIGNAL
        struct{
            
            //
            // a signal itself, SIGTERM etc
            //
            UInt32 signum;
            
            //
            // a process that sent a signal
            //
            pid_t  senderPid;
            
            //
            // a process to which a signal was sent
            //
            pid_t  receiverPid;
            
            //
            // 0x1  if the signal delivery was rejected by the driver
            //
            UInt32 deliveryRejected;
            
        } DLD_ALIGNMENT Signal;
        
        // DLD_EVENT_FORCED_STOP
        struct{
            
            UInt32 reserved[4];
            
        } DLD_ALIGNMENT ForcedStop;
        
        // DLD_EVENT_IMPORT_DLS
        struct{
            
            //
            // a size of the DLS file path placed after this structure. the length includes the terminating zero
            //
            UInt32 filePathLength;
            
            //
            // if not zero then do not check for DLS file signing instead check for admin rights
            //
            UInt32 adminRequest;

            //
            // a user ID of a process that sent a signal
            //
            uid_t  senderUid;

        } DLD_ALIGNMENT ImportDLS;
        
        // a response from the service to the driver
        // DLD_EVENT_RESPONSE
        struct{
            IOReturn  operationCompletionStatus;
        } DLD_ALIGNMENT Response;
        
    } Tail;
    
} DLD_ALIGNMENT DldDriverEventData;

//
// DldDriverEventData defines the buffer that is enable to contain any event with the trailing data,
// it does not defines exactly the data layout, as the actual data size might be smaller
//
typedef struct _DldDriverEventDataMax{
    
    DldDriverEventData event;
    
    union{
        
        struct{// DLD_EVENT_IMPORT_DLS
            
            //
            // a DLS file path, the actual length is defined by filePathLength
            //
            char  filePath[MAXPATHLEN/*filePathLength*/];
            
        } importDLS;
        
    } trailingData;
    
} DLD_ALIGNMENT DldDriverEventDataMax;

//
// the maximum event size including all trailing data
//
#define DLD_MAX_EVENT_SIZE   (sizeof(DldDriverEventDataMax))

//--------------------------------------------------------------------

typedef struct _DldSocketID
{
    //
    // a socket handle ( a socket address currently )
    //
    UInt64                      socket;
    
    //
    // a socket sequence to distinguish socket reusage and to not apply properties to reused socket
    //
    UInt64                      socketSequence;
    
} DldSocketID;

typedef union _DldSocketObjectAddress {
    struct sockaddr     hdr;
    struct sockaddr_in	addr4;		// ipv4 local addr
    struct sockaddr_in6	addr6;		// ipv6 local addr
} DldSocketObjectAddress;

typedef enum _DldSocketFilterEvent{
    DldSocketFilterEventUnknown = 0x0,
    DldSocketFilterEventConnected,
    DldSocketFilterEventDisconnected,
    DldSocketFilterEventShutdown,
    DldSocketFilterEventCantrecvmore,
    DldSocketFilterEventCantsendmore,
    DldSocketFilterEventClosing,
    DldSocketFilterEventBound,
    DldSocketFilterEventDataIn,
    DldSocketFilterEventDataOut,
    
    //
    // always the last, used to prevent the compiler from shrinking the enumerator size to 16 bytes
    //
    DldSocketFilterEventMax = 0xFFFFFFFF
} DldSocketFilterEvent;

//
// the notification layout is
//   DldSocketFilterNotification
//   DldSocketFilterEventXXXXXData
//   Data
//

typedef struct _DldSocketFilterEventConnectedData{
    sa_family_t sa_family;
    DldSocketObjectAddress localAddress;
	DldSocketObjectAddress remoteAddress;
} DLD_ALIGNMENT DldSocketFilterEventConnectedData;


typedef struct _DldSocketFilterEventDisconnectedData{
    
    UInt32  reserved;
    
} DLD_ALIGNMENT DldSocketFilterEventDisconnectedData;


typedef struct _DldSocketFilterEventShutdownData{
    
    UInt32  reserved;
    
} DLD_ALIGNMENT DldSocketFilterEventShutdownData;


typedef struct _DldSocketFilterEventCantrecvmoreData{
    
    UInt32  reserved;
    
} DLD_ALIGNMENT DldSocketFilterEventCantrecvmoreData;


typedef struct _DldSocketFilterEventCantsendmoreData{
    
    UInt32  reserved;
    
} DLD_ALIGNMENT DldSocketFilterEventCantsendmoreData;


typedef struct _DldSocketFilterEventClosingData{
    
    UInt32  reserved;
    
} DLD_ALIGNMENT DldSocketFilterEventClosingData;


typedef struct _DldSocketFilterEventBoundData{
    
    UInt32  reserved;
    
} DLD_ALIGNMENT DldSocketFilterEventBoundData;

#define kt_DldSocketBuffersNumber  40 // the maximum number 254
// static char StaticAssert_DldSocketBuffersNumber[ (kt_DldSocketBuffersNumber <= 254) ? 0 : -1 ];

#ifndef UINT8_MAX
    #define UINT8_MAX         255
#endif // UINT8_MAX

#ifndef UINT32_MAX
    #define UINT32_MAX        4294967295U
#endif // UINT32_MAX

typedef struct _DldSocketFilterEventIoData{
    
    //
    // a full data size that follows this event,
    // a client must fetch all entries up to this
    // data size that follows this event
    //
    UInt32  dataSize;
    
    //
    // internal index, unique only for the socket, not uniqie across sockets
    //
    UInt32  dataIndex;
    
    //
    // data are placed in the buffers that are mapped by a call to IOConnectMapMemory
    // contains buffer indices, the terminating element has 0xFF value
    // if there is no terminating element the data occupies all buffers
    //
    UInt8   buffers[ kt_DldSocketBuffersNumber ];
    
} DLD_ALIGNMENT DldSocketFilterEventIoData;


typedef struct _DldSocketFilterNotification{
    
    //
    // type of the event
    //
    DldSocketFilterEvent   event;
    
    //
    // the full size of the notification structure, including all trailing data
    //
    UInt32                 size;
    
    //
    // a handle for a socket, an internal driver representation
    //
    DldSocketID            socketId;
    
    //
    // union is to assist the compiler with data size and aligning
    //
    union{
        struct{
            UInt32   notificationForDisconnectedSocket: 0x1;
        } separated;
        
        UInt32  combined;
    } flags;
    
    //
    // event's data
    //
    union{
        DldSocketFilterEventConnectedData       connected;
        DldSocketFilterEventDisconnectedData    disconnected;
        DldSocketFilterEventShutdownData        shutdown;
        DldSocketFilterEventCantrecvmoreData    cantrecvmore;
        DldSocketFilterEventCantsendmoreData    cantsendmore;
        DldSocketFilterEventClosingData         closing;
        DldSocketFilterEventBoundData           bound;
        DldSocketFilterEventIoData              inputoutput; // DldSocketFilterEventDataIn OR DldSocketFilterEventDataOut
    }  eventData;
    
} DLD_ALIGNMENT DldSocketFilterNotification;


typedef enum _DldSocketDataPropertyType{
    DldSocketDataPropertyTypeUnknown = 0x0,
    DldSocketDataPropertyTypePermission = 0x1,
    
    //
    // just to help a compiler to infer data type
    //
    DldSocketDataPropertyMax = UINT32_MAX
} DldSocketDataPropertyType;

//--------------------------------------------------------------------

typedef enum _DldCapturingMode{
    DldCapturingModeInvalid = 0x0,    // an invalid value
    DldCapturingModeAll = 0x1,        // capture all trafic
    DldCapturingModeNothing = 0x2,    // do not capture, passthrough traffic
    DldCapturingModeMax = UINT32_MAX  // to guide the compiler with type inferring
} DldCapturingMode;

typedef struct _DldSocketDataProperty{
    
    DldSocketDataPropertyType   type;
    
    //
    // a data index as reported by DldSocketFilterEventIoData
    //
    SInt32                      dataIndex;
    
    
    //
    // a handle for a socket as reported by DldSocketFilterEventIoData
    //
    DldSocketID                 socketId;
    
    //
    // a socket sequence to distinguish socket reusage and to not apply properties to reused socket
    //
    UInt64                      socketSequence;
    
    union{
        
        //
        // DldSocketDataPropertyTypePermission
        //
        struct {
            uint8_t allowData;
        } permission;
        
    } value;
    
} DLD_ALIGNMENT DldSocketDataProperty;

#define kt_DldSocketDataPropertiesNumber kt_DldSocketBuffersNumber

typedef struct _DldSocketFilterServiceResponse
{
    //
    // if the type is DldSocketDataPropertyTypeUnknown then the property is ignored
    // and the left properties are not being processed
    //
    DldSocketDataProperty  property[ kt_DldSocketDataPropertiesNumber ];
    
    //
    // the terminating value is UIN8_MAX or the entire array is processed
    //
    UInt8   buffersToRelease[ kt_DldSocketBuffersNumber ];
    
} DLD_ALIGNMENT DldSocketFilterServiceResponse;

//--------------------------------------------------------------------

//
// the parameters directory layout for {plist}\IOKitPersonalities\DeviceLockIOKitDriver is as follows
// DldParameters {dic}
//   LogFlags  {dic}
//     SCSI  {bool}
//     .....
//     ACCESS_DENIED  {bool}
//     .....
//
//
#define kDldParameters      "DldParameters"
#define kLogFilePath        "LogFilePath"
#define kLogErrors          "LogErrors"
#define kLogFlags           "LogFlags"
#define kCOMMON             "COMMON"
#define kQUIRKS             "QUIRKS"
#define kSCSI               "SCSI"
#define kDVD_WHITE_LIST     "DVD_WHITE_LIST"
#define kWHITE_LIST         "WHITE_LIST"
#define kACL_EVALUATUION    "ACL_EVALUATUION"
#define kACCESS_DENIED      "ACCESS_DENIED"
#define kFILE_VAULT         "FILE_VAULT"
#define kPOWER_MANAGEMENT   "POWER_MANAGEMENT"
#define kDEVICE_ACCESS      "DEVICE_ACCESS"
#define kNET_PACKET_FLOW    "NET_PACKET_FLOW"

//--------------------------------------------------------------------

typedef struct _DldProcessSecurity{
    
    pid_t  pid;
    
    // acl is passed as a standalone parameter
    
    //
    // an ACL describes access rights for the process,
    // the rights is a combination of DL_PROCESS_* rights
    //
    // a user space pointer to the kauth_filesec structure
    //
    mach_vm_address_t acl;//kauth_filesec
    mach_vm_size_t    aclSize;
    
} DLD_ALIGNMENT DldProcessSecurity;

//--------------------------------------------------------------------

typedef struct _DldDLAdminsSettings
{
    UInt8 EnableDefaultSecurity;
    UInt8 EnableUnhookProtection;
    UInt8 EnableSysFilesProtection;
    UInt8 UseStrongIntegrCheck;
    
} DLD_ALIGNMENT DldDLAdminsSettings;

//--------------------------------------------------------------------

#define DldFileSecurityAceFlagsEnable   0x1
#define DldFileSecurityAceFlagsDisable  0x2

typedef struct _DldAceWithProcs{
    
    //
    // DldFileSecurityAceFlags***
    //
    UInt32              flags;
    
    //
    // if PID is (-1) than the entry is applied to all
    //
    pid_t               pid;
    
    //
    // kauth access rights KAUTH_VNODE_*
    //
    kauth_ace_rights_t  rights;
    
} DLD_ALIGNMENT DldAceWithProcs;


typedef struct _DldAclWithProcs{
    
    //
    // number of ACEs in the array
    //
    UInt32          count;
    
    //
    // array of ACEs
    //
    DldAceWithProcs  ace[1];
    
} DLD_ALIGNMENT DldAclWithProcs;

#define DldAclWithProcsSize( _AclWithProcs_ ) ( offsetof( DldAclWithProcs, ace ) + (_AclWithProcs_)->count * sizeof((_AclWithProcs_)->ace[0]) )

//--------------------------------------------------------------------

typedef struct _DldFileSecurity{
    
    mach_vm_address_t   path;
    mach_vm_size_t      pathSize; // in bytes, including terminating zero
    
    //
    // acls are passed as a standalone parameter
    //
    
    //
    // an ACL describes access rights for a file,
    // the rights is a combination of kauth_ace_rights_t rights
    //
    // a user space pointer to the kauth_filesec structure
    //
    mach_vm_address_t   usersAcl; // kauth_filesec, might be NULL
    mach_vm_size_t      usersAclSize;
    
    //
    // a file ACL describing processes access rights
    //
    mach_vm_address_t   processesAcl; // DldAclWithProcs, might be NULL
    mach_vm_size_t      processesAclSize;
    
} DLD_ALIGNMENT DldFileSecurity;

//--------------------------------------------------------------------

#define DLD_IPCOP_INVALID           (0x0) // an invalid request
#define DLD_IPCOP_STOP_SERVICE      (0x1) // input DldIpcRequest, no output
#define DLD_IPCOP_IMPORT_DLS        (0x2) // input DldIpcRequest, no output
#define DLD_IPCOP_GPUPDATE          (0x3) // input DldIpcRequest, no output

typedef struct _DldIpcRequest{
    
    //
    // the full size, including any trailing data, i.e. sizeof(DldIpcRequest)+size of trailing data 
    //
    UInt32   size;
    
    //
    // one of DLD_IPCOP_XXXX
    //
    UInt32   operation;
    
    union {
        
        // DLD_IPCOP_STOP_SERVICE
        struct {
            
            UInt32 reserved32[2];
            
        } StopService;
        
        struct {
            
            //
            // a size of the DLS file path placed after this structure. the length includes the terminating zero
            //
            UInt32 filePathLength;
            
            //
            // if not zero then do not check for DLS file signing instead check for admin rights
            //
            UInt32 adminRequest;
            
        } ImportDLS;
        
    } Tail;
    
} DLD_ALIGNMENT DldIpcRequest;

//--------------------------------------------------------------------

typedef struct _DldEncryptionProviderProperties{
    EncryptionProviderEnum    provider;
    UInt8                     enabled;
} DLD_ALIGNMENT DldEncryptionProviderProperties;

//--------------------------------------------------------------------

#endif//_DLDUSERTOKERNEL_H

