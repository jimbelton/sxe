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

void
sxe_jitson_free_indeces(struct sxe_jitson *jitson)
{
    unsigned i, len;
    uint32_t index, type;

    if (((type = sxe_jitson_get_type(jitson)) & SXE_JITSON_TYPE_INDEXED) == 0)
        return;

    len = sxe_jitson_get_size(jitson);

    switch (type) {
    case SXE_JITSON_TYPE_ARRAY:
        for (i = 0; i < len - 1; i++)
            sxe_jitson_free_indeces(sxe_jitson_array_get_element(jitson, i));

        break;

    case SXE_JITSON_TYPE_OBJECT:
        for (i = 0; i < len; i++)                                                   // For each bucket
            for (index = jitson->index[i]; index; index = jitson[index].link)       // For each member in the bucket
                sxe_jitson_free_indeces(SXE_JITSON_MEMBER_SKIP(&jitson[index]));    // Free any indeces used by the value

        break;
    }

    free(jitson->index);
}

void
sxe_jitson_free(struct sxe_jitson *jitson)
{
    if (jitson) {
        sxe_jitson_free_indeces(jitson);
        free(jitson);
    }
}

unsigned
sxe_jitson_get_type(const struct sxe_jitson *jitson)
{
    return jitson->type & SXE_JITSON_TYPE_MASK;
}

const char *
sxe_jitson_type_to_str(unsigned type)
{
    switch (type & SXE_JITSON_TYPE_MASK) {
    case SXE_JITSON_TYPE_INVALID:
        break;
    case SXE_JITSON_TYPE_NUMBER:
        return "NUMBER";
    case SXE_JITSON_TYPE_STRING:
        return "STRING";
    case SXE_JITSON_TYPE_OBJECT:
        return "OBJECT";
    case SXE_JITSON_TYPE_ARRAY:
        return "ARRAY";
    case SXE_JITSON_TYPE_BOOL:
        return "BOOL";
    case SXE_JITSON_TYPE_NULL:
        return "NULL";
    case SXE_JITSON_TYPE_MEMBER:
        return "MEMBER";
    default:
        SXEA6(type, "Unrecognized JITSON type %u", type);
    }

    return "INVALID";
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
 * Get the string value of a jitson whose type is SXE_JITSON_TYPE_STRING
 *
 * @param jitson Pointer to the jitson
 * @param size_out NULL or a pointer to a variable of type size_t to return the length of the string in
 *
 * @return The string value; if the jitson is a string, results are undefined
 */
const char *
sxe_jitson_get_string(struct sxe_jitson *jitson, unsigned *size_out)
{
    SXEA6(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_STRING,
          "Can't get the string value of a %s", sxe_jitson_type_to_str(jitson->type));

    if (size_out)
        *size_out = sxe_jitson_get_size(jitson);    // If it's a string_ref, may need to compute the length

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

size_t
sxe_jitson_get_size(struct sxe_jitson *jitson)
{
    size_t size;

    switch (sxe_jitson_get_type(jitson)) {
    case SXE_JITSON_TYPE_STRING:
        if (jitson->size == 0 && (jitson->type & SXE_JITSON_TYPE_IS_REF)) {
            size         = strlen(jitson->reference);
            jitson->size = (uint32_t)size == size ? size : 0;    // Can't cache the length if > 4294967295
            return size;
        }

        /* FALL THRU */

    case SXE_JITSON_TYPE_OBJECT:
    case SXE_JITSON_TYPE_ARRAY:
        return jitson->size;

    case SXE_JITSON_TYPE_MEMBER:
        return jitson->member.len;
    }

    SXEL2("JSON type %s has no size", sxe_jitson_type_to_str(jitson->type));
    return 0;
}

/* This function is only usable when and array/object has not yet been indexed.
 */
static struct sxe_jitson *
sxe_jitson_skip(struct sxe_jitson *jitson)
{
    switch (jitson->type) {
    case SXE_JITSON_TYPE_NUMBER:
    case SXE_JITSON_TYPE_BOOL:
    case SXE_JITSON_TYPE_NULL:
        return jitson + 1;

    case SXE_JITSON_TYPE_STRING:
        return jitson + 1 + (jitson->size + SXE_JITSON_TOKEN_SIZE - SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE;

    case SXE_JITSON_TYPE_MEMBER:
        return SXE_JITSON_MEMBER_SKIP(jitson);

    case SXE_JITSON_TYPE_OBJECT:
    case SXE_JITSON_TYPE_ARRAY:
        return jitson + jitson->integer;    // Add the offset of the next stack frame
    }

    SXEA1(false, "Unexpected JSON type %s", sxe_jitson_type_to_str(jitson->type));    /* COVERAGE EXCLUSION: Can't happen */
    return NULL;                                                                      /* COVERAGE EXCLUSION: Can't happen */
}

struct sxe_jitson *
sxe_jitson_object_get_member(struct sxe_jitson *jitson, const char *name, unsigned len)
{
    struct sxe_jitson *member;
    unsigned           i;
    uint32_t          *bucket;

    SXEA1(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_OBJECT || (jitson->type & SXE_JITSON_TYPE_PARTIAL),
          "Can't get a member value from a %s%s", (jitson->type & SXE_JITSON_TYPE_PARTIAL) ? "partial " : "",
          sxe_jitson_type_to_str(jitson->type));
    len = len ?: strlen(name);                       // Determine the length of the member name if not provided

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {
        if (!(jitson->index = MOCKFAIL(MOCK_FAIL_OBJECT_GET_MEMBER, NULL, calloc(1, jitson->size * sizeof(uint32_t)))))
            return NULL;

        for (member = jitson + 1, i = 0; i < jitson->size; i++) {
            bucket       = &jitson->index[sxe_hash_sum(member->member.name, member->member.len) % jitson->size];
            member->link = *bucket;
            *bucket      = member - jitson;
            member       = sxe_jitson_skip(member);    // Skip the member name
            member       = sxe_jitson_skip(member);    // Skip the member value
        }
    }

    jitson->type |= SXE_JITSON_TYPE_INDEXED;

    for (bucket = &jitson->index[sxe_hash_sum(name, len) % jitson->size]; *bucket != 0;  bucket = &member->link)
    {
        member = &jitson[*bucket];
        SXEA6(member->type == SXE_JITSON_TYPE_MEMBER, "Object buckets contain members, not %s", sxe_jitson_type_to_str(member->type));

        if (member->member.len == len && memcmp(member->member.name, name, len) == 0)
            return sxe_jitson_skip(member);    // Skip the member name, returning the value
    }

    return NULL;
}

struct sxe_jitson *
sxe_jitson_array_get_element(struct sxe_jitson *jitson, unsigned idx)
{
    struct sxe_jitson *element;
    unsigned           i;

    SXEA1(sxe_jitson_get_type(jitson) == SXE_JITSON_TYPE_ARRAY|| (jitson->type & SXE_JITSON_TYPE_PARTIAL),
          "Can't get an element value from a %s%s", (jitson->type & SXE_JITSON_TYPE_PARTIAL) ? "partial " : "",
          sxe_jitson_type_to_str(jitson->type));

    if (idx >= jitson->size) {
        SXEL2("Array element %u is not less than size %u", idx, jitson->size);
        errno = ERANGE;
        return NULL;
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED)) {
        if (!(jitson->index = MOCKFAIL(MOCK_FAIL_ARRAY_GET_ELEMENT, NULL, malloc(jitson->size * sizeof(uint32_t)))))
            return NULL;

        for (element = jitson + 1, i = 0; i < jitson->size; i++, element = sxe_jitson_skip(element))
            jitson->index[i] = element - jitson;
    }

    jitson->type |= SXE_JITSON_TYPE_INDEXED;
    return &jitson[jitson->index[idx]];
}

bool
sxe_jitson_test(const struct sxe_jitson *jitson)
{
    switch (sxe_jitson_get_type(jitson)) {
    case SXE_JITSON_TYPE_NULL:   return false;
    case SXE_JITSON_TYPE_BOOL:   return jitson->boolean;
    case SXE_JITSON_TYPE_NUMBER: return jitson->number != 0.0;
    case SXE_JITSON_TYPE_STRING:
        return jitson->type & SXE_JITSON_TYPE_IS_REF ? *(const uint8_t *)jitson->reference : jitson->size;
    }

    SXEA6(false, "SXE jitson type %u is not valid", sxe_jitson_get_type(jitson));
    return false;
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
