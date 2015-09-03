/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDUSBWHITELIST_H
#define DLDUSBWHITELIST_H

#include "DldCommon.h"
#include "DldAclObject.h"
#include "DldSupportingCode.h"

//--------------------------------------------------------------------

typedef enum _DldWhiteListType{
    
    // permanent white lists
    kDldWhiteListUnknown = 0x0,  // a fake list
    kDldWhiteListUSBVidPid,      // an object with DldDeviceVidPidDscr copy
    kDldWhiteListUSBUid,         // an object with DldDeviceUIDDscr copy
    kDldWhiteListDVDUid,         // an object with DldDeviceUIDDscr copy
    
    // temporary white lists
    kDldTempWhiteListUSBVidPid,  // an object with DldDeviceVidPidDscr copy
    kDldTempWhiteListUSBUid,     // an object with DldDeviceUIDDscr copy
    
    // always the last , the process white list is not in this list as it is hardcoded internally
    kDldWhiteListMax
} DldWhiteListType;

//--------------------------------------------------------------------

bool
DldIsProcessinWhiteList(
    __in vfs_context_t   vfsContext,
    __in dld_classic_rights_t winRequestedAccess
    );

bool
DldIsDLServiceOrItsChild(
    __in pid_t   pid
    );

//--------------------------------------------------------------------

inline
const char*
DldWhiteListTypeToName( __in DldWhiteListType type )
{
    switch( type ){
            
        case kDldWhiteListUSBVidPid:
            return "kDldWhiteListUSBVidPid";
        case kDldWhiteListUSBUid:
            return "kDldWhiteListUSBUid";
        case kDldWhiteListDVDUid:
            return "kDldWhiteListDVDUid";
        case kDldTempWhiteListUSBVidPid:
            return "kDldTempWhiteListUSBVidPid";
        case kDldTempWhiteListUSBUid:
            return "kDldTempWhiteListUSBUid";
        default:
            return"UnknownWhiteListType";
    }// end type
}

//--------------------------------------------------------------------

class DldWhiteList: public OSObject{
    
    OSDeclareDefaultStructors( DldWhiteList )
    
private:
    
    //
    // a watermark is used to provide a client of the class with the ability
    // to track white list changes, notr that kDldWhiteListUSBVidPid and
    // kDldWhiteListUSBUid share the same watermark ( i.e. kDldWhiteListUSBVidPid )
    // as this two properties are not independent from each other
    //
    //
    volatile UInt32      watermark[ kDldWhiteListMax ];
    
    //
    // white list desciptors, the format is specific for each white list type
    //
    OSData*      wlDscr[ kDldWhiteListMax ];
    
    //
    // an array of ACLs, each ACL defines users and groups
    // to which a corresponding white list is applied
    // if there is no ACL ( i.e. NULL ) then
    // the white list is applied indiscriminately
    // to all users ( this is a Windows DL behaviour,
    // which is not good and not scalable )
    //
    DldAclObject* wlACLs[ kDldWhiteListMax ];
    
    //
    // a rw-lock to protect objects and internal data for each white list
    //
    IORWLock*   rwLock[ kDldWhiteListMax ];
    
#if defined( DBG )
    thread_t    exclusiveThread[ kDldWhiteListMax ];
#endif//DBG
    
    void LockShared( __in DldWhiteListType type );
    void UnLockShared( __in DldWhiteListType type );
    
    void LockExclusive( __in DldWhiteListType type );
    void UnLockExclusive( __in DldWhiteListType type);
    
    DldWhiteListType  castWatermarkType( __in DldWhiteListType type )
    {
        //
        // all white lists compose a single set of white listed devices
        //
        switch( type ){
            case kDldWhiteListUSBVidPid:
            case kDldTempWhiteListUSBVidPid:
            case kDldWhiteListUSBUid:
            case kDldTempWhiteListUSBUid:
                type = kDldWhiteListUSBVidPid;
                break;
            default: // just to calm a compiler
                break;
        }
        
        return type;
    }
    
    void updateWatermark( __in DldWhiteListType type )
    { 
        
        //
        // a memory barrier to prevent CPU from reodering( unlikely needed
        // as usually the following UnLockExclusive is also put a memory barrier
        // and the the WL object change is impossible when the lock
        // is held )
        //
        DldMemoryBarrier();
        this->watermark[ this->castWatermarkType( type ) ] += 0x1; 
    };
    
    
protected:
    
    virtual bool init();
    virtual void free();
    
private:
    
    //
    // returns the entry copy for the VidPid, if entry is not found the false is returned,
    // the caller must provide a space for the outEntry entry,
    // the returned *outAclObject is referenced
    //
    bool getVidPidEnrtyEx( __in DldWhiteListType type,
                           __in DldUsbVidPid* VidPid,
                           __inout DldUsbVidPidEntry* outEntry,
                           __out_opt DldAclObject** outAclObject = NULL );
    
public:
    
    static DldWhiteList* newWhiteList();
    
    //
    // copies the wl body, if the wl value is NULL the current
    // wl is removed
    //
    bool setWhiteListWithCopy( __in     DldWhiteListType type,
                               __in_opt const void   * whiteList,
                               __in     unsigned int numBytes,
                               __in_opt const kauth_acl_t  acl = NULL );
    
    
    static bool isWhiteListValid(  __in DldWhiteListType type,
                                   __in const void   * whiteList,
                                   __in unsigned int   numBytes );
    
    //
    // returns the entry for the VidPid, if entry is not found the NULL value is returned,
    // the entry is valid as long as the dscr is valid
    //
    static DldUsbVidPidEntry* getVidPidEnrty( __in DldUsbVidPid* VidPid, __in DldDeviceVidPidDscr* dscr );
    
    //
    // returns the entry for the UID, if entry is not found the NULL value is returned,
    // the entry is valid as long as the dscr is valid,
    // uid must be 16 bytes long
    //
    static DldDeviceUIDEntry* getDeviceUIDEnrty( __in DldDeviceUID* uid, __in DldDeviceUIDDscr* dscr );
    
    //
    // returns the entry copy for the VidPid, if entry is not found the false is returned,
    // the caller must provide a space for the outEntry entry,
    // the returned *outAclObject is referenced
    //
    bool getVidPidEnrty( __in DldUsbVidPid* VidPid,
                         __inout DldUsbVidPidEntry* outEntry,
                         __out_opt DldAclObject** outAclObject = NULL );
    
    //
    // returns the entry copy for the UID, if entry is not found the false is returned,
    // the caller must provide a space for the outEntry entry
    // uid must be 16 bytes long,
    // the returned *outAclObject is referenced
    //
    bool getUsbUIDEnrty( __in DldDeviceUID* uid,
                         __inout DldDeviceUIDEntry* outEntry,
                         __out_opt DldAclObject** outAclObject = NULL );
    
    bool getDvdUIDEnrty( __in CDDVDDiskID* uid,
                         __inout DldDeviceUIDEntry* outEntry,
                         __out_opt DldAclObject** outAclObject = NULL );
    
    bool getDeviceUIDEnrty( __in DldWhiteListType type,
                            __in DldDeviceUID* uid,
                            __inout DldDeviceUIDEntry* outEntry,
                           __out_opt DldAclObject** outAclObject );
    
    void ApplyWhiteList( __in DldWhiteListType type );
    void ApplyWhiteListUSB( __in DldWhiteListType type );
    void ApplyWhiteListDVD( __in DldWhiteListType type );
    
    //
    // as the access is without the lock, the caller must be aware that the returned
    // value might be mangled in case of concurrent access ( not an issue for the Intel CPU family ),
    // if a CPU doesn't provide Lamport's style sequential execution for concurrent access the caller
    // must use either the lock ot read-remember-read-compare sequence
    //
    UInt32 getWatermark( __in DldWhiteListType type ){ 
        
        UInt32   wm;
        
        wm = this->watermark[ this->castWatermarkType( type ) ]; 
        
        //
        // a memory barrier to prevent CPU from reodering( unlikely needed
        // as the following UnLockExclusive is also put a memory barrier
        // and the the WL object reference is impossible when the lock
        // is held )
        //
        DldMemoryBarrier();
        
        return wm;
    };
    
};

//--------------------------------------------------------------------

extern DldWhiteList*    gWhiteList;

//--------------------------------------------------------------------

#endif//DLDUSBWHITELIST_H