/* Copyright (c) 2021 Jim Belton
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

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-hash.h"
#include "sxe-jitson.h"
#include "sxe-log.h"
#include "sxe-spinlock.h"
#include "sxe-unicode.h"

static uint64_t        identifier_chars[4] = {0x03FF400000000000, 0x07FFFFFE87FFFFFE, 0x0000000000000000, 0x00000000000000};
static pthread_mutex_t type_indexing       = PTHREAD_MUTEX_INITIALIZER;     // Lock around (slow) just in time indexing

bool
sxe_jitson_is_reference(const struct sxe_jitson * jitson)
{
    return (jitson->type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_REFERENCE;
}

/**
 * Parse identifier characters until a non-identifier character is reached. Can be called after parsing the first character.
 *
 * @param json String to parse
 *
 * @return Pointer to first non-identifier character in json
 */
const char *
sxe_jitson_parse_identifier(const char *json)
{
    unsigned c;

    for (c = (unsigned char)*json; identifier_chars[c >> 6] & (1UL << (c & 0x3f)); c = (unsigned char)*json)
        json++;

    return json;
}

/**
 * Allocate a jitson object and parse a JSON string into it.
 *
 * @param json A '\0' terminated JSON string.
 *
 * @return A jitson object or NULL with errno ENOMEM, EINVAL, EILSEQ, EMSGSIZE, ENODATA, ENOTUNIQ, or EOVERFLOW
 */
struct sxe_jitson *
sxe_jitson_new(const char *json)
{
    struct sxe_jitson_stack *stack = sxe_jitson_stack_get_thread();

    if (!stack)
        return NULL;

    if (!sxe_jitson_stack_parse_json(stack, json)) {
        sxe_jitson_stack_clear(stack);
        return NULL;
    }

    return sxe_jitson_stack_get_jitson(stack);
}

/**
 * Get the unsigned integer value of a jitson whose type is SXE_JITSON_TYPE_NUMBER
 *
 * @return The numeric value as a uint64_t or ~0ULL if the value is can't be represented as a uint64_t
 *
 * @note If the value is can't be represented as a uint64_t, errno is set to EOVERFLOW
 */
uint64_t
sxe_jitson_get_uint(const struct sxe_jitson *jitson)
{
    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    SXEA6(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_NUMBER,
          "Can't get the numeric value of a %s", sxe_jitson_type_to_str(jitson->type));

    if (!(jitson->type & SXE_JITSON_TYPE_IS_UINT)) {
        uint64_t uint = jitson->number;

        if ((double)uint != jitson->number) {
            errno = EOVERFLOW;
            return ~0ULL;
        }

        return uint;
    }

    return jitson->integer;
}

/**
 * Get the numeric (double) value of a jitson whose type is SXE_JITSON_TYPE_NUMBER
 *
 * @return The numeric value as a double or NAN if the value is can't be represented as a double
 *
 * @note If the value is can't be represented as a double, errno is set to EOVERFLOW
 */
double
sxe_jitson_get_number(const struct sxe_jitson *jitson)
{
    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    SXEA6(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_NUMBER,
          "Can't get the numeric value of a %s", sxe_jitson_type_to_str(jitson->type));

    if (jitson->type & SXE_JITSON_TYPE_IS_UINT) {
        double number = jitson->integer;

        if ((uint64_t)number != jitson->integer) {
            errno = EOVERFLOW;
            return NAN;
        }

        return number;
    }

    return jitson->number;
}

/**
 * Get the string value of a jitson whose type is SXE_JITSON_TYPE_STRING or SXE_JITSON_TYPE_MEMBER_NAME
 *
 * @param jitson  Pointer to the jitson
 * @param len_out NULL or a pointer to a variable of type size_t to return the length of the string in
 *
 * @return The string value; if the jitson is a non string, results are undefined
 */
const char *
sxe_jitson_get_string(const struct sxe_jitson *jitson, size_t *len_out)
{
    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    SXEA6(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_STRING,
          "Can't get the string value of a %s", sxe_jitson_type_to_str(jitson->type));

    if (len_out)
        *len_out = sxe_jitson_len(jitson);    // If it's a string_ref, may need to compute the length

    return jitson->type & SXE_JITSON_TYPE_IS_REF ? jitson->reference : jitson->string;
}

/**
 * Get the boolen value of a jitson whose type is SXE_JITSON_TYPE_BOOL
 *
 * return The bool value; if the jitson is not an boolean, results are undefined
 */
bool
sxe_jitson_get_bool(const struct sxe_jitson *jitson)
{
    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    SXEA6(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_BOOL,
          "Can't get the boolean value of a %s", sxe_jitson_type_to_str(jitson->type));
    return jitson->boolean;
}

/**
 * Get a member's value from an object
 *
 * @param jitson An object
 * @param name   The member name
 * @param len    Length of the member name or 0 if not known
 *
 * @return The member's value or NULL on error (ENOMEM) or if the member name was not found (ENOKEY).
 */
const struct sxe_jitson *
sxe_jitson_object_get_member(const struct sxe_jitson *jitson, const char *name, unsigned len)
{
    const struct sxe_jitson *member;
    uint32_t                *bucket, *index;
    const char              *memname;
    size_t                   memlen;
    unsigned                 i;

    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    SXEA1(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_OBJECT, "Can't get a member value from a %s",
          sxe_jitson_type_to_str(jitson->type));
    len = len ?: strlen(name);                       // Determine the length of the member name if not provided

    if (jitson->len == 0) {    // Empty object
        errno = ENOKEY;
        return NULL;
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {    // If not already, index thread safely
        SXEA1(pthread_mutex_lock(&type_indexing) == 0, "Can't take indexing lock");

        if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {    // Recheck under the lock in case of a race
            /* Allocate an array of len buckets + 1 to store the size in jitsons
            */
            if (!(index = MOCKFAIL(MOCK_FAIL_OBJECT_GET_MEMBER, NULL, sxe_calloc(1, (jitson->len + 1) * sizeof(uint32_t))))) {
                pthread_mutex_unlock(&type_indexing);
                return NULL;
            }

            index[jitson->len]                              = jitson->integer;         // Store the size at the end of the index
            ((struct sxe_jitson *)(uintptr_t)jitson)->type |= SXE_JITSON_TYPE_INDEXED;
            ((struct sxe_jitson *)(uintptr_t)jitson)->index = index;

            for (member = jitson + 1, i = 0; i < jitson->len; i++) {
                // Only time it's safe to use member->len to get the length of the member name (if it's not a reference)
                memlen       = member->type & SXE_JITSON_TYPE_IS_REF ? strlen(member->reference) : member->len;
                bucket       = &index[sxe_hash_sum(sxe_jitson_get_string(member, NULL), memlen) % jitson->len];
                ((struct sxe_jitson *)(uintptr_t)member)->link = *bucket;
                *bucket      = member - jitson;
                member       = member + sxe_jitson_size(member);    // Skip the member name
                member       = member + sxe_jitson_size(member);    // Skip the member value
            }
        }

        pthread_mutex_unlock(&type_indexing);
    }

    for (i = jitson->index[sxe_hash_sum(name, len) % jitson->len]; i != 0; i = member->link)
    {
        member = &jitson[i];
        SXEA6(sxe_jitson_get_type(member) == SXE_JITSON_TYPE_STRING, "Object keys must be strings, not %s",
              sxe_jitson_type_to_str(member->type));
        memname = sxe_jitson_get_string(member, &memlen);
        SXEA6(memname, "A member name should always be a valid string");

        if (memlen == len && memcmp(memname, name, len) == 0)
            return member + sxe_jitson_size(member);    // Skip the member name, returning the value
    }

    errno = ENOKEY;
    return NULL;
}

/**
 * Get a element's value from an array
 *
 * @param jitson An object
 * @param idx    The index of the element
 *
 * @return The member's value or NULL on error (ENOMEM) or if the index was out of range (ERANGE).
 */
const struct sxe_jitson *
sxe_jitson_array_get_element(const struct sxe_jitson *jitson, unsigned idx)
{
    const struct sxe_jitson *element;
    uint32_t                *index;
    unsigned                 i;

    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    SXEA1(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_ARRAY, "Can't get an element value from a %s",
          sxe_jitson_type_to_str(jitson->type));

    if (idx >= jitson->len) {
        SXEL2("Array element %u is not less than len %u", idx, jitson->len);
        errno = ERANGE;
        return NULL;
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {    // If not already, index thread safely
        SXEA1(pthread_mutex_lock(&type_indexing) == 0, "Can't take indexing lock");

        if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {    // Recheck under the lock in case of a race
            /* Allocate an array of len offsets + 1 to store the size in jitsons
            */
            if (!(index = MOCKFAIL(MOCK_FAIL_ARRAY_GET_ELEMENT, NULL, sxe_malloc((jitson->len + 1) * sizeof(uint32_t))))) {
                pthread_mutex_unlock(&type_indexing);
                return NULL;
            }

            index[jitson->len] = jitson->integer;            // Store the size at the end of the index
            ((struct sxe_jitson *)(uintptr_t)jitson)->type      |= SXE_JITSON_TYPE_INDEXED;
            ((struct sxe_jitson *)(uintptr_t)jitson)->index      = index;

            for (element = jitson + 1, i = 0; i < jitson->len; i++, element = element + sxe_jitson_size(element))
                index[i] = element - jitson;
        }

        pthread_mutex_unlock(&type_indexing);
    }

    return &jitson[jitson->index[idx]];
}

struct sxe_jitson *
sxe_jitson_make_null(struct sxe_jitson *jitson)
{
    jitson->type = SXE_JITSON_TYPE_NULL;
    return jitson;
}

struct sxe_jitson *
sxe_jitson_make_bool(struct sxe_jitson *jitson, bool boolean)
{
    jitson->type    = SXE_JITSON_TYPE_BOOL;
    jitson->boolean = boolean;
    return jitson;
}

struct sxe_jitson *
sxe_jitson_make_number(struct sxe_jitson *jitson, double number)
{
    jitson->type   = SXE_JITSON_TYPE_NUMBER;
    jitson->number = number;
    return jitson;
}

/**
 * Create a jitson string value that references an immutable C string
 */
struct sxe_jitson *
sxe_jitson_make_string_ref(struct sxe_jitson *jitson, const char *string)
{
    jitson->type      = SXE_JITSON_TYPE_STRING | SXE_JITSON_TYPE_IS_REF;
    jitson->len       = 0;         // The length will be computed if and when needed and cached here if <= 4294967295
    jitson->reference = string;
    return jitson;
}

/**
 * Create a reference to another jitson that will behave exactly like the original jitson
 *
 * @param jitson Pointer to jitson to create
 * @param to     jitson to refer to
 *
 * @note References are only valid during the lifetime of the jitson they refer to
 */
struct sxe_jitson *
sxe_jitson_make_reference(struct sxe_jitson *jitson, const struct sxe_jitson *to)
{
    jitson->type   = SXE_JITSON_TYPE_REFERENCE;
    jitson->jitref = to->type == SXE_JITSON_TYPE_REFERENCE ? to->jitref : to;    // Don't create references to references
    return jitson;
}

/**
 * Duplicate a jitson value in allocated storage, deep cloning all indeces/owned content.
 *
 * @param jitson The jitson value to duplicate
 *
 * @return The duplicate jitson or NULL on allocation failure
 */
struct sxe_jitson *
sxe_jitson_dup(const struct sxe_jitson *jitson)
{
    struct sxe_jitson *dup;
    size_t             size;

    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;    // OK because refs to refs are not allowed
    size   = sxe_jitson_size(jitson) * sizeof(*jitson);

    if (!(dup = MOCKFAIL(MOCK_FAIL_DUP, NULL, sxe_malloc(size))))
        return NULL;

    memcpy(dup, jitson, size);

    if (!sxe_jitson_clone(jitson, dup)) {    // If the type requires a deep clone, do it
        sxe_free(dup);
        return NULL;
    }

    dup->type |= SXE_JITSON_TYPE_ALLOCED;
    return dup;
}

char *
sxe_jitson_to_json(const struct sxe_jitson *jitson, size_t *len_out)
{
    struct sxe_factory factory[1];
    char              *json;

    SXEE7("(jitson=%p,len_out=%p)", jitson, len_out);
    sxe_factory_alloc_make(factory, 0, 0);
    json = sxe_jitson_build_json(jitson, factory) ? sxe_factory_remove(factory, len_out) : NULL;
    SXER7("return json=%p", json);
    return json;
}
