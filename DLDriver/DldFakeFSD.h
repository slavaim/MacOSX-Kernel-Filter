/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _DLDFAKEFSD_H
#define _DLDFAKEFSD_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
    
#ifdef __cplusplus
}
#endif

#include "DldCommon.h"

//
// a type for the vnode operations
//
typedef int (*VOPFUNC)(void *) ;
typedef int (*VFSFUNC)(void *) ;

//
// the structure defines an offset from the start of a v_op vector for a function
// implementing a corresponding vnode operation
//
typedef struct _DldVnodeOpvOffsetDesc {
	struct vnodeop_desc *opve_op;   /* which operation this is, NULL for the terminating entry */
	vm_offset_t offset;		/* offset in bytes from the start of v_op, (-1) means "unknown" */
} DldVnodeOpvOffsetDesc;

//
// CAVEAT - this value is also an index for an array, so do not use some fancy valuse like 0x80000001
//
typedef enum _DldVfsOperation{
    kDldVfsOpUnknown = 0x0,
    kDldVfsOpUnmount,
    
    //
    // the last operation
    //
    kDldVfsOpMax
    
} DldVfsOperation;

//
// the structure defined an offset for VFS operation in the VFS operations vector
//
typedef struct _DldVfsOperationDesc{
    DldVfsOperation   operation;
    vm_offset_t       offset;		  // offset in bytes from the start of v_op, (-1) means "unknown"
    VFSFUNC           sampleFunction; // address od the sample function
} DldVfsOperationDesc;

#define DLD_VOP_UNKNOWN_OFFSET ((vm_offset_t)(-1))

//
// an offset from the start of vnode to the v_op member, in bytes
// an intial value is DLD_VOP_UNKNOWN_OFFSET
//
// extern vm_size_t    gVNodeVopOffset;
VOPFUNC*
DldGetVnodeOpVector(
    __in vnode_t vnode
    );

extern
IOReturn
DldGetVnodeLayout();

extern
DldVnodeOpvOffsetDesc*
DldRetriveVnodeOpvOffsetDescByVnodeOpDesc(
    __in struct vnodeop_desc *opve_op
    );

extern 
IOReturn
DldUnRegisterFakeFsd();

extern
VFSFUNC*
DldGetVfsOperations(
    __in mount_t   mnt
    );

DldVfsOperationDesc*
DldGetVfsOperationDesc( __in DldVfsOperation   operation);

#endif//_DLDFAKEFSD_H