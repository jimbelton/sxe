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

/* SXE jitson stacks are factories for building sxe-jitson.
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

#define JITSON_STACK_INIT_SIZE 1       // The initial numer of tokens in a per thread stack
#define JITSON_STACK_MAX_INCR  4096    // The maximum the stack will grow by

// A per thread stack kept for parsing. Kept per thread for lockless thread safety. Automatically grows as needed.
static unsigned                          jitson_stack_init_size = JITSON_STACK_INIT_SIZE;
static __thread struct sxe_jitson_stack *jitson_stack           = NULL;

struct sxe_jitson_stack *
sxe_jitson_stack_new(unsigned init_size)
{
    struct sxe_jitson_stack *stack;

    SXEA1(SXE_JITSON_TOKEN_SIZE == 16, "Expected token size 16, got %zu", SXE_JITSON_TOKEN_SIZE);

    if (!(stack = MOCKFAIL(MOCK_FAIL_STACK_NEW_OBJECT, NULL, calloc(1, sizeof(*stack)))))
        return NULL;

    if (!(stack->jitsons = MOCKFAIL(MOCK_FAIL_STACK_NEW_JITSONS, NULL, malloc((size_t)init_size * sizeof(*stack->jitsons))))) {
        free(stack);
        return NULL;
    }

    stack->maximum = init_size;
    return stack;
}

/**
 * Extract the jitson parsed or constructed on a stack
 *
 * @note Aborts if there is no jitson on the stack or if there is a partially constructed one
 */
struct sxe_jitson *
sxe_jitson_stack_get_jitson(struct sxe_jitson_stack *stack)
{
    struct sxe_jitson *ret = stack->jitsons;

    SXEA1(stack->jitsons, "Can't get a jitson from an empty stack");
    SXEA1(!stack->open,   "Can't get a jitson there's an open collection");
    SXEE6("(stack=%p)", stack);

    if (stack->maximum > stack->count)
        ret = realloc(ret, stack->count * sizeof(*stack->jitsons)) ?: stack->jitsons;

    stack->jitsons = NULL;
    stack->count   = 0;
    ret->type     |= SXE_JITSON_TYPE_ALLOCED;    // The token at the base of the stack is marked as allocated

    SXER6("return %p; // type=%s", ret, ret ? sxe_jitson_type_to_str(sxe_jitson_get_type(ret)) : "NONE");
    return ret;
}

/**
 * Clear the content of a parse stack
 */
void
sxe_jitson_stack_clear(struct sxe_jitson_stack *stack)
{
    stack->count = 0;
}

struct sxe_jitson_stack *
sxe_jitson_stack_get_thread(void)
{
    if (!jitson_stack)
        jitson_stack = sxe_jitson_stack_new(jitson_stack_init_size);

    return jitson_stack;
}

void
sxe_jitson_stack_free(struct sxe_jitson_stack *stack)
{
    free(stack->jitsons);
    free(stack);
}

void
sxe_jitson_stack_free_thread(void)
{
    if (jitson_stack) {
        sxe_jitson_stack_free(jitson_stack);
        jitson_stack = NULL;
    }
}

/* Return the index of the first free slot on the stack, expanding it if needed.
 */
static unsigned
sxe_jitson_stack_next(struct sxe_jitson_stack *stack)
{
    if (stack->count >= stack->maximum) {
        unsigned new_maximum = stack->maximum + (stack->maximum < JITSON_STACK_MAX_INCR ? stack->maximum : JITSON_STACK_MAX_INCR);

        struct sxe_jitson *new_jitsons = MOCKFAIL(MOCK_FAIL_STACK_NEXT, NULL,
                                                  realloc(stack->jitsons, (size_t)new_maximum * sizeof(*stack->jitsons)));

        if (!new_jitsons)
            return SXE_JITSON_STACK_ERROR;

        stack->maximum = new_maximum;
        stack->jitsons = new_jitsons;    // If the array moved, point current into the new one.
    }
    else if (!stack->jitsons && !(stack->jitsons = MOCKFAIL(MOCK_FAIL_STACK_NEXT_AFTER_GET, NULL,
                                                            malloc(((size_t)stack->maximum * sizeof(*stack->jitsons))))))
        return SXE_JITSON_STACK_ERROR;

    return stack->count++;
}

static const char *
sxe_jitson_skip_whitespace(const char *json)
{
    while (isspace(*json))
        json++;

    return json;
}

static const char *
sxe_jitson_stack_parse_string(struct sxe_jitson_stack *stack, const char *json, bool is_member_name)
{
    if (*(json = sxe_jitson_skip_whitespace(json)) != '"') {
        errno = ENOMSG;
        return NULL;
    }

    unsigned index = sxe_jitson_stack_next(stack);

    if (index == SXE_JITSON_STACK_ERROR)
        return NULL;

    char               utf8[4];
    unsigned           i;
    unsigned           unicode = 0;
    struct sxe_jitson *jitson  = &stack->jitsons[index];
    jitson->type               = SXE_JITSON_TYPE_STRING | (is_member_name ? SXE_JITSON_TYPE_IS_KEY : 0);
    jitson->len                = 0;

    while (*++json != '"') {
        if (*json == '\0') {   // No terminating "
            errno = EINVAL;
            return NULL;
        }

        i = 1;

        if (*json == '\\')
            switch (*++json) {
            case '"':
            case '\\':
            case '/':
                jitson->string[jitson->len++] = *json;
                break;

            case 'b':
                jitson->string[jitson->len++] = '\b';
                break;

            case 'f':
                jitson->string[jitson->len++] = '\f';
                break;

            case 'n':
                jitson->string[jitson->len++] = '\n';
                break;

            case 'r':
                jitson->string[jitson->len++] = '\r';
                break;

            case 't':
                jitson->string[jitson->len++] = '\t';
                break;

            case 'u':
                for (i = 0; i < 4; i++)
                    switch (*++json) {
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        unicode = (unicode << 4) + *json - '0';
                        break;

                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                        unicode = (unicode << 4) + *json - 'a' + 10;
                        break;

                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                        unicode = (unicode << 4) + *json - 'A' + 10;
                        break;

                    default:
                        errno = EILSEQ;
                        return NULL;
                    }

                i = sxe_unicode_to_utf8(unicode, utf8);
                jitson->string[jitson->len++] = utf8[0];
                break;

            default:
                errno = EILSEQ;
                return NULL;
            }
        else
            jitson->string[jitson->len++] = *json;

        unsigned edge = jitson->len % SXE_JITSON_TOKEN_SIZE;

        if (edge >= SXE_JITSON_TOKEN_SIZE / 2 && edge < SXE_JITSON_TOKEN_SIZE / 2 + i) {
            if (sxe_jitson_stack_next(stack) == SXE_JITSON_STACK_ERROR)
                return NULL;

            jitson = &stack->jitsons[index];    // In case the jitsons were moved by realloc
        }

        if (i > 1) {    // For UTF8 strings > 1 character, copy the rest of the string
            memcpy(&jitson->string[jitson->len], &utf8[1], i - 1);
            jitson->len += i - 1;
        }
    }

    jitson->string[jitson->len] = '\0';
    return json + 1;
}

const char *
sxe_jitson_stack_parse_json(struct sxe_jitson_stack *stack, const char *json)
{
    double      sign  = 1.0;
    const char *token = NULL;
    char       *endptr;
    unsigned    index;

    if (*(json = sxe_jitson_skip_whitespace(json)) == '\0') {    // Nothing but whitespace
        errno = ENODATA;
        return NULL;
    }

    if ((index = sxe_jitson_stack_next(stack)) == SXE_JITSON_STACK_ERROR)    // Get an empty jitson
        return NULL;

    switch (*json) {
    case '"':    // It's a string
        stack->count--;    // Return the jitson just allocated. The parse_string function will get it back.
        return (json = sxe_jitson_stack_parse_string(stack, json, false));

    case '{':    // It's an object
        stack->jitsons[index].type = SXE_JITSON_TYPE_OBJECT;
        stack->jitsons[index].len  = 0;
        json = sxe_jitson_skip_whitespace(json + 1);

        if (*json == '}')    // If it's an empty object, return it
            return json + 1;

        do {
            if (!(json = sxe_jitson_stack_parse_string(stack, json, true)))    // Member name must be a string
                return NULL;

            json = sxe_jitson_skip_whitespace(json);

            if (*json != ':')
                goto INVALID;

            if (!(json = sxe_jitson_stack_parse_json(stack, json + 1)))    // Value can be any JSON value
                return NULL;

            stack->jitsons[index].len++;
            json = sxe_jitson_skip_whitespace(json);
        } while (*json++ == ',');

        if (*(json - 1) == '}') {
            stack->jitsons[index].integer = stack->count - index;    // Store the offset past the object
            return json;
        }

        goto INVALID;

    case '[':    // It's an array
        stack->jitsons[index].type = SXE_JITSON_TYPE_ARRAY;
        stack->jitsons[index].len  = 0;
        json = sxe_jitson_skip_whitespace(json + 1);

        if (*json == ']') {   // If it's an empty array, return it
            stack->jitsons[index].integer = 1;    // Offset past the empty array
            return json + 1;
        }

        do {
            if (!(json = sxe_jitson_stack_parse_json(stack, json)))    // Value can be any JSON value
                return NULL;

            stack->jitsons[index].len++;
            json = sxe_jitson_skip_whitespace(json);
        } while (*json++ == ',');

        if (*(json - 1) == ']') {
            stack->jitsons[index].integer = stack->count - index;    // Store the offset past the object
            return json;
        }

        goto INVALID;

    case '-':
        sign = -1.0;
        json++;
        /* FALL THRU */

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        token = json;

        while (isdigit(*(json + 1)))
            json ++;

        /* FALL THRU */

    case '0':
        token = token ?: json;    // In the non-fall thru case, point to the '0'
        json++;

        if (*json == '.') {    // If theres a fraction
            if (!isdigit(*++json))
                goto INVALID;

            while (isdigit(*json))
                json++;
        }

        if (*json == 'E' || *json == 'e') {    // If there's an exponent
            if (*++json == '-' || *json == '+')
                json++;

            if (!isdigit(*json))
                goto INVALID;

            while (isdigit(*json))
                json++;
        }

        stack->jitsons[index].type   = SXE_JITSON_TYPE_NUMBER;
        stack->jitsons[index].number = sign * strtod(token, &endptr);
        SXEA6(endptr == json, "strtod failed to parse '%.*s'", (int)(json - token), token);
        return json;

    case 'f':
        json = sxe_jitson_parse_identifier((token = json) + 1);

        if (json - token != sizeof("false") - 1 || memcmp(token + 1, "alse", sizeof("alse") - 1) != 0)
            goto INVALID;

        stack->jitsons[index].type    = SXE_JITSON_TYPE_BOOL;
        stack->jitsons[index].boolean = false;
        return json;

    case 'n':
        json = sxe_jitson_parse_identifier((token = json) + 1);

        if (json - token != sizeof("null") - 1 || memcmp(token + 1, "ull", sizeof("ull") - 1) != 0)
            goto INVALID;

        stack->jitsons[index].type = SXE_JITSON_TYPE_NULL;
        return json;

    case 't':
        json = sxe_jitson_parse_identifier((token = json) + 1);

        if (json - token != sizeof("true") - 1 || memcmp(token + 1, "rue", sizeof("rue") - 1) != 0)
            goto INVALID;

        stack->jitsons[index].type    = SXE_JITSON_TYPE_BOOL;
        stack->jitsons[index].boolean = true;
        return json;

    default:
        break;
    }

INVALID:
    errno = EINVAL;
    return NULL;
}

/**
 * Begin construction of an object on a stack
 *
 * @param type SXE_JITSON_TYPE_OBJECT or SXE_JITSON_TYPE_ARRAY
 *
 * @return true on success, false on allocation failure
 */
bool
sxe_jitson_stack_open_collection(struct sxe_jitson_stack *stack, uint32_t type)
{
    unsigned index;

    SXEA6(type == SXE_JITSON_TYPE_ARRAY || type == SXE_JITSON_TYPE_OBJECT, "Only arrays and objects can be constructed");

    if ((index = sxe_jitson_stack_next(stack)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    stack->jitsons[index].type               = type;
    stack->jitsons[index].len                = 0;
    stack->jitsons[index].partial.no_value   = false;
    stack->jitsons[index].partial.nested     = false;
    stack->jitsons[index].partial.collection = stack->open;
    stack->open                              = index + 1;
    return true;
}

/* Internal function that is use to add either a string to an oprm object or array or a member name to an open object
 */
static bool
sxe_jitson_stack_add_string_or_member_name(struct sxe_jitson_stack *stack, const char *name, uint32_t type)
{
    unsigned index;

    if ((index = sxe_jitson_stack_next(stack)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    stack->jitsons[stack->open - 1].partial.no_value = type & SXE_JITSON_TYPE_IS_KEY ? true : false;
    stack->jitsons[index].type                       = SXE_JITSON_TYPE_STRING | type;

    if (type & SXE_JITSON_TYPE_IS_REF) {    // Not a copy (a reference, possibly giving ownership to the object)
        stack->jitsons[index].reference = name;
        stack->jitsons[index].len       = 0;
        return true;
	}

    size_t len = strlen(name);

    if ((uint32_t)len != len) {
        errno = ENAMETOOLONG;    /* COVERAGE EXCLUSION: Member name > 4294967295 characters */
        return false;            /* COVERAGE EXCLUSION: Member name > 4294967295 characters */
    }

    stack->jitsons[index].len = len;

    if (len < SXE_JITSON_STRING_SIZE) {
        memcpy(stack->jitsons[index].string, name, len + 1);
        return true;
    }

    memcpy(stack->jitsons[index].string, name, SXE_JITSON_STRING_SIZE);

    for (name += SXE_JITSON_STRING_SIZE, len -= SXE_JITSON_STRING_SIZE; ; len -= SXE_JITSON_TOKEN_SIZE) {
        if ((index = sxe_jitson_stack_next(stack)) == SXE_JITSON_STACK_ERROR)
            return false;    /* COVERAGE EXCLUSION: Out of memory condition */

        if (len < SXE_JITSON_TOKEN_SIZE) {
            memcpy(&stack->jitsons[index], name, len + 1);
            return true;
        }

        memcpy(&stack->jitsons[index], name, SXE_JITSON_TOKEN_SIZE);
        name += SXE_JITSON_TOKEN_SIZE;
    }

    return true;
}

/**
 * Add a member name to the object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param name  The member name
 * @param type  SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_member_name(struct sxe_jitson_stack *stack, const char *name, uint32_t type)
{
    unsigned object;

    SXEA1(stack->open,                                    "Can't add a member name when there is no object under construction");
    SXEA1(stack->jitsons[object = stack->open - 1].type == SXE_JITSON_TYPE_OBJECT, "Member names can only be added to objects");
    SXEA1(!stack->jitsons[object].partial.no_value,                                "Member name already added without a value");
    SXEA1(!(type & ~(SXE_JITSON_TYPE_IS_REF | SXE_JITSON_TYPE_IS_OWN)),            "Unexected type flags 0x%x", (unsigned)type);

    return sxe_jitson_stack_add_string_or_member_name(stack, name, type | SXE_JITSON_TYPE_IS_KEY);
}

/**
 * Add a string to the object or array being constructed on the stack
 *
 * @param stack The jitson stack
 * @param name  The member name
 * @param type  SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_string(struct sxe_jitson_stack *stack, const char *name, uint32_t type)
{
    unsigned collection = stack->open - 1;
    bool     ret;

    SXEA1(stack->open, "Can't add a value when there is no array or object under construction");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY || stack->jitsons[collection].partial.no_value,
          "Member name must be added added to an object before adding a string value");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_OBJECT || stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY,
          "Strings can only be added to arrays or objects");
    SXEA1(!(type & ~(SXE_JITSON_TYPE_IS_REF | SXE_JITSON_TYPE_IS_OWN)), "Unexected type flags 0x%x", (unsigned)type);

    if ((ret = sxe_jitson_stack_add_string_or_member_name(stack, name, type)))
        stack->jitsons[collection].len++;

    return ret;
}

static unsigned
sxe_jitson_stack_add_value(struct sxe_jitson_stack *stack)
{
    unsigned collection = stack->open - 1;
    unsigned index;

    SXEA1(stack->open, "Can't add a value when there is no array or object under construction");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY || stack->jitsons[collection].partial.no_value,
          "Member name must be added added to an object before adding a value");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_OBJECT || stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY,
          "Values can only be added to arrays or objects");

    if ((index = sxe_jitson_stack_next(stack)) == SXE_JITSON_STACK_ERROR)
        return SXE_JITSON_STACK_ERROR;    /* COVERAGE EXCLUSION: Out of memory condition */

    stack->jitsons[collection].len++;
    stack->jitsons[collection].partial.no_value = false;
    return index;
}

/**
 * Add a null value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_null(struct sxe_jitson_stack *stack)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    sxe_jitson_make_null(&stack->jitsons[index]);
    return true;
}

/**
 * Add a boolean value to the array or object being constructed on the stack
 *
 * @param stack   The jitson stack
 * @param boolean The boolean value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_bool(struct sxe_jitson_stack *stack, bool boolean)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    sxe_jitson_make_bool(&stack->jitsons[index], boolean);
    return true;
}

/**
 * Add a number to the array or object being constructed on the stack
 *
 * @param stack  The jitson stack
 * @param number The numeric value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_number(struct sxe_jitson_stack *stack, double number)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    sxe_jitson_make_number(&stack->jitsons[index], number);
    return true;
}
/**
 * Finish construction of an object or array on a stack
 *
 * @note Aborts if the object is not a collection under construction, has an incomplete nested object, or is an object and has a
 *       member name without a matching value.
 */
void
sxe_jitson_stack_close_collection(struct sxe_jitson_stack *stack)
{
    unsigned index;

    SXEA1(stack->open, "There must be an open collection on the stack");
    index = stack->open - 1;
    SXEA1(!stack->jitsons[index].partial.no_value, "Index %u is an object with a member name with no value", index);
    SXEA1(!stack->jitsons[index].partial.nested,   "Index %u is a collection with a nested open collection", index);

    stack->open                   = stack->jitsons[index].partial.collection;
    stack->jitsons[index].integer = stack->count - index;    // Store the offset past the object or array
}
