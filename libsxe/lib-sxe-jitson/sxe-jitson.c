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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-hash.h"
#include "sxe-jitson.h"
#include "sxe-log.h"
#include "sxe-unicode.h"

static uint64_t identifier_chars[4] = {0x03FF400000000000, 0x07FFFFFE87FFFFFE, 0x0000000000000000, 0x00000000000000};

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
 * @param json A '\0' terminated JSON string. This string must be mutable, and will be modified by jitson.
 *
 * @return A jitson object or NULL with errno ENOMEM, EINVAL, EILSEQ, EMSGSIZE, ENODATA, ENOTUNIQ, or EOVERFLOW)
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
 * Get the numeric value of a jitson whose type is SXE_JITSON_TYPE_NUMBER
 *
 * return The numeric value as a double; if the jitson is not an integer, results are undefined
 */
double
sxe_jitson_get_number(const struct sxe_jitson *jitson)
{
    SXEA6(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_NUMBER,
          "Can't get the numeric value of a %s", sxe_jitson_type_to_str(jitson->type));
    return jitson->number;
}

/**
 * Get the string value of a jitson whose type is SXE_JITSON_TYPE_STRING or SXE_JITSON_TYPE_MEMBER_NAME
 *
 * @param jitson  Pointer to the jitson
 * @param len_out NULL or a pointer to a variable of type size_t to return the length of the string in
 *
 * @return The string value; if the jitson is a string, results are undefined
 */
const char *
sxe_jitson_get_string(struct sxe_jitson *jitson, unsigned *len_out)
{
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
 * @return Thw mwmber's value or NULL on error (ENOMEM) or if the member name was not found (ENOKEY).
 */
struct sxe_jitson *
sxe_jitson_object_get_member(struct sxe_jitson *jitson, const char *name, unsigned len)
{
    struct sxe_jitson *member;
    size_t             memlen;
    unsigned           i;
    uint32_t          *bucket;

    SXEA1(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_OBJECT, "Can't get a member value from a %s",
          sxe_jitson_type_to_str(jitson->type));
    len = len ?: strlen(name);                       // Determine the length of the member name if not provided

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {
        if (!(jitson->index = MOCKFAIL(MOCK_FAIL_OBJECT_GET_MEMBER, NULL, sxe_calloc(1, jitson->len * sizeof(uint32_t)))))
            return NULL;

        for (member = jitson + 1, i = 0; i < jitson->len; i++) {
            // Only time it's safe to use member->len to get the length of the member name (if it's not a reference)
            memlen       = member->type & SXE_JITSON_TYPE_IS_REF ? strlen(member->reference) : member->len;
            bucket       = &jitson->index[sxe_hash_sum(sxe_jitson_get_string(member, NULL), memlen) % jitson->len];
            member->link = *bucket;
            *bucket      = member - jitson;
            member       = member + sxe_jitson_size(member);    // Skip the member name
            member       = member + sxe_jitson_size(member);    // Skip the member value
        }
    }

    jitson->type |= SXE_JITSON_TYPE_INDEXED;

    for (bucket = &jitson->index[sxe_hash_sum(name, len) % jitson->len]; *bucket != 0;  bucket = &member->link)
    {
        member = &jitson[*bucket];
        SXEA6(sxe_jitson_get_type(member) == SXE_JITSON_TYPE_STRING, "Object keys must be strings, not %s",
              sxe_jitson_type_to_str(member->type));

        if (strcmp(sxe_jitson_get_string(member, NULL), name) == 0)
            return member + sxe_jitson_size(member);    // Skip the member name, returning the value
    }

    errno = ENOKEY;
    return NULL;
}

struct sxe_jitson *
sxe_jitson_array_get_element(struct sxe_jitson *jitson, unsigned idx)
{
    struct sxe_jitson *element;
    unsigned           i;

    SXEA1(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_ARRAY, "Can't get an element value from a %s",
          sxe_jitson_type_to_str(jitson->type));

    if (idx >= jitson->len) {
        SXEL2("Array element %u is not less than len %u", idx, jitson->len);
        errno = ERANGE;
        return NULL;
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {
        if (!(jitson->index = MOCKFAIL(MOCK_FAIL_ARRAY_GET_ELEMENT, NULL, sxe_malloc(jitson->len * sizeof(uint32_t)))))
            return NULL;

        for (element = jitson + 1, i = 0; i < jitson->len; i++, element = element + sxe_jitson_size(element))
            jitson->index[i] = element - jitson;
    }

    jitson->type |= SXE_JITSON_TYPE_INDEXED;
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

struct sxe_jitson *
sxe_jitson_make_string_ref(struct sxe_jitson *jitson, const char *string)
{
    jitson->type      = SXE_JITSON_TYPE_STRING | SXE_JITSON_TYPE_IS_REF;
    jitson->reference = string;
    return jitson;
}

char *
sxe_jitson_to_json(struct sxe_jitson *jitson, size_t *len_out)
{
    struct sxe_factory factory[1];

    sxe_factory_alloc_make(factory, 0, 0);
    return sxe_jitson_build_json(jitson, factory) ? sxe_factory_remove(factory, len_out) : NULL;
}
