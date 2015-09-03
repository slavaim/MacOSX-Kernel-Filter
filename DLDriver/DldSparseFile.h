/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#ifndef _DLDSPARSEFILE_H
#define _DLDSPARSEFILE_H

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSString.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include "DldCommon.h"
#include "DldCommonHashTable.h"

//--------------------------------------------------------------------

//
// the class represents a sparse file
//
class DldSparseFile: public OSObject{
    
    OSDeclareDefaultStructors( DldSparseFile )
    
    friend class DldSparseFilesHashTable;
    
protected:
    
    //
    // all sparse files are placed in the global list,
    // the objects are referenced
    //
    static LIST_ENTRY   sSparseFileListHead;
    LIST_ENTRY          listEntry;
    
    //
    // the entry for the temporay list used by the reaper thread
    //
    LIST_ENTRY          staleListEntry;
    
    //
    // the sSparseFileListHead is protected by the RW lock
    //
    static IORWLock*    sSparseFileListLock;
    
    //
    // if false the entry is not in the hash, be careful as the stalled entry 
    // and the valid one might exists in tha same time, the stalled entry might exists
    // because of reference to be released, if the entry is not in hash then
    // it should not be used as becomes invalid
    //
    bool        inHashTable;
    
    //
    // if greater than 0x0, then must not be prepared for reclaiming even if the corresponding
    // covering vnode is being reclaimed, this behaviour is required for the concurrent
    // vnodes termination and creation, normally this count is 0x1 and becomes bigger
    // for a small time lapse
    //
    SInt32      activeUsersCount;
    
    //
    // true if the sparse file has been accessed in the last interval set by the reaper thread
    //
    bool        accessed;
    UInt32      accessedCachedNodesCount;
    
    //
    // a usage threshold when cache shrinking is started
    //
    static UInt32 sCacheUsageThreshold;
    
    //
    // inserts the sparse file in the sSparseFileListHead list
    //
    void insertInList();
    void removeFromList(); // undo insertInList()
    
    //
    // a thread's continuation, frees unused cached nodes
    //
    static void sCachedNodesSetTrimmer( void* );
    static thread_t  sCachedNodesSetTrimmerThread;
    
    //
    // the synchronization event to wake up the sCachedNodesSetTrimmerThread thread
    //
    static UInt32   sCachedNodeReaperEvent;
    static UInt32   sCachedNodeReaperTerminatedEvent;
    
    //
    // if true the sCachedNodesSetTrimmerThread is being terminated
    //
    static bool sTerminateCachedNodeReaper;
    
public:
    
    //
    // in case of sInitSparseFileSubsystem returns an error
    // the caller must call sFreeSparseFileSubsystem()
    // to free alocated resources
    //
    static IOReturn sInitSparseFileSubsystem();
    static void sFreeSparseFileSubsystem();
    
    //
    // the data are saved in chunks, if there is no data
    // for the start or the end of a chunk then they are zeroed,
    // the chunk size must be a power of 2
    //
    const static unsigned int BlockSize = 4*PAGE_SIZE;
    const static off_t  InvalidOffset = (off_t)(-1);
    
    //
    // the sparse file layout is as follows
    //  Header (occupies the entire first block)
    //  Sparse offsets B-Tree root
    //  Tags B-Tree root
    //  Data
    //
    const static off_t HeaderOffset = 0x0;
    
    
    //
    // used by the CAWL subsystem
    //
    typedef struct _BlockTag{
        
        SInt64  timeStamp;
        
    } BlockTag;
    
    typedef struct _IdentificatioInfo{
        
        //
        // a CAWL covering vnode associated with the
        // sparse file, if exists
        //
        vnode_t             cawlCoveringVnode;
        
        //
        // a CAWL covered vnode unique ID, if exists, this is not the same as DldIOVnodeID,
        // this ID defines uniquely the data stream on the storage and stays the same regardles
        // of whether any vnode exists for the data stream
        //
        unsigned char       cawlCoveredVnodeID[16];
        
        const char*         identificationString; // optional
        
    } IdentificatioInfo;
    
private:
    
    typedef struct _OffsetMapEntry{
        
        //
        // an offset in a sparse file,
        // if the value is (-1) the entry is invalid
        //
        off_t       sparseFileOffset;
        
        //
        // an offset in a dataFile
        //
        off_t       dataFileOffset;
        
        //
        // a tag associated with the chunk,
        // used by the CAWL subsystem
        //
        BlockTag    tag;
        
    } OffsetMapEntry;
    
    
    typedef struct _TagListEntry{
        
        //
        // all blocks are arranged by their tags
        // in the circular list, the following
        // offsets are forward and back links
        // expressed as offsets to blocks,
        // if they are equal to the block offset
        // the tag is related to then the block
        // is not in the list, also the offsets
        // mught be set to InvalidOffset meaning
        // the same - the block is not in the list
        //
        
        off_t   flink;
        off_t   blink;
    } TagListEntry;
    
    typedef struct _FreeChunkHeader{
        
        //
        // free chunks are in a single linked list
        //
        
        //
        // offset to the next forward free block or InvalidOffset if invalid
        //
        off_t   flink;
        
        //
        // number of valid entries
        //
        int     numberOfValidEntries;
        
        //
        // array of entries
        //
        off_t   entries[ 0x1 ];       
        
    } FreeChunkHeader;

    static const int sFreeChunksArrayCapacity = ( DldSparseFile::BlockSize - sizeof( DldSparseFile::FreeChunkHeader) + sizeof(off_t) )/ sizeof( off_t );
    
    typedef struct _BackingFile{
        
        //
        // only the user reference is held, the io reference is not held,
        // so always call vnode_getwithref() and check for error before
        // calling any KPI function which accepts this vnode as a parameter,
        //
        vnode_t         vnode;
        
        //
        // a context for all operations
        //
        vfs_context_t   vfsContext;
        
        //
        // a full file size, must be greater or equal to breakOffset
        //
        off_t           fileSize;
        
        //
        // an offset of the first free chunk descriptor in the disk( this chunk is also cached in the memory - 
        // freeChunksDescriptor ), must be smaller or equal to breakOffset,
        // each free chunk descriptor starts with the FreeChunkHeader, if there is no free
        // chunk then InvalidOffset, in that case the allocation is made from
        // a pool starting at breakOffset
        //
        off_t           firstFreeChunk;
        
        //
        // a pointer to in memory free chunks descriptor, NULL if doesn't exist
        //
        FreeChunkHeader*  freeChunksDescriptor;
        
        //
        // an offset of the completely free area up to the end of the file,
        // the free chunks are not created there, they just allocated by demand
        // from this area
        //
        off_t           breakOffset;
        
    } BackingFile;
    
    //
    // a header for the file, synchronized only on covering vnode reclaim,
    // occupies the first block
    //
    typedef struct _FileHeader{
        
#define DLD_SPARSE_FILE_SIGNATURE 0xABDDEFEF
        SInt64              signature;
        
        IdentificatioInfo   identificationInfo;
        
        //
        // not all fields are valid, it is obvious
        // that vnode is invalid
        //
        BackingFile         dataFile;
        
        //
        // a tree root
        //
        off_t               sparseFileOffsetBTreeRoot;
        
        //
        // an offset for the block with the oldest time stamp, this is an offset in the file
        // not in the data file representing the sparse file, i.e. the offset is a key for
        // the sparseFileOffsetBTree B-Tree
        //
        off_t               oldestWrittenBlock;
        
        //
        // currently contains the path for the file which data the sparse file contains,
        // could be used ony for the debug purposses
        //
        char                identificationString[ 128 ];
        
    } FileHeader;
    
    //
    // set ups a file descriptor, creates the file
    //
    errno_t createDataFileByName( __in const char* name, __in IdentificatioInfo*  info );
    
    //
    // a counterpart for createBackingFileByName
    //
    void freeDataFile();
    
    //
    // allocates a chunk from a file
    //
    off_t  allocateChunkFromDataFile( __in bool acquireLock );
    
    //
    // return back a chunk allocated by allocateChunkFromDataFile()
    //
    errno_t returnChunkToFreeList( __in off_t chunkOffset, __in bool acquireLock  );
    
    //
    // read and write one chunk of data
    //
    errno_t readChunkFromDataFile( __in void* buffer, __in off_t offsetInDataFile );
    errno_t writeChunkToDataFile( __in void* buffer, __in off_t offsetInDataFile );
    
private:
    
    struct{
        
        //
        // if 0x1 the the entire B-Tree and data structure
        // must be preserved in the data file so it can be
        // reopened and used to retrive data
        //
        unsigned int preserveDataFile:0x1;
        
    } flags;
    
    //
    // a file which contains data, the data are written in chunks
    // and the chunks are raw data with a corresponding B-tree key
    // saved in the file in the chunks of the same size as for the data,
    // the user reference is bumped to retain the vnode
    //
    BackingFile         dataFile;
    
    //
    // a divisor that defines the file grow policy, it defines
    // how many add to the file in the terms of the file size,
    // set it to power of 2, if 1 then the file size doubles
    //
    static int          sGrowDivisor;
    
    //
    // contains a map for valid data, one bit for each chunk,
    // might be NULL if the map is entirely in the memory(i.e. in validDataMapCache),
    // the user reference is bumped to retain the vnode
    // TO DO - do we need it?
    // vnode_t              validDataMapFile; // optional
    
    //
    // a cached offset map, TO DO - it seems the node cache will do this
    //
    // OffsetMapEntry  offsetMapCache[ 8 ];
    
    //
    // a cached valid data map, one bit represents one chunk
    // on the corresponding offset in the sparse file,
    // the cache is a window in the full map desribing a region
    // at the validDataMapCacheOffset offset in the sparse file
    // TO DO - do we need it?
    // uint8_t         validDataMapCache[ 32 ];
    // off_t           validDataMapCacheOffset; // set to InvalidOffset if the map cache is invalid
    
    //
    // a RW lock to protect the internal data
    //
    IORWLock*       rwLock;
    
#if defined(DBG)
    thread_t        exclusiveThread;
#endif//DBG
    
    //
    // a mount structutre for the related voume under the CAWL control,
    // must not be referenced, used only as a key to retrieve the DldVfsMntHook object
    //
    mount_t         mnt;
    
protected: // identificationInfo must be accessible for the hash table friend class
    
    //
    // identification info as it was passed to the create routine,
    // so all pointers are invalid if touched outside the create routine
    //
    IdentificatioInfo   identificationInfo;
    
protected:
    
    virtual void free();
    virtual bool init();
    
    virtual void LockShared();
    virtual void UnLockShared();
    virtual void LockExclusive();
    virtual void UnLockExclusive();
    
public:
    
    //
    // returns true if the counter has been incremented and the exisiting object is safe to assign to a covering vnode
    //
    bool incrementUsersCount();
    void decrementUsersCount();
    
private:
    
    //
    // B-Tree node layout in a file, each node occupies a complete chunk,
    // the size of BTreeNode must be smaller that the size of the chunk!
    //
    //  A B-tree T is a rooted tree (whose root is root[T ]) having the following properties:
    //
    //  1.	Every node x has the following fields: a. n[x], the number of keys currently stored in node x,
    //      b. the n[x] keys themselves, stored in nondecreasing order, so that key1[x] ≤ key2[x] ≤ ··· ≤ keyn[x][x],
    //      c. leaf[x], a boolean value that is TRUE if x is a leaf and FALSE if x is an internal node.
    //
    //  2. Each internal node x also contains n[x]+1 pointers c1[x],c2[x],...,cn[x]+1[x] to its children.
    //     Leaf nodes have no children, so their ci fields are undefined.
    //
    //  3. The keys keyi[x] separate the ranges of keys stored in each subtree: if ki is any key stored in
    //     the subtree with root ci [x ], then
    //      k1 ≤ key1[x] ≤ k2 ≤ key2[x] ≤ ··· ≤ keyn[x][x] ≤ kn[x]+1 .
    //
    //  4.	All leaves have the same depth, which is the tree’s height h.
    //
    //  5. There are lower and upper bounds on the number of keys a node can contain. These bounds can be expressed
    //     in terms of a fixed integer t ≥ 2 called the minimum degree of the B-tree:
    //      a. Every node other than the root must have at least t − 1 keys. Every internal node other than the
    //         root thus has at least t children. If the tree is nonempty, the root must have at least one key.
    //      b. Every node can contain at most 2t − 1 keys. Therefore, an internal node can have at most 2t children.
    //         We say that a node is full if it contains exactly 2t − 1 keys.
    //
    // for reference see "Introduction to algorithms" / Thomas H. Cormen . . . [et al.].—2nd ed. pp 434 - 452
    //
    
    typedef struct _BTreeKey{
        
        OffsetMapEntry  mapEntry;
        
    } BTreeKey;
    
    //
    // a B-Tree node header
    //
    typedef struct _BTreeNodeHeader {
        
#if defined(DBG)
#define DLD_BTNODE_SIGNATURE    (0xABCD9977)
        uint32_t        signature;
#endif // DBG
        
        //
        // a list entry, all entries cached in the memory are put
        // in the list anchored in the BTree structure
        //
        LIST_ENTRY      listEntry;
        
        //
        // a reference count, used to manage the lifespan of the cahed entry
        //
        SInt32          referenceCount;
        
        //
        // this node offset in the file
        //
        off_t           nodeOffset;
        
        //
        // Used to indicate whether leaf or not
        //
        bool            leaf;
        
        //
        // true if the node is not synchronized with the on disk copy
        //
        bool            dirty;
        
        //
        // true if the node has been accessed
        //
        bool            accessed;
        
        //
        // Number of active keys
        //
        unsigned int    nrActive;
        
        //
        // Level in the B-Tree
        //
        unsigned int    level;
        
    } BTreeNodeHeader;
    
    
    typedef struct _BTreeKeyEntry{
        
        BTreeKey        keyValue;
        
        //
        // each vaid key entry should be in the tags list which is maintained in ascending order,
        // the list entries are bound to the keyVals, if the keyVals is removed or moved the list
        // entry must be also moved or removed respectively, so this is no-brainer to copy keyVals 
        // operation with tagListEnties
        //
        TagListEntry    tagListEntry;
        
    } BTreeKeyEntry;
    
    
    //
    // the order of the tree, the order t means that there are maximum 2t children and 2t-1 keys
    //
    const static unsigned int BTreeOrder = (BlockSize - sizeof(BTreeNodeHeader)) / (2*(sizeof(BTreeKeyEntry) + sizeof(off_t)));
    
    //
    // a B-Tree node, nodes are stashed in a chunk
    //
    typedef struct _BTreeNode {
        
        //
        // a header, hence an "h"
        //
        BTreeNodeHeader h;
        
        //
        // Array of keys and values, a node with (2t - 1) keys has at most (2t) children!
        //
        BTreeKeyEntry   keyEntries[ (2 * DldSparseFile::BTreeOrder) - 0x1 ];

        //
        // Array of offsets for child nodes, if the offset doesn't point to any child
        // the value must be set to InvalidOffset, this is important as comparing the
        // last child offset value ( i.e. at children[h.nrActive]) with the invalid
        // value is the only way to infer whether the offset points to a valid child
        //
        off_t           children[ 2 * DldSparseFile::BTreeOrder ];
        
    } BTreeNode;
    
    typedef enum _BTreeKeyCmp{
        kKeysEquals = 0x0,
        kKeyOneBigger = 0x1,
        kKeyOneSmaller = 0x2
    } BTreeKeyCmp;
    
    //
    //  key1 == key2 - kKeysEquals
    //  key1 > key2  - kKeyOneBigger
    //  key2 < key2  - kKeyOneSmaller
    //
    typedef BTreeKeyCmp (*CompareKeysFtr)( __in BTreeKey* key1, __in BTreeKey* key2);
    
    //
    // a B-Tree itself
    //
    typedef struct _BTree {
        
        //
        // if true there were errors in the tree processing and it can't be trusted anymore
        //
        bool            damaged;
        
        //
        // a function for keys comparision
        //
        CompareKeysFtr  compareKeys;
        
        //
        // current root node of the B-Tree, the node is referenced(!)
        // and retained even if empty
        //
        BTreeNode*      root;
        
    } BTree;
    
    typedef enum {left = -1,right = 1} BtPosition;
    
    typedef struct _BtNodePos{
        
        //
        // this node pointer is optional and may be NULL
        //
        BTreeNode*      node;
        
        //
        // node offset, must be valid if node is NULL, else optional
        //
        off_t           nodeOffset;
        
        //
        // the key index, must be valid
        //
        unsigned int    index;
        
    } BtNodePos;
    
    //
    // if rootNodeOffset is InvalidOffset a new root is allocated,
    // else a root is read from the file
    //
    BTree* createBTree( __in CompareKeysFtr  comparator, __in off_t rootNodeOffset, __in bool acquireLock );
    void deleteBTree( __in BTree* btree, __in bool acquireLock );
    
    //
    // tries to find and reference an entry in the cache
    //
    BTreeNode* referenceCachedBTreeNodeByOffset( __in off_t offset, __in bool acquireLock );
    
    //
    // flushes the cached nodes and purges them from the cache
    //
    void flushAndPurgeCachedNodes( __in bool prepareForReclaim );
    
    //
    // if the offset is set to DldSparseFile::InvalidOffset a new node is allocated with the reserved space,
    // the entry must be dereferenced when no longer needed
    //
    BTreeNode* getBTreeNodeReference( __in off_t offset, __in bool acquireLock );
    
    //
    // reference the node
    //
    void referenceBTreeNode( __in BTreeNode* node );
    
    //
    // if the reference count drops to zero the node is removed
    // from the cache list and from the memory but not from
    // a backing store
    //
    void dereferenceBTreeNode( __in BTreeNode* node, __in bool acquireLock );
    
    //
    // frees the allocated memory, doesn't remove from the backing store
    //
    void freeBTreeNodeMemory( __in BTreeNode* node );
    
    //
    // removed the node from the backing store
    //
    errno_t removeBTreeNodeFromBackingStore( __in BTreeNode* node, __in bool acquireLock );
    
    //
    // the function splits a full node y (having 2t − 1 keys) around its median key keyt [y]
    // into two nodes having t − 1 keys each. The median key moves up into y’s parent to identify
    // the dividing point between the two new trees,
    // @param index Index of the child node
    //
    errno_t BTreeSplitChild( __in    BTree*         btree,
                             __inout BTreeNode*     parent, 
                             __in    unsigned int   index,
                             __inout BTreeNode*     child,
                             __in    bool           acquireLock );
    
    //
    // the function inserts key k into node x, which is assumed to be nonfull when the procedure is called.
    //
    errno_t BTreeInsertNonfull( __in    BTree*      btree,
                                __inout BTreeNode*  parentNode,
                                __in    BTreeKey*   keyVal,
                                __in    bool        acquireLock );
    
    //
    //  The function is used to insert node into a B-Tree
    //
    errno_t BTreeInsertKey( __inout BTree*      btree,
                            __in    BTreeKey*   key_val,
                            __in    bool        acquireLock );
    
    //
    // The function is used to get the position of the MAX or MIN key within the subtree
    // @param btree The btree
    // @param subtree The subtree to be searched
    // @return The node containing the key and position of the key,
    //  if there was an error then nodePos.nodeOffset == DldSparseFile::InvalidOffset
    //
    BtNodePos BTreeGetMaxOrMinKeyPos( __in BTree* btree,
                                      __in BTreeNode* subtree,
                                      __in bool getMax, // if true returns MAX, else returns MIN
                                      __in bool acquireLock );
    
    BtNodePos BTreeGetMaxKeyPos( __in BTree* btree,
                                 __in BTreeNode* subtree,
                                 __in bool acquireLock );
    
    BtNodePos BTreeGetMinKeyPos( __in BTree* btree,
                                 __in BTreeNode* subtree,
                                 __in bool acquireLock );
    
    
    BTreeNode* BTreeMergeSiblings( __inout BTree* btree,
                                   __in    BTreeNode* parent,
                                   __in    unsigned int index, 
                                   __in    BtPosition pos,
                                   __in    bool acquireLock );
    
    errno_t BTreeMoveKey( __inout BTree*       btree,
                          __in    BTreeNode*   node,
                          __in    unsigned int index,
                          __in    BtPosition   pos,
                          __in    bool         acquireLock );
    
    //
    // Merge nodes n1 and n2
    // @param n1 First node
    // @param n2 Second node
    // @return combined node
    //
    BTreeNode* BTreeMergeNodes( __inout BTree*          btree,
                                __in    BTreeNode*      n1,
                                __in    BTreeKeyEntry*  kv,
                                __in    BTreeNode*      n2,
                                __in    bool            acquireLock );
    
    //
    // delete a key from the B-tree node
    // @param btree The btree
    // @param node The node from which the key is to be deleted 	
    // @param key The key to be deleted
    // @return 0 on success -1 on error 
    //
    errno_t BTreeDeleteKeyFromNode( __inout BTree*      btree,
                                    __in    BtNodePos*  nodePos,
                                    __in    bool        acquireLock );
    
    //
    // Function is used to delete a node from a  B-Tree
    // @param btree The B-Tree
    // @param key Key of the node to be deleted
    // @param value function to map the key to an unique integer value        
    // @param compare Function used to compare the two nodes of the tree
    // @return success or failure
    //
    errno_t BTreeDeleteKey( __inout BTree*      btree,
                            __inout BTreeNode*  subtree,
                            __in    BTreeKey*   key,
                            __in    bool        acquireLock );
    
    //
    // Function used to get the node containing the given key
    // @param btree The btree to be searched
    // @param key The the key to be searched
    // @return The node and position of the key within the node
    // the non NULL node is returned and must be freed by a caller!
    //
    BtNodePos BTreeGetNodeByKey( __in BTree*    btree,
                                 __in BTreeKey* key,
                                 __in bool      acquireLock );
    
    //
    // returns the index of the element that are equal or bigger or nrActive if there is no bigger vaue,
    // a caller must hold the lock
    //
    int BTreeGetKeyIndex( __in BTree*     btree,
                          __in BTreeNode* node,
                          __in BTreeKey*  key );
    
    //
    // Function used to get the node either
    //  - containing the given key
    //  - containing the next bigger key if the equal key value doesn't exist
    //
    // @param btree The btree to be searched
    // @param key The the key to be searched
    // @return The node and position of the key within the node
    // the non NULL node is returned and must be freed by a caller
    // the caller must be cautios in changing the node as the node
    // might be a root one so any changes must be reflected in the
    // BTree root
    //
    BtNodePos BTreeGetEqualOrBiggerNodeByKey( __in BTree*    btree,
                                              __in BTreeKey* key,
                                              __in bool      acquireLock );
    
    errno_t BTreeUpdateTagsListPosition( __inout BTree*      btree,
                                         __inout BTreeNode*  node,
                                         __in    int         index,
                                         __in    bool        acquireLock );
    
    errno_t BTreeDeleteFromTagsListPosition( __inout BTree*      btree,
                                             __inout BTreeNode*  node,
                                             __in    int         index,
                                             __in    bool        acquireLock );
    
    //
    // synchronize the node with its copy in the backing store
    //
    errno_t BTreeFlushNode( __in BTree* btree, __in BTreeNode* node );
    
    static BTreeKeyCmp compareOffsets( __in BTreeKey* key1, __in BTreeKey* key2);
    static BTreeKeyCmp compareTags( __in BTreeKey* key1, __in BTreeKey* key2);
    
    //
    // a B-Tree for sparse file offsets,
    // might be NULL if the file is small
    // enought to be processed using only the cache
    //
    BTree*  sparseFileOffsetBTree; // optional
    
    //
    // an initial root node offset, do not used after the initialization was completed,
    // if set to InvalidValue a new root is created instead reading the existing one from the file
    //
    off_t   sparseFileOffsetBTreeRootInit;
    
    //
    // an offset for the block with the oldest time stamp, this is an offset in the file
    // not in the data file representing the sparse file, i.e. the offset is a key for
    // the sparseFileOffsetBTree B-Tree
    //
    off_t   oldestWrittenBlock;
    
    //
    // a list of cached BTreeNode entries, the list is not sorted
    //
    LIST_ENTRY      cachedBTreeNodesList;
    
    //
    // a number of node entries in the cached list
    //
    SInt32          numberOfCachedNodes;
    
    
    
public:
    
    static DldSparseFile*  withPath( __in const char* sparseFilePath,
                                     __in_opt vnode_t cawlCoveringVnode,
                                     __in_opt unsigned char* cawlCoveredVnodeID, // char bytes[16]
                                     __in_opt const char* identificationString );
    
    //
    // a descriptor for a single block IO operation,
    // the buffer should not cross the block, the IO
    // can be performed only for one block or its part
    //
    typedef struct _IoBlockDesriptor{
        
        //
        // offset in the sparse file, allowed to be unaligned
        // but unaligned write can fail if there is no full block
        // found, in that case the blockIsNotPresent bit is set,
        // if the offset is set to InvalidOffset nothing
        // will be done
        //
        off_t           fileOffset;
        
        //
        // a tag for write, ignored for read
        //
        BlockTag        tag;
        
        //
        // ponter to data
        //
        void*           buffer;
        
        //
        // data size, must be smaller or equal to BlockSize
        //
        unsigned int    size;
        
        //
        // IO operation result, if failed check for blockIsNotPresent
        // flag that signals that the operation must be repeated with
        // an aligned bock for write and that the data is not present
        // for read, in the latter case a caller should either zero the
        // buffer or read the data from another source
        //
        errno_t         error;
        
        struct{
            
            //
            // 0x0 for read
            // 0x1 for write
            //
            unsigned int write:0x1;
            
            //
            // output value, if 0x1 then not aligned block write
            // was not processed as a block was not found in the
            // sparse file, for read means the same - the block
            // is not present in the file, the buffer is not zeroed
            //
            unsigned int blockIsNotPresent:0x1;
            
        } flags;
        
    } IoBlockDescriptor;
    
    void performIO( __inout IoBlockDescriptor*  descriptors, __in unsigned int dscrCount );
    
    //
    // flushes the data from this sparse file to coveredVnode up to fileDataSize,
    // the flushed data has the time stamp less or equal to timeStamp, a caller
    // is responsible for protecting from file trnuncation ( the growth is not a
    // problem as the new data will have a greater time stamp)
    //
    errno_t flushUpToTimeStamp( __in SInt64 timeStamp,
                                __in vnode_t coveredVnode,
                                __in off_t fileDataSize,
                                __in vfs_context_t vfsContext );
    
    //
    // purges data fromn the sparse file starting from the purgeOffset
    //
    errno_t purgeFromOffset( __in off_t   purgeOffset );
    
    //
    // returns a pointer to 16 bytes ID
    //
    const unsigned char* sparseFileID() { return this->identificationInfo.cawlCoveredVnodeID; };
    
    //
    // sets a new CAWL related vnode, returns the old one
    //
    vnode_t exchangeCawlRelatedVnode( __in vnode_t );
    
    //
    // prepares the sparse file object for reclaiming by releasing all external references
    //
    void prepareForReclaim();
    
    //
    // returns a referenced vnode related to the sparse file, used by the CAWL,
    // a caller must call vnode_put() to release the returned vnode
    //
    static vnode_t getCoveringVnodeRefBySparseFileID( __in const unsigned char* sparseFileID/*char[16]*/ );
    
    //
    // aligns offset to lower block boundary
    //
    static off_t alignToBlock( __in off_t fileOffset ) { return (fileOffset & ~((off_t)DldSparseFile::BlockSize - 1)); }
    static off_t roundToBlock( __in off_t fileOffset ) { return (( fileOffset + (off_t)DldSparseFile::BlockSize - 1 ) & ~((off_t)DldSparseFile::BlockSize - 1)); }
    
    //
    // should be used mainly for fast checks or tests as doesn't acquire any lock
    //
    off_t   getOldestWrittenBlockOffset() { return this->oldestWrittenBlock; }
    
    static void test();
    
};

//--------------------------------------------------------------------

class DldSparseFilesHashTable
{
    
private:
    
    ght_hash_table_t*  HashTable;
    IORWLock*          RWLock;
    
#if defined(DBG)
    thread_t           ExclusiveThread;
#endif//DBG
    
    //
    // returns an allocated hash table object
    //
    static DldSparseFilesHashTable* withSize( int size, bool non_block );
    
    //
    // free must be called before the hash table object is deleted
    //
    void free();
    
    //
    // as usual for IOKit the desctructor and constructor do nothing
    // as it is impossible to return an error from the constructor
    // in the kernel mode
    //
    DldSparseFilesHashTable()
    {
        
        this->HashTable = NULL;
        this->RWLock = NULL;
#if defined(DBG)
        this->ExclusiveThread = NULL;
#endif//DBG
        
    }
    
    //
    // the destructor checks that the free() has been called
    //
    ~DldSparseFilesHashTable()
    {
        
        assert( !this->HashTable && !this->RWLock );
    };
    
public:
    
    static bool CreateStaticTableWithSize( int size, bool non_block );
    static void DeleteStaticTable();
    
    //
    // adds an entry to the hash table, the entry is referenced so the caller must
    // dereference the entry if it has been referenced
    //
    bool   AddEntry( __in unsigned char* coveredVnodeID/*char[16]*/, __in DldSparseFile* entry );
    
    //
    // removes the entry from the hash and returns the removed entry, NULL if there
    // is no entry for an object, the returned entry is referenced
    //
    DldSparseFile*   RemoveEntry( __in const unsigned char* coveredVnodeID/*char[16]*/ );
    
    //
    // remove entry by object, i.e. removes the object from the hash
    // a caller must not acquire the lock
    //
    void RemoveEntryByObject( __in DldSparseFile* sparseFile );
    
    //
    // returns an entry from the hash table, the returned entry is referenced
    // if the refrence's value is "true"
    //
    DldSparseFile*   RetrieveEntry( __in const unsigned char* coveredVnodeID/*char[16]*/, __in bool reference = true );
    
    void
    LockShared()
    {   assert( this->RWLock );
        assert( preemption_enabled() );
        
        IORWLockRead( this->RWLock );
    };
    
    
    void
    UnLockShared()
    {   assert( this->RWLock );
        assert( preemption_enabled() );
        
        IORWLockUnlock( this->RWLock );
    };
    
    
    void
    LockExclusive()
    {
        assert( this->RWLock );
        assert( preemption_enabled() );
        
#if defined(DBG)
        assert( current_thread() != this->ExclusiveThread );
#endif//DBG
        
        IORWLockWrite( this->RWLock );
        
#if defined(DBG)
        assert( NULL == this->ExclusiveThread );
        this->ExclusiveThread = current_thread();
#endif//DBG
        
    };
    
    
    void
    UnLockExclusive()
    {
        assert( this->RWLock );
        assert( preemption_enabled() );
        
#if defined(DBG)
        assert( current_thread() == this->ExclusiveThread );
        this->ExclusiveThread = NULL;
#endif//DBG
        
        IORWLockUnlock( this->RWLock );
    };
    
    static DldSparseFilesHashTable* sSparseFilesHashTable;
};

//--------------------------------------------------------------------

#endif // _DLDSPARSEFILE_H