/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 *
 * the code is based on DTrace implementation ( fbt_***.c files, fbt_x86.c for example is used for the Mach O 
 * files parcing )
 */
    
#include "DldMachO.h"

//--------------------------------------------------------------------

//
// DldRetrieveModuleGlobalSymbolAddress was extraxted from __fbt_provide_module
//
vm_offset_t
DldRetrieveModuleGlobalSymbolAddress(
    __in DldModInfo* ModInfo,
    __in const char* SymName
    )
/*
 the function disregards leading underscores!
 can't be used for the kernel as its LINKEDIT segment is jettisonned
 */
{
    
#if !defined(__i386__) && !defined(__x86_64__)
    #error "Unsupported architecture"
#endif
    
	kernel_mach_header_t		*mh;
	struct load_command         *cmd;
	kernel_segment_command_t	*orig_ts = NULL, *orig_le = NULL;
	struct symtab_command       *orig_st = NULL;
	struct nlist                *sym = NULL;
	char						*strings;
	//const char				    *modname;
	unsigned int				i;
    size_t                      symNameLen;
    vm_offset_t                 foundAddress = NULL; 
    
    //
    // leading undescores must be skipped
    //
    assert( '_' != SymName[0] );
    
    symNameLen = strlen(SymName);
    
	mh = (kernel_mach_header_t *)(ModInfo->address);
	//modname = ModInfo->modname;
    
    //
    // Has the linker been jettisoned?
    //
	if( NULL == ModInfo->address )
		return NULL;
    
    
#if defined(__i386__)
	if( mh->magic != MH_MAGIC )
		return NULL;
#elif defined(__x86_64__)
    if( mh->magic != MH_MAGIC_64 )
		return NULL;
#endif//defined(__x86_64__)

    
	cmd = (struct load_command *) &mh[1];
	for( i = 0; i < mh->ncmds; i++ ){
		if (cmd->cmd == LC_SEGMENT_KERNEL) {
			kernel_segment_command_t *orig_sg = (kernel_segment_command_t *) cmd;
            
			if (LIT_STRNEQL(orig_sg->segname, SEG_TEXT))
				orig_ts = orig_sg;
			else if (LIT_STRNEQL(orig_sg->segname, SEG_LINKEDIT))
				orig_le = orig_sg;
			else if (LIT_STRNEQL(orig_sg->segname, ""))
				orig_ts = orig_sg; /* kexts have a single unnamed segment */
		}
		else if (cmd->cmd == LC_SYMTAB)
			orig_st = (struct symtab_command *) cmd;
        
		cmd = (struct load_command *) ((caddr_t) cmd + cmd->cmdsize);
	}
    
    
	if ((orig_ts == NULL) || (orig_st == NULL) || (orig_le == NULL))
		return NULL;
    
    
	sym = (struct nlist *)(orig_le->vmaddr + orig_st->symoff - orig_le->fileoff);
	strings = (char *)(orig_le->vmaddr + orig_st->stroff - orig_le->fileoff);
    
    
	for (i = 0; i < orig_st->nsyms; i++) {
        
		uint8_t        n_type = sym[i].n_type & (N_TYPE | N_EXT);
		const char*    name = strings + sym[i].n_un.n_strx;
    
        //
		// Check that the symbol is a global and that it has a name.
        //
		if (((N_SECT | N_EXT) != n_type && (N_ABS | N_EXT) != n_type))
			continue;
        
        //
        // iff a null, "", name. 
        //
		if (0 == sym[i].n_un.n_strx)
			continue;
		
        //
		// Lop off omnipresent leading underscore.
        //
		if (*name == '_')
			name += 1;
        
        if( symNameLen != strlen( name ) || 0x0 != strncmp( SymName, name, symNameLen ) )
            continue;
        
        foundAddress = sym[i].n_value;
        
        break;
        
    }// end for
    
    
    return foundAddress;
}

//--------------------------------------------------------------------

