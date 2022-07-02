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

#include <sxe-alloc.h>
#include <sxe-jitson.h>
#include <sxe-log.h>
 
struct sxe_jitson_op {
    bool is_binary;
    union {
        struct sxe_jitson *(*unary)( struct sxe_jitson *);
        struct sxe_jitson *(*binary)(struct sxe_jitson *, struct sxe_jitson *);
    } def;
};

struct sxe_jitson_type {
    const char *name;
    void      (*free)(      struct sxe_jitson *);
    bool      (*test)(      struct sxe_jitson *);
    uint32_t  (*size)(      struct sxe_jitson *);
    size_t    (*len)(       struct sxe_jitson *);
    char *    (*build_json)(struct sxe_jitson *jitson, struct sxe_factory *factory);
    unsigned    num_ops;    // Number of additonal operations supported by this type
    union {
        struct sxe_jitson *(*unary)( struct sxe_jitson *);
        struct sxe_jitson *(*binary)(struct sxe_jitson *, struct sxe_jitson *);
    } *ops;
};

static uint32_t                type_count = 0;
static struct sxe_jitson_type *types      = NULL;
static struct sxe_factory      type_factory[1];

uint32_t
sxe_jitson_type_register(const char *name, 
                         void      (*free)(      struct sxe_jitson *),
                         bool      (*test)(      struct sxe_jitson *), 
                         uint32_t  (*size)(      struct sxe_jitson *),
                         size_t    (*len)(       struct sxe_jitson *),
                         char *    (*build_json)(struct sxe_jitson *, struct sxe_factory *))
{
    uint32_t type = type_count++;
    
    SXEA1(types, "%s:sxe_jitson_type_init has not been called", __func__);
    SXEA1(types = (struct sxe_jitson_type *)sxe_factory_reserve(type_factory, type_count * sizeof(*types)), 
          "Couldn't allocate %u jitson types", type_count);
    types[type].name       = name;
    types[type].free       = free;
    types[type].test       = test;
    types[type].size       = size;
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
    return jitson->type & SXE_JITSON_TYPE_MASK;
}

/* Most types can use this as a free function, and those that don't should call it after any special work they do.
 */
void
sxe_jitson_free_base(struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_OWN) {    // If this jitson contains a reference to a value or index that it owns
        sxe_free(jitson->index);
        jitson->reference = NULL;
    }
    
    if (!(jitson->type & SXE_JITSON_TYPE_ALLOCED))
        return;
        
    free(jitson);
}

uint32_t
sxe_jitson_size_1(struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return 1;
}

static bool
sxe_jitson_null_test(struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return false;
}

static char *
sxe_jitson_null_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    SXE_UNUSED_PARAMETER(jitson);
    sxe_factory_add(factory, "null", 4);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_null(void)
{
    return sxe_jitson_type_register("null", sxe_jitson_free_base, sxe_jitson_null_test, sxe_jitson_size_1, NULL,
                                    sxe_jitson_null_build_json);
}

static bool
sxe_jitson_bool_test(struct sxe_jitson *jitson)
{
    return jitson->boolean;
}

static char *
sxe_jitson_bool_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
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
    return sxe_jitson_type_register("bool", sxe_jitson_free_base, sxe_jitson_bool_test, sxe_jitson_size_1, NULL,    
                                    sxe_jitson_bool_build_json);
}

static bool
sxe_jitson_number_test(struct sxe_jitson *jitson)
{
    return jitson->number != 0.0;
}

#define DOUBLE_MAX_LEN 24

static char *
sxe_jitson_number_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    char    *ret;
    unsigned len;
    
    if ((ret = sxe_factory_reserve(factory, DOUBLE_MAX_LEN)) == NULL)
        return ret;

    len = snprintf(ret, DOUBLE_MAX_LEN + 1, "%G", sxe_jitson_get_number(jitson));
    SXEA6(len <= DOUBLE_MAX_LEN, "As a string, numeric value %G is more than %u characters long",
          sxe_jitson_get_number(jitson), DOUBLE_MAX_LEN);
    sxe_factory_commit(factory, len);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_number(void)
{
    return sxe_jitson_type_register("number", sxe_jitson_free_base, sxe_jitson_number_test, sxe_jitson_size_1, NULL,
                                    sxe_jitson_number_build_json);
}

static size_t
sxe_jitson_string_len(struct sxe_jitson *jitson)
{
    size_t len;
    
    if (jitson->type & SXE_JITSON_TYPE_IS_KEY)    // Object keys use the len field to store a link offset
        return jitson->type & SXE_JITSON_TYPE_IS_REF ? strlen(jitson->reference) : strlen(jitson->string);
        
    if (jitson->len == 0 && (jitson->type & SXE_JITSON_TYPE_IS_REF)) {
        len         = strlen(jitson->reference);
        jitson->len = (uint32_t)len == len ? len : 0;    // Can't cache the length if > 4294967295
        return len;
    }

    return jitson->len;
}

static uint32_t
sxe_jitson_string_size(struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_REF)
        return 1;

    return 1 + (sxe_jitson_string_len(jitson) + SXE_JITSON_TOKEN_SIZE - SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE;
}

static bool
sxe_jitson_string_test(struct sxe_jitson *jitson)
{
    return jitson->type & SXE_JITSON_TYPE_IS_REF ? *(const uint8_t *)jitson->reference : jitson->len;
}

static char *
sxe_jitson_string_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    const char *string;
    unsigned    first, i, len;
    char        unicode_str[8];

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

            snprintf(unicode_str, sizeof(unicode_str), "\\u00%02x", (unsigned char)string[i]);
            SXEA6(strlen(unicode_str) == 6, "Unicode escape sequence should always be 6 characters long");
            sxe_factory_add(factory, unicode_str, 6);
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
                                    sxe_jitson_string_len, sxe_jitson_string_build_json);
}

static uint32_t
sxe_jitson_not_indexed_size(struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_INDEXED) {    // Once an array or object is indexed, it can't be sized
        errno = ENOTSUP;
        return 0;
    }
    
    return jitson->integer;    // Return the offset of the next stack frame
}

static bool
sxe_jitson_len_test(struct sxe_jitson *jitson)
{
    return jitson->len != 0;
}

// Length function for types that store their lengths in the len field
static size_t
sxe_jitson_type_len(struct sxe_jitson *jitson)
{
    return jitson->len;
}

static void
sxe_jitson_free_array(struct sxe_jitson *jitson)
{
    unsigned i, len;

    if (jitson->type & SXE_JITSON_TYPE_INDEXED) {
        len = jitson->len;

        for (i = 0; i < len - 1; i++)
            sxe_jitson_free(sxe_jitson_array_get_element(jitson, i));
    }
    
    sxe_jitson_free_base(jitson);
}

static char *
sxe_jitson_array_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
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
    return sxe_jitson_type_register("array", sxe_jitson_free_array, sxe_jitson_len_test, sxe_jitson_not_indexed_size, 
                                    sxe_jitson_type_len, sxe_jitson_array_build_json);
}

static void
sxe_jitson_free_object(struct sxe_jitson *jitson)
{
    unsigned i, len;
    uint32_t index;

    if (jitson->type & SXE_JITSON_TYPE_INDEXED) {
        len = jitson->len;

        for (i = 0; i < len; i++)                                                   // For each bucket
            for (index = jitson->index[i]; index; index = jitson[index].link) {     // For each member in the bucket
                struct sxe_jitson *key = &jitson[index];
                sxe_jitson_free(key + sxe_jitson_size(key));                        // Free any memory owned by the value
                sxe_jitson_free(key);                                                // Free the member name if its owned
            }
    }

    sxe_jitson_free_base(jitson);
}

static char *
sxe_jitson_object_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    unsigned i, index, len;
    
    if ((len = jitson->len) == 0) {
        sxe_factory_add(factory, "{}", 2);
        return sxe_factory_look(factory, NULL);
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED))    // Force indexing. Would it be better to walk the unindexed oject?
        sxe_jitson_object_get_member(jitson, "", 0);

    *sxe_factory_reserve(factory, 1) = '{';

    for (i = 0; i < len; i++) {                                                // For each bucket
        for (index = jitson->index[i]; index; index = jitson[index].link) {    // For each member in the bucket
            sxe_factory_commit(factory, 1);                                    // Commit the '{' or ','
            sxe_jitson_build_json(&jitson[index], factory);                    // Output the member name
            sxe_factory_add(factory, ":", 1);
            sxe_jitson_build_json(&jitson[index] + sxe_jitson_size(&jitson[index]), factory);    // Output the value
            *sxe_factory_reserve(factory, 1) = ',';
        }
    }

    *sxe_factory_reserve(factory, 1) = '}';    // Repeating a reservation is safe, allowing the last ',' to be overwritten
    sxe_factory_commit(factory, 1);
    return sxe_factory_look(factory, NULL);
}

uint32_t
sxe_jitson_type_register_object(void)
{
    return sxe_jitson_type_register("object", sxe_jitson_free_object, sxe_jitson_len_test, sxe_jitson_not_indexed_size, 
                                    sxe_jitson_type_len, sxe_jitson_object_build_json);
}

/**
 * Initialize the types and register INVALID (0) and all JSON types: null, bool, number, string, array, object, and pointer.
 * 
 * @param mintypes The minimum number of types to preallocate. Should be SXE_JITSON_MIN_TYPES + the number of additional types
 */
void
sxe_jitson_type_init(uint32_t mintypes)
{
    sxe_factory_alloc_make(type_factory, mintypes < SXE_JITSON_MIN_TYPES ? SXE_JITSON_MIN_TYPES : mintypes, 256);
    types = (struct sxe_jitson_type *)sxe_factory_reserve(type_factory, mintypes * sizeof(*types));
    SXEA1(sxe_jitson_type_register("INVALID", NULL, NULL, NULL, NULL, NULL) == 0, "Type 0 is the 'INVALID' type");
    SXEA1(sxe_jitson_type_register_null()   == SXE_JITSON_TYPE_NULL,              "Type 1 is 'null'");
    SXEA1(sxe_jitson_type_register_bool()   == SXE_JITSON_TYPE_BOOL,              "Type 2 is 'bool'");
    SXEA1(sxe_jitson_type_register_number() == SXE_JITSON_TYPE_NUMBER,            "Type 3 is 'number'");
    SXEA1(sxe_jitson_type_register_string() == SXE_JITSON_TYPE_STRING,            "Type 4 is 'string'");
    SXEA1(sxe_jitson_type_register_array()  == SXE_JITSON_TYPE_ARRAY,             "Type 5 is 'array'");
    SXEA1(sxe_jitson_type_register_object() == SXE_JITSON_TYPE_OBJECT,            "Type 6 is 'object'");
}

/**
 * Free a jitson object
 */
void
sxe_jitson_free(struct sxe_jitson *jitson)
{
    if (jitson)
        types[sxe_jitson_get_type(jitson)].free(jitson);
}

/**
 * Test a jitson object
 */
bool
sxe_jitson_test(struct sxe_jitson *jitson)
{
    return types[sxe_jitson_get_type(jitson)].test(jitson);
}

/**
 * Determine the size in jitson tokens of a jitson object
 */
uint32_t
sxe_jitson_size(struct sxe_jitson *jitson)
{
    return types[sxe_jitson_get_type(jitson)].size(jitson);
}

/**
 * Determine the length of a jitson object. For strings, this is the string length, for collections, the number of members.
 */
size_t
sxe_jitson_len(struct sxe_jitson *jitson)
{
    uint32_t type = sxe_jitson_get_type(jitson);
    SXEA1(types[type].len, "%s:Type %s does not support taking its length", __func__,  sxe_jitson_type_to_str(type));
    return types[type].len(jitson);
}

/**
 * Build a JSON string from a jitson object
 * 
 * @param jitson  The jitson object to encode in JSON
 * @param factory The factory object used to build the JSON string
 */
char *
sxe_jitson_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    return types[sxe_jitson_get_type(jitson)].build_json(jitson, factory);
}
