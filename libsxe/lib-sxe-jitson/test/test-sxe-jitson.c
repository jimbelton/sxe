#include <errno.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <tap.h>

#include "mockfail.h"
#include "sxe-jitson.h"

static char    *json_mutable = NULL;
static unsigned json_size    = 0;

static struct sxe_jitson *
test_jitson_new(const char *json, unsigned size)
{
    if (!size)
        size = strlen(json);

    if (json_mutable)
        if (json_size < size) {
            free(json_mutable);
            json_mutable = NULL;
        }

    if (!json_mutable) {
        json_mutable = malloc(size + 1);
        json_size    = size;
    }

    memcpy(json_mutable, json, size);
    json_mutable[size] = '\0';
    return sxe_jitson_new(json_mutable);
}

int
main(void)
{
    struct sxe_jitson *jitson;
    unsigned           len;

    plan_tests(73);

    diag("Memory allocation failure tests");
    {
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_OBJECT);
        is(test_jitson_new("0", 0), NULL, "Failed to allocate the thread stack object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_JITSONS);
        is(test_jitson_new("0", 0), NULL, "Failed to allocate the thread stack's initial jitsons");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_DUP);
        is(test_jitson_new("0", 0), NULL, "Failed to dup the parsed object from the thread stack");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(test_jitson_new("{\"one\":1}", 0), NULL, "Failed to realloc the thread stack's jitsons on a string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(test_jitson_new("\"01234567\"", 0), NULL, "Failed to realloc the thread stack's jitsons on a long string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(test_jitson_new("[0,1]", 0), NULL, "Failed to realloc the thread stack's jitsons");
        MOCKFAIL_END_TESTS();
    }

    diag("Happy path parsing");
    {
        ok(jitson = test_jitson_new("0", 0),                        "Parsed '0' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),     SXE_JITSON_TYPE_NUMBER, "'0' is a number");
        ok(sxe_jitson_get_number(jitson) == 0.0 ,                   "Value %f == 0.0", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new(" 666\t", 0),                   "Parsed ' 666\\t' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),     SXE_JITSON_TYPE_NUMBER, "'666' is a number");
        ok(sxe_jitson_get_number(jitson) == 666.0,                  "Value %f == 666.0", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new(" -0.1", 0),                    "Parsed '-0.1' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),     SXE_JITSON_TYPE_NUMBER, "'-0.1'' is a number");
        ok(sxe_jitson_get_number(jitson) == -0.1,                   "Value %f == -0.1", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("1E-100", 0),                   "Parsed '1E=100' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),     SXE_JITSON_TYPE_NUMBER, "1E100' is a number");
        ok(sxe_jitson_get_number(jitson) == 1E-100,                 "Value %f == 1E100", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("\"\"", 0),                            "Parsed '\"\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "",                     "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new(" \"x\"\n", 0),                        "Parsed ' \"x\"\\n' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "' \"x\"\\n' is a string");
        is_eq(sxe_jitson_get_string(jitson, &len), "x",                    "Correct value");
        is(len,                                    1,                      "Correct length");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", 0),  "Parsed '\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' (error %s)",
           strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "\"\\/\b\f\n\r\t",      "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("\"\\u20aC\"", 0),                     "Parsed '\"\\u20aC\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\\u20aC\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "\xE2\x82\xAC",         "Correct UTF-8 value");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new(" {\t} ", 0),               "Parsed ' {\\t} ' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "' {\\t} ' is an object" );
        is(jitson->size,                0,                      "Correct size");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("{\"key\":\"value\"}", 0),  "Parsed '{\"key\":\"value\"}' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "'{\"key\":\"value\"}' is an object");
        is(jitson->size,                1,                      "Correct size");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("[1, 2,4]", 0),             "Parsed '[1, 2,4]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[1, 2,4]' is an array" );
        is(jitson->size,                3,                      "Correct size");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("[]", 0),                  "Parsed '[]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[]' is an array" );
        is(jitson->size,                0,                     "Correct size");
        sxe_jitson_free(jitson);
    }

    diag("Test identifiers and parsing of terminals");
    {
        char         test_id[2] = "\0";
        const char * next;
        int          i;

        for (i = -128; i <= 127; i++) {
            test_id[0] = (char)i;
            next = sxe_jitson_parse_identifier(test_id);

            if (i == '.' || i == '_' || (i >= '0' && i <= '9') || (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z')) {
                if (next == test_id)
                    break;
            } else if (next != test_id)
                break;
        }

        is(i, 128, "All identifier characters are correctly classified");

        ok(jitson = test_jitson_new("true", 0),                 "Parsed 'true' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_BOOL,   "'true' is a boolean" );
        is(sxe_jitson_get_bool(jitson), true,                   "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("false", 0),                "Parsed 'false' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_BOOL,   "'false' is a boolean" );
        is(sxe_jitson_get_bool(jitson), false,                  "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = test_jitson_new("null", 0),                "Parsed 'null' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NULL,  "'null' is the null type" );
        sxe_jitson_free(jitson);
    }

    diag("Cover edge cases of paraing");
    {
        ok(!test_jitson_new("{0:1}", 0),    "Failed to parse non-string key '{0:1}' (error %s)",           strerror(errno));
        ok(!test_jitson_new("\"", 0),       "Failed to parse unterminated string '\"' (error %s)",         strerror(errno));
        ok(!test_jitson_new("\"\\", 0),     "Failed to parse unterminated escape '\"\\' (error %s)",       strerror(errno));
        ok(!test_jitson_new("\"\\u", 0),    "Failed to parse truncated unicode escape '\"\\u' (error %s)", strerror(errno));
        ok(!test_jitson_new("", 0),         "Failed to parse empty string '' (error %s)",                  strerror(errno));
        ok(!test_jitson_new("{\"k\"0}", 0), "Failed to parse object missing colon '{\"k\"0}' (error %s)",  strerror(errno));
        ok(!test_jitson_new("{\"k\":}", 0), "Failed to parse object missing value '{\"k\":}' (error %s)",  strerror(errno));
        ok(!test_jitson_new("{\"k\":0", 0), "Failed to parse object missing close '{\"k\":0' (error %s)",  strerror(errno));
        ok(!test_jitson_new("[0", 0),       "Failed to parse array missing close '[0' (error %s)",         strerror(errno));
        ok(!test_jitson_new("0.", 0),       "Failed to parse invalid fraction '0.' (error %s)",            strerror(errno));
        ok(!test_jitson_new("1.0E", 0),     "Failed to parse invalid exponent '1.0E' (error %s)",          strerror(errno));
        ok(!test_jitson_new("fathead", 0),  "Failed to parse invalid token 'fathead' (error %s)",          strerror(errno));
        ok(!test_jitson_new("n", 0),        "Failed to parse invalid token 'n' (error %s)",                strerror(errno));
        ok(!test_jitson_new("twit", 0),     "Failed to parse invalid token 'twit' (error %s)",             strerror(errno));
    }

    diag("Cover type to string");
    {
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_INVALID), "INVALID", "INVALID type");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NUMBER),  "NUMBER",  "NUMBER");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_STRING),  "STRING",  "STRING");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_OBJECT),  "OBJECT",  "OBJECT");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_ARRAY),   "ARRAY",   "ARRAY");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_BOOL),    "BOOL",    "BOOL");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NULL),    "NULL",    "NULL");
    }

    sxe_jitson_stack_free_thread();    // Currently, just for coverage
    free(json_mutable);
    return exit_status();
}
