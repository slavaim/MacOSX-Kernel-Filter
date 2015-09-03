/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDBACKINGFILE_H
#define _DLDBACKINGFILE_H

#include "DldCommon.h"

//--------------------------------------------------------------------

//
// the type for the size of the range alocated from the file,
// also used as an offset in the allocated range
//
typedef int rng_size;

//--------------------------------------------------------------------

class DldBackingFile: public OSObject {
    
    OSDeclareDefaultStructors( DldBackingFile )
    
public:
    
    //
    // the structure describes a range allocated from the file
    //
    typedef struct _RangeDscr{
        
        //
        // an offset in the file
        //
        off_t     offset;
        
        //
        // a size of the allocated range, usually bigger
        // than the size of written data
        //
        rng_size  fullAllocatedSize;
        
    } RangeDscr;
    
    //
    // a caller must allocate a range in advance by calling fAllocateRange(),
    // the size of written data ( length ) must not exceed the size of
    // the range space after the offsetInRange position
    //
    IOReturn
    fWriteData( __in vm_offset_t  data,
                __in rng_size length,
                __in rng_size offsetInRange,
                __in DldBackingFile::RangeDscr*  rangeDscr );
    
    IOReturn
    fReadData( __inout vm_offset_t  buffer,
               __in rng_size bufferLength,
               __in rng_size offsetInRange,
               __in DldBackingFile::RangeDscr*  rangeDscr );
 
    //
    // a caller must free a returned rabge by calling fFreeRange
    // when it is not longer needed, the caller must allocate
    // memory for the *rangeDscr parameter
    //
    IOReturn
    fAllocateRange( __in rng_size length,
                    __out DldBackingFile::RangeDscr*  rangeDscr );
    
    //
    // a counterpart for fAllocateRange()
    //
    IOReturn
    fFreeRange( __in DldBackingFile::RangeDscr*  rangeDscr );
};

//--------------------------------------------------------------------

#endif _DLDBACKINGFILE_H