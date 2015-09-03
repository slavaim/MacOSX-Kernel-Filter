/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include <sys/types.h>
#include <mach/vm_types.h>

#ifndef _DLDMSTOMACTYPES_H
#define _DLDMSTOMACTYPES_H

//
// convert MS types to UNIX types
//

typedef SInt64      LONGLONG;
typedef UInt64      ULONGLONG;
typedef UInt32      DWORD;
typedef SInt32      LONG;
typedef UInt32      ULONG;
typedef UInt8       UCHAR;
typedef SInt8       CHAR;
typedef UInt16      USHORT;
typedef SInt16      SHORT;
typedef vm_size_t   ULONG_PTR;
typedef void*       PVOID;
typedef ULONG*      PULONG;
typedef CHAR*       PCHAR;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } ;
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;


typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } ;
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

#endif//_DLDMSTOMACTYPES_H