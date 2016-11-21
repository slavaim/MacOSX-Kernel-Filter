/*
 * I believe this code was partially borrowed from Amit Singh's "Mac OS X Internals: A Systems Approach"
 */

#include "DldLogHandler.h"

//--------------------------------------------------------------------

#define printVnodeIfAction(action, name) \
{ if (action & KAUTH_VNODE_##name) { printf("%s ", #name); } }

//--------------------------------------------------------------------

#define printFileopIfAction(action, name) \
{ if (action == KAUTH_FILEOP_##name) { printf("%s ", #name); } }

//--------------------------------------------------------------------

#define KAUTH_FILEOP_OPEN			1
#define KAUTH_FILEOP_CLOSE			2
#define KAUTH_FILEOP_RENAME			3
#define KAUTH_FILEOP_EXCHANGE			4
#define KAUTH_FILEOP_LINK			5
#define KAUTH_FILEOP_EXEC			6
#define KAUTH_FILEOP_DELETE			7 

void
fileop_action_print(UInt32 action, int isdir)
{
    printf("{ ");
    
    printFileopIfAction(action, OPEN);   // open file
    printFileopIfAction(action, CLOSE);  // close file
    printFileopIfAction(action, RENAME); 
    printFileopIfAction(action, EXCHANGE);
    printFileopIfAction(action, LINK);
    printFileopIfAction(action, EXEC);
    printFileopIfAction(action, DELETE);
    
    printf("}\n");
}

//--------------------------------------------------------------------

void
vnode_action_print(UInt32 action, int isdir)
{
    printf("{ ");
    
    if (isdir)
        goto dir;
    
    printVnodeIfAction(action, READ_DATA);   // read contents of file
    printVnodeIfAction(action, WRITE_DATA);  // write contents of file
    printVnodeIfAction(action, EXECUTE);     // execute contents of file
    printVnodeIfAction(action, APPEND_DATA); // append to contents of file
    goto common;
    
dir:
    printVnodeIfAction(action, LIST_DIRECTORY);   // enumerate directory contents
    printVnodeIfAction(action, ADD_FILE);         // add file to directory
    printVnodeIfAction(action, SEARCH);           // look up specific directory item
    printVnodeIfAction(action, ADD_SUBDIRECTORY); // add subdirectory in directory
    printVnodeIfAction(action, DELETE_CHILD);     // delete an item in directory
    
common:
    printVnodeIfAction(action, DELETE);              // delete a file system object
    printVnodeIfAction(action, READ_ATTRIBUTES);     // read standard attributes
    printVnodeIfAction(action, WRITE_ATTRIBUTES);    // write standard attributes
    printVnodeIfAction(action, READ_EXTATTRIBUTES);  // read extended attributes
    printVnodeIfAction(action, WRITE_EXTATTRIBUTES); // write extended attributes
    printVnodeIfAction(action, READ_SECURITY);       // read ACL
    printVnodeIfAction(action, WRITE_SECURITY);      // write ACL
    printVnodeIfAction(action, TAKE_OWNERSHIP);      // change ownership
    // printVnodeIfAction(action, SYNCHRONIZE);      // unused
    printVnodeIfAction(action, LINKTARGET);          // create a new hard link
    printVnodeIfAction(action, CHECKIMMUTABLE);      // check for immutability
    
    printVnodeIfAction(action, ACCESS);              // special flag
    printVnodeIfAction(action, NOIMMUTABLE);         // special flag
    
    printf("}\n");
}

//--------------------------------------------------------------------

const char *
vtype_name(enum vtype vtype)
{
    static const char *vtype_names[] = {
        "VNON",  "VREG",  "VDIR", "VBLK", "VCHR", "VLNK",
        "VSOCK", "VFIFO", "VBAD", "VSTR", "VCPLX",
    };
    
    return vtype_names[vtype];
}

//--------------------------------------------------------------------

const char *
vtag_name(enum vtagtype vtag)
{
    static const char *vtag_names[] = {
        "VT_NON",   "VT_UFS",    "VT_NFS",    "VT_MFS",    "VT_MSDOSFS",
        "VT_LFS",   "VT_LOFS",   "VT_FDESC",  "VT_PORTAL", "VT_NULL",
        "VT_UMAP",  "VT_KERNFS", "VT_PROCFS", "VT_AFS",    "VT_ISOFS",
        "VT_UNION", "VT_HFS",    "VT_VOLFS",  "VT_DEVFS",  "VT_WEBDAV",
        "VT_UDF",   "VT_AFP",    "VT_CDDA",   "VT_CIFS",   "VT_OTHER",
    };
    
    return vtag_names[vtag];
}

//--------------------------------------------------------------------

IOReturn
vnodeNotificationHandler(io_connect_t connection)
{
    kern_return_t       kr; 
    DldDriverDataLog    vdata;
    uint32_t            dataSize;
    IODataQueueMemory  *queueMappedMemory;
    vm_size_t           queueMappedMemorySize;
#if !__LP64__ || defined(IOCONNECT_MAPMEMORY_10_6)
    vm_address_t        address = nil;
    vm_size_t           size = 0;
#else
    mach_vm_address_t   address = NULL;
    mach_vm_size_t      size = 0x0;
#endif
    mach_port_t         recvPort;
    
    //
    // allocate a Mach port to receive notifications from the IODataQueue
    //
    if (!(recvPort = IODataQueueAllocateNotificationPort())) {
        PRINT_ERROR(("failed to allocate notification port\n"));
        return kIOReturnError;
    }
    
    //
    // this will call registerNotificationPort() inside our user client class
    //
    kr = IOConnectSetNotificationPort(connection, kt_DldNotifyTypeLog, recvPort, 0);
    if (kr != kIOReturnSuccess) {
        
        PRINT_ERROR(("failed to register notification port (%d)\n", kr));
        mach_port_destroy(mach_task_self(), recvPort);
        return kr;
    }
    
    //
    // this will call clientMemoryForType() inside our user client class
    //
    kr = IOConnectMapMemory( connection,
                             kt_DldNotifyTypeLog,
                             mach_task_self(),
                             &address,
                             &size,
                             kIOMapAnywhere );
    if (kr != kIOReturnSuccess) {
        PRINT_ERROR(("failed to map memory (%d)\n",kr));
        mach_port_destroy(mach_task_self(), recvPort);
        return kr;
    }
    
    queueMappedMemory = (IODataQueueMemory *)address;
    queueMappedMemorySize = size;    
    
    printf("before the while loop\n");
    
    //bool first_iteration = true;
    
    while( kIOReturnSuccess == IODataQueueWaitForAvailableData(queueMappedMemory, recvPort) ) {       
        
        //first_iteration = false;
        //printf("a buffer has been received\n");
        
        while (IODataQueueDataAvailable(queueMappedMemory)) {   
            
            dataSize = sizeof(vdata);
            
            kr = IODataQueueDequeue(queueMappedMemory, &vdata, &dataSize);
            
            if (kr == kIOReturnSuccess) {
                
                if (*(UInt32 *)&vdata == kt_DldStopListeningToMessages)
                    goto exit;
                
                printf( "\"%s\" %s %i(%s) ",
                        vdata.Fsd.path,
                        vtype_name(vdata.Fsd.v_type),
                        (int)vdata.Fsd.pid,
                        vdata.Fsd.p_comm );
                
                if( DLD_KAUTH_SCOPE_VNODE_ID == vdata.Fsd.scopeID )
                    vnode_action_print(vdata.Fsd.action, (vdata.Fsd.v_type & VDIR));
                else
                    fileop_action_print(vdata.Fsd.action, (vdata.Fsd.v_type & VDIR));
                    
            } else {
                PRINT_ERROR(("*** error in receiving data (%d)\n", kr));
            }
             
            
        }// end while
        
    }// end while
    
exit:
    
    kr = IOConnectUnmapMemory( connection,
                               kt_DldNotifyTypeLog,
                               mach_task_self(),
                               address );
    if (kr != kIOReturnSuccess){
        PRINT_ERROR(("failed to unmap memory (%d)\n", kr));
    }
    
    mach_port_destroy(mach_task_self(), recvPort);
    
    return kr;
}

//--------------------------------------------------------------------
