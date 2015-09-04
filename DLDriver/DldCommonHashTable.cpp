/*
 based on Simon Kagstrom's C hash table implementation
 Copyright (C) 2001-2005,  Simon Kagstrom
 
 Changes history:
 2010,  Slava Imameev - porting to Mac OS X kernel
 */

#include "DldCommonHashTable.h"

//--------------------------------------------------------------------

__BEGIN_DECLS
/*
 * Kernel Memory allocator
 */
void *	mac_kalloc	(vm_size_t size, int how);
void	mac_kfree	(void *data, vm_size_t size);
__END_DECLS

//--------------------------------------------------------------------

/* Flags for the elements. This is currently unused. */
#define FLAGS_NONE     0 /* No flags */
#define FLAGS_NORMAL   0 /* Normal item. All user-inserted stuff is normal */
#define FLAGS_INTERNAL 1 /* The item is internal to the hash table */

/* Prototypes */
static inline void              transpose(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry);
static inline void              move_to_front(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry);
static inline void              free_entry_chain(ght_hash_table_t *p_ht, ght_hash_entry_t *p_entry);

#if !defined( DBG )
static inline
#endif//!DBG
ght_hash_entry_t *search_in_bucket(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_key_t *p_key, unsigned char i_heuristics);

static inline void              hk_fill(ght_hash_key_t *p_hk, int i_size, const void *p_key);
static inline ght_hash_entry_t *he_create(ght_hash_table_t *p_ht, void *p_data, unsigned int i_key_size, const void *p_key_data);
static inline void              he_finalize(ght_hash_table_t *p_ht, ght_hash_entry_t *p_he);

//--------------------------------------------------------------------

/* --- private methods --- */

/* Move p_entry one up in its list. */
static inline void transpose(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry)
{
    /*
     *  __    __    __    __
     * |A_|->|X_|->|Y_|->|B_|
     *             /
     * =>        p_entry
     *  __    __/   __    __
     * |A_|->|Y_|->|X_|->|B_|
     */
    if (p_entry->p_prev) /* Otherwise p_entry is already first. */
    {
        ght_hash_entry_t *p_x = p_entry->p_prev;
        ght_hash_entry_t *p_a = p_x?p_x->p_prev:NULL;
        ght_hash_entry_t *p_b = p_entry->p_next;
        
        if (p_a)
        {
            p_a->p_next = p_entry;
        }
        else /* This element is now placed first */
        {
            p_ht->pp_entries[l_bucket] = p_entry;
        }
        
        if (p_b)
        {
            p_b->p_prev = p_x;
        }
        if (p_x)
        {
            p_x->p_next = p_entry->p_next;
            p_x->p_prev = p_entry;
        }
        p_entry->p_next = p_x;
        p_entry->p_prev = p_a;
    }
}

//--------------------------------------------------------------------

/* Move p_entry first */
static inline void move_to_front(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry)
{
    /*
     *  __    __    __
     * |A_|->|B_|->|X_|
     *            /
     * =>  p_entry
     *  __/   __    __
     * |X_|->|A_|->|B_|
     */
    if (p_entry == p_ht->pp_entries[l_bucket])
    {
        return;
    }
    
    /* Link p_entry out of the list. */
    p_entry->p_prev->p_next = p_entry->p_next;
    if (p_entry->p_next)
    {
        p_entry->p_next->p_prev = p_entry->p_prev;
    }
    
    /* Place p_entry first */
    p_entry->p_next = p_ht->pp_entries[l_bucket];
    p_entry->p_prev = NULL;
    p_ht->pp_entries[l_bucket]->p_prev = p_entry;
    p_ht->pp_entries[l_bucket] = p_entry;
}

//--------------------------------------------------------------------

static inline void remove_from_chain(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p)
{
    if (p->p_prev)
    {
        p->p_prev->p_next = p->p_next;
    }
    else /* first in list */
    {
        p_ht->pp_entries[l_bucket] = p->p_next;
    }
    if (p->p_next)
    {
        p->p_next->p_prev = p->p_prev;
    }
    
    if (p->p_older)
    {
        p->p_older->p_newer = p->p_newer;
    }
    else /* oldest */
    {
        p_ht->p_oldest = p->p_newer;
    }
    if (p->p_newer)
    {
        p->p_newer->p_older = p->p_older;
    }
    else /* newest */
    {
        p_ht->p_newest = p->p_older;
    }
}

//--------------------------------------------------------------------

/* Search for an element in a bucket */
#if !defined( DBG )
static inline
#endif//!DBG
ght_hash_entry_t*
search_in_bucket(
    __in ght_hash_table_t *p_ht,
    __in ght_uint32_t l_bucket,
    __in ght_hash_key_t *p_key,
    __in unsigned char i_heuristics
    )
{
    ght_hash_entry_t *p_e;
#if defined( DBG )
    int   entries = 0x0;
#endif//DBG
    
    for (p_e = p_ht->pp_entries[l_bucket];
         p_e;
         p_e = p_e->p_next)
    {
#if defined( DBG )
        ++entries;
        assert( p_e->key_shadow.i_size == p_e->key.i_size &&
               0x0 == memcmp( p_e->key.p_key, p_e->key_shadow.p_key, p_e->key_shadow.i_size ) );
#endif//DBG
        
        if( (p_e->key.i_size == p_key->i_size) &&
            (memcmp(p_e->key.p_key, p_key->p_key, p_e->key.i_size) == 0) )
        {
            /* Matching entry found - Apply heuristics, if any */
            switch (i_heuristics)
            {
                case GHT_HEURISTICS_MOVE_TO_FRONT:
                    move_to_front(p_ht, l_bucket, p_e);
                    break;
                case GHT_HEURISTICS_TRANSPOSE:
                    transpose(p_ht, l_bucket, p_e);
                    break;
                default:
                    break;
            }
            //
            // I do not have intention to insert NULL in the hash table
            //
            assert( p_e->p_data );
            return p_e;
        }
    }
    
#if defined( DBG )
    assert( entries == p_ht->p_nr[ l_bucket ] );
#endif//DBG
    
    return NULL;
}

//--------------------------------------------------------------------

/* Free a chain of entries (in a bucket) */
static inline
void
free_entry_chain(
    __in ght_hash_table_t *p_ht, 
    __in ght_hash_entry_t *p_entry
    )
{
    ght_hash_entry_t *p_e = p_entry;
    
    while(p_e)
    {
        ght_hash_entry_t *p_e_next = p_e->p_next;
        he_finalize(p_ht, p_e);
        p_e = p_e_next;
    }
}

//--------------------------------------------------------------------

/* Fill in the data to a existing hash key */
static inline void hk_fill(ght_hash_key_t *p_hk, int i_size, const void *p_key)
{
    assert(p_hk);
    
    p_hk->i_size = i_size;
    p_hk->p_key = p_key;
}

//--------------------------------------------------------------------

/* Create a hash entry */
static inline
ght_hash_entry_t*
he_create(
    __in ght_hash_table_t *p_ht,
    __in void *p_data,
    __in unsigned int i_key_size,
    __in const void *p_key_data
   )
{
    ght_hash_entry_t *p_he;
    size_t size;
    
    assert( p_ht->non_block || preemption_enabled() );
    
    /*
     * An element like the following is allocated:
     *        elem->p_key
     *       /   elem->p_key->p_key_data
     *  ____|___/________
     * |elem|key|key data|
     * |____|___|________|
     *
     * That is, the key and the key data is stored "inline" within the
     * hash entry.
     *
     * This saves space since mac_kalloc only is called once and thus avoids
     * some fragmentation. Thanks to Dru Lemley for this idea.
     */
    size = sizeof(ght_hash_entry_t)+i_key_size;
    if( !(p_he = (ght_hash_entry_t*)p_ht->fn_alloc( size, p_ht->non_block? M_NOWAIT :M_WAITOK ) ) )
    {
        DBG_PRINT_ERROR( ( "p_he = p_ht->fn_alloc( %d, %d ) failed!\n", (int)size, p_ht->non_block? M_NOWAIT :M_WAITOK ) );
        
        //
        // if the alloaction was non-blocking try the blocking one even if there is a risk of a deadlock
        //
        if( p_ht->non_block && !(p_he = (ght_hash_entry_t*)p_ht->fn_alloc( size, M_WAITOK ) ) ){
            
            DBG_PRINT_ERROR( ( "p_he = p_ht->fn_alloc( %d, %d ) second fail!\n", (int)size, M_WAITOK ) );
            return NULL;
            
        }
    
        if( !p_he )
            return NULL;
    }
    
#if defined( DBG )
    //
    // allocate a shadow copy for the key, used to test the entry consistency
    //
    p_he->key_shadow.i_size = i_key_size;
    p_he->key_shadow.p_key  = p_ht->fn_alloc( i_key_size, p_ht->non_block? M_NOWAIT :M_WAITOK );
    assert( p_he->key_shadow.p_key );
    if( !p_he->key_shadow.p_key ){
        
        p_ht->fn_free( p_he, size );
        return NULL;
    }
    memcpy( (void*)p_he->key_shadow.p_key, p_key_data, i_key_size );
#endif//DBG
    
    p_he->size    = size;
    p_he->p_data  = p_data;
    p_he->p_next  = NULL;
    p_he->p_prev  = NULL;
    p_he->p_older = NULL;
    p_he->p_newer = NULL;
    
    /* Create the key */
    p_he->key.i_size = i_key_size;
    memcpy(p_he+1, p_key_data, i_key_size);
    p_he->key.p_key = (void*)(p_he+1);
    
    return p_he;
}

//--------------------------------------------------------------------

/* Finalize (free) a hash entry */
static inline void he_finalize(ght_hash_table_t *p_ht, ght_hash_entry_t *p_he)
{
    assert( p_he );
    assert( p_ht->non_block || preemption_enabled() );
    
#if defined(DBG)
    p_he->p_next = NULL;
    p_he->p_prev = NULL;
    p_he->p_older = NULL;
    p_he->p_newer = NULL;
#endif /* DBG */
    
#if defined( DBG )
    if( p_he->key_shadow.p_key ){
        
        p_ht->fn_free( (void*)p_he->key_shadow.p_key, p_he->key_shadow.i_size );
    }
#endif//DBG
    
    /* Free the entry */
    p_ht->fn_free( p_he, p_he->size );
}

//--------------------------------------------------------------------

#if 0
/* Tried this to avoid recalculating hash values by caching
 * them. Overhead larger than benefits.
 */
static inline ght_uint32_t get_hash_value(ght_hash_table_t *p_ht, ght_hash_key_t *p_key)
{
    int i;
    
    if (p_key->i_size > sizeof(uint64_t))
        return p_ht->fn_hash(p_key);
    
    /* Lookup in the hash value cache */
    for (i = 0; i < GHT_N_CACHED_HASH_KEYS; i++)
    {
        if ( p_key->i_size == p_ht->cached_keys[i].key.i_size &&
             memcmp(p_key->p_key, p_ht->cached_keys[i].key.p_key, p_key->i_size) == 0)
             return p_ht->cached_keys[i].hash_val;
    }
    p_ht->cur_cache_evict = (p_ht->cur_cache_evict + 1) % GHT_N_CACHED_HASH_KEYS;
    p_ht->cached_keys[ p_ht->cur_cache_evict ].key = *p_key;
    p_ht->cached_keys[ p_ht->cur_cache_evict ].hash_val = p_ht->fn_hash(p_key);
    
    return p_ht->cached_keys[ p_ht->cur_cache_evict ].hash_val;
}
#else
# define get_hash_value(p_ht, p_key) ( (p_ht)->fn_hash(p_key) )
#endif

//--------------------------------------------------------------------

/* --- Exported methods --- */
/* Create a new hash table */
ght_hash_table_t*
ght_create(
    __in unsigned int i_size,
    __in bool   non_block
    )
{
    ght_hash_table_t *p_ht;
    int i=1;
    
    assert( non_block || preemption_enabled() );
     
    if ( !(p_ht = (ght_hash_table_t*)mac_kalloc( sizeof(ght_hash_table_t), non_block? M_NOWAIT : M_WAITOK )) )
    {
        DBG_PRINT_ERROR( ( "ght_create-> p_ht = mac_kalloc( %d, %d ) failed\n",
                            (int)sizeof(ght_hash_table_t),
                            non_block? M_NOWAIT : M_WAITOK ) );
        return NULL;
    }
    
    /* Set the size of the hash table to the nearest 2^i higher then i_size */
    p_ht->i_size = 1;
    while(p_ht->i_size < i_size)
    {
        p_ht->i_size = 1<<i++;
    }
    
    p_ht->i_size_mask = (1<<(i-1))-1; /* Mask to & with */
    p_ht->i_items = 0;
    
    p_ht->fn_hash = ght_one_at_a_time_hash;
    
    /* Standard values for allocations */
    p_ht->fn_alloc = mac_kalloc;
    p_ht->fn_free = mac_kfree;
    
    /* Set flags */
    p_ht->i_heuristics = GHT_HEURISTICS_NONE;
    p_ht->i_automatic_rehash = FALSE;
    
    p_ht->bucket_limit = 0;
    p_ht->fn_bucket_free = NULL;
    
    p_ht->non_block = non_block;
    
    /* Create an empty bucket list. */
    if ( !(p_ht->pp_entries = (ght_hash_entry_t**)mac_kalloc( p_ht->i_size*sizeof(ght_hash_entry_t*), non_block? M_NOWAIT : M_WAITOK ) ) )
    {
        DBG_PRINT_ERROR( ( "ght_create-> p_ht->pp_entries = mac_kalloc( %d, %d ) failed\n",
                           (int)(p_ht->i_size*sizeof(ght_hash_entry_t*)),
                           non_block? M_NOWAIT : M_WAITOK ) );
        mac_kfree( p_ht, sizeof(ght_hash_table_t) );
        return NULL;
    }
    memset(p_ht->pp_entries, 0, p_ht->i_size*sizeof(ght_hash_entry_t*));
    
    /* Initialise the number of entries in each bucket to zero */
    if ( !(p_ht->p_nr = (int*)mac_kalloc( p_ht->i_size*sizeof(int), non_block? M_NOWAIT : M_WAITOK ) ) )
    {
        DBG_PRINT_ERROR( ( "ght_create-> p_ht->p_nr = mac_kalloc( %d, %d ) failed\n",
                           (int)(p_ht->i_size*sizeof(int)),
                           non_block? M_NOWAIT : M_WAITOK ) );

        mac_kfree( p_ht->pp_entries, p_ht->i_size*sizeof(ght_hash_entry_t*) );
        mac_kfree( p_ht, sizeof(ght_hash_table_t) );
        return NULL;
    }
    
    memset( p_ht->p_nr, 0, p_ht->i_size*sizeof(int) );
    
    p_ht->p_oldest = NULL;
    p_ht->p_newest = NULL;
    
    return p_ht;
}

//--------------------------------------------------------------------

/* Set the allocation/deallocation function to use */
void ght_set_alloc(ght_hash_table_t *p_ht, ght_fn_alloc_t fn_alloc, ght_fn_free_t fn_free)
{
    p_ht->fn_alloc = fn_alloc;
    p_ht->fn_free = fn_free;
}

//--------------------------------------------------------------------

/* Set the hash function to use */
void ght_set_hash(ght_hash_table_t *p_ht, ght_fn_hash_t fn_hash)
{
    p_ht->fn_hash = fn_hash;
}

//--------------------------------------------------------------------

/* Set the heuristics to use. */
void ght_set_heuristics(ght_hash_table_t *p_ht, int i_heuristics)
{
    p_ht->i_heuristics = i_heuristics;
}

//--------------------------------------------------------------------

/* Set the rehashing status of the table. */
void ght_set_rehash(ght_hash_table_t *p_ht, int b_rehash)
{
    p_ht->i_automatic_rehash = b_rehash;
}

//--------------------------------------------------------------------

void ght_set_bounded_buckets(ght_hash_table_t *p_ht, unsigned int limit, ght_fn_bucket_free_callback_t fn)
{
    p_ht->bucket_limit = limit;
    p_ht->fn_bucket_free = fn;
    
    if( limit > 0 && fn == NULL )
    {
        panic( "ght_set_bounded_buckets: The bucket callback function is NULL but the limit is %d\n", limit );
    }
}

//--------------------------------------------------------------------

/* Get the number of items in the hash table */
unsigned int ght_size(ght_hash_table_t *p_ht)
{
    return p_ht->i_items;
}

//--------------------------------------------------------------------

/* Get the size of the hash table */
unsigned int ght_table_size(ght_hash_table_t *p_ht)
{
    return p_ht->i_size;
}

//--------------------------------------------------------------------

/* Insert an entry into the hash table */
GHT_STATUS_CODE
ght_insert(
    __in ght_hash_table_t *p_ht,
    __in void *p_entry_data,
    __in unsigned int i_key_size,
    __in const void *p_key_data
    )
{
    ght_hash_entry_t *p_entry;
    ght_uint32_t l_key;
    ght_hash_key_t key;
    
    assert(p_ht);
    
    hk_fill(&key, i_key_size, p_key_data);
    l_key = get_hash_value(p_ht, &key) & p_ht->i_size_mask;
    if (search_in_bucket(p_ht, l_key, &key, 0))
    {
        /* Don't insert if the key is already present. */
        return GHT_ALREADY_IN_HASH;
    }
    if (!(p_entry = he_create( p_ht, p_entry_data,
                               i_key_size, p_key_data)))
    {
        DBG_PRINT_ERROR( ( "ght_insert-> he_create failed\n" ) );
        return GHT_ERROR;
    }
    
    /* Rehash if the number of items inserted is too high. */
    if( p_ht->i_automatic_rehash && ( p_ht->i_items > 2*p_ht->i_size ) )
    {
        ght_rehash( p_ht, 2*p_ht->i_size );
        /* Recalculate l_key after ght_rehash has updated i_size_mask */
        l_key = get_hash_value(p_ht, &key) & p_ht->i_size_mask;
    }
    
    /* Place the entry first in the list. */
    p_entry->p_next = p_ht->pp_entries[l_key];
    p_entry->p_prev = NULL;
    if (p_ht->pp_entries[l_key])
    {
        p_ht->pp_entries[l_key]->p_prev = p_entry;
    }
    p_ht->pp_entries[l_key] = p_entry;
    
    /* If this is a limited bucket hash table, potentially remove the last item */
    if( p_ht->bucket_limit != 0 &&
        p_ht->p_nr[l_key] >= p_ht->bucket_limit)
    {
        ght_hash_entry_t *p;
        
        /* Loop through entries until the last
         *
         * FIXME: Better with a pointer to the last entry
         */
        for (p = p_ht->pp_entries[l_key];
             p->p_next != NULL;
             p = p->p_next);
        
        assert(p && p->p_next == NULL);
        
        remove_from_chain(p_ht, l_key, p); /* To allow it to be reinserted in fn_bucket_free */
        p_ht->fn_bucket_free(p->p_data, p->key.p_key);
        
        he_finalize( p_ht, p );
    }
    else
    {
        p_ht->p_nr[l_key]++;
        
        assert( p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1 );
        
        p_ht->i_items++;
    }
    
    if (p_ht->p_oldest == NULL)
    {
        p_ht->p_oldest = p_entry;
    }
    p_entry->p_older = p_ht->p_newest;
    
    if (p_ht->p_newest != NULL)
    {
        p_ht->p_newest->p_newer = p_entry;
    }
    
    p_ht->p_newest = p_entry;
    
    return GHT_OK;
}

//--------------------------------------------------------------------

/* Get an entry from the hash table. The entry is returned, or NULL if it wasn't found */
void*
ght_get(
    __in ght_hash_table_t *p_ht,
    __in unsigned int i_key_size,
    __in const void *p_key_data
    )
{
    ght_hash_entry_t *p_e;
    ght_hash_key_t key;
    ght_uint32_t l_key;
    
    assert(p_ht);
    
    hk_fill(&key, i_key_size, p_key_data);
    
    l_key = get_hash_value(p_ht, &key) & p_ht->i_size_mask;
    
    /* Check that the first element in the list really is the first. */
    assert( p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1 );
    
    /* LOCK: p_ht->pp_entries[l_key] */
    p_e = search_in_bucket(p_ht, l_key, &key, p_ht->i_heuristics);
    /* UNLOCK: p_ht->pp_entries[l_key] */
    
    return (p_e?p_e->p_data:NULL);
}

//--------------------------------------------------------------------

/* Replace an entry from the hash table. The entry is returned, or NULL if it wasn't found */
void *ght_replace(ght_hash_table_t *p_ht,
                  void *p_entry_data,
                  unsigned int i_key_size, const void *p_key_data)
{
    ght_hash_entry_t *p_e;
    ght_hash_key_t key;
    ght_uint32_t l_key;
    void *p_old;
    
    assert(p_ht);
    
    hk_fill(&key, i_key_size, p_key_data);
    
    l_key = get_hash_value(p_ht, &key) & p_ht->i_size_mask;
    
    /* Check that the first element in the list really is the first. */
    assert( p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1 );
    
    /* LOCK: p_ht->pp_entries[l_key] */
    p_e = search_in_bucket(p_ht, l_key, &key, p_ht->i_heuristics);
    /* UNLOCK: p_ht->pp_entries[l_key] */
    
    if ( !p_e )
        return NULL;
    
    p_old = p_e->p_data;
    p_e->p_data = p_entry_data;
    
    return p_old;
}

//--------------------------------------------------------------------

/* Remove an entry from the hash table. The removed entry, or NULL, is
 returned (and NOT free'd). */
void *ght_remove(ght_hash_table_t *p_ht,
                 unsigned int i_key_size, const void *p_key_data)
{
    ght_hash_entry_t *p_out;
    ght_hash_key_t key;
    ght_uint32_t l_key;
    void *p_ret=NULL;
    
    assert(p_ht);
    
    hk_fill(&key, i_key_size, p_key_data);
    l_key = get_hash_value(p_ht, &key) & p_ht->i_size_mask;
    
    /* Check that the first element really is the first */
    assert( (p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1) );
    
    /* LOCK: p_ht->pp_entries[l_key] */
    p_out = search_in_bucket(p_ht, l_key, &key, 0);
    
    /* Link p_out out of the list. */
    if (p_out)
    {
        remove_from_chain(p_ht, l_key, p_out);
        
        /* This should ONLY be done for normal items (for now all items) */
        p_ht->i_items--;
        
        p_ht->p_nr[l_key]--;
        /* UNLOCK: p_ht->pp_entries[l_key] */
#if !defined(NDEBUG)
        p_out->p_next = NULL;
        p_out->p_prev = NULL;
#endif /* NDEBUG */
        
        p_ret = p_out->p_data;
        he_finalize(p_ht, p_out);
    }
    /* else: UNLOCK: p_ht->pp_entries[l_key] */
    
    return p_ret;
}

//--------------------------------------------------------------------

static inline void *first_keysize(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, const void **pp_key, unsigned int *size)
{
    assert(p_ht && p_iterator);
    
    /* Fill the iterator */
    p_iterator->p_entry = p_ht->p_oldest;
    
    if (p_iterator->p_entry)
    {
        p_iterator->p_next = p_iterator->p_entry->p_newer;
        *pp_key = p_iterator->p_entry->key.p_key;
        if (size != NULL)
            *size = p_iterator->p_entry->key.i_size;
        
        return p_iterator->p_entry->p_data;
    }
    
    p_iterator->p_next = NULL;
    *pp_key = NULL;
    if (size != NULL)
        *size = 0;
    
    return NULL;
}

//--------------------------------------------------------------------

/* Get the first entry in an iteration */
void *ght_first(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, const void **pp_key)
{
    return first_keysize(p_ht, p_iterator, pp_key, NULL);
}

//--------------------------------------------------------------------

void *ght_first_keysize(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, const void **pp_key, unsigned int *size)
{
    return first_keysize(p_ht, p_iterator, pp_key, size);
}

//--------------------------------------------------------------------

static inline void *next_keysize(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, const void **pp_key, unsigned int *size)
{
    assert(p_ht && p_iterator);
    
    if (p_iterator->p_next)
    {
        /* More entries */
        p_iterator->p_entry = p_iterator->p_next;
        p_iterator->p_next = p_iterator->p_next->p_newer;
        
        *pp_key = p_iterator->p_entry->key.p_key;
        if (size != NULL)
            *size = p_iterator->p_entry->key.i_size;
        
        return p_iterator->p_entry->p_data; /* We know that this is non-NULL */
    }
    
    /* Last entry */
    p_iterator->p_entry = NULL;
    p_iterator->p_next = NULL;
    
    *pp_key = NULL;
    if (size != NULL)
        *size = 0;
    
    return NULL;
}

//--------------------------------------------------------------------

/* Get the next entry in an iteration. You have to call ght_first
 once initially before you use this function */
void *ght_next(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, const void **pp_key)
{
    return next_keysize(p_ht, p_iterator, pp_key, NULL);
}

//--------------------------------------------------------------------

void *ght_next_keysize(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, const void **pp_key, unsigned int *size)
{
    return next_keysize(p_ht, p_iterator, pp_key, size);
}

//--------------------------------------------------------------------

/* Finalize (free) a hash table */
void
ght_finalize(
    __in ght_hash_table_t *p_ht
    )
{
    int i;
    
    assert(p_ht);
    
    if (p_ht->pp_entries)
    {
        /* For each bucket, free all entries */
        for (i=0; i<p_ht->i_size; i++)
        {
            free_entry_chain( p_ht, p_ht->pp_entries[i] );
            p_ht->pp_entries[i] = NULL;
        }
        
        mac_kfree( p_ht->pp_entries, p_ht->i_size*sizeof(ght_hash_entry_t*) );
        p_ht->pp_entries = NULL;
        
    }
    if (p_ht->p_nr)
    {
        mac_kfree( p_ht->p_nr, p_ht->i_size*sizeof(int) );
        p_ht->p_nr = NULL;
    }
    
    mac_kfree( p_ht, sizeof(ght_hash_table_t) );
}

//--------------------------------------------------------------------

/* Rehash the hash table (i.e. change its size and reinsert all
 * items). This operation is slow and should not be used frequently.
 */
void
ght_rehash(
    __in ght_hash_table_t *p_ht,
    __in unsigned int i_size
    )
{
    ght_hash_table_t *p_tmp;
    ght_iterator_t iterator;
    const void *p_key;
    void *p;
    int i;
    
    assert(p_ht);
    
    /* Recreate the hash table with the new size */
    p_tmp = ght_create(i_size, p_ht->non_block );
    assert(p_tmp);
    
    /* Set the flags for the new hash table */
    ght_set_hash(p_tmp, p_ht->fn_hash);
    ght_set_alloc(p_tmp, p_ht->fn_alloc, p_ht->fn_free);
    ght_set_heuristics(p_tmp, GHT_HEURISTICS_NONE);
    ght_set_rehash(p_tmp, FALSE);
    
    /* Walk through all elements in the table and insert them into the temporary one. */
    for (p = ght_first(p_ht, &iterator, &p_key); p; p = ght_next(p_ht, &iterator, &p_key))
    {
        assert(iterator.p_entry);
        
        /* Insert the entry into the new table */
        if (ght_insert(p_tmp,
                       iterator.p_entry->p_data,
                       iterator.p_entry->key.i_size, iterator.p_entry->key.p_key) < 0)
        {
            DBG_PRINT_ERROR( ( "DldCommonHashTable.cpp ERROR: Out of memory error or entry already in hash table\n"
                                "when rehashing (internal error)\n" ) );
        }
    }
    
    /* Remove the old table... */
    for (i=0; i<p_ht->i_size; i++)
    {
        if (p_ht->pp_entries[i])
        {
            /* Delete the entries in the bucket */
            free_entry_chain (p_ht, p_ht->pp_entries[i]);
            p_ht->pp_entries[i] = NULL;
        }
    }
    
    mac_kfree( p_ht->pp_entries, p_ht->i_size*sizeof(ght_hash_entry_t*) );
    mac_kfree( p_ht->p_nr, p_ht->i_size*sizeof(int) );
    
    /* ... and replace it with the new */
    p_ht->i_size = p_tmp->i_size;
    p_ht->i_size_mask = p_tmp->i_size_mask;
    p_ht->i_items = p_tmp->i_items;
    p_ht->pp_entries = p_tmp->pp_entries;
    p_ht->p_nr = p_tmp->p_nr;
    
    p_ht->p_oldest = p_tmp->p_oldest;
    p_ht->p_newest = p_tmp->p_newest;
    
    /* Clean up */
    p_tmp->pp_entries = NULL;
    p_tmp->p_nr = NULL;
    mac_kfree( p_tmp, sizeof(ght_hash_table_t) );
}

//--------------------------------------------------------------------

static ght_uint32_t crc32_table[256] =
{
    0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,
    0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
    0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,
    0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
    0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,
    0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
    0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,
    0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
    0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,
    0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
    0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,
    0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
    0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,
    0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
    0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,
    0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
    0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,
    0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
    0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,
    0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
    0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,
    0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
    0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,
    0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
    0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,
    0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
    0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,
    0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
    0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,
    0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
    0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,
    0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4
};

//--------------------------------------------------------------------

/* One-at-a-time hash (found in a web article from ddj), this is the
 * standard hash function.
 *
 * See http://burtleburtle.net/bob/hash/doobs.html
 * for the hash functions used here.
 */
ght_uint32_t ght_one_at_a_time_hash(ght_hash_key_t *p_key)
{
    ght_uint32_t i_hash=0;
    int i;
    
    // TEST
    //return 0x0;
    
    assert(p_key);
    
    for (i=0; i<p_key->i_size; ++i)
    {
        i_hash += ((unsigned char*)p_key->p_key)[i];
        i_hash += (i_hash << 10);
        i_hash ^= (i_hash >> 6);
    }
    i_hash += (i_hash << 3);
    i_hash ^= (i_hash >> 11);
    i_hash += (i_hash << 15);
    
    return i_hash;
}

//--------------------------------------------------------------------

/* CRC32 hash based on code from comp.compression FAQ.
 * Added by Dru Lemley <spambait@lemley.net>
 */
ght_uint32_t ght_crc_hash(ght_hash_key_t *p_key)
{
    unsigned char *p, *p_end;
    ght_uint32_t  crc;
    
    assert(p_key);
    
    crc = 0xffffffff;       /* preload shift register, per CRC-32 spec */
    p = (unsigned char *)p_key->p_key;
    p_end = p + p_key->i_size;
    while (p < p_end)
        crc = (crc << 8) ^ crc32_table[(crc >> 24) ^ *(p++)];
    return ~crc;            /* transmit complement, per CRC-32 spec */
}

//--------------------------------------------------------------------

/* Rotating hash function. */
ght_uint32_t ght_rotating_hash(ght_hash_key_t *p_key)
{
    ght_uint32_t i_hash=0;
    int i;
    
    assert(p_key);
    
    for (i=0; i<p_key->i_size; ++i)
    {
        i_hash = (i_hash<<4)^(i_hash>>28)^((unsigned char*)p_key->p_key)[i];
    }
    
    return i_hash;
}

//--------------------------------------------------------------------

