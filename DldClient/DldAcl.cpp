/*
 * 2009 Slava Imameev. 
 * testSystemAcl was borrowed from a source which location I don't remember.
 */

#include "DldAcl.h"

/*
 
 http://lists.apple.com/archives/Darwin-dev/2009/Apr/msg00081.html
 
 In Linux, we were using these APIs for processing the data between user managed space and kernel space. Similar requirement has come up for Mac OS X.
 
 So basically you are using it as a communications channel between user space and the kernel. Bad idea.
 
 The internal representation in the kernel is as a filesec_t, and there are APIs for dealing with that *in the kernel*. In user space, you are expected to use the user space interfaces for dealing with getting and setting ACLs. There is no opportunity for using this interface as a covert channel to pass data in and out of, for example, a third party FS. If that's what you want to do, you will need to look elsewhere (such as using fcntl(0 to access through VNOP_IOCTL into the filesystem).
 
 You are basically not allowed to look into the opaque type information in user space (for one thing, the filesec_t is an opaque pointer type; for another, we do not guarantee byte order, endienness, or structure packing between platforms or between kernel and user space; you basically get whatever the native volume format is, and for that, you get whatever the native endienness and packing was on the system it was created on.
 
 
 So User Application cannot use this APIs since it is not available in the man pages right?
 
 Incorrect. Whether or not there is a man page for something, an exported API is an exported API. However, not all exported API is useful, and some exported API is not documented, either to deprecate it or to strongly discourage its use. Anything you did with the internally formatted information would basically not be portable between releases or software updates, since you are supposed to treat the contents as opaque.
 
 If you google the FreeBSD implementation of POSIX 1003.1e, which is what the OpenBSD, MacOS X, SGI IRIX, Tru64 UNIX (from your own company), Linux, and other implementations are based upon, you will see that out of the > 100 first results you get back, absolutely none of them are code examples trying to use them from user space.
 
 There is a reason for that. It's because of the opaque parts not having defined accessor/mutator functions usable from user space. Those are intentionally not defined by most vendors (including Apple) because there is a problem with integer UID vs. ACL UUID coherency, if you let user space jam any UUID they want into things.
 
 Specifically, no one has solved the cardinality issue where multiple UUIDs in multiple directory services end up mapping to the same 32 bit inter value of a uid_t or gid_t in a normal UNIX credential. This happens because the people who came up with UUIDs in the first place ignored a lot of inconvenient problems, such as existing file systems and backup archives. Effectively, that means that you can't take an integer value and map it back to a single UUID when you are trying to put an ACL on something, you basically have to guess, or you have to set a priority ordering on your directory service.
 
 
 If you use the read/modify/write paradigm, for example, and explicitly check for the attribute information after the read before the modify/ write, the interfaces could possibly be useful for backup/restore. But if you were trying to create one from scratch and then use acl_copy_int() to intern it for the kernel to see, your code is going to be broken. It will also be broken if you store and retrieve the internal data format, and also if you communicate the internal data format between computers in an effort to replicate contents. The read step is necessary to ensure that the opaque parts of the data for which there is no useful user space portion (or API for modification from user space) is filled out correctly by the kernel.
 
 
 Which are the appropriate APIs that can be used for exchanging the ACL data between kernel and user space.
 
 chmodx_np, fchmodx_np, openx_np, accessx_np, getattrlist, setattrlist, and so on are the appropriate APIs for use in user space ACL manipulation.
 
 In kernel space, your file system is supposed to implement the VNOPs that correspond to ACL operations. See our published source code for various file systems for examples.
 
 -- Terry
 */
//--------------------------------------------------------------------

kern_return_t
DldSetAcl(
    __in io_connect_t    connection,
    __in DldDeviceType   deviceType,
    __in DldAclType      aclType,
    __in acl_t           acl
    )
{
    kern_return_t            kr;
    DldAclDescriptorHeader*  aclDscrHeader;
    size_t                   aclDscSize;
    size_t                   aclSize;
    size_t                   notUsed;
    void*                    kauthAcl;
    
    aclSize = acl_size( acl );
    aclDscSize = sizeof( *aclDscrHeader ) + aclSize;
    
    aclDscrHeader = (DldAclDescriptorHeader*)malloc( aclDscSize );
    if( !aclDscrHeader ){
        
        PRINT_ERROR(("aclDscrHeader = malloc( aclDscSize ) failed\n"));
        return kIOReturnNoMemory;
    }
    
    memset( aclDscrHeader, 0x0, aclDscSize );
    
    aclDscrHeader->size = aclDscSize;
    aclDscrHeader->deviceType = deviceType;
    aclDscrHeader->aclType = aclType;
    
    kauthAcl = (void*)( aclDscrHeader + 1 );
    
    //
    // convert the user space acl to the kernel kauth_acl
    //
    if( (-1) == acl_copy_ext_native( kauthAcl, acl, aclSize ) ){
        
        PRINT_ERROR(("acl_copy_ext_native() failed\n"));
        kr = kIOReturnError;
        goto __exit;
    }
    
    notUsed = aclDscSize;
    
    kr = IOConnectCallStructMethod( connection,
                                    kt_DldUserClientSetACL,
                                    (const void*)aclDscrHeader,
                                    aclDscSize,
                                    aclDscrHeader,
                                    &notUsed );
    if( kIOReturnSuccess != kr ){
        
        PRINT_ERROR(("IOConnectCallStructMethod failed with kr = 0x%X\n", kr));
        goto __exit;
    }
    
__exit:
    
    if( kIOReturnSuccess != kr ){
        
        PRINT_ERROR(("DldSetAcl(%d, %d) failed\n", (int)deviceType.combined, (int)aclType));
    }
    
    free( aclDscrHeader );
    return kr;
}

//--------------------------------------------------------------------

/* parse each acl line
 * format: <user|group>:
 *	    [<uuid>]:
 *	    [<user|group>]:
 *	    [<uid|gid>]:
 *	    <allow|deny>[,<flags>]
 *	    [:<permissions>[,<permissions>]]
 *
 * only one of the user/group identifies is required
 * the first one found is used
 *
 */

/*
static struct {
    acl_perm_t      perm;
    char            *name;
    int             type;
} acl_perms[] = {
    {ACL_READ_DATA,         "read",         ACL_TYPE_FILE},
    //      {ACL_LIST_DIRECTORY,    "list",         ACL_TYPE_DIR},
    {ACL_WRITE_DATA,        "write",        ACL_TYPE_FILE},
    //      {ACL_ADD_FILE,          "add_file",     ACL_TYPE_DIR},
    {ACL_EXECUTE,           "execute",      ACL_TYPE_FILE},
    //      {ACL_SEARCH,            "search",       ACL_TYPE_DIR},
    {ACL_DELETE,            "delete",       ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_APPEND_DATA,       "append",       ACL_TYPE_FILE},
    //      {ACL_ADD_SUBDIRECTORY,  "add_subdirectory", ACL_TYPE_DIR},
    {ACL_DELETE_CHILD,      "delete_child", ACL_TYPE_DIR},
    {ACL_READ_ATTRIBUTES,   "readattr",     ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_WRITE_ATTRIBUTES,  "writeattr",    ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_READ_EXTATTRIBUTES, "readextattr", ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_WRITE_EXTATTRIBUTES, "writeextattr", ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_READ_SECURITY,     "readsecurity", ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_WRITE_SECURITY,    "writesecurity", ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_CHANGE_OWNER,      "chown",        ACL_TYPE_FILE | ACL_TYPE_DIR},
    {0, NULL, 0}
};

static struct {
    acl_flag_t      flag;
    char            *name;
    int             type;
} acl_flags[] = {
    {ACL_FLAG_DEFER_INHERIT,        "defer_inherit",        ACL_TYPE_ACL},
    {ACL_ENTRY_FILE_INHERIT,        "file_inherit",         ACL_TYPE_DIR},
    {ACL_ENTRY_DIRECTORY_INHERIT,   "directory_inherit",    ACL_TYPE_DIR},
    {ACL_ENTRY_LIMIT_INHERIT,       "limit_inherit",        ACL_TYPE_FILE | ACL_TYPE_DIR},
    {ACL_ENTRY_ONLY_INHERIT,        "only_inherit",         ACL_TYPE_DIR},
    {0, NULL, 0}
};
*/

//--------------------------------------------------------------------

kern_return_t
DldSetAclFromText(
          __in io_connect_t    connection,
          __in DldDeviceType   deviceType,
          __in DldAclType      aclType,
          __in char*           acl_text
          )
{
    kern_return_t    kr;
    acl_t acl;
    
    acl = acl_from_text( acl_text );
    if( NULL == acl ){
        
        PRINT_ERROR(("acl_from_text(%s) failed\n", acl_text));
        return kIOReturnBadArgument;
    }
    
    printAclOnConsole( acl );
    
    kr = DldSetAcl( connection, deviceType, aclType, acl );
    if( kIOReturnSuccess != kr ){
        
        PRINT_ERROR(("DldSetAcl(%d, %d, %s) failed\n", (int)deviceType.combined, (int)aclType, acl_text));
    }
    
    acl_free( acl );
    return kr;
}

//--------------------------------------------------------------------

//
// an internal represenattion for acl_entry, only for the test purpose
//
struct dld_acl_entry {
	u_int32_t	ae_magic;
#define _ACL_ENTRY_MAGIC	0xac1ac101
	u_int32_t	ae_tag;
	guid_t		ae_applicable;
	u_int32_t	ae_flags;
	u_int32_t	ae_perms;
};

/*
 * Internal representation of an ACL.
 * XXX static allocation is wasteful. 
 */
struct dld_acl {
	u_int32_t	a_magic;
#define _ACL_ACL_MAGIC		0xac1ac102
	unsigned	a_entries;
	int		a_last_get;
	u_int32_t	a_flags;
	struct dld_acl_entry a_ace[ACL_MAX_ENTRIES];
};

//--------------------------------------------------------------------

void
printAclOnConsole( __in acl_t   acl )
{
    char* acl_str;
    ssize_t  len;
    
    acl_str = acl_to_text( acl, &len );
    
    if( !acl_str )
        return;
    
    char* buf = acl_str;
    char* entry;
    
    while ((entry = strsep(&buf, "\n")) && *entry){
        
        printf( "  %s\n", entry );
    }
    
    acl_free( acl_str );
}

//--------------------------------------------------------------------

void
testSystemAcl()
{
    int           ret, acl_count = 4;
    acl_t         acl;
    acl_entry_t   acl_entry;
    acl_permset_t acl_permset;
    acl_perm_t    acl_perm;
    uuid_t        uu;
    char*         acl_rever;
    ssize_t       len;
    
    #define EXIT_ON_ERROR(msg, retval) if (retval) { printf(msg); return; }
    
    // translate Unix user ID to UUID
    ret = mbr_uid_to_uuid(getuid(), uu);
    EXIT_ON_ERROR("mbr_uid_to_uuid", ret);
    
    // allocate and initialize working storage for an ACL with acl_count entries
    if ((acl = acl_init(acl_count)) == (acl_t)NULL) {
        return;
    }
    
    // create a new ACL entry in the given ACL
    ret = acl_create_entry(&acl, &acl_entry);
    EXIT_ON_ERROR("acl_create_entry", ret);
    
    // retrieve descriptor to the permission set in the given ACL entry
    ret = acl_get_permset(acl_entry, &acl_permset);
    EXIT_ON_ERROR("acl_get_permset", ret);
    
    // a permission
    acl_perm = ACL_DELETE;
    
    // add the permission to the given permission set
    ret = acl_add_perm(acl_permset, acl_perm);
    EXIT_ON_ERROR("acl_add_perm", ret);
    
    // set the permissions of the given ACL entry to those contained in this set
    ret = acl_set_permset(acl_entry, acl_permset);
    EXIT_ON_ERROR("acl_set_permset", ret);
    
    // set the tag type (we want to deny delete permissions)
    ret = acl_set_tag_type(acl_entry,  ACL_EXTENDED_DENY);
    EXIT_ON_ERROR("acl_set_tag_type", ret);
    
    // set qualifier (in the case of ACL_EXTENDED_DENY, this should be a uuid_t)
    ret = acl_set_qualifier(acl_entry, (const void *)uu);
    EXIT_ON_ERROR("acl_set_qualifier", ret);
    
    acl_rever = acl_to_text( acl, &len );
    if( acl_rever )
        printf( "%s\n",acl_rever );
    
    // free ACL working space
    ret = acl_free((void *)acl);
    EXIT_ON_ERROR("acl_free", ret);
    
    #undef EXIT_ON_ERROR
    
    return;
}

void
testSystemAcl2()
{
    acl_t acl;
    
    const char* acl_str = "!#acl 1\n"
                          "user::Apple::deny:write\n"
                          "group::Administrators::deny:write\n"
                          "user::test1::deny:write";
    
    acl = acl_from_text( acl_str );
    if( NULL == acl ){
        
        PRINT_ERROR(("acl_from_text(%s) failed\n", acl_str));
        return;
    }
    
    //
    // set a breakpoint here to investigate the acl
    //
    //struct dld_acl* dld_acl = (struct dld_acl*)acl;
    
    char* acl_rever;
    ssize_t  len;
    acl_rever = acl_to_text( acl, &len );
    
    testSystemAcl();
    
    acl_free( acl );
    
    return;
    
}

//--------------------------------------------------------------------
