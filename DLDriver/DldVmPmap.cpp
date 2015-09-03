/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldVmPmap.h"

//--------------------------------------------------------------------

#if !defined(__i386__) && !defined(__x86_64__)
    #error "Unsupported architecture"
#endif

addr64_t
DldVirtToPhys(
   __in vm_offset_t addr
    )
/* 
   Converts a kernel virtual address to a physical which is suitable for
   a PTE which can be used in a page table,
   the function's ancestor is 
   addr64_t kvtophys( vm_offset_t addr )
 */
{
	pmap_paddr_t pa;
    
    //
    // always use the kernel's pmap
    //
    
    //
    // TO DO - pmap_find_phys was moved to the unsupported library in the KPI which
    // is the only available interface for 64 bit kext(!)
    //
    // TO DO - kernel_pmap was moved to the unsupported library in the KPI which
    // is the only available interface for 64 bit kext(!)
    //
    // look at the IOMappedWrite32 (64) functions family implementations from IOLib
    //
	pa = ((pmap_paddr_t)pmap_find_phys(kernel_pmap, addr)) << INTEL_PGSHIFT;
	if (pa)
		pa |= (addr & INTEL_OFFMASK);
    
	return ((addr64_t)pa);
}

//--------------------------------------------------------------------

unsigned int
DldWriteWiredSrcToWiredDst(
    __in vm_offset_t  src,
    __in vm_offset_t  dst,
    __in vm_size_t    len
    )
/*
    the source and the destanation must be WIRED and in the kernel map! The function
    creates a temporary mapping for the src and destanation thus defeating any protection,
    the function's ancestor is
    unsigned kdp_vm_write( caddr_t src, caddr_t dst, unsigned len)
 */
{       
	addr64_t cur_virt_src, cur_virt_dst;
	addr64_t cur_phys_src, cur_phys_dst;
	unsigned long resid, cnt, cnt_src, cnt_dst;
    
	cur_virt_src = (addr64_t)((unsigned long)src);
	cur_virt_dst = (addr64_t)((unsigned long)dst);
    
	resid = len;
    
	while (resid != 0) {
		if ((cur_phys_dst = DldVirtToPhys( cur_virt_dst )) == 0) 
			break;
        
		if ((cur_phys_src = DldVirtToPhys( cur_virt_src )) == 0) 
			break;
        
        //
        // Copy as many bytes as possible without crossing a page
        //
		cnt_src = (unsigned long)(PAGE_SIZE - (cur_phys_src & PAGE_MASK));
		cnt_dst = (unsigned long)(PAGE_SIZE - (cur_phys_dst & PAGE_MASK));
        
		if (cnt_src > cnt_dst)
			cnt = cnt_dst;
		else
			cnt = cnt_src;
		if (cnt > resid) 
			cnt = resid;
        
        //
        // bcopy_phys calls pmap_get_mapwindow with a read allowed PTE for a src
        // and a write allowed PTE for the destanation
        //
		bcopy_phys(cur_phys_src, cur_phys_dst, cnt);		/* Copy stuff over */
        
		cur_virt_src +=cnt;
		cur_virt_dst +=cnt;
		resid -= cnt;
	}
    
	return (len - resid);
}

//--------------------------------------------------------------------
