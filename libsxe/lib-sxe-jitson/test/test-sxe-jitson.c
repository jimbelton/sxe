#include <errno.h>
#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <tap.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-jitson.h"
#include "sxe-test-memory.h"

int
main(void)
{
    struct sxe_jitson_stack *stack;
    struct sxe_jitson       *clone, *jitson;      // Constructed jitson values are returned as non-const
    const struct sxe_jitson *element, *member;    // Accessed jitson values are returned as const
    char                    *json_out;
    size_t                   len;
    size_t                   start_memory = test_memory();

    plan_tests(258);

    ok(!sxe_jitson_is_init(), "Not yet initialized");
    sxe_jitson_type_init(0, 0);    // Initialize the JSON types, and don't enable hexadecimal
    ok(sxe_jitson_is_init(),  "Now initialized");

    diag("Memory allocation failure tests");
    {
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_OBJECT);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate the thread stack object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_JITSONS);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate the thread stack's initial jitsons");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("{\"one\":1}"), NULL, "Failed to realloc the thread stack's jitsons on a string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("\"01234567\""), NULL, "Failed to realloc the thread stack's jitsons on a long string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("[0,1]"), NULL, "Failed to realloc the thread stack's jitsons");
        MOCKFAIL_END_TESTS();

        jitson = sxe_jitson_new("999999999");
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND_AFTER_GET);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate a new stack after getting the parsed object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("{\"x\": 0}"), NULL, "Failed to realloc the thread stack's jitsons on string inside an object");
        MOCKFAIL_END_TESTS();

        /* Don't do this at the beginning of the program (need to test failure to allocate above)
         */
        stack = sxe_jitson_stack_get_thread();

        MOCKFAIL_START_TESTS(6, MOCK_FAIL_STACK_EXPAND);
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY), "Opened an array on the stack");
        ok(!sxe_jitson_stack_add_null(stack),              "Failed to realloc to add null to an open array");
        ok(!sxe_jitson_stack_add_bool(stack, true),        "Failed to realloc to add true to an open array");
        ok(!sxe_jitson_stack_add_number(stack, 0.0),       "Failed to realloc to add 0.0 to an open array");
        ok(!sxe_jitson_stack_add_reference(stack, jitson), "Failed to realloc to add a reference to an open array");
        ok(!sxe_jitson_stack_add_dup(stack, jitson),       "Failed to realloc to add a duplicate to an open array");
        sxe_jitson_stack_clear(stack);
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, sxe_factory_reserve);
        is(sxe_jitson_to_json(jitson, NULL), NULL, "Failed to encode large to JSON on realloc failure");
        MOCKFAIL_END_TESTS();
        sxe_jitson_free(jitson);
    }

    diag("Happy path parsing");
    {
        ok(jitson = sxe_jitson_new("0"),                        "Parsed '0' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'0' is a number");
        ok(sxe_jitson_get_number(jitson) == 0.0 ,               "Value %f == 0.0", sxe_jitson_get_number(jitson));
        is(sxe_jitson_get_uint(jitson), 0,                      "Value %"PRIu64" == 0", sxe_jitson_get_uint(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" 666\t"),                   "Parsed ' 666\\t' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'666' is a number");
        ok(sxe_jitson_get_number(jitson) == 666.0,              "Value %f == 666.0", sxe_jitson_get_number(jitson));
        is(sxe_jitson_get_uint(jitson), 666,                    "Value %"PRIu64" == 666", sxe_jitson_get_uint(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" -0.1"),                    "Parsed '-0.1' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'-0.1'' is a number");
        ok(sxe_jitson_get_number(jitson) == -0.1,               "Value %f == -0.1", sxe_jitson_get_number(jitson));
        is(errno, 0,                                            "Error = '%s'", strerror(errno));
        is(sxe_jitson_get_uint(jitson), ~0ULL,                  "Value %"PRIu64" == ~OULL", sxe_jitson_get_uint(jitson));
        is(errno, EOVERFLOW,                                    "Error = '%s'", strerror(errno));
        sxe_jitson_free(jitson);
        errno = 0;

        ok(jitson = sxe_jitson_new("1E-100"),                   "Parsed '1E=100' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "1E100' is a number");
        ok(sxe_jitson_get_number(jitson) == 1E-100,             "Value %f == 1E100", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("0xDEADBEEF"),               "Parsed '0xDEADBEEF' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEF' is parsed as a number");
        is(sxe_jitson_get_uint(jitson), 0,                      "0x* == 0 (hex not enabled)");
        sxe_jitson_free(jitson);

        sxe_jitson_flags |= SXE_JITSON_FLAG_ALLOW_HEX;
        ok(jitson = sxe_jitson_new("0xDEADBEEF"),               "Parsed '0xDEADBEEF' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEF' is a number");
        is(sxe_jitson_get_uint(jitson), 0xDEADBEEF,             "0x%"PRIx64" == 0xDEADBEEF", sxe_jitson_get_uint(jitson));
        is(sxe_jitson_get_number(jitson), (double)0xDEADBEEF,   "%f == (double)0xDEADBEEF", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("0xDEADBEEFDEADBEEF"),       "Parsed '0xDEADBEEFDEADBEEF' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEFDEADBEEF' is a number");
        is(sxe_jitson_get_uint(jitson), 0xDEADBEEFDEADBEEF,     "0x%"PRIx64" == 0xDEADBEEFDEADBEEF", sxe_jitson_get_uint(jitson));
        ok(isnan(sxe_jitson_get_number(jitson)),                "%f == NAN", sxe_jitson_get_number(jitson));
        is(errno, EOVERFLOW,                                    "Error = %s", strerror(errno));
        sxe_jitson_free(jitson);
        errno = 0;

        ok(jitson = sxe_jitson_new("0xDEADBEEFDEADBEEFDEAD"),   "Parsed '0xDEADBEEFDEADBEEFDEAD' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEFDEADBEEFDEAD' is a number");
        is(sxe_jitson_get_uint(jitson), 0xFFFFFFFFFFFFFFFF,     "0x%"PRIx64" == 0xFFFFFFFFFFFFFFFF", sxe_jitson_get_uint(jitson));
        is(errno, ERANGE,                                       "Error = %s", strerror(errno));
        sxe_jitson_free(jitson);
        errno = 0;

        ok(jitson = sxe_jitson_new("\"\""),                                "Parsed '\"\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "",                     "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" \"x\"\n"),                            "Parsed ' \"x\"\\n' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "' \"x\"\\n' is a string");
        is_eq(sxe_jitson_get_string(jitson, &len), "x",                    "Correct value");
        is(len,                                    1,                      "Correct length");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\""),      "Parsed '\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' (error %s)",
           strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "\"\\/\b\f\n\r\t",      "Correct value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "\"\\u0022\\u005c/\\u0008\\u000c\\u000a\\u000d\\u0009\"",
              "Control characters are correctly encoded");
        sxe_jitson_free(jitson);
        free(json_out);

        ok(jitson = sxe_jitson_new("\"\\u20aC\""),                          "Parsed '\"\\u20aC\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),             SXE_JITSON_TYPE_STRING, "'\"\\u20aC\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL),  "\xE2\x82\xAC",         "Correct UTF-8 value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "\"\xE2\x82\xAC\"", "Valid UTC code points are not escaped");
        sxe_jitson_free(jitson);
        free(json_out);

        ok(jitson = sxe_jitson_new(" {\t} "),                   "Parsed ' {\\t} ' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "' {\\t} ' is an object" );
        is(sxe_jitson_len(jitson),      0,                      "Correct len");
        is(sxe_jitson_size(jitson),     1,                      "Correct size");                       // Test DPT-1404b
        ok(!sxe_jitson_test(jitson),                            "Empty objects test false");
        is(sxe_jitson_object_get_member(jitson, "x", 1), NULL,  "Search for any member will fail");    // Test DPT-1404a
        is_eq(sxe_jitson_to_json(jitson, NULL), "{}",           "Encoded back to JSON correctly");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("{\"key\":\"value\"}"),      "Parsed '{\"key\":\"value\"}' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "'{\"key\":\"value\"}' is an object");
        is(sxe_jitson_len(jitson),      1,                      "Correct len");
        is(sxe_jitson_size(jitson),     3,                      "Correct size");
        ok(sxe_jitson_test(jitson),                             "Nonempty objects test true");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[1, 2,4]"),                "Parsed '[1, 2,4]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[1, 2,4]' is an array" );
        is(sxe_jitson_len(jitson),      3,                     "Correct len");
        ok(sxe_jitson_test(jitson),                            "Nonempty arrays test true");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[]"),                      "Parsed '[]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[]' is an array" );
        is(sxe_jitson_len(jitson),      0,                     "Correct len");
        ok(!sxe_jitson_test(jitson),                           "Empty arrays test false");
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

        ok(jitson = sxe_jitson_new("true"),                   "Parsed 'true' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_BOOL, "'true' is a boolean" );
        is(sxe_jitson_get_bool(jitson), true,                 "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("false"),                          "Parsed 'false' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),         SXE_JITSON_TYPE_BOOL, "'false' is a boolean" );
        is(sxe_jitson_get_bool(jitson),         false,                "Correct value");
        is_eq(sxe_jitson_to_json(jitson, NULL), "false",              "Encoded back to JSON correctly");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("null"),                   "Parsed 'null' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NULL, "'null' is the null type" );
        sxe_jitson_free(jitson);
    }

    diag("Cover edge cases of paraing");
    {
        ok(!sxe_jitson_new("{0:1}"),    "Failed to parse non-string key '{0:1}' (error %s)",           strerror(errno));
        ok(!sxe_jitson_new("\""),       "Failed to parse unterminated string '\"' (error %s)",         strerror(errno));
        ok(!sxe_jitson_new("\"\\"),     "Failed to parse unterminated escape '\"\\' (error %s)",       strerror(errno));
        ok(!sxe_jitson_new("\"\\u"),    "Failed to parse truncated unicode escape '\"\\u' (error %s)", strerror(errno));
        ok(!sxe_jitson_new(""),         "Failed to parse empty string '' (error %s)",                  strerror(errno));
        ok(!sxe_jitson_new("{\"k\"0}"), "Failed to parse object missing colon '{\"k\"0}' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("{\"k\":}"), "Failed to parse object missing value '{\"k\":}' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("{\"k\":0"), "Failed to parse object missing close '{\"k\":0' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("[0"),       "Failed to parse array missing close '[0' (error %s)",         strerror(errno));
        ok(!sxe_jitson_new("0."),       "Failed to parse invalid fraction '0.' (error %s)",            strerror(errno));
        ok(!sxe_jitson_new("1.0E"),     "Failed to parse invalid exponent '1.0E' (error %s)",          strerror(errno));
        ok(!sxe_jitson_new("fathead"),  "Failed to parse invalid token 'fathead' (error %s)",          strerror(errno));
        ok(!sxe_jitson_new("n"),        "Failed to parse invalid token 'n' (error %s)",                strerror(errno));
        ok(!sxe_jitson_new("twit"),     "Failed to parse invalid token 'twit' (error %s)",             strerror(errno));

        char json[65562];    // Big enough for an object with a > 64K member name
        json[len = 0] = '{';
        json[++len]   = '"';

        while (len < 65538)
            json[++len] = 'm';

        json[++len] = '"';
        ok(!sxe_jitson_new(json), "Failed to parse member name of 64K chanracters (error %s)", strerror(errno));
        errno = 0;
    }

    diag("Cover type to string");
    {
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_INVALID), "INVALID", "INVALID type");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NUMBER),  "number",  "number");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_STRING),  "string",  "string");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_OBJECT),  "object",  "object");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_ARRAY),   "array",   "array");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_BOOL),    "bool",    "bool");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NULL),    "null",    "null");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_MASK),    "ERROR",   "out of range type is an ERROR");
        is(errno,                                              ERANGE,    "Errno is ERANGE");
        errno = 0;
    }

    diag("Test object membership function, object duplication, and reencoding");
    {
        ok(jitson = sxe_jitson_new("{\"a\": 1, \"biglongname\": \"B\", \"c\": [2, 3], \"d\" : {\"e\": 4}, \"f\": true}"),
           "Parsed complex object (error %s)", strerror(errno));
        is(sxe_jitson_size(jitson), 16, "Object is %zu bytes", sizeof(struct sxe_jitson) * sxe_jitson_size(jitson));

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_OBJECT_GET_MEMBER);
        ok(!sxe_jitson_object_get_member(jitson, "a", 0),                   "Can't access object on failure to calloc index");
        MOCKFAIL_END_TESTS();

        ok(member = sxe_jitson_object_get_member(jitson, "a", 0),           "Object has a member 'a'");
        is(sxe_jitson_get_number(member), 1,                                "Member is the number 1");
        ok(!sxe_jitson_object_get_member(jitson, "biglongname", 1),         "Object has no member 'b'");
        ok(member = sxe_jitson_object_get_member(jitson, "biglongname", 0), "Object has a member 'biglongname'");
        is_eq(sxe_jitson_get_string(member, NULL), "B",                     "Member is the string 'B'");
        ok(member = sxe_jitson_object_get_member(jitson, "c", 0),           "Object has a member 'c'");
        is(sxe_jitson_len(member), 2,                                       "Member is an array of 2 elements");
        ok(member = sxe_jitson_object_get_member(jitson, "d", 1),           "Object has a member 'd'");
        is(sxe_jitson_len(member), 1,                                       "Member is an object with 1 member");
        ok(member = sxe_jitson_object_get_member(jitson, "f", 0),           "Object has a member 'f'");
        ok(sxe_jitson_get_bool(member),                                     "Member is 'true'");

        /* Test duplication
         */
        ok(clone = sxe_jitson_dup(jitson),                        "Duplicated the object");
        is(5, sxe_jitson_len(clone),                              "Clone has 5 members too");
        ok(member  = sxe_jitson_object_get_member(clone, "c", 0), "One of the members is 'c'");
        ok(element = sxe_jitson_array_get_element(member, 1),     "Got second element of array member 'c'");
        is(sxe_jitson_get_uint(element), 3,                       "It's the unsinged integer 3");

        is_eq(json_out = sxe_jitson_to_json(jitson, NULL),
              "{\"biglongname\":\"B\",\"a\":1,\"c\":[2,3],\"f\":true,\"d\":{\"e\":4}}",
              "Encoder spat out same JSON as we took in");

        is(sxe_jitson_size(jitson), 16, "Objects can be sized once indexed");
        sxe_jitson_free(jitson);
        free(json_out);
    }

    diag("Test array element function and reencoding");
    {
        ok(jitson = sxe_jitson_new("[0, \"anotherlongstring\", {\"member\": null}, true]"),
           "Parsed complex array (error %s)", strerror(errno));

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_ARRAY_GET_ELEMENT);
        ok(!sxe_jitson_array_get_element(jitson, 0),                        "Can't access array on failure to malloc index");
        MOCKFAIL_END_TESTS();

        ok(element = sxe_jitson_array_get_element(jitson, 0),               "Array has a element 0");
        is(sxe_jitson_get_number(element), 0,                               "Element is the number 0");
        ok(element = sxe_jitson_array_get_element(jitson, 1),               "Array has a element 1");
        is_eq(sxe_jitson_get_string(element, NULL), "anotherlongstring",    "Element is the string 'anotherlongstring'");
        ok(element = sxe_jitson_array_get_element(jitson, 2),               "Array has a element 2");
        is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_OBJECT,            "Elememt is an object");
        ok(element = sxe_jitson_array_get_element(jitson, 3),               "Array has a element 3");
        ok(sxe_jitson_get_bool(element),                                    "Element is 'true'");
        ok(!sxe_jitson_array_get_element(jitson, 4),                        "Object has no element 4");

        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[0,\"anotherlongstring\",{\"member\":null},true]",
              "Encoder spat out same JSON as we took in");
        is(sxe_jitson_size(jitson), 8, "Arrays can be sized once indexed");
        sxe_jitson_free(jitson);
        free(json_out);
    }

    diag("Test bug fixes against regressions");
    {
         ok(jitson = sxe_jitson_new("{\"A.D.\": 1, \"x\": 0}"),       "Parsed problem member name (error %s)", strerror(errno));
         ok(member = sxe_jitson_object_get_member(jitson, "A.D.", 0), "Object has a member 'A.D.'");
         is(sxe_jitson_get_type(member), SXE_JITSON_TYPE_NUMBER,      "A.D.'s value is a number");

        // Test DPT-1404b (an object that contains an empty object)
        ok(jitson = sxe_jitson_new("{\"catalog\":{\"osversion-current\":{}, \"version\": 1}}"), "Parsed object containing {}");
        ok(!sxe_jitson_object_get_member(jitson, "osversion-current", 0), "Object has no member 'osversion-current'");

        // Test DPT-1408.1 (sxe_jitson_stack_clear should clear the open collection index)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        sxe_jitson_stack_clear(stack);
        is(stack->open , 0, "Open collection flag was cleared");

        // Test DPT-1408.2 (sxe_jitson_stack_parse_string should clear the stack on failure)
        ok(!sxe_jitson_stack_parse_json(stack, "\""), "Failed to parse an unterminated string");
        is(stack->count, 0, "Stack was cleared");

        // Test DPT-1408.3 (sxe_jitson_stack_parse_string should clear the stack on failure)
        ok(!sxe_jitson_stack_parse_json(stack, "{"), "Failed to parse a truncated object");
        is(stack->count, 0, "Stack was cleared");

        // Test DPT-1408.4 (It should be possible to construct an object with an array as a member)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_name(stack, "endpoint.certificates", SXE_JITSON_TYPE_IS_COPY), "Add a member");
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),  "Member's value is an empty array");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close array - can't fail");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close outer object - can't fail");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the object from the stack");
        free(jitson);

        // Test DPT-1408.5 (sxe_jitson_object_get_member should allow non-NUL terminated member names)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT),       "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_number(stack, "a", 1),                     "Add member 'a' value 1");
        sxe_jitson_stack_close_collection(stack);                               // Close object
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                           "Got the object from the stack");
        ok(sxe_jitson_object_get_member(jitson, "a+", 1),                         "Got the member with non-terminated name");

        // Test DPT-1408.8 (Need to be able to duplicate into a collection)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),        "Opened an array on the stack");
        ok(sxe_jitson_stack_add_dup(stack, jitson),                               "Duplicated previous test object in array");
        ok(sxe_jitson_stack_add_null(stack),                                      "Followed by a null");
        sxe_jitson_stack_close_collection(stack);                               // Close array
        ok(clone   = sxe_jitson_stack_get_jitson(stack),                          "Got the array from the stack");
        ok(element = sxe_jitson_array_get_element(clone, 0),                      "Got the cloned first element");
        ok(member  = sxe_jitson_object_get_member(element, "a", 0),               "Got the member 'a' from it");
        is(sxe_jitson_get_number(member), 1,                                      "It's value is correct");
        ok(!(element->type & SXE_JITSON_TYPE_ALLOCED), "Cloned object isn't marked allocated even tho the object cloned was");
        free(clone);

        // Test DPT-1408.9 (sxe_dup should duplicate referred to jitson values)
        struct sxe_jitson reference[1];
        sxe_jitson_make_reference(reference, jitson);
        clone = sxe_jitson_dup(reference);
        ok(!sxe_jitson_is_reference(clone), "The duped reference is not itself a reference");
        free(clone);

        free(jitson);
    }

    diag("Test simple construction");
    {
        struct sxe_jitson primitive[1];

        sxe_jitson_make_null(primitive);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_NULL, "null is null");
        ok(!sxe_jitson_test(primitive),                          "null tests false");

        sxe_jitson_make_bool(primitive, true);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_BOOL, "true is a bool");
        ok(sxe_jitson_test(primitive),                           "true tests true");

        sxe_jitson_make_number(primitive, 1.3E100);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_NUMBER, "1.3E100 is a number");
        ok(sxe_jitson_test(primitive),                             "1.3E100 tests true");

        sxe_jitson_make_number(primitive, 1.0);
        is(sxe_jitson_get_uint(primitive), 1,                      "Number 1.0 is uint 1");

        sxe_jitson_make_string_ref(primitive, "hello, world");
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_STRING,    "A string_ref is a string");
        is_eq(sxe_jitson_get_string(primitive, NULL), "hello, world", "String_refs values can be retrieved");
        is(primitive->len, 0,                                         "String_refs don't know their lengths on creation");
        ok(sxe_jitson_test(primitive),                                "Non-empty string ref is true");
        is(sxe_jitson_len(primitive), 12,                             "String_ref is 12 characters");
        is(primitive->len, 12,                                        "String_refs cache their lengths");
        sxe_jitson_make_string_ref(primitive, "");
        ok(!sxe_jitson_test(primitive),                               "Empty string_ref tests false");
    }

    diag("Test object construction and duplication failures");
    {
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the object from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "{}",            "Look, it's an empty object");
        free(json_out);

        ok(clone = sxe_jitson_dup(jitson),                         "Cloned an empty array");
        is(sxe_jitson_get_type(clone), SXE_JITSON_TYPE_OBJECT,     "Clone is an '%s' (expected 'object')",
           sxe_jitson_get_type_as_str(clone));
        is(sxe_jitson_len(clone), 0,                               "Clone has 0 length (it is empty)");
        sxe_jitson_free(clone);
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT),                "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_name(stack, "null", SXE_JITSON_TYPE_IS_REF),        "Added a member name reference");
        ok(sxe_jitson_stack_add_null(stack),                                               "Added a null value");
        ok(sxe_jitson_stack_add_member_bool(stack, "bool", true),                          "Added a member 'bool' value true");
        ok(sxe_jitson_stack_add_member_number(stack, "number", 1.14159),                   "Added a member number in one call");
        ok(sxe_jitson_stack_add_member_name(stack, "hello.world", SXE_JITSON_TYPE_IS_REF), "Added long member name reference");
        ok(sxe_jitson_stack_add_string(stack, sxe_strdup("hello, world"), SXE_JITSON_TYPE_IS_OWN),
           "Added long string owned reference");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson  = sxe_jitson_stack_get_jitson(stack),              "Got the object from the stack");
        ok(element = sxe_jitson_object_get_member(jitson, "bool", 0), "Got the 'bool' member");
        ok(clone   = sxe_jitson_dup(element),                         "Cloned the bool member");
        ok(clone->type & SXE_JITSON_TYPE_ALLOCED,                     "Clone is allocated even though value wasn't");
        sxe_jitson_free(clone);
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL),
              "{\"number\":1.14159,\"bool\":true,\"hello.world\":\"hello, world\",\"null\":null}", "Got the expected object");
        free(json_out);

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_DUP);
        ok(!sxe_jitson_dup(jitson), "Can't duplicate an object if malloc fails");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_OBJECT_CLONE);
        ok(!sxe_jitson_dup(jitson), "Can't clone an object if malloc fails");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STRING_CLONE);
        ok(!sxe_jitson_dup(jitson), "Can't clone an object that contains an owned string if strdup returns NULL");
        MOCKFAIL_END_TESTS();

        sxe_jitson_free(jitson);
    }

    diag("Test array construction and string edge cases");
    {
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),  "Opened an array on the stack");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the array from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[]",            "Look, it's an empty array");
        free(json_out);

        ok(clone = sxe_jitson_dup(jitson),                         "Cloned an empty array");
        is(sxe_jitson_get_type(clone), SXE_JITSON_TYPE_ARRAY,      "Clone is an '%s' (expected 'array')",
           sxe_jitson_get_type_as_str(clone));
        is(sxe_jitson_len(clone), 0,                               "Clone has 0 length (it is empty)");
        sxe_jitson_free(clone);
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),          "Opened an array on the stack");
        ok(sxe_jitson_stack_add_string(stack, "shortly",  SXE_JITSON_TYPE_IS_COPY), "Added a copy of a short string");
        ok(sxe_jitson_stack_add_string(stack, "longerly", SXE_JITSON_TYPE_IS_COPY), "Added a copy of a longer string");
        ok(sxe_jitson_stack_add_string(stack, "longer than 23 characters", SXE_JITSON_TYPE_IS_COPY),
           "Added a copy of a long string needing more than 2 tokens");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the array from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[\"shortly\",\"longerly\",\"longer than 23 characters\"]",
              "Got the expected JSON");
        free(json_out);

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_ARRAY_CLONE);
        ok(!sxe_jitson_dup(jitson), "Can't clone an array if malloc fails");
        MOCKFAIL_END_TESTS();

        ok(clone = sxe_jitson_dup(jitson),                         "Cloned a non-empty array");
        is(sxe_jitson_get_type(clone), SXE_JITSON_TYPE_ARRAY,      "Clone is an '%s' (expected 'array')",
           sxe_jitson_get_type_as_str(clone));
        is(sxe_jitson_len(clone), 3,                               "Clone has expected length");
        sxe_jitson_free(jitson);
    }

    diag("Test references and cloning a string that's an owned reference");
    {
        struct sxe_jitson  reference[1];
        struct sxe_jitson *array;

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),              "Opened an array on the stack");
        ok(sxe_jitson_stack_add_string(stack, "one",   SXE_JITSON_TYPE_IS_COPY),        "Added a copy of a string");
        ok(sxe_jitson_stack_add_string(stack, "two",   SXE_JITSON_TYPE_IS_REF),         "Added a weak reference to a string");
        ok(sxe_jitson_stack_add_string(stack, strdup("three"), SXE_JITSON_TYPE_IS_OWN), "Strong (ownership) ref to a string");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the array from the stack");
        is(sxe_jitson_size(jitson), 4,                  "Array of 3 short strings is 4 jitsons");

        sxe_jitson_make_reference(reference, jitson);
        is(sxe_jitson_get_type(reference), SXE_JITSON_TYPE_ARRAY, "A reference to an array has type array");
        ok(sxe_jitson_test(reference),                            "A reference to a non-empty array tests true");
        is(sxe_jitson_size(reference), 1,                         "A reference to a non-empty array requires only 1 jitson");
        is(sxe_jitson_len(reference), 3,                          "A reference to an array has len of the array");
        is_eq(sxe_jitson_to_json(reference, NULL), "[\"one\",\"two\",\"three\"]", "A reference to an array's json the array's");
        ok(member = sxe_jitson_array_get_element(reference, 2),   "Can get a element of an array from a reference");
        is_eq(sxe_jitson_get_string(member, NULL), "three",       "Got the correct element");

        MOCKFAIL_START_TESTS(2, MOCK_FAIL_STRING_CLONE);
        ok(!sxe_jitson_dup(member), "Can't clone a string if strdup returns NULL");
        ok(!sxe_jitson_dup(jitson), "Can't clone an array that contains an owned string if strdup returns NULL");
        MOCKFAIL_END_TESTS();

        ok(clone = sxe_jitson_dup(member),                                            "Cloned the owned string");
        is_eq(sxe_jitson_get_string(member, NULL), "three",                           "Got the correct content");
        ok(sxe_jitson_get_string(member, NULL) != sxe_jitson_get_string(clone, NULL), "The owned string is duplicated");
        sxe_jitson_free(clone);
        sxe_jitson_free(reference);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),                 "Opened another array on the stack");
        ok(sxe_jitson_stack_add_reference(stack, sxe_jitson_array_get_element(jitson, 2)), "Added a reference to an array");
        sxe_jitson_stack_close_collection(stack);
        ok(array = sxe_jitson_stack_get_jitson(stack),      "Got the array from the stack");
        ok(member = sxe_jitson_array_get_element(array, 0), "Got the reference from the array");
        is_eq(sxe_jitson_get_string(member, NULL), "three", "Got the correct element");
        is(member->type, SXE_JITSON_TYPE_REFERENCE,         "Raw type of reference is %s (expected REFERENCE)",
                                                            sxe_jitson_type_to_str(member->type));
        sxe_jitson_free(array);
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "After freeing all references, array is still an array");
        sxe_jitson_free(jitson);
    }

    sxe_jitson_stack_free_thread();    // Currently, just for coverage

    if (test_memory() != start_memory)
        diag("Memory in use is %zu, not %zu as it was at the start of the test program", test_memory(), start_memory);

    return exit_status();
}
