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


#include "sxe-hash-private.h"

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

unsigned
sxe_hash_set(void * array, const char * sha1_as_char, unsigned sha1_key_len, unsigned value)
{
    SXE_HASH *  hash = SXE_HASH_ARRAY_TO_IMPL(array);
    unsigned    id;

    SXE_UNUSED_PARAMETER(sha1_key_len);

    SXEE85("sxe_hash_set(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u,value=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len, value);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_HEX_LENGTH, "sha1 length is incorrect");

    if ((id = sxe_hash_take(array)) == SXE_HASH_FULL) {
        goto SXE_EARLY_OUT;
    }

    SXEL91("setting key and value at index=%u", id);
    sha1_from_hex(&((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].sha1, sha1_as_char);
    ((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].value = value;
    sxe_hash_add(array, id);

SXE_EARLY_OUT:
    SXER82(id == SXE_HASH_FULL ? "%sSXE_HASH_FULL" : "%s%u", "return id=", id);
    return id;
}

/**
 * Get the value of an element in a hash with fixed size elements (SHA1 + unsigned) by SHA1 key in hex
 */
unsigned
sxe_hash_get(void * array, const char * sha1_as_char, unsigned sha1_key_len)
{
    SXE_HASH *  hash  = SXE_HASH_ARRAY_TO_IMPL(array);
    unsigned    value = SXE_HASH_KEY_NOT_FOUND;
    unsigned    id;
    SOPHOS_SHA1 sha1;

    SXE_UNUSED_PARAMETER(sha1_key_len);
    SXEE84("sxe_hash_get(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_HEX_LENGTH, "sha1 length is incorrect");

    if (sha1_from_hex(&sha1, sha1_as_char) != SXE_RETURN_OK) {
        goto SXE_EARLY_OUT;
    }

    id = sxe_hash_look(array, &sha1);

    if (id != SXE_HASH_KEY_NOT_FOUND) {
        value = ((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].value;
    }

SXE_EARLY_OUT:
    SXER81("return value=%u", value);
    return value;
}

unsigned
sxe_hash_remove(void * array, const char * sha1_as_char, unsigned sha1_key_len)
{
    SXE_HASH *  hash  = SXE_HASH_ARRAY_TO_IMPL(array);
    unsigned    value = SXE_HASH_KEY_NOT_FOUND;
    unsigned    id;
    SOPHOS_SHA1 sha1;

    SXE_UNUSED_PARAMETER(sha1_key_len);
    SXEE84("sxe_hash_remove(hash=%p,sha1_as_char=%.*s,sha1_key_len=%u)", hash, sha1_key_len, sha1_as_char, sha1_key_len);
    SXEA60(sha1_key_len == SXE_HASH_SHA1_AS_HEX_LENGTH, "sha1 length is incorrect");

    if (sha1_from_hex(&sha1, sha1_as_char) != SXE_RETURN_OK) {
        goto SXE_EARLY_OUT;
    }

    id = sxe_hash_look(array, &sha1);

    if (id != SXE_HASH_KEY_NOT_FOUND) {
        value = ((SXE_HASH_KEY_VALUE_PAIR *)hash->pool)[id].value;
        sxe_hash_give(array, id);
    }

SXE_EARLY_OUT:
    SXER81("return value=%u", value);
    return value;
}
