/* 
 * Copyright (c) 2012 Slava Imameev. All rights reserved.
 */

#include <IOKit/IOMemoryDescriptor.h>
#include <sys/kpi_mbuf.h>

#include "DldCommon.h"

class DldDataBuffer: public OSObject{
    
    OSDeclareDefaultStructors( DldDataBuffer );
    
private:
    
    //
    // memory descriptor, self-explanatory
    //
    vm_size_t            size;
    vm_address_t         data;
    
    //
    // might be null until getMemoryDescriptor() is called
    //
    IOMemoryDescriptor*  memoryDescriptor;
    
    volatile SInt32      ownersCount;
    
    //
    // each buffer can have an assigned index
    //
    UInt32               index;
    
protected:
    
    virtual bool init();
    virtual void free();
    
public:
    
    static DldDataBuffer* withSize( __in vm_size_t size, __in UInt32 index );
    
    errno_t  write( __in size_t offset,
                    __in size_t size,
                    __in vm_address_t address,
                    __out size_t* bytesWritten );
    
    errno_t  copyDataMbuf( __in size_t offsetInBuffer,
                           __in size_t bytesToCopy,
                           __in size_t offsetInMbuf,
                           __in const mbuf_t mbuf,
                           __out size_t* bytesCopied );
    
    errno_t  read( __in size_t offset,
                   __in size_t size,
                   __inout vm_address_t address,
                   __out size_t* bytesRead );
    
    //
    // returns a referenced descriptor or NULL
    //
    IOMemoryDescriptor* getMemoryDescriptor();
    
    //
    // return the maximum buffer size, not a valid data size!
    //
    size_t getSize(){ return this->size; }
    
    UInt32 getIndex(){ return this->index; }
    
    //
    // grants the buffer ownership, if false is returned the ownership was not granted
    //
    bool acquireForIO();
    void releaseFromIO();
};