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
    struct sxe_jitson *member;
    unsigned           len;

    plan_tests(102);

    diag("Memory allocation failure tests");
    {
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_OBJECT);
        is(test_jitson_new("0", 0), NULL, "Failed to allocate the thread stack object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_JITSONS);
        is(test_jitson_new("0", 0), NULL, "Failed to allocate the thread stack's initial jitsons");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_GET_JITSON);
        is(test_jitson_new("0", 0), NULL, "Failed to allocate a new stack after getting the parsed object");
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

        test_jitson_new("0", 0);
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(test_jitson_new("{\"x\": 0}", 0), NULL, "Failed to realloc the thread stack's jitsons on string inside an object");
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

        char json[65562];    // Big enough for an object with a > 64K member name
        json[len = 0] = '{';
        json[++len]   = '"';

        while (len < 65538)
            json[++len] = 'm';

        json[++len] = '"';
        ok(!test_jitson_new(json, 0), "Failed to parse member name of 64K chanracters (error %s)", strerror(errno));
        errno = 0;
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
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_MEMBER),  "MEMBER",  "MEMBER internal type");
    }

    diag("Test membership function");
    {
        ok(jitson = test_jitson_new("{\"a\": 1, \"biglongname\": \"B\", \"c\": [2, 3], \"d\" : {\"e\": 4}, \"f\": true}", 0),
           "Parsed complex object (error %s)", strerror(errno));
        ok(member = sxe_jitson_object_get_member(jitson, "a", 0),           "Object has a member 'a'");
        is(sxe_jitson_get_number(member), 1,                                "Member is the number 1");
        ok(!sxe_jitson_object_get_member(jitson, "biglongname", 1),         "Object has no member 'b'");
        ok(member = sxe_jitson_object_get_member(jitson, "biglongname", 0), "Object has a member 'biglongname'");
        is_eq(sxe_jitson_get_string(member, NULL), "B",                     "Member is the string 'B'");
        ok(member = sxe_jitson_object_get_member(jitson, "c", 0),           "Object has a member 'c'");
        is(sxe_jitson_get_size(member), 2,                                  "Member is an array of 2 elements");
        ok(member = sxe_jitson_object_get_member(jitson, "d", 1),           "Object has a member 'd'");
        is(sxe_jitson_get_size(member), 1,                                  "Member is an object with 1 member");
        ok(member = sxe_jitson_object_get_member(jitson, "f", 0),           "Object has a member 'f'");
        ok(sxe_jitson_get_bool(member),                                     "Member is 'true'");
        is(sxe_jitson_get_size(member), 0,                                  "Can't take the size of a number, bool, or null");
        sxe_jitson_free(jitson);
    }

    diag("Test array element function");
    {
        struct sxe_jitson *element;

        ok(jitson = test_jitson_new("[0, \"anotherlongstring\", {\"member\": null}, true]", 0),
           "Parsed complex array (error %s)", strerror(errno));
        ok(element = sxe_jitson_array_get_element(jitson, 0),            "Array has a element 0");
        is(sxe_jitson_get_number(element), 0,                            "Element is the number 0");
        ok(element = sxe_jitson_array_get_element(jitson, 1),            "Array has a element 1");
        is_eq(sxe_jitson_get_string(element, NULL), "anotherlongstring", "Element is the string 'anotherlongstring'");
        ok(element = sxe_jitson_array_get_element(jitson, 2),            "Array has a element 2");
        is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_OBJECT,         "Elememt is an object");
        ok(element = sxe_jitson_array_get_element(jitson, 3),            "Array has a element 3");
        ok(sxe_jitson_get_bool(element),                                  "Element is 'true'");
        ok(!sxe_jitson_array_get_element(jitson, 4),                     "Object has no element 4");;
        sxe_jitson_free(jitson);
    }

    diag("Test bug fixes against regressions");
    {
         ok(jitson = test_jitson_new("{\"A.D.\": 1, \"x\": 0}", 0),   "Parsed problem member name (error %s)", strerror(errno));
         ok(member = sxe_jitson_object_get_member(jitson, "A.D.", 0), "Object has a member 'A.D.'");
         is(sxe_jitson_get_type(member), SXE_JITSON_TYPE_NUMBER,      "A.D.'s value is a number");
    }

    sxe_jitson_stack_free_thread();    // Currently, just for coverage
    free(json_mutable);
    return exit_status();
}
