/*
 **********************************************************************
 ** md5.h -- Header file for implementation of MD5                   **
 ** RSA Data Security, Inc. MD5 Message Digest Algorithm             **
 **********************************************************************
 */
 
#ifndef _VFS_HASH_H
#define _VFS_HASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* typedef a 32 bit type */
//typedef unsigned long int UINT4; this is a 64 bit type on Mac OS X 64 bit, while it is always 32 bit on Windows
typedef uint32_t UINT4;

/* Data structure for MD5 (Message Digest) computation */
typedef struct {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                 /* scratch buffer */
  unsigned char in1[64];         /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
} MD5_CTX;

void DldMD5Init ( MD5_CTX *mdContext );
void DldMD5Update ( MD5_CTX *mdContext, unsigned char *inBuf, unsigned int inLen );
void DldMD5Final ( MD5_CTX *mdContext );

#define MAGIC ((UInt64)0x1234DEADBEEF4321)
    
#ifdef __cplusplus
}
#endif

#endif