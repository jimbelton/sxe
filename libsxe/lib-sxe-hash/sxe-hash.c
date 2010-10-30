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

#include <errno.h>
#include <string.h>

#include "sha1.h"
#include "sxe.h"
#include "sxe-hash.h"
#include "sxe-pool.h"
#include "sxe-util.h"

#define SXE_HASH_UNUSED_BUCKET    0
#define SXE_HASH_NEW_BUCKET       1
#define SXE_HASH_BUCKETS_RESERVED 2

#define SXE_HASH_ARRAY_TO_IMPL(array) ((SXE_HASH *)sxe_pool_to_base(array) - 1)

/**
 * Allocate and contruct a hash
 *
 * @param name          = Name of the hash, used in diagnostics
 * @param element_count = Maximum number of elements in the hash
 * @param element_size  = Size of each element in the hash
 * @param options       = SXE_HASH_OPTION_LOCKED | SXE_HASH_OPTION_UNLOCKED
 *
 * @return A pointer to an array of hash elements
 */
void *
sxe_hash_new_plus(const char * name, unsigned element_count, unsigned element_size, unsigned key_offset, unsigned key_size,
                  unsigned options)
{
    SXE_HASH * hash;
    unsigned   size;

    SXEE86("sxe_hash_new(name=%s,element_count=%u,element_size=%u,key_offset=%u,key_size=$u,options=%u)", name, element_count,
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

    SXER81("return array=%p", hash->pool);
    return hash->pool;
}

/**
 * Allocate and contruct a hash with fixed size elements (SHA1 + unsigned)
 *
 * @param name          = Name of the hash, used in diagnostics
 * @param element_count = Maximum number of elements in the hash
 *
 * @return A pointer to an array of hash elements
 *
 * @note This hash table is not thread safe
 */
void *
sxe_hash_new(const char * name, unsigned element_count)
{
    void * array;

    SXEE82("sxe_hash_new(name=%s,element_count=%u)", name, element_count);

    array = sxe_hash_new_plus(name, element_count, sizeof(SXE_HASH_KEY_VALUE_PAIR), 0, sizeof(SXE_SHA1), SXE_HASH_OPTION_UNLOCKED);
    SXER81("return array=%p", array);
    return array;
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

    if (id == SXE_POOL_NO_INDEX)
    {
        id = SXE_HASH_FULL;
    }

    SXER81("return %d // -1 == SXE_HASH_FULL", id);
    return id;
}

/**
 * Default hash key function; returns the first word of the key; useful when key is a SHA1
 *
 * @param key  = Key to hash
 * @param size = Size of key to hash
 *
 * @return Checksum (i.e. hash value) of key
 */
static unsigned
sxe_hash_key_default(const void * key, unsigned size)
{
    SXEE83("sxe_hash_key_default(key=%.*s, size=%u)", size, key, size);
    SXE_UNUSED_PARAMETER(size);
    SXER81("return sum=%u", *(const unsigned *)key);
    return *(const unsigned *)key;
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
    bucket = sxe_hash_key_default(key, hash->key_size) % hash->count + SXE_HASH_BUCKETS_RESERVED;
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
    bucket = sxe_hash_key_default(key, hash->key_size) % hash->count + SXE_HASH_BUCKETS_RESERVED;
    SXEL82("Adding element %u to bucket %u", id, bucket);
    sxe_pool_set_indexed_element_state(array, id, SXE_HASH_NEW_BUCKET, bucket);
    SXER80("return");
}

unsigned
sxe_hash_set(void * array, const char * sha1_as_char, unsigned sha1_key_len, unsigned value)
{
    SXE_HASH *  hash = SXE_HASH_ARRAY_TO_IMPL(array);
    unsigned    bucket_index;
    unsigned    id;
    SOPHOS_SHA1 sha1;

    SXE_UNUSED_PARAMETER(sha1_key_len);

    SXEE85("sxe_hash_set(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u,value=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len, value);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_HEX_LENGTH, "sha1 length is incorrect");

    if ((id = sxe_hash_take(array)) == SXE_HASH_FULL)
    {
        goto SXE_EARLY_OUT;
    }

    sha1_from_hex(&sha1, sha1_as_char);
    bucket_index = sxe_hash_key_default(&sha1, sizeof(sha1)) % hash->count + SXE_HASH_BUCKETS_RESERVED;
    sxe_pool_set_indexed_element_state(hash->pool, id, SXE_HASH_NEW_BUCKET, bucket_index);

    SXEL91("setting key and value at index=%u", id);
    memcpy(&((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].sha1, &sha1, sizeof(sha1));
    ((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].value = value;

SXE_EARLY_OUT:
    SXER81("return id=%u", id);
    return id;
}

/**
 * Get the value of an element in a hash with fixed size elements (SHA1 + unsigned) by SHA1 key in hex
 */
int
sxe_hash_get(void * array, const char * sha1_as_char, unsigned sha1_key_len)
{
    SXE_HASH *  hash  = SXE_HASH_ARRAY_TO_IMPL(array);
    int         value = SXE_HASH_KEY_NOT_FOUND;
    unsigned    id;
    SOPHOS_SHA1 sha1;

    SXE_UNUSED_PARAMETER(sha1_key_len);
    SXEE84("sxe_hash_get(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_HEX_LENGTH, "sha1 length is incorrect");

    sha1_from_hex(&sha1, sha1_as_char);
    id = sxe_hash_look(array, &sha1);

    if (id != SXE_HASH_KEY_NOT_FOUND) {
        value = ((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].value;
    }

    SXER81("return value=%u", value);
    return value;
}

void
sxe_hash_give(void * array, unsigned id)
{
    SXE_HASH * hash  = SXE_HASH_ARRAY_TO_IMPL(array);

    SXEE82("sxe_hash_give(hash=%s,id=%u)", sxe_pool_get_name(array), id);
    sxe_pool_set_indexed_element_state(hash->pool, id, sxe_pool_index_to_state(array, id), SXE_HASH_UNUSED_BUCKET);
    SXER80("return");
}

int
sxe_hash_remove(void * array, const char * sha1_as_char, unsigned sha1_key_len)
{
    SXE_HASH *  hash  = SXE_HASH_ARRAY_TO_IMPL(array);
    int         value = SXE_HASH_KEY_NOT_FOUND;
    unsigned    id;
    SOPHOS_SHA1 sha1;

    SXE_UNUSED_PARAMETER(sha1_key_len);
    SXEE84("sxe_hash_remove(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_HEX_LENGTH, "sha1 length is incorrect");

    sha1_from_hex(&sha1, sha1_as_char);
    id = sxe_hash_look(array, &sha1);

    if (id != SXE_HASH_KEY_NOT_FOUND) {
        value = ((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].value;
        sxe_hash_give(array, id);
    }

    SXER81("return value=%u", value);
    return value;
}
