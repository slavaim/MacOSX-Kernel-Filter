/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOShadowFile.h"
#include "DldVnodeHashTable.h"
#include <sys/fcntl.h>
#include <sys/vm.h>
#include "DldIOShadow.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldIOShadowFile, OSObject )

//--------------------------------------------------------------------

bool DldIOShadowFile::initWithFileName( __in char* name, __in size_t nameLength )
{
    assert( preemption_enabled() );
    
    
    if( nameLength > (MAXPATHLEN + sizeof( L'\0' )) ){
        
        DBG_PRINT_ERROR(("length > (MAXPATHLEN + sizeof( L'\\0' ))\n"));
        return false;
    }
    
    
    if( !super::init() ){
        
        DBG_PRINT_ERROR(("super::init() failed\n"));
        return false;
    }
    
    this->rwLock = IORWLockAlloc();
    assert( this->rwLock );
    if( !this->rwLock ){
        
        DBG_PRINT_ERROR(("this->rwLock = IORWLockAlloc() failed\n" ));
        return false;
    }
    
    //
    // set a maximum file size to infinity
    //
    this->maxSize = DLD_IGNR_FSIZE;
    
    //
    // set switch size to 512 MB
    //
    this->switchSize = 0x20000000ll;
    
    this->path = (char*)IOMalloc( nameLength );
    assert( this->path );
    if( !this->path ){
        
        DBG_PRINT_ERROR(("this->path = IOMalloc( %u ) failed\n", (int)nameLength ));
        return false;
    }
    
    this->pathLength = nameLength;
    
    memcpy( this->path, name, nameLength );
    assert( L'\0' == this->path[ this->pathLength - 0x1 ] );
    
    //
    // open or create the file
    //
    errno_t  error;
    vfs_context_t   vfs_context;
    
    vfs_context = vfs_context_create( NULL );
    assert( vfs_context );
    if( !vfs_context )
        return false;
    
    //
    // open or create a file, truncate if the file exists
    //
    error = vnode_open( this->path,
                        O_EXLOCK | O_RDWR | O_CREAT | O_TRUNC | O_SYNC, // fmode
                        0644,// cmode
                        0x0,// flags
                        &this->vnode,
                        vfs_context );
    vfs_context_rele( vfs_context );
    if( error ){
        
        DBG_PRINT_ERROR(("vnode_open() failed with the %u error\n", error));
        return false;
    }
    
    //
    // vn_open returns with both a use_count
    // and an io_count on the found vnode
    //
    
    //
    // mark as an internal non shadowed vnode, use the CreateAndAddIOVnodByBSDVnode
    // as the kauth callback might have not been registered so the vnode open
    // was not seen by the DL driver
    //
    DldIOVnode*  dldVnode;
    dldVnode = DldVnodeHashTable::sVnodesHashTable->CreateAndAddIOVnodByBSDVnode( this->vnode );
    assert( dldVnode );
    if( !dldVnode ){
        
        //
        // we can't use this vnode as will be unable to distinguish
        // the DL internal writes from the user writes this will result
        // in an endless loop
        //
        DBG_PRINT_ERROR(("RetrieveReferencedIOVnodByBSDVnode() failed\n"));
        
        this->release();
        return NULL;
    }
    
    //
    // mark as skipped for log, shadowing and any other operations
    //
    dldVnode->flags.internal = 0x1;
    
    dldVnode->release();
    DLD_DBG_MAKE_POINTER_INVALID( dldVnode );
    
    this->expandFile();
    
    //
    // mark as noncached vnode, writes must be sector aligned!
    //
    //vnode_setnocache( this->vnode );
    
    return true;
}

//--------------------------------------------------------------------

void DldIOShadowFile::setID( __in UInt32 ID )
{
    //
    // it is abnormal to change the valid file's ID
    //
    assert( 0x0 == this->ID );
    
    this->ID = ID;
}

//--------------------------------------------------------------------

void DldIOShadowFile::setFileSizes( __in off_t maxFileSize, __in off_t switchFileSize )
{
    assert( DLD_IGNR_FSIZE == this->maxSize );
    
    //
    // check switchFileSize for validity and fix if not valid
    //
    if( DLD_IGNR_FSIZE != maxFileSize && switchFileSize > maxFileSize )
        switchFileSize = 0;
    
    //
    // do not honour small alocations
    //
    if( DLD_IGNR_FSIZE != maxFileSize && maxFileSize < DLD_SHADOW_FILE_DEFAULT_MINSIZE )
        maxFileSize = DLD_SHADOW_FILE_DEFAULT_MINSIZE;
    
    this->maxSize = maxFileSize;

    if( DLD_IGNR_FSIZE != maxFileSize ){
        
        //
        // check for a mad client
        //
        if( DLD_IGNR_FSIZE == switchFileSize || 0x0 == switchFileSize || switchFileSize < (maxFileSize/2) )
            switchFileSize = (maxFileSize - maxFileSize/8);
        
        this->switchSize = min( switchFileSize, (maxFileSize - maxFileSize/8) );
        
    } else if( DLD_IGNR_FSIZE != switchFileSize && 0x0 != switchFileSize ){
        
        assert( DLD_IGNR_FSIZE == maxFileSize );
        
        //
        // the client seems to be quite sane but do not allow to set
        // the switch threshold low than 128 MB
        //
        this->switchSize = max( switchFileSize, DLD_SHADOW_FILE_DEFAULT_MINSIZE );
        
    } else {
        
        assert( DLD_IGNR_FSIZE == maxFileSize );

        //
        // the client can't make its mind, set to 512 MB
        //
        this->switchSize = 4*DLD_SHADOW_FILE_DEFAULT_MINSIZE;
    }
    
}

//--------------------------------------------------------------------

bool DldIOShadowFile::isQuota()
{
    return ( DLD_IGNR_FSIZE != this->maxSize );
}

//--------------------------------------------------------------------

bool DldIOShadowFile::isFileSwicthingRequired()
{
    assert( DLD_IGNR_FSIZE != this->switchSize && 0x0 != this->switchSize );
    
    return ( this->switchSize <= this->offset );
}

//--------------------------------------------------------------------

off_t DldIOShadowFile::expandFile( __in off_t lowTargetSize )
{
    assert( preemption_enabled() );
    
    off_t             target;
    off_t             currentLastExpansion;
#if defined( DBG )
    currentLastExpansion = DLD_IGNR_FSIZE;
#endif//DBG
    
    //
    // Make sure that we are root.  Growing a file
    // without zero filling the data is a security hole 
    // root would have access anyway so we'll allow it
    //
    
    //
    // TO DO - is_suser is not supported for 64 bit kext
    //

/*
#if defined(__i386__)
    assert( is_suser() );
    if( !is_suser() )
        return this->lastExpansion;// an unsafe unprotected access to 64b value
#elif defined(__x86_64__)
#endif//defined(__x86_64__)
*/

    
    //
    // check that the limit has not been reached
    //
    if( DLD_IGNR_FSIZE != this->maxSize  && this->lastExpansion == this->maxSize )
        return this->maxSize;// a safe access to 64b value as the value can't change
    
    
    this->LockExclusive();
    {// start of the lock
        
        off_t   valueToAdd;
        
        currentLastExpansion = this->lastExpansion;
        
        //
        // try to extend the file on 128 MB
        // ( the same value is used as a maximum value for a mapping
        //   range in DldIOShadow::processFileWriteWQ )
        //
        valueToAdd = 0x8*0x1000*0x1000;
        
        
        if( this->offset > this->lastExpansion )
            target = this->offset + valueToAdd;
        else
            target = this->lastExpansion + valueToAdd;
        
        if( target < lowTargetSize )
            target  = lowTargetSize;
        
        if( DLD_IGNR_FSIZE != this->maxSize && target > this->maxSize )
            target = this->maxSize;
        
    }// end of the lock
    this->UnLockExclusive();
    
    
    if( target < lowTargetSize ){
        
        //
        // there is no point for extension as the goal won't be reached, fail the request
        //
        return currentLastExpansion;
    }
    
    assert( DLD_IGNR_FSIZE != currentLastExpansion && currentLastExpansion <= target );
    
    //
    // extend the file
    //
    vfs_context_t       vfs_context;
    int                 error;
    
    error = vnode_getwithref( this->vnode );
    assert( !error );
    if( !error ){
        
        struct vnode_attr	va;
        
        VATTR_INIT(&va);
        VATTR_SET(&va, va_data_size, target);
        va.va_vaflags =(IO_NOZEROFILL) & 0xffff;
        
        vfs_context = vfs_context_create( NULL );
        assert( vfs_context );
        if( vfs_context ){
            
            this->LockExclusive();
            {// start of the lock
                
                assert( currentLastExpansion <= this->lastExpansion );
                //
                // do not truncate the file incidentally!
                //
                if( target > this->offset ){
                    
                    error = vnode_setattr( this->vnode, &va, vfs_context );
                    if( !error ){
                        
                        this->lastExpansion = target;
                        currentLastExpansion = target;
                    }
                    
                } else {
                  
                    //
                    // the file has been extended by the write operation
                    //
                    this->lastExpansion = this->offset;
                    currentLastExpansion = this->offset;

                }// end if( target < this->offset )
                
            }// end of the lock
            this->UnLockExclusive();
            
            vfs_context_rele( vfs_context );
            DLD_DBG_MAKE_POINTER_INVALID( vfs_context );
            
        }// end if( vfs_context )
        
        vnode_put( this->vnode );
        
    }// end if( !error )
    
    assert( DLD_IGNR_FSIZE != currentLastExpansion );
    
    return currentLastExpansion;
}

//--------------------------------------------------------------------

bool DldIOShadowFile::reserveOffset( __in off_t sizeToWrite, __out off_t*  reservedOffset_t )
{
    
    assert( preemption_enabled() );
    
    //
    // Make sure that we are root.  Growing a file
    // without zero filling the data is a security hole 
    // root would have access anyway so we'll allow it
    //
/*
#if defined(__i386__)
    assert( is_suser() );
#elif defined(__x86_64__)
#endif//defined(__x86_64__)
 */
    
    off_t  newOffset;
    off_t  reservedOffset;
    off_t  currentLastExpansion;
    bool   quotaExceeded = false;
    bool   expansionRequired = false;
    
    if( 0x0 == sizeToWrite ){
        
        this->LockShared();
        {// start of the lock
            *reservedOffset_t = this->offset;
        }// end of the lock
        this->UnLockShared();
        
        return true;
    }
    
    this->LockExclusive();
    {// start of the lock
        
        currentLastExpansion = this->lastExpansion;
        reservedOffset = this->offset;
        
        newOffset = this->offset + sizeToWrite;
        
        //
        // check for quota
        //
        if( DLD_IGNR_FSIZE != this->maxSize && newOffset > this->maxSize ){
            
            quotaExceeded = true;
        }
        
        if( !quotaExceeded ){
            
            this->offset = newOffset;
            
            expansionRequired =  this->offset > this->lastExpansion ||  // we crossed the border and the next write will extend the file OR
               ( this->lastExpansion - this->offset ) < 0x1000*0x1000;  // we are on the brink of the free space depletion
            
        }// end if( !quotaExceeded )

    }// end of the lock
    this->UnLockExclusive();
    
    
    if( quotaExceeded ){
        
        //
        // remeber that we failed at least once to shadow data due to quota exceeding
        //
        gShadow->quotaExceedingDetected();
        
        return false;
    }
    
    
    *reservedOffset_t = reservedOffset;
    
    //
    // extend the file,
    // only one expansion at any given time is allowed,
    // access fields w/o lock as concurrency is tolerable here
    //
    if( expansionRequired ){

        currentLastExpansion = this->expandFile( reservedOffset + sizeToWrite );
    }
    
    //
    // return true if there is a place for the following write
    //
    return ( reservedOffset + sizeToWrite ) <= currentLastExpansion;
}

//--------------------------------------------------------------------

IOReturn DldIOShadowFile::write( __in void* data, __in off_t length, __in_opt off_t offsetForShadowFile )
{
    /*!
     @function vn_rdwr
     @abstract Read from or write to a file.
     @discussion vn_rdwr() abstracts the details of constructing a uio and picking a vnode operation to allow
     simple in-kernel file I/O.
     @param rw UIO_READ for a read, UIO_WRITE for a write.
     @param vp The vnode on which to perform I/O.
     @param base Start of buffer into which to read or from which to write data.
     @param len Length of buffer.
     @param offset Offset within the file at which to start I/O.
     @param segflg What kind of address "base" is.   See uio_seg definition in sys/uio.h.  UIO_SYSSPACE for kernelspace, UIO_USERSPACE for userspace.
     UIO_USERSPACE32 and UIO_USERSPACE64 are in general preferred, but vn_rdwr will make sure that has the correct address sizes.
     @param ioflg Defined in vnode.h, e.g. IO_NOAUTH, IO_NOCACHE.
     @param cred Credential to pass down to filesystem for authentication.
     @param aresid Destination for amount of requested I/O which was not completed, as with uio_resid().
     @param p Process requesting I/O.
     @return 0 for success; errors from filesystem, and EIO if did not perform all requested I/O and the "aresid" parameter is NULL.
     */
    
    assert( preemption_enabled() );
    assert( length < 0x0FFFFFFF00000000ll );
    
    off_t  offset = offsetForShadowFile;
    
    if( DLD_IGNR_FSIZE == offsetForShadowFile && !this->reserveOffset( length, &offset ) ){
        
        DBG_PRINT_ERROR(("A quota has been exceeded for the %s file, the quota is %llu \n", this->path, this->maxSize ));
        return kIOReturnNoResources;
    }
    
#if defined( DBG )
    static UInt32 gWriteCount = 0x0;
    UInt32 writeCount = OSIncrementAtomic( &gWriteCount ) + 0x1;
#endif//DBG
    
    int   error = 0x0;
    off_t bytesWritten = 0x0;
    
    while( !error && length != bytesWritten ){
        
        unsigned int bytesToWrite;
        
#if defined( DBG )
        //
        // write by 32МВ chunks for each 2nd function invocation, and 1GB for others
        //
        if( (length - bytesWritten) > ((writeCount%2)? 0x40000000ll : 0x2000000ll) )
            bytesToWrite = (writeCount%2)? 0x40000000ll : 0x2000000ll;
        else
            bytesToWrite = (int)(length - bytesWritten);
#else
        //
        // write by 1GB chunks
        //
        if( (length - bytesWritten) > 0x40000000ll )
            bytesToWrite = 0x40000000;
        else
            bytesToWrite = (int)(length - bytesWritten);
#endif//!DBG
        
        
        error = vn_rdwr( UIO_WRITE,
                         this->vnode,
                         (char*)data,
                         bytesToWrite,
                         offset + bytesWritten,
                         UIO_SYSSPACE,
                         IO_NOAUTH | IO_SYNC,//IO_UNIT,
                         kauth_cred_get(),
                         NULL,
                         current_proc() );
        
        if( !error )
            bytesWritten = bytesWritten + bytesToWrite;
        else {
            DBG_PRINT_ERROR(("vn_rdwr( %s, %u ) failed with the 0x%X error\n", this->path, bytesToWrite, error));
        }

        
    }
    
    return error;
}

//--------------------------------------------------------------------

DldIOShadowFile* DldIOShadowFile::withFileName( __in char* name, __in size_t nameLength )
{
    assert( preemption_enabled() );
    
    //
    // check for the correct file format, the name must be a full path
    //
    if( 0x0 == nameLength || L'\0' != name[ nameLength - 0x1 ] || L'/' != name[0] ){
        
        DBG_PRINT_ERROR(("an invalid file name format\n"));
        return NULL;
    }
    
    
    DldIOShadowFile*  newFile = new DldIOShadowFile();
    assert( newFile );
    if( !newFile )
        return NULL;
    
    
    if( !newFile->initWithFileName( name, nameLength ) ){
        
        newFile->release();
        return NULL;
    }
    
    return newFile;
}

//--------------------------------------------------------------------

void DldIOShadowFile::free()
{
    assert( preemption_enabled() );
    
    if( NULLVP != this->vnode ){
        
        //
        // write a terminator
        //
        UInt64  terminator = DLD_SHADOW_FILE_TERMINATOR;
        this->write( &terminator, sizeof( terminator ), DLD_IGNR_FSIZE );
     
        //
        // TO DO - ponder the vfs context retrieval as it seems vfs_context_current might be 
        // an arbitrary one which differs from the open context
        //
        vfs_context_t   vfs_context;
        vfs_context = vfs_context_create( NULL );
        assert( vfs_context );
        if( vfs_context ){
        
            VNOP_FSYNC( this->vnode, MNT_WAIT, vfs_context );
            vnode_close( this->vnode, ( this->bytesWritten )? FWASWRITTEN: 0x0, vfs_context );
            vfs_context_rele( vfs_context );
            
        }// end if( vfs_context )
    }
    
    if( this->path ){
        
        assert( this->pathLength );
        IOFree( this->path, this->pathLength );
    }
    
    if( this->rwLock )
        IORWLockFree( this->rwLock );
    
}

//--------------------------------------------------------------------

void
DldIOShadowFile::LockShared()
{   
    assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( this->rwLock );
};

//--------------------------------------------------------------------

void
DldIOShadowFile::UnLockShared()
{   assert( this->rwLock );
    assert( preemption_enabled() );
    
    IORWLockUnlock( this->rwLock );
};

//--------------------------------------------------------------------

void
DldIOShadowFile::LockExclusive()
{
    assert( this->rwLock );
    assert( preemption_enabled() );
    
#if defined(DBG)
    assert( current_thread() != this->exclusiveThread );
#endif//DBG
    
    IORWLockWrite( this->rwLock );
    
#if defined(DBG)
    assert( NULL == this->exclusiveThread );
    this->exclusiveThread = current_thread();
#endif//DBG
    
};

//--------------------------------------------------------------------

void
DldIOShadowFile::UnLockExclusive()
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