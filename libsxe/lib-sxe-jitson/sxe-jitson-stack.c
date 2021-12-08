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

struct sxe_jitson *
sxe_jitson_stack_get_jitson(struct sxe_jitson_stack *stack)
{
    struct sxe_jitson *ret = stack->jitsons;

    if (stack->maximum > stack->count)
        ret = realloc(ret, stack->count * sizeof(*stack->jitsons)) ?: stack->jitsons;

    stack->jitsons = NULL;
    stack->count   = 0;
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
    jitson->type               = is_member_name ? SXE_JITSON_TYPE_MEMBER     : SXE_JITSON_TYPE_STRING;
    jitson->size               = is_member_name ? SXE_JITSON_MEMBER_LEN_SIZE : 0;

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
                jitson->string[jitson->size++] = *json;
                break;

            case 'b':
                jitson->string[jitson->size++] = '\b';
                break;

            case 'f':
                jitson->string[jitson->size++] = '\f';
                break;

            case 'n':
                jitson->string[jitson->size++] = '\n';
                break;

            case 'r':
                jitson->string[jitson->size++] = '\r';
                break;

            case 't':
                jitson->string[jitson->size++] = '\t';
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
                jitson->string[jitson->size++] = utf8[0];
                break;

            default:
                errno = EILSEQ;
                return NULL;
            }
        else
            jitson->string[jitson->size++] = *json;

        unsigned edge = jitson->size % SXE_JITSON_TOKEN_SIZE;

        if (edge >= SXE_JITSON_TOKEN_SIZE / 2 && edge < SXE_JITSON_TOKEN_SIZE / 2 + i) {
            if (sxe_jitson_stack_next(stack) == SXE_JITSON_STACK_ERROR)
                return NULL;

            jitson = &stack->jitsons[index];    // In case the jitsons were moved by realloc
        }

        if (i > 1) {    // For UTF8 strings > 1 character, copy the rest of the string
            memcpy(&jitson->string[jitson->size], &utf8[1], i - 1);
            jitson->size += i - 1;
        }
    }

    jitson->string[jitson->size] = '\0';

    if (is_member_name) {
        if ((uint16_t)jitson->size != jitson->size) {
            errno = ENAMETOOLONG;
            return NULL;
        }

        jitson->member.len = jitson->size - SXE_JITSON_MEMBER_LEN_SIZE;
        jitson->size       = 1 + (SXE_JITSON_TOKEN_SIZE - SXE_JITSON_MEMBER_NAME_SIZE + jitson->member.len) / SXE_JITSON_TOKEN_SIZE;
    }

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
        stack->jitsons[index].size = 0;
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

            stack->jitsons[index].size++;
            json = sxe_jitson_skip_whitespace(json);
        } while (*json++ == ',');

        if (*(json - 1) == '}') {
            stack->jitsons[index].integer = stack->count - index;    // Store the offset past the object
            return json;
        }

        goto INVALID;

    case '[':    // It's an array
        stack->jitsons[index].type = SXE_JITSON_TYPE_ARRAY;
        stack->jitsons[index].size = 0;
        json = sxe_jitson_skip_whitespace(json + 1);

        if (*json == ']') {   // If it's an empty array, return it
            stack->jitsons[index].integer = 1;    // Offset past the empty array
            return json + 1;
        }

        do {
            if (!(json = sxe_jitson_stack_parse_json(stack, json)))    // Value can be any JSON value
                return NULL;

            stack->jitsons[index].size++;
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
