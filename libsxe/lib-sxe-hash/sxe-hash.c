/* Copyright (c) 2010 Sophos Group.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>

#include "lookup3.h"
#include "sxe-hash-private.h"

#define SXE_HASH_UNUSED_BUCKET    0
#define SXE_HASH_NEW_BUCKET       1
#define SXE_HASH_BUCKETS_RESERVED 2

/**
 * Default hash key function; returns the first word of the key; useful when key is a SHA1
 *
 * @param key  = Key to hash
 * @param size = Size of key to hash
 *
 * @return Checksum (i.e. hash value) of key
 */
static unsigned
sxe_prehashed_key_hash(const void * key, unsigned size)
{
    SXEE83("sxe_prehashed_key_hash(key=%.*s, size=%u)", size, key, size);
    SXE_UNUSED_PARAMETER(size);
    SXER81("return sum=%u", *(const unsigned *)key);
    return *(const unsigned *)key;
}

/**
 * Allocate and contruct a hash
 *
 * @param name          = Name of the hash, used in diagnostics
 * @param element_count = Maximum number of elements in the hash
 * @param element_size  = Size of each element in the hash in bytes
 * @param key_offset    = Offset of start of key from start of element in bytes
 * @param key_size      = Size of the key in bytes
 * @param options       = SXE_HASH_OPTION_UNLOCKED  | SXE_HASH_OPTION_LOCKED       (single threaded  or use locking)
 *                      + SXE_HASH_OPTION_PREHASHED | SXE_HASH_OPTION_LOOKUP3_HASH (key is prehashed or use lookup3)
 *
 * @return A pointer to an array of hash elements
 */
void *
sxe_hash_new_plus(const char * name, unsigned element_count, unsigned element_size, unsigned key_offset, unsigned key_size,
                  unsigned options)
{
    SXE_HASH * hash;
    unsigned   size;

    SXEE86("sxe_hash_new_plus(name=%s,element_count=%u,element_size=%u,key_offset=%u,key_size=%u,options=%u)", name, element_count,
           element_size, key_offset, key_size, options);
    size = sizeof(SXE_HASH) + sxe_pool_size(element_count, element_size, element_count + SXE_HASH_BUCKETS_RESERVED);
    SXEA12((hash = malloc(size)) != NULL, "Unable to allocate %u bytes of memory for hash %s", size, name);
    SXEL82("Base address of hash %s = %p", name, hash);

    /* Note: hash + 1 == pool base */
    hash->pool   = sxe_pool_construct(hash + 1, name, element_count, element_size, element_count + SXE_HASH_BUCKETS_RESERVED,
                                      options & SXE_HASH_OPTION_LOCKED ? SXE_POOL_OPTION_LOCKED : 0);
    hash->count      = element_count;
    hash->size       = element_size;
    hash->key_offset = key_offset;
    hash->key_size   = key_size;
    hash->options    = options;
    hash->hash_key   = options & SXE_HASH_OPTION_LOOKUP3_HASH ? lookup3_hash : sxe_prehashed_key_hash;

    SXER81("return array=%p", hash->pool);
    return hash->pool;
}

void
sxe_hash_reconstruct(void * array)
{
    SXE_HASH * hash = SXE_HASH_ARRAY_TO_IMPL(array);
    SXEE81("sxe_hash_reconstruct(hash=%s)", sxe_pool_get_name(array));
    hash->pool  = sxe_pool_construct(hash + 1, sxe_pool_get_name(array),
                                     hash->count, hash->size, hash->count + SXE_HASH_BUCKETS_RESERVED,
                                     hash->options & SXE_HASH_OPTION_LOCKED ? SXE_POOL_OPTION_LOCKED : 0);
    SXER80("return");
}
/**
 * Delete a hash created with sxe_hash_new()
 *
 * @param array = Pointer to the hash array
 */
void
sxe_hash_delete(void * array)
{
    SXE_HASH * hash = SXE_HASH_ARRAY_TO_IMPL(array);
    SXEE81("sxe_hash_delete(hash=%s)", sxe_pool_get_name(array));
    free(hash);
    SXER80("return");
}

/**
 * Take an element from the free queue of the hash
 *
 * @param array = Pointer to the hash array
 *
 * @return The index of the element or SXE_HASH_FULL if the hash is full
 *
 * @note The element is moved to the new queue until the caller adds it to the hash
 */
unsigned
sxe_hash_take(void * array)
{
    SXE_HASH * hash = SXE_HASH_ARRAY_TO_IMPL(array);
    unsigned   id;

    SXEE81("sxe_hash_take(hash=%s)", sxe_pool_get_name(array));
    id = sxe_pool_set_oldest_element_state(hash->pool, SXE_HASH_UNUSED_BUCKET, SXE_HASH_NEW_BUCKET);

    if (id == SXE_POOL_NO_INDEX) {
        id = SXE_HASH_FULL;
    }

    SXER82(id == SXE_HASH_FULL ? "%sSXE_HASH_FULL" : "%s%u", "return id=", id);
    return id;
}

/**
 * Look for a key in the hash
 *
 * @param array = Pointer to the hash array
 * @param key   = Pointer to the key value
 *
 * @return Index of the element found or SXE_HASH_KEY_NOT_FOUND
 */
unsigned
sxe_hash_look(void * array, const void * key)
{
    SXE_HASH      * hash = SXE_HASH_ARRAY_TO_IMPL(array);
    unsigned        id   = SXE_HASH_KEY_NOT_FOUND;
    unsigned        bucket;
    SXE_POOL_WALKER walker;

    SXEE82("sxe_hash_look(hash=%s,key=%p)", sxe_pool_get_name(array), key);
    bucket = hash->hash_key(key, hash->key_size) % hash->count + SXE_HASH_BUCKETS_RESERVED;
    SXEL81("Looking in bucket %u", bucket);
    sxe_pool_walker_construct(&walker, array, bucket);

    while ((id = sxe_pool_walker_step(&walker)) != SXE_HASH_KEY_NOT_FOUND) {
        if (memcmp((char *)array + id * hash->size + hash->key_offset, key, hash->key_size) == 0) {
            break;
        }
    }

    SXER82(id == SXE_HASH_KEY_NOT_FOUND ? "%sSXE_HASH_KEY_NOT_FOUND" : "%s%u", "return id=", id);
    return id;
}

/**
 * Add an element to the hash
 *
 * @param array = Pointer to the hash array
 * @param id    = Index of the element to hash
 */
void
sxe_hash_add(void * array, unsigned id)
{
    SXE_HASH * hash = SXE_HASH_ARRAY_TO_IMPL(array);
    void     * key;
    unsigned   bucket;

    SXEE82("sxe_hash_add(hash=%s,id=%u)", sxe_pool_get_name(array), id);

    key = &((char *)array)[id * hash->size + hash->key_offset];
    bucket = hash->hash_key(key, hash->key_size) % hash->count + SXE_HASH_BUCKETS_RESERVED;
    SXEL82("Adding element %u to bucket %u", id, bucket);
    sxe_pool_set_indexed_element_state(array, id, SXE_HASH_NEW_BUCKET, bucket);
    SXER80("return");
}

void
sxe_hash_give(void * array, unsigned id)
{
    SXE_HASH * hash  = SXE_HASH_ARRAY_TO_IMPL(array);

    SXEE82("sxe_hash_give(hash=%s,id=%u)", sxe_pool_get_name(array), id);
    sxe_pool_set_indexed_element_state(hash->pool, id, sxe_pool_index_to_state(array, id), SXE_HASH_UNUSED_BUCKET);
    SXER80("return");
}
