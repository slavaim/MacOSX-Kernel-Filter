/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOSHADOWFILE_H
#define _DLDIOSHADOWFILE_H

#include <libkern/c++/OSObject.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include "DldCommon.h"

//--------------------------------------------------------------------

class DldIOShadowFile: public OSObject{
    
    OSDeclareDefaultStructors( DldIOShadowFile )
    
private:
    
    char*        path;
    size_t       pathLength;//including the zero terminator, up to MAXPATHLEN + sizeof( L'\0' )
    
    
    UInt64       bytesWritten;
    
    //
    // the vnode is marked as noncached
    //
    vnode_t      vnode;
    
    //
    // an internal ID set by a user mode client
    //
    UInt32       ID;
    
    //
    // current offset, the offset from
    // which the next write starts
    //
    off_t        offset;
    
    //
    // the offset to which the last asynchronous extension
    // was made, might be smaller than the offset value
    // as doesn't reflect the current actual file size
    // but it represents the size which can be allocated
    // without file expansion
    //
    off_t        lastExpansion;
    
    //
    // a maximum allowed size for the file,
    // if DLD_IGNR_FSIZE the value is ignored
    //
    off_t        maxSize;
    
    //
    // a file size at which the driver tries to switch to the next
    // shadow file, if DLD_IGNR_FSIZE then ignored and the file
    // will never be switched if there is no file to switch for
    //
    off_t        switchSize;
    
    //
    // a rw-lock to protect internal data
    //
    IORWLock*    rwLock;
    
#if defined( DBG )
    thread_t     exclusiveThread;
#endif//DBG
    
    virtual bool initWithFileName( __in char* name, __in size_t nameLength );
    
    //
    // returns the current size to which the file has been extended,
    // if the lowTargetSize value is 0x0 it is calculated empirically,
    // the file might be extended beyound the lowTargetSize value which
    // defines the low boundary
    //
    off_t expandFile( __in off_t lowTargetSize = 0x0 );
    
    void LockShared();
    void UnLockShared();
    
    void LockExclusive();
    void UnLockExclusive();
    
protected:
    
    virtual void free();
    
public:
    
    //
    // length includes the zero terminator
    //
    static DldIOShadowFile* withFileName( __in char* name, __in size_t nameLength );
    
    //
    // used only for logging
    //
    char* getPath(){ return this->path; }
    
    //
    // the buffer must be from the kernel space
    //
    IOReturn write( __in void* kernelAddress, __in off_t length, __in_opt off_t offsetForShadowFile = DLD_IGNR_FSIZE );
    
    void setFileSizes( __in off_t maxFileSize, __in off_t switchFileSize );
    bool reserveOffset( __in off_t sizeToWrite, __out off_t*  reservedOffset );
    
    //
    // returns true if there is a quota for file
    //
    bool isQuota();
    
    //
    // returns true if the file should be released and
    // the current shadowing file should be switched to
    // the next if possible
    //
    bool isFileSwicthingRequired();
    
    void setID( __in UInt32 ID );
    UInt32 getID(){ return this->ID; }
    
};

//--------------------------------------------------------------------

#endif//_DLDIOSHADOWFILE_H
