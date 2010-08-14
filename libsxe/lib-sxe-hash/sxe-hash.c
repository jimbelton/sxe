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

#include "sxe.h"
#include "sxe-hash.h"
#include "sxe-pool.h"
#include "sxe-util.h"

#define SXE_HASH_UNUSED_BUCKET 0U

static void
sha1_key_as_char_to_uint(const char * sha1_as_char, uint32_t * sha1_as_uint, unsigned sha1_as_uint_len)
{
    unsigned i, j;
    
    SXEA90(sha1_as_uint_len == SXE_HASH_SHA1_AS_UINT_LENGTH, "sha1_as_uint must have length of 5");
    for (i = 0; i < sha1_as_uint_len; i++)
    {
        sha1_as_uint[i] = 0;
        for (j = 0; j < 8; j++)
        {
            char c = sha1_as_char[8 * i + j];
            if (c >= '0' && c <= '9')
            {
                sha1_as_uint[i] += c - '0';
            }
            else if (c >= 'A' && c <= 'F')
            {
                sha1_as_uint[i] += c - 'A' + 10;
            }
            else if (c >= 'a' && c <= 'f')
            {
                sha1_as_uint[i] += c - 'a' + 10;
            }
            else
            {
                SXEA91(0, "Invalid character in sha1_as_char: '%c'", c);
            }

            if (j < 7)
            {
                sha1_as_uint[i] <<= 4;
            }
        }

        SXEL93("sha1_as_char=%.*s, sha1_as_uint=%u", 8, &sha1_as_char[8 * i], sha1_as_uint[i]);
    }
}

static unsigned
sha1_key_as_bucket_index(uint32_t * sha1_as_uint, unsigned bucket_count)
{
    unsigned bucket_index;

    SXEE96("sha1_key_as_bucket_index(sha1_as_uint=%8x%8x%8x%8x%8x,bucket_count=%u)",
           sha1_as_uint[0],
           sha1_as_uint[1],
           sha1_as_uint[2],
           sha1_as_uint[3],
           sha1_as_uint[4],
           bucket_count);

    bucket_index = sha1_as_uint[SXE_HASH_SHA1_AS_UINT_LENGTH - 1];
    SXEL91("converted sha1_key to '%u'", bucket_index);
    bucket_index = (bucket_index % bucket_count) + 1;

    SXER91("return bucket_index=%u", bucket_index);
    return bucket_index;
}

static unsigned
get_index_of_key(SXE_HASH_KEY_VALUE_PAIR * pool, uint32_t * sha1_as_uint, unsigned bucket_index)
{
    unsigned i;
    unsigned count;
    unsigned result_index = SXE_HASH_KEY_NOT_FOUND;

    SXEE87("get_index_of_key(pool=%p,sha1_as_uint=%8x%8x%8x%8x%8x,bucket_index=%u)",
           pool,
           sha1_as_uint[0],
           sha1_as_uint[1],
           sha1_as_uint[2],
           sha1_as_uint[3],
           sha1_as_uint[4],
           bucket_index);

    count = sxe_pool_get_number_in_state(pool, bucket_index);

    if (count == 0)
    {
        SXEL90("key not found");
        goto SXE_EARLY_OUT;
    }

    for (i = 0; i < count; i++)
    {
        unsigned id = sxe_pool_get_oldest_element_index(pool, bucket_index);

        SXEL96("trying index=%u: key=%8x%8x%8x%8x%8x",
               id,
               pool[id].sha1_as_uint[0],
               pool[id].sha1_as_uint[1],
               pool[id].sha1_as_uint[2],
               pool[id].sha1_as_uint[3],
               pool[id].sha1_as_uint[4]);

        SXEA90(SXE_HASH_SHA1_AS_UINT_LENGTH == 5, "The length of as_uint should be 5 or the code below must change");

        if ((pool[id].sha1_as_uint[0] == sha1_as_uint[0])
        &&  (pool[id].sha1_as_uint[1] == sha1_as_uint[1])
        &&  (pool[id].sha1_as_uint[2] == sha1_as_uint[2])
        &&  (pool[id].sha1_as_uint[3] == sha1_as_uint[3])
        &&  (pool[id].sha1_as_uint[4] == sha1_as_uint[4]))
        {
            SXEL91("found matching key at index=%u", id);
            result_index = id;
            goto SXE_EARLY_OUT;
        }

        sxe_pool_touch_indexed_element(pool, id);
    }

SXE_EARLY_OUT:
    SXER81("return result_index=%u", result_index);
    return result_index;
}

SXE_HASH *
sxe_hash_new(const char * name, unsigned bucket_count)
{
    SXE_HASH * hash;

    SXEE82("sxe_hash_new(name=%s,bucket_count=%u)", name, bucket_count);

    hash = malloc(sizeof(SXE_HASH));

    hash->pool = sxe_pool_new(name, bucket_count, sizeof(SXE_HASH_KEY_VALUE_PAIR), bucket_count + 1);
    hash->size = bucket_count;

    SXER83("return hash=%p // hash->pool=%p, hash->size=%u", hash, hash->pool, hash->size);
    return hash;
}

unsigned
sxe_hash_set(SXE_HASH * hash, const char * sha1_as_char, unsigned sha1_key_len, unsigned value)
{
    unsigned bucket_index;
    unsigned id;
    uint32_t sha1_as_uint[SXE_HASH_SHA1_AS_UINT_LENGTH];

    SXE_UNUSED_PARAMETER(sha1_key_len);

    SXEE85("sxe_hash_set(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u,value=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len, value);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_CHAR_LENGTH, "sha1 length is incorrect");

    sha1_key_as_char_to_uint(sha1_as_char, sha1_as_uint, SXE_HASH_SHA1_AS_UINT_LENGTH);

    bucket_index = sha1_key_as_bucket_index(sha1_as_uint, hash->size);
    id = sxe_pool_set_oldest_element_state(hash->pool, SXE_HASH_UNUSED_BUCKET, bucket_index);

    if (id == SXE_POOL_NO_INDEX)
    {
        id = SXE_HASH_FULL;
        goto SXE_EARLY_OUT;
    }

    SXEL91("setting key and value at index=%u", id);

    SXEA90(SXE_HASH_SHA1_AS_UINT_LENGTH == 5, "The length of as_uint should be 5 or the code below must change");

    hash->pool[id].sha1_as_uint[0] = sha1_as_uint[0];
    hash->pool[id].sha1_as_uint[1] = sha1_as_uint[1];
    hash->pool[id].sha1_as_uint[2] = sha1_as_uint[2];
    hash->pool[id].sha1_as_uint[3] = sha1_as_uint[3];
    hash->pool[id].sha1_as_uint[4] = sha1_as_uint[4];

    hash->pool[id].value = value;

SXE_EARLY_OUT:
    SXER81("return id=%u", id);
    return id;
}

int
sxe_hash_get(SXE_HASH * hash, const char * sha1_as_char, unsigned sha1_key_len)
{
    int      value = SXE_HASH_KEY_NOT_FOUND;
    unsigned bucket_index;
    unsigned id;
    uint32_t sha1_as_uint[SXE_HASH_SHA1_AS_UINT_LENGTH];

    SXE_UNUSED_PARAMETER(sha1_key_len);

    SXEE84("sxe_hash_get(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_CHAR_LENGTH, "sha1 length is incorrect");

    sha1_key_as_char_to_uint(sha1_as_char, sha1_as_uint, SXE_HASH_SHA1_AS_UINT_LENGTH);

    bucket_index = sha1_key_as_bucket_index(sha1_as_uint, hash->size);
    id = get_index_of_key(hash->pool, sha1_as_uint, bucket_index);
    if (id != SXE_HASH_KEY_NOT_FOUND)
    {
        value = hash->pool[id].value;
    }

    SXER81("return value=%u", value);
    return value;
}

int
sxe_hash_delete(SXE_HASH * hash, const char * sha1_as_char, unsigned sha1_key_len)
{
    int      value = SXE_HASH_KEY_NOT_FOUND;
    unsigned id;
    unsigned bucket_index;
    uint32_t sha1_as_uint[SXE_HASH_SHA1_AS_UINT_LENGTH];

    SXE_UNUSED_PARAMETER(sha1_key_len);

    SXEE84("sxe_hash_delete(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_CHAR_LENGTH, "sha1 length is incorrect");

    sha1_key_as_char_to_uint(sha1_as_char, sha1_as_uint, SXE_HASH_SHA1_AS_UINT_LENGTH);

    bucket_index = sha1_key_as_bucket_index(sha1_as_uint, hash->size);
    id = get_index_of_key(hash->pool, sha1_as_uint, bucket_index);
    if (id != SXE_HASH_KEY_NOT_FOUND)
    {
        value = hash->pool[id].value;
        sxe_pool_set_indexed_element_state(hash->pool, id, bucket_index, SXE_HASH_UNUSED_BUCKET);
    }

    SXER81("return value=%u", value);
    return value;
}
