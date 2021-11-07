#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mockfail.h"
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

    SXEA1(sizeof(struct sxe_jitson) == SXE_JITSON_TOKEN_SIZE, "Expected token size %u, got %zu",
          SXE_JITSON_TOKEN_SIZE, sizeof(struct sxe_jitson));

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

    if (!(stack->jitsons = MOCKFAIL(MOCK_FAIL_STACK_GET_JITSON, NULL,
                                    malloc(((size_t)stack->maximum * sizeof(*stack->jitsons)))))) {
        stack->jitsons = ret;
        return NULL;
    }

    stack->count = 0;
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

    return stack->count++;
}

static char *
sxe_jitson_skip_whitespace(char *json)
{
    while (isspace(*json))
        json++;

    return json;
}

static char *
sxe_jitson_stack_parse_string(struct sxe_jitson_stack *stack, char *json)
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
    jitson->type               = SXE_JITSON_TYPE_STRING;
    jitson->size               = 0;

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
    return json + 1;
}

static uint64_t identifier_chars[4] = {0x03FF400000000000, 0x07FFFFFE87FFFFFE, 0x0000000000000000, 0x00000000000000};

/**
 * Parse identifier characters until a non-identifier character is reached. Can be called after parsing the first character.
 *
 * @param json String to parse
 *
 * @return Pointer to first non-identifier character in json
 */
char *
sxe_jitson_parse_identifier(char *json)
{
    unsigned c;

    for (c = (unsigned char)*json; identifier_chars[c >> 6] & (1UL << (c & 0x3f)); c = (unsigned char)*json)
        json++;

    return json;
}

char *
sxe_jitson_stack_parse_json(struct sxe_jitson_stack *stack, char *json)
{
    double   sign  = 1.0;
    char    *token = NULL;
    char    *endptr;
    unsigned index;

    if (*(json = sxe_jitson_skip_whitespace(json)) == '\0') {    // Nothing but whitespace
        errno = ENODATA;
        return NULL;
    }

    if ((index = sxe_jitson_stack_next(stack)) == SXE_JITSON_STACK_ERROR)    // Get an empty jitson
        return NULL;

    switch (*json) {
    case '"':    // It's a string
        stack->count--;    // Return the jitson just allocated. The parse_string function will get it back.
        return (json = sxe_jitson_stack_parse_string(stack, json));

    case '{':    // It's an object
        stack->jitsons[index].type = SXE_JITSON_TYPE_OBJECT;
        stack->jitsons[index].size = 0;
        json = sxe_jitson_skip_whitespace(json + 1);

        if (*json == '}')    // If it's an empty object, return it
            return json + 1;

        do {
            if (!(json = sxe_jitson_stack_parse_string(stack, json)))    // Key must be a string
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

/**
 * Allocate a jitson object and parse a JSON string into it.
 *
 * @param json A '\0' terminated JSON string. This string must be mutable, and will be modified by jitson.
 *
 * @return A jitson object or NULL with errno ENOMEM, EINVAL, EILSEQ, EMSGSIZE, ENODATA, ENOTUNIQ, or EOVERFLOW)
 */
struct sxe_jitson *
sxe_jitson_new(char *json)
{
    struct sxe_jitson_stack *stack = sxe_jitson_stack_get_thread();

    if (!stack)
        return NULL;

    if (!sxe_jitson_stack_parse_json(stack, json)) {
        sxe_jitson_stack_clear(jitson_stack);
        return NULL;
    }

    return sxe_jitson_stack_get_jitson(jitson_stack);
}

void
sxe_jitson_free(struct sxe_jitson *jitson)
{
    if (jitson)
        free(jitson);
}

unsigned
sxe_jitson_get_type(struct sxe_jitson *jitson)
{
    return jitson ? jitson->type : SXE_JITSON_TYPE_INVALID;
}

const char *
sxe_jitson_type_to_str(unsigned type)
{
    switch (type) {
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
sxe_jitson_get_number(struct sxe_jitson *jitson)
{
    SXEA6(jitson->type == SXE_JITSON_TYPE_NUMBER, "Can't get the numeric value of a %s", sxe_jitson_type_to_str(jitson->type));
    return jitson->number;
}

/**
 * Get the string value of a jitson whose type is SXE_JITSON_TYPE_STRING
 *
 * return The string value as a double; if the jitson is not an integer, results are undefined
 */
const char *
sxe_jitson_get_string(struct sxe_jitson *jitson, unsigned *size)
{
    SXEA6(jitson->type == SXE_JITSON_TYPE_STRING, "Can't get the string value of a %s", sxe_jitson_type_to_str(jitson->type));

    if (size)
        *size = jitson->size;

    return jitson->string;
}

/**
 * Get the boolen value of a jitson whose type is SXE_JITSON_TYPE_BOOL
 *
 * return The bool value; if the jitson is not an boolean, results are undefined
 */
bool
sxe_jitson_get_bool(struct sxe_jitson *jitson)
{
    SXEA6(jitson->type == SXE_JITSON_TYPE_BOOL, "Can't get the boolean value of a %s", sxe_jitson_type_to_str(jitson->type));
    return jitson->boolean;
}

unsigned
sxe_jitson_get_size(struct sxe_jitson *jitson)
{
    switch (jitson->type) {
    case SXE_JITSON_TYPE_STRING:
    case SXE_JITSON_TYPE_OBJECT:
    case SXE_JITSON_TYPE_ARRAY:
        return jitson->size;
    }

    SXEL2("JSON type %s has no size", sxe_jitson_type_to_str(jitson->type));
    return 0;
}

static struct sxe_jitson *
sxe_jitson_skip(struct sxe_jitson *jitson)
{
    switch (jitson->type) {
    case SXE_JITSON_TYPE_NUMBER:
    case SXE_JITSON_TYPE_BOOL:
    case SXE_JITSON_TYPE_NULL:
        return jitson + 1;

    case SXE_JITSON_TYPE_STRING:
        return jitson + 1 + (jitson->size + SXE_JITSON_TOKEN_SIZE / 2) / SXE_JITSON_TOKEN_SIZE;

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

    SXEA1(jitson->type == SXE_JITSON_TYPE_OBJECT, "Can't get a member value from a %s", sxe_jitson_type_to_str(jitson->type));
    member = jitson + 1;
    len    = len ?: strlen(name);    // Determine the length of the member name if not provided

    for (i = 0; i < jitson->size; i++) {
        if (member->size == len && memcmp(member->string, name, len) == 0)
            return sxe_jitson_skip(member);    // Skip the member name, returning the value

        member = sxe_jitson_skip(member);    // Skip the member name
        member = sxe_jitson_skip(member);    // Skip the member value
    }

    return NULL;
}

struct sxe_jitson *
sxe_jitson_array_get_element(struct sxe_jitson *jitson, unsigned idx)
{
    struct sxe_jitson *element;
    unsigned           i;

    SXEA1(jitson->type == SXE_JITSON_TYPE_ARRAY, "Can't get an element value from a %s", sxe_jitson_type_to_str(jitson->type));

    if (idx >= jitson->size) {
        SXEL2("Array element %u is not less than size %u", idx, jitson->size);
        return NULL;
    }

    element = jitson + 1;

    for (i = 0; i < idx; i++)
        element = sxe_jitson_skip(element);    // Skip the elemeent

    return element;
}
