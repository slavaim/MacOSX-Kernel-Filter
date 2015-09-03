/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDVMPMAP_H
#define _DLDVMPMAP_H

#include "DldCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef uint64_t  pmap_paddr_t;
    
    #include <mach/mach_types.h>
    //#include <mach/i386/vm_param.h>
    #include <mach/vm_param.h>
    
    #define	INTEL_PGBYTES		I386_PGBYTES
    #define INTEL_PGSHIFT		I386_PGSHIFT
    #define INTEL_OFFMASK	(I386_PGBYTES - 1)
    #define PG_FRAME        0x000FFFFFFFFFF000ULL
    
    typedef void* pmap_t;
    
    extern
    void
    bcopy_phys(
               addr64_t src64,
               addr64_t dst64,
               vm_size_t bytes);
    
    extern
    ppnum_t
    pmap_find_phys(pmap_t pmap, addr64_t va);
    
    extern pmap_t	kernel_pmap;

#ifdef __cplusplus
}
#endif

addr64_t
DldVirtToPhys(
    __in vm_offset_t addr
   );


unsigned int
DldWriteWiredSrcToWiredDst(
    __in vm_offset_t  src,
    __in vm_offset_t  dst,
    __in vm_size_t    len
    );

#endif _DLDVMPMAP_H