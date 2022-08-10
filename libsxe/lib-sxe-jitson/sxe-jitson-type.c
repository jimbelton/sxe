/* Copyright (c) 2022 Jim Belton
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
#include <stdio.h>
#include <string.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-jitson.h"
#include "sxe-log.h"
#include "sxe-util.h"

struct sxe_jitson_op {
    bool is_binary;
    union {
        struct sxe_jitson *(*unary)( const struct sxe_jitson *);
        struct sxe_jitson *(*binary)(const struct sxe_jitson *, const struct sxe_jitson *);
    } def;
};

struct sxe_jitson_type {
    const char *name;
    void      (*free)(            struct sxe_jitson *);    // Free any memory allocated to the value (e.g. indices)
    bool      (*test)(      const struct sxe_jitson *);    // Return a true/false value for an value
    uint32_t  (*size)(      const struct sxe_jitson *);    // The size in "struct sxe_jitson"s of the value.
    size_t    (*len)(       const struct sxe_jitson *);    // The logical length (strlen, number of elements, number of members)
    bool      (*clone)(     const struct sxe_jitson *, struct sxe_jitson *);     // Clone (deep copy) a value's data
    char *    (*build_json)(const struct sxe_jitson *, struct sxe_factory *);    // Format a value
    unsigned    num_ops;    // Number of additonal operations supported by this type
    union {
        struct sxe_jitson *(*unary)( const struct sxe_jitson *);
        struct sxe_jitson *(*binary)(const struct sxe_jitson *, struct sxe_jitson *);
    } *ops;
};

uint32_t sxe_jitson_flags = 0;     // Flags past at initialization

static uint32_t                type_count = 0;
static struct sxe_jitson_type *types      = NULL;
static struct sxe_factory      type_factory[1];

uint32_t
sxe_jitson_type_register(const char *name,
                         void     (*free)(            struct sxe_jitson *),
                         bool     (*test)(      const struct sxe_jitson *),
                         uint32_t (*size)(      const struct sxe_jitson *),
                         size_t   (*len)(       const struct sxe_jitson *),
                         bool     (*clone)(     const struct sxe_jitson *, struct sxe_jitson *),
                         char *   (*build_json)(const struct sxe_jitson *, struct sxe_factory *))
{
    uint32_t type = type_count++;

    SXEA1(types, "%s:sxe_jitson_type_init has not been called", __func__);
    SXEA1(types = (struct sxe_jitson_type *)sxe_factory_reserve(type_factory, type_count * sizeof(*types)),
          "Couldn't allocate %u jitson types", type_count);
    types[type].name       = name;
    types[type].free       = free;
    types[type].test       = test;
    types[type].size       = size;
    types[type].clone      = clone;
    types[type].build_json = build_json;
    types[type].len        = len;
    return type;
}

const char *
sxe_jitson_type_to_str(unsigned type)
{
    if (type > type_count) {
        errno = ERANGE;
        return "ERROR";
    }

    return types[type].name;
}

unsigned
sxe_jitson_get_type(const struct sxe_jitson *jitson)
{
    if (sxe_jitson_is_reference(jitson))
        return sxe_jitson_get_type(jitson->jitref);

    return jitson->type & SXE_JITSON_TYPE_MASK;
}

/* Most types can use this as a free function, and those that don't should call it after any special work they do.
 */
void
sxe_jitson_free_base(struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_OWN) {    // If this jitson contains a reference to a value or index that it owns
        // Atomically nullify the reference in case there is a race, though calling code should ensure this is not the case
        void *reference = __sync_lock_test_and_set(&jitson->index, NULL);
        sxe_free(reference);
    }

    if (!(jitson->type & SXE_JITSON_TYPE_ALLOCED))
        return;

    jitson->type = SXE_JITSON_TYPE_INVALID;
    sxe_free((struct sxe_jitson *)(uintptr_t)jitson);
}

uint32_t
sxe_jitson_size_1(const struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return 1;
}

static bool
sxe_jitson_null_test(const struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return false;
}

static char *
sxe_jitson_null_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    SXE_UNUSED_PARAMETER(jitson);
    sxe_factory_add(factory, "null", 4);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_null(void)
{
    return sxe_jitson_type_register("null", sxe_jitson_free_base, sxe_jitson_null_test, sxe_jitson_size_1, NULL, NULL,
                                    sxe_jitson_null_build_json);
}

static bool
sxe_jitson_bool_test(const struct sxe_jitson *jitson)
{
    return jitson->boolean;
}

static char *
sxe_jitson_bool_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    if (sxe_jitson_get_bool(jitson))
        sxe_factory_add(factory, "true", 4);
    else
        sxe_factory_add(factory, "false", 5);

    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_bool(void)
{
    return sxe_jitson_type_register("bool", sxe_jitson_free_base, sxe_jitson_bool_test, sxe_jitson_size_1, NULL, NULL,
                                    sxe_jitson_bool_build_json);
}

static bool
sxe_jitson_number_test(const struct sxe_jitson *jitson)
{
    return jitson->number != 0.0;
}

#define DOUBLE_MAX_LEN 24

static char *
sxe_jitson_number_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    char    *ret;
    unsigned len;

    if ((ret = sxe_factory_reserve(factory, DOUBLE_MAX_LEN)) == NULL)
        return NULL;

    len = snprintf(ret, DOUBLE_MAX_LEN + 1, "%G", sxe_jitson_get_number(jitson));
    SXEA6(len <= DOUBLE_MAX_LEN, "As a string, numeric value %G is more than %u characters long",
          sxe_jitson_get_number(jitson), DOUBLE_MAX_LEN);
    sxe_factory_commit(factory, len);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_number(void)
{
    return sxe_jitson_type_register("number", sxe_jitson_free_base, sxe_jitson_number_test, sxe_jitson_size_1, NULL, NULL,
                                    sxe_jitson_number_build_json);
}

static size_t
sxe_jitson_string_len(const struct sxe_jitson *jitson)
{
    size_t len;

    if (jitson->type & SXE_JITSON_TYPE_IS_KEY)    // Object keys use the len field to store a link offset
        return jitson->type & SXE_JITSON_TYPE_IS_REF ? strlen(jitson->reference) : strlen(jitson->string);

    if (jitson->len == 0 && (jitson->type & SXE_JITSON_TYPE_IS_REF)) {
        len         = strlen(jitson->reference);

        if ((uint32_t)len == len)    // Can't cache the length if > 4294967295
            /* This is thread safe because the assignment is atomic and the referenced string is immutable
             */
            ((struct sxe_jitson *)(uintptr_t)jitson)->len = len;

        return len;
    }

    return jitson->len;
}

static uint32_t
sxe_jitson_string_size(const struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_REF)
        return 1;

    return 1 + (sxe_jitson_string_len(jitson) + SXE_JITSON_TOKEN_SIZE - SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE;
}

static bool
sxe_jitson_string_test(const struct sxe_jitson *jitson)
{
    return jitson->type & SXE_JITSON_TYPE_IS_REF ? *(const uint8_t *)jitson->reference : jitson->len;
}

static bool
sxe_jitson_string_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    /* If the jitson owns the string it's refering to, it must be duplicated
     */
    if ((jitson->type & SXE_JITSON_TYPE_IS_OWN)
     && (clone->reference = MOCKFAIL(MOCK_FAIL_STRING_CLONE, NULL, sxe_strdup(jitson->string))) == NULL) {
        SXEL2("Failed to duplicate a %zu byte string", strlen(jitson->string));
        return false;
    }

    return true;
}

static char *
sxe_jitson_string_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    const char *string;
    char       *buffer;
    unsigned    first, i, len;

    len    = sxe_jitson_len(jitson);
    string = sxe_jitson_get_string(jitson, NULL);
    sxe_factory_add(factory, "\"", 1);

    for (first = i = 0; i < len; i++)
        /* If the character is a control character or " or \, encode it as a unicode escape sequence.
         * (unsigned char) casts are used to allow any UTF8 encoded string.
         */
        if ((unsigned char)string[i] <= 0x1F || string[i] == '"' || string[i] == '\\') {
            if (first < i)
                sxe_factory_add(factory, &string[first], i - first);

            if ((buffer = sxe_factory_reserve(factory, sizeof("\\u0000"))) == NULL)
                return NULL;    /* COVERAGE EXCLUSION: Memory allocation failure */

            snprintf(buffer, sizeof("\\u0000"), "\\u00%02x", (unsigned char)string[i]);
            SXEA6(strlen(buffer) == sizeof("\\u0000") - 1, "Unicode escape sequence should always be 6 characters long");
            sxe_factory_commit(factory, sizeof("\\u0000") - 1);
            first = i + 1;
        }

    if (first < len)
        sxe_factory_add(factory, &string[first], len - first);

    sxe_factory_add(factory, "\"", 1);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_string(void)
{
    return sxe_jitson_type_register("string", sxe_jitson_free_base, sxe_jitson_string_test, sxe_jitson_string_size,
                                    sxe_jitson_string_len, sxe_jitson_string_clone, sxe_jitson_string_build_json);
}

static uint32_t
sxe_jitson_size_indexed(const struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_INDEXED)    // Once an array or object is indexed, it's size is at the end of the index
        return jitson->index[jitson->len];

    return jitson->integer;    // Prior to indexing, the offset past the end of the object/array is stored here
}

static bool
sxe_jitson_len_test(const struct sxe_jitson *jitson)
{
    return jitson->len != 0;
}

/* Length function for types that store their lengths in the len field
 */
static size_t
sxe_jitson_type_len(const struct sxe_jitson *jitson)
{
    return jitson->len;
}

/* Free a jitson array without using the index so that referenced data owned by it's members will be freed.
 */
static void
sxe_jitson_free_array(struct sxe_jitson *jitson)
{
    unsigned i;
    uint32_t offset, size;

    for (i = 0, offset = 1; i < jitson->len; i++, offset += size) {
        size = sxe_jitson_size(jitson + offset);
        sxe_jitson_free(jitson + offset);
    }

    sxe_jitson_free_base(jitson);
}

static bool
sxe_jitson_array_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    size_t   size;
    unsigned i, len;

    if ((len = jitson->len) == 0)
        return true;

    if (jitson->type & SXE_JITSON_TYPE_INDEXED) {
        if (!(clone->index = MOCKFAIL(MOCK_FAIL_ARRAY_CLONE, NULL, sxe_malloc((size = (len + 1) * sizeof(jitson->index[0]))))))
        {
            SXEL2("Failed to allocate %zu bytes to clone an array", (len + 1) * sizeof(jitson->index[0]));
            return false;
        }

        memcpy(clone->index, jitson->index, size);
    }

    for (i = 0; i < len; i++)
        if (!sxe_jitson_clone(sxe_jitson_array_get_element(jitson, i),
                              (struct sxe_jitson *)(uintptr_t)sxe_jitson_array_get_element(clone, i))) {
            while (i > 0)    // On error, free any allocations done
                sxe_jitson_free(SXE_CAST_NOCONST(struct sxe_jitson *, sxe_jitson_array_get_element(clone, --i)));

            if (clone->type & SXE_JITSON_TYPE_INDEXED)
                sxe_free(clone->index);

            return false;
        }

    return true;
}

static char *
sxe_jitson_array_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    unsigned i, len;

    sxe_factory_add(factory, "[", 1);

    if ((len = jitson->len) > 0) {
        for (i = 0; i < len - 1; i++) {
            sxe_jitson_build_json(sxe_jitson_array_get_element(jitson, i), factory);
            sxe_factory_add(factory, ",", 1);
        }

        sxe_jitson_build_json(sxe_jitson_array_get_element(jitson, len - 1), factory);
    }

    sxe_factory_add(factory, "]", 1);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_array(void)
{
    return sxe_jitson_type_register("array", sxe_jitson_free_array, sxe_jitson_len_test, sxe_jitson_size_indexed,
                                    sxe_jitson_type_len, sxe_jitson_array_clone, sxe_jitson_array_build_json);
}

/* Free a jitson object without using the index so that referenced data owned by it's members will be freed.
 */
static void
sxe_jitson_free_object(struct sxe_jitson *jitson)
{
    unsigned i, len;
    uint32_t offset, size;

    len = jitson->len * 2;    // Objects are pairs of member name/value

    for (i = 0, offset = 1; i < len; i++, offset += size) {
        size = sxe_jitson_size(jitson + offset);
        sxe_jitson_free(jitson + offset);
    }

    sxe_jitson_free_base(jitson);
}

static bool
sxe_jitson_object_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    const struct sxe_jitson *content;
    size_t                   size;
    unsigned                 i, len;
    uint32_t                 index, stop;
    bool                     key;

    if ((len = jitson->len) == 0)
        return true;

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED))    // Force indexing. Would it be better to walk the unindexed object?
        sxe_jitson_object_get_member(jitson, "", 0);

    if (!(clone->index = MOCKFAIL(MOCK_FAIL_OBJECT_CLONE, NULL, sxe_malloc((size = (len + 1) * sizeof(jitson->index[0])))))) {
        SXEL2("Failed to allocate %zu bytes to clone an object", (len + 1) * sizeof(jitson->index[0]));
        return false;
    }

    clone->type |= SXE_JITSON_TYPE_INDEXED;
    memcpy(clone->index, jitson->index, size);

    for (i = 0; i < len; i++)                                                   // For each bucket
        for (index = jitson->index[i]; index; index = jitson[index].link) {     // For each member in the bucket
            content = &jitson[index];
            size    = sxe_jitson_size(content);

            if (!(key = sxe_jitson_clone(content, &clone[index])) || !sxe_jitson_clone(content + size, &clone[index + size])) {
                stop = index;

                /* Free any space allocated for members
                 */
                for (i = 0; i < len; i++)                                                  // For each bucket
                    for (index = jitson->index[i]; index; index = jitson[index].link) {    // For each member in the bucket
                        if (key)    // If the key was cloned, free it
                            sxe_jitson_free(&clone[index]);

                        if (index == stop) {
                            sxe_free(clone->index);
                            return false;
                        }

                        sxe_jitson_free(&clone[index + size]);
                    }

                SXEA1(false, "Clone failed at index %u but it was not found during the unwind", stop);    /* COVERAGE EXCLUSION - Can't happen */
            }
        }

    return true;
}

static char *
sxe_jitson_object_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    unsigned i, index, len;
    bool     first;

    if ((len = jitson->len) == 0) {
        sxe_factory_add(factory, "{}", 2);
        return sxe_factory_look(factory, NULL);
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED))    // Force indexing. Would it be better to walk the unindexed object?
        sxe_jitson_object_get_member(jitson, "", 0);

    sxe_factory_add(factory, "{", 1);

    for (first = true, i = 0; i < len; i++) {                                  // For each bucket
        for (index = jitson->index[i]; index; index = jitson[index].link) {    // For each member in the bucket
            if (!first)
                 sxe_factory_add(factory, ",", 1);

            sxe_jitson_build_json(&jitson[index], factory);                                      // Output the member name
            sxe_factory_add(factory, ":", 1);
            sxe_jitson_build_json(&jitson[index] + sxe_jitson_size(&jitson[index]), factory);    // Output the value
            first = false;
        }
    }

    sxe_factory_add(factory, "}", 1);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_object(void)
{
    return sxe_jitson_type_register("object", sxe_jitson_free_object, sxe_jitson_len_test, sxe_jitson_size_indexed,
                                    sxe_jitson_type_len, sxe_jitson_object_clone, sxe_jitson_object_build_json);
}

static bool
sxe_jitson_reference_test(const struct sxe_jitson *jitson)
{
    return types[sxe_jitson_get_type(jitson->jitref)].test(jitson->jitref);
}

static size_t
sxe_jitson_reference_len(const struct sxe_jitson *jitson)
{
    return types[sxe_jitson_get_type(jitson->jitref)].len(jitson->jitref);
}

char *
sxe_jitson_reference_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    return types[sxe_jitson_get_type(jitson->jitref)].build_json(jitson->jitref, factory);
}

uint32_t
sxe_jitson_type_register_reference(void)
{
    return sxe_jitson_type_register("reference", sxe_jitson_free_base, sxe_jitson_reference_test, sxe_jitson_size_1,
                                    sxe_jitson_reference_len, NULL, sxe_jitson_reference_build_json);
}

/**
 * Initialize the types and register INVALID (0) and all JSON types: null, bool, number, string, array, object, and pointer.
 *
 * @param mintypes The minimum number of types to preallocate. Should be SXE_JITSON_MIN_TYPES + the number of additional types
 * @param flags    0 for standard JSON or SXE_JITSON_FLAG_ALLOW_HEX to allow hexadecimal unsigned integers
 */
void
sxe_jitson_type_init(uint32_t mintypes, uint32_t flags)
{
    sxe_factory_alloc_make(type_factory, mintypes < SXE_JITSON_MIN_TYPES ? SXE_JITSON_MIN_TYPES : mintypes, 256);
    types = (struct sxe_jitson_type *)sxe_factory_reserve(type_factory, mintypes * sizeof(*types));
    SXEA1(sxe_jitson_type_register("INVALID", NULL, NULL, NULL, NULL, NULL, NULL) == 0, "Type 0 is the 'INVALID' type");
    SXEA1(sxe_jitson_type_register_null()      == SXE_JITSON_TYPE_NULL,                 "Type 1 is 'null'");
    SXEA1(sxe_jitson_type_register_bool()      == SXE_JITSON_TYPE_BOOL,                 "Type 2 is 'bool'");
    SXEA1(sxe_jitson_type_register_number()    == SXE_JITSON_TYPE_NUMBER,               "Type 3 is 'number'");
    SXEA1(sxe_jitson_type_register_string()    == SXE_JITSON_TYPE_STRING,               "Type 4 is 'string'");
    SXEA1(sxe_jitson_type_register_array()     == SXE_JITSON_TYPE_ARRAY,                "Type 5 is 'array'");
    SXEA1(sxe_jitson_type_register_object()    == SXE_JITSON_TYPE_OBJECT,               "Type 6 is 'object'");
    SXEA1(sxe_jitson_type_register_reference() == SXE_JITSON_TYPE_REFERENCE,            "Type 7 is 'reference'");
    sxe_jitson_flags = flags;
}

/**
 * Finalize memory used for type and clear the types variables
 */
void
sxe_jitson_type_fini(void)
{
    sxe_free(sxe_factory_remove(type_factory, NULL));
    types      = NULL;
    type_count = 0;
}

bool
sxe_jitson_is_init(void)
{
    return types ? true : false;
}

/**
 * Free a jitson object
 *
 * @note All other threads that might access the object must remove their references to it before this function is called,
 *       unless all just in time operations (i.e. string length of a referenced string, construction of indeces) can be
 *       guaranteed to have already happened.
 */
void
sxe_jitson_free(struct sxe_jitson *jitson)
{
    if (jitson)
        types[jitson->type & SXE_JITSON_TYPE_MASK].free(jitson);
}

/**
 * Test a jitson object
 */
bool
sxe_jitson_test(const struct sxe_jitson *jitson)
{
    return types[jitson->type & SXE_JITSON_TYPE_MASK].test(jitson);
}

/**
 * Determine the size in jitson tokens of a jitson object
 */
uint32_t
sxe_jitson_size(const struct sxe_jitson *jitson)
{
    return types[jitson->type & SXE_JITSON_TYPE_MASK].size(jitson);
}

/**
 * Determine the length of a jitson object. For strings, this is the string length, for collections, the number of members.
 */
size_t
sxe_jitson_len(const struct sxe_jitson *jitson)
{
    uint32_t type = jitson->type & SXE_JITSON_TYPE_MASK;
    SXEA1(types[type].len, "%s:Type %s does not support taking its length", __func__,  sxe_jitson_type_to_str(type));
    return types[type].len(jitson);
}

bool
sxe_jitson_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    uint32_t type = jitson->type & SXE_JITSON_TYPE_MASK;
    bool     success;

    SXEE6("(jitson=%p,clone=%p) // type=%u", jitson, clone, type);
    success = !types[type].clone || types[type].clone(jitson, clone);    // Succeeds if no need to clone or clone succeeds
    SXER6("return %s;", success ? "true" : "false");
    return success;
}

/**
 * Build a JSON string from a jitson object
 *
 * @param jitson  The jitson object to encode in JSON
 * @param factory The factory object used to build the JSON string
 */
char *
sxe_jitson_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    return types[jitson->type & SXE_JITSON_TYPE_MASK].build_json(jitson, factory);
}
