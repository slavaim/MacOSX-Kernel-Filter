/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDMACHO_H
#define _DLDMACHO_H

#include "DldCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include "nlist.h"

typedef struct _DldModInfo{
    vm_map_offset_t    address;
    const char*        modname;//optional
} DldModInfo;
    
#if defined(__LP64__)
    
    typedef struct mach_header_64	kernel_mach_header_t;
    typedef struct segment_command_64 kernel_segment_command_t;
    typedef struct section_64		kernel_section_t;
    
#define LC_SEGMENT_KERNEL		LC_SEGMENT_64
#define SECT_CONSTRUCTOR		"__mod_init_func"
#define SECT_DESTRUCTOR			"__mod_term_func"
    
#else
    
    typedef struct mach_header		kernel_mach_header_t;
    typedef struct segment_command	kernel_segment_command_t;
    typedef struct section			kernel_section_t;
    
#define LC_SEGMENT_KERNEL		LC_SEGMENT
#define SECT_CONSTRUCTOR		"__constructor"
#define SECT_DESTRUCTOR			"__destructor"
    
#endif
    
    extern kernel_mach_header_t _mh_execute_header;
    
    
    /*
     * Safe counted string compare against a literal string. The sizeof() intentionally
     * counts the trailing NUL, and so ensures that all the characters in the literal
     * can participate in the comparison.
     */
    #define LIT_STRNEQL(s1, lit_s2) (0 == strncmp( (s1), (lit_s2), sizeof((lit_s2)) ))
    
    /*
     * Safe counted string compare of a literal against the beginning of a string. Here
     * the sizeof() is reduced by 1 so that the trailing null of the literal does not
     * participate in the comparison.
     */
    #define LIT_STRNSTART(s1, lit_s2) (0 == strncmp( (s1), (lit_s2), sizeof((lit_s2)) - 1 ))
    
    
#ifdef __cplusplus
}
#endif

vm_offset_t
DldRetrieveModuleGlobalSymbolAddress(
    __in DldModInfo* ModInfo,
    __in const char* SymName
    );

#endif	/* _DLDMACHO_H */