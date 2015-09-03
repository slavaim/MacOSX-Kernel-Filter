/* 
 * Copyright (c) 2012 Slava Imameev. All rights reserved.
 */


#include "DldDataBuffer.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldDataBuffer, OSObject )

//--------------------------------------------------------------------

DldDataBuffer* DldDataBuffer::withSize( __in vm_size_t size, __in UInt32 index  )
{
    assert( 0x0 != size );
    if( 0x0 == size )
        return NULL;
    
    DldDataBuffer*  newBuffer = new DldDataBuffer();
    assert( newBuffer );
    if( ! newBuffer ){
        
        DBG_PRINT_ERROR(("operator new failed\n"));
        return NULL;
    }
    
    if( ! newBuffer->init() ){
        
        DBG_PRINT_ERROR(("init() failed\n"));
        newBuffer->release();
        return NULL;
    }
    
    newBuffer->data = (vm_address_t)IOMalloc( size );
    assert( newBuffer->data );
    if( ! newBuffer->data ){
        
        DBG_PRINT_ERROR(("IOMalloc() failed\n"));
        newBuffer->release();
        return NULL;
    }
    
    newBuffer->size = size;
    newBuffer->index = index;
    
    return newBuffer;
}

//--------------------------------------------------------------------

bool DldDataBuffer::init()
{
    if( ! super::init() ){
        
        assert( !"super::init() failed" );
        DBG_PRINT_ERROR(( "super::init() failed\n" ));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

void DldDataBuffer::free()
{
    assert( 0x0 == this->ownersCount );
    
    //
    // the order of release does matter!
    // the first is the descriptor
    //
    if( this->memoryDescriptor ){
        
        assert( 0x0 != this->size && this->data );
        this->memoryDescriptor->release();
    }
    
    if( this->data ){
        
        assert( 0x0 != this->size );
        IOFree( (void*)this->data, this->size );
    }
}

//--------------------------------------------------------------------

errno_t  DldDataBuffer::write( __in size_t offset, __in size_t bytesToWrite, __in vm_address_t address, __out size_t* bytesWritten )
{
    
    if( (offset + size) > this->size )
        bytesToWrite = this->size - offset;

    memcpy( (void*)(this->data + offset), (const void*)address, bytesToWrite );
    
    *bytesWritten = bytesToWrite;
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

errno_t  DldDataBuffer::copyDataMbuf( __in size_t offsetInBuffer,
                                      __in size_t bytesToCopy,
                                      __in size_t offsetInMbuf,
                                      __in const mbuf_t mbuf,
                                      __out size_t* bytesCopied )
{
    errno_t error;
    assert( bytesToCopy <= mbuf_pkthdr_len( mbuf ) );
    
    if( (offsetInBuffer + bytesToCopy) > this->size )
        bytesToCopy = this->size - offsetInBuffer;
    
    error = mbuf_copydata( mbuf, offsetInMbuf, bytesToCopy, (void*)this->data );
    if( 0x0 == error )
        *bytesCopied = bytesToCopy;
    
    return error;
}

//--------------------------------------------------------------------

errno_t  DldDataBuffer::read( __in size_t offset, __in size_t bytesToRead, __inout vm_address_t address, __out size_t* bytesRead )
{
    if( (offset + size) > this->size )
        bytesToRead = this->size - offset;  
    
    memcpy( (void*)address, (const void*)(this->data + offset), bytesToRead );
    
    *bytesRead = bytesToRead;
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

//
// returns a referenced descriptor or NULL
//
IOMemoryDescriptor* DldDataBuffer::getMemoryDescriptor()
{
    assert( preemption_enabled() );
    
    if( this->memoryDescriptor ){
        
        this->memoryDescriptor->retain();
        return this->memoryDescriptor;
    }
    
    IOMemoryDescriptor *descriptor;
    
    assert( this->data );
    
    descriptor = IOMemoryDescriptor::withAddress( (void*)this->data, this->size, kIODirectionOutIn );
    assert( descriptor );
    if( ! descriptor ){
        
        DBG_PRINT_ERROR(( "IOMemoryDescriptor::withAddress() failed\n" ));
        return NULL;
    }
    
    if( ! OSCompareAndSwapPtr( NULL, descriptor, &this->memoryDescriptor ) ){
        
        //
        // a concurrent request managed to create a descriptor
        //
        descriptor->release();
        descriptor = NULL;
    }
    
    return getMemoryDescriptor();
}

//--------------------------------------------------------------------

//
// grants the buffer ownership, if false is returned the ownership was not granted
//
bool DldDataBuffer::acquireForIO()
{
    return OSCompareAndSwap( 0x0, 0x1, &this->ownersCount );
}

void DldDataBuffer::releaseFromIO()
{
    assert( 0x1 == this->ownersCount );
    this->ownersCount = 0x0;
}

//--------------------------------------------------------------------
